// stl_slicer.cpp
#include "stl_slicer.h"
#include "detected_object.h"
#include "stl_utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <map>
#include <set>
#include <queue>

using namespace Clipper2Lib;

// 辅助函数：计算轮廓质心
static Point2D computeContourCentroid(const Contour2D& cont) {
    if (cont.empty()) return Point2D(0.0, 0.0);
    double sumX = 0.0, sumY = 0.0;
    for (const auto& p : cont) {
        sumX += p.x;
        sumY += p.y;
    }
    return Point2D(sumX / cont.size(), sumY / cont.size());
}

// 判断两个轮廓是否连通（用于内轮廓跨层合并）
static bool areInnerContoursConnected(const LayerContour& a, const LayerContour& b, double threshold) {
    if (a.isOuter || b.isOuter) return false;

    const Contour2D& contA = a.offsets.empty() ? a.original : a.offsets[0];
    const Contour2D& contB = b.offsets.empty() ? b.original : b.offsets[0];

    Point2D centA = computeContourCentroid(contA);
    Point2D centB = computeContourCentroid(contB);
    double dist = std::sqrt(std::pow(centA.x - centB.x, 2) + std::pow(centA.y - centB.y, 2));
    return dist < threshold;
}

// ---------- 几何辅助函数 ----------
double SlicePathGenerator::polygonArea(const Contour2D& poly) {
    if (poly.size() < 3) return 0.0;
    double area = 0.0;
    for (size_t i = 0; i < poly.size() - 1; ++i) {
        area += poly[i].x * poly[i+1].y - poly[i+1].x * poly[i].y;
    }
    return area * 0.5;
}

bool SlicePathGenerator::pointInPolygon(const Point2D& pt, const Contour2D& poly) {
    int wn = 0;
    size_t n = poly.size();
    for (size_t i = 0; i < n - 1; ++i) {
        const Point2D& p1 = poly[i];
        const Point2D& p2 = poly[i+1];
        if (p1.y <= pt.y) {
            if (p2.y > pt.y && ((p2.x - p1.x)*(pt.y - p1.y) - (pt.x - p1.x)*(p2.y - p1.y)) > 0)
                ++wn;
        } else {
            if (p2.y <= pt.y && ((p2.x - p1.x)*(pt.y - p1.y) - (pt.x - p1.x)*(p2.y - p1.y)) < 0)
                --wn;
        }
    }
    return wn != 0;
}

// ---------- 构造与工厂 ----------
SlicePathGenerator::SlicePathGenerator() = default;

SlicePathGenerator::SlicePathGenerator(const std::vector<std::shared_ptr<Triangler>>& triangles)
    : m_triangles(triangles) {
    if (!m_triangles.empty()) {
        double minZ = std::numeric_limits<double>::max();
        double maxZ = std::numeric_limits<double>::lowest();
        for (const auto& tri : m_triangles) {
            minZ = std::min(minZ, tri->min_z);
            maxZ = std::max(maxZ, tri->max_z);
        }
        m_params.topZ = maxZ;
        m_params.bottomZ = minZ;
    }
}

std::unique_ptr<SlicePathGenerator> SlicePathGenerator::fromSTLObject(const STLObject* obj) {
    if (!obj) return nullptr;

    std::vector<std::shared_ptr<Triangler>> tris;
    const std::string& cls = obj->getClassName();

    if (cls == "stp_model" || cls == "step_model") {
        tris = obj->getTriangles();
    } else if (cls == "stl_model") {
        const auto& verts = obj->getVertices();
        const auto& inds  = obj->getIndices();
        for (size_t i = 0; i < inds.size(); i += 3) {
            const VertexTWO& v0 = verts[inds[i]];
            const VertexTWO& v1 = verts[inds[i+1]];
            const VertexTWO& v2 = verts[inds[i+2]];
            Point3D p0(v0.position[0], v0.position[1], v0.position[2]);
            Point3D p1(v1.position[0], v1.position[1], v1.position[2]);
            Point3D p2(v2.position[0], v2.position[1], v2.position[2]);
            tris.push_back(std::make_shared<Triangler>(p0, p1, p2));
        }
    }

    if (tris.empty()) {
        qWarning() << "SlicePathGenerator: No triangles extracted from STLObject";
        return nullptr;
    }

    auto gen = std::unique_ptr<SlicePathGenerator>(new SlicePathGenerator(tris));
    return gen;
}

// ---------- 切片核心 ----------
std::vector<SliceSegment> SlicePathGenerator::sliceAtZ(double z) const {
    std::vector<SliceSegment> segments;
    for (const auto& tri : m_triangles) {
        if (tri->max_z < z - 1e-9 || tri->min_z > z + 1e-9)
            continue;

        const Point3D& v1 = tri->v1;
        const Point3D& v2 = tri->v2;
        const Point3D& v3 = tri->v3;

        std::vector<Point3D> intersections;
        auto intersect = [z](const Point3D& a, const Point3D& b) -> Point3D {
            double t = (z - a.z) / (b.z - a.z);
            return Point3D(a.x + t * (b.x - a.x),
                           a.y + t * (b.y - a.y), z);
        };
        auto addIfCrossing = [&](const Point3D& p1, const Point3D& p2) {
            if ((p1.z > z && p2.z < z) || (p1.z < z && p2.z > z))
                intersections.push_back(intersect(p1, p2));
        };
        addIfCrossing(v1, v2);
        addIfCrossing(v2, v3);
        addIfCrossing(v3, v1);

        if (intersections.size() == 2) {
            segments.emplace_back(intersections[0], intersections[1]);
        }
    }
    return segments;
}

// 线段连接成轮廓（使用 Clipper2 的布尔联合）
static double distSq(const Point2D& a, const Point2D& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return dx*dx + dy*dy;
}

std::vector<Contour2D> SlicePathGenerator::connectSegmentsToContours(const std::vector<SliceSegment>& segments) const {
    using namespace Clipper2Lib;
    if (segments.empty()) return {};

    Paths64 rawPaths;
    rawPaths.reserve(segments.size());
    for (const auto& seg : segments) {
        Path64 path;
        path.push_back(Point64(static_cast<int64_t>(seg.p1.x * 1e6),
                               static_cast<int64_t>(seg.p1.y * 1e6)));
        path.push_back(Point64(static_cast<int64_t>(seg.p2.x * 1e6),
                               static_cast<int64_t>(seg.p2.y * 1e6)));
        rawPaths.push_back(path);
    }

    Clipper64 clipper;
    clipper.AddSubject(rawPaths);
    PolyTree64 solution;
    clipper.Execute(ClipType::Union, FillRule::NonZero, solution);

    std::vector<Contour2D> contours;
    std::function<void(const PolyPath64&)> extract = [&](const PolyPath64& node) {
        const Path64& poly = node.Polygon();
        if (!poly.empty()) {
            Contour2D cont;
            cont.reserve(poly.size());
            for (const auto& pt : poly) {
                cont.emplace_back(pt.x * 1e-6, pt.y * 1e-6);
            }
            if (cont.size() > 2 && (cont.front().x != cont.back().x || cont.front().y != cont.back().y)) {
                cont.push_back(cont.front());
            }
            contours.push_back(cont);
        }
        for (const auto& child : node) {
            extract(*child);
        }
    };
    for (const auto& child : solution) {
        extract(*child);
    }

    if (!contours.empty()) {
        return contours;
    }

    // 手动连接容差逻辑（略，保持原有）
    const double tolerance = 0.01;
    const double tolSq = tolerance * tolerance;

    std::vector<Point2D> uniquePoints;
    std::vector<std::pair<int, int>> edgeIndices;

    auto getOrCreatePointIndex = [&](const Point2D& pt) -> int {
        for (size_t i = 0; i < uniquePoints.size(); ++i) {
            if (distSq(uniquePoints[i], pt) <= tolSq) {
                return static_cast<int>(i);
            }
        }
        uniquePoints.push_back(pt);
        return static_cast<int>(uniquePoints.size() - 1);
    };

    for (const auto& seg : segments) {
        Point2D p1(seg.p1.x, seg.p1.y);
        Point2D p2(seg.p2.x, seg.p2.y);
        int idx1 = getOrCreatePointIndex(p1);
        int idx2 = getOrCreatePointIndex(p2);
        if (idx1 != idx2) {
            edgeIndices.emplace_back(idx1, idx2);
        }
    }

    int n = static_cast<int>(uniquePoints.size());
    std::vector<std::vector<int>> adj(n);
    for (const auto& e : edgeIndices) {
        adj[e.first].push_back(e.second);
        adj[e.second].push_back(e.first);
    }

    std::vector<bool> visited(n, false);
    std::vector<Contour2D> manualContours;

    for (int i = 0; i < n; ++i) {
        if (visited[i]) continue;
        std::vector<int> chain;
        std::function<void(int)> dfs = [&](int u) {
            visited[u] = true;
            chain.push_back(u);
            for (int v : adj[u]) {
                if (!visited[v]) {
                    dfs(v);
                    break;
                }
            }
        };
        dfs(i);
        if (chain.size() >= 3) {
            int first = chain.front();
            int last = chain.back();
            bool isClosed = false;
            for (int v : adj[last]) {
                if (v == first) {
                    isClosed = true;
                    break;
                }
            }
            if (!isClosed && distSq(uniquePoints[first], uniquePoints[last]) <= tolSq) {
                isClosed = true;
            }
            if (isClosed) {
                Contour2D cont;
                for (int idx : chain) {
                    cont.push_back(uniquePoints[idx]);
                }
                cont.push_back(uniquePoints[first]);
                manualContours.push_back(cont);
            }
        }
    }

    if (!manualContours.empty()) {
        qDebug() << "手动连接成功，生成轮廓数:" << manualContours.size();
        return manualContours;
    }

    qDebug() << "手动连接也未产生轮廓，线段数:" << segments.size();
    return {};
}

// 轮廓偏置（使用 Clipper2 的 InflatePaths）
std::vector<Contour2D> SlicePathGenerator::offsetContour(const Contour2D& contour, double distance) const {
    using namespace Clipper2Lib;
    if (std::abs(distance) < 1e-9) {
        return {contour};
    }

    Path64 path;
    path.reserve(contour.size());
    for (const auto& pt : contour) {
        path.push_back(Point64(static_cast<int64_t>(pt.x * 1e6),
                               static_cast<int64_t>(pt.y * 1e6)));
    }
    if (path.size() > 1 && path.front() == path.back()) {
        path.pop_back();
    }

    Paths64 result = InflatePaths(Paths64{path}, distance * 1e6,
                                  JoinType::Round, EndType::Polygon, 2.0);

    std::vector<Contour2D> out;
    out.reserve(result.size());
    for (const auto& p : result) {
        Contour2D cont;
        cont.reserve(p.size() + 1);
        for (const auto& pt : p) {
            cont.emplace_back(pt.x * 1e-6, pt.y * 1e-6);
        }
        if (!cont.empty() && (cont.front().x != cont.back().x || cont.front().y != cont.back().y)) {
            cont.push_back(cont.front());
        }
        out.push_back(cont);
    }
    return out;
}

// 构建轮廓层级（外/内判定）
std::vector<LayerContour> SlicePathGenerator::buildContourHierarchy(const std::vector<Contour2D>& rawContours) const {
    std::vector<LayerContour> result;
    if (rawContours.empty()) return result;

    for (const auto& cont : rawContours) {
        int level = 0;
        for (const auto& other : rawContours) {
            if (&cont == &other) continue;
            if (pointInPolygon(cont[0], other))
                ++level;
        }
        LayerContour lc;
        lc.original = cont;
        lc.level = level;
        lc.area = polygonArea(cont);
        lc.isOuter = (level % 2 == 0);
        result.push_back(lc);
    }
    return result;
}

std::vector<Point3D> SlicePathGenerator::getAllPathPoints() const {
    std::vector<Point3D> all;
    for (const auto& layer : m_layers) {
        all.insert(all.end(), layer.pathPoints.begin(), layer.pathPoints.end());
    }
    return all;
}

bool SlicePathGenerator::generateContoursOnly() {
    m_layers.clear();
    if (m_triangles.empty()) {
        qWarning() << "SlicePathGenerator: 无三角面片数据";
        return false;
    }

    double modelMinZ = std::numeric_limits<double>::max();
    double modelMaxZ = std::numeric_limits<double>::lowest();
    for (const auto& tri : m_triangles) {
        modelMinZ = std::min(modelMinZ, tri->min_z);
        modelMaxZ = std::max(modelMaxZ, tri->max_z);
    }
    qDebug() << "模型实际 Z 范围: [" << modelMinZ << ", " << modelMaxZ << "]";

    double startZ = m_params.topZ;
    double endZ   = m_params.bottomZ;

    if (std::isinf(startZ) && startZ > 0) {
        startZ = modelMaxZ;
    }
    if (std::isinf(endZ) && endZ < 0) {
        endZ = modelMinZ;
    }

    if (startZ < endZ) {
        std::swap(startZ, endZ);
    }

    double step = m_params.layerHeight;
    if (step <= 1e-9) {
        qWarning() << "SlicePathGenerator: 层高无效 (" << step << ")";
        return false;
    }

    int layerCount = static_cast<int>(std::ceil((startZ - endZ) / step)) + 1;
    layerCount = std::max(1, layerCount);

    qDebug() << "切片范围: Z 从" << startZ << "到" << endZ
             << ", 层高:" << step << ", 总层数:" << layerCount;

    double toolRadius = m_params.toolDiameter / 2.0;
    double baseOffset = toolRadius + m_params.sideMargin;

    qDebug() << "偏置量(首圈):" << baseOffset << ", 步距:" << m_params.stepover;
    qDebug() << "尺寸限制 - 矩形: 宽=" << m_params.maxAllowedWidth
             << ", 高=" << m_params.maxAllowedHeight
             << ", 圆形半径=" << m_params.maxAllowedRadius;

    int globalContourId = 0;

    for (int i = 0; i < layerCount; ++i) {
        double z = startZ - i * step;
        if (z < endZ - 1e-9) {
            z = endZ;
        }

        qDebug() << "正在处理层 Z =" << z;

        auto segments = sliceAtZ(z);
        if (segments.empty()) {
            qDebug() << "  该层无线段";
            continue;
        }

        std::vector<Contour2D> rawContours = connectSegmentsToContours(segments);
        if (rawContours.empty()) {
            qDebug() << "  未能连接成轮廓";
            continue;
        }

        auto hierContours = buildContourHierarchy(rawContours);

        LayerData layer;
        layer.z = z;
        layer.rawSegments = std::move(segments);

        for (auto& hc : hierContours) {
            const Contour2D& cont = hc.original;
            bool closed = false;
            if (cont.size() >= 3) {
                double dx = cont.front().x - cont.back().x;
                double dy = cont.front().y - cont.back().y;
                closed = (std::sqrt(dx*dx + dy*dy) < 1e-6);
            }
            hc.isClosed = closed;
            hc.id = globalContourId++;

            if (hc.isOuter) {
                int pass = 0;
                bool stopFurtherOffset = false;

                while (pass < m_params.maxOffsetPasses && !stopFurtherOffset) {
                    double currentOffset = baseOffset + pass * m_params.stepover;
                    auto offsetRes = offsetContour(hc.original, currentOffset);
                    if (offsetRes.empty()) break;

                    for (const auto& ring : offsetRes) {
                        if (std::abs(polygonArea(ring)) < 0.5) continue;

                        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
                        for (const auto& pt : ring) {
                            minX = std::min(minX, pt.x);
                            maxX = std::max(maxX, pt.x);
                            minY = std::min(minY, pt.y);
                            maxY = std::max(maxY, pt.y);
                        }
                        double width  = maxX - minX;
                        double height = maxY - minY;

                        bool exceedLimit = false;
                        if (m_params.maxAllowedWidth > 0 && m_params.maxAllowedHeight > 0) {
                            if (width > m_params.maxAllowedWidth || height > m_params.maxAllowedHeight) {
                                exceedLimit = true;
                                qDebug() << "外轮廓偏置超出矩形限制，停止。当前尺寸:" << width << "x" << height;
                            }
                        } else if (m_params.maxAllowedRadius > 0) {
                            double radius = std::sqrt(width*width + height*height) * 0.5;
                            if (radius > m_params.maxAllowedRadius) {
                                exceedLimit = true;
                                qDebug() << "外轮廓偏置超出圆形限制，停止。当前半径:" << radius;
                            }
                        }

                        if (exceedLimit) {
                            stopFurtherOffset = true;
                            break;
                        }

                        hc.offsets.push_back(ring);
                    }

                    ++pass;
                }

                if (hc.offsets.empty()) {
                    hc.offsets.push_back(hc.original);
                }
            } else { // 内轮廓
                // 第一圈：向内偏置 baseOffset（实线）
                double firstOffset = -baseOffset;
                auto firstRes = offsetContour(hc.original, firstOffset);
                if (!firstRes.empty()) {
                    // 第一圈的所有结果（可能分裂）
                    for (const auto& ring : firstRes) {
                        if (std::abs(polygonArea(ring)) < 0.5) continue;
                        hc.offsets.push_back(ring);  // 实线
                    }

                    // 后续圈：从每个第一圈结果继续向内偏置 stepover（虚线）
                    if (m_params.stepover > 1e-6) {
                        // 使用队列进行广度优先处理，支持分裂
                        std::deque<Contour2D> queue;
                        for (const auto& ring : firstRes) {
                            if (std::abs(polygonArea(ring)) >= 0.5) {
                                queue.push_back(ring);
                            }
                        }

                        int pass = 1;
                        while (!queue.empty() && pass < m_params.maxOffsetPasses) {
                            std::deque<Contour2D> nextQueue;
                            for (const auto& currentRing : queue) {
                                auto nextRes = offsetContour(currentRing, -m_params.stepover);
                                for (const auto& nextRing : nextRes) {
                                    if (std::abs(polygonArea(nextRing)) < 0.5) continue;
                                    hc.offsets.push_back(nextRing);  // 虚线
                                    nextQueue.push_back(nextRing);
                                }
                            }
                            queue = std::move(nextQueue);
                            ++pass;
                        }
                    }
                } else {
                    // 偏置失败时保留原始轮廓
                    hc.offsets.push_back(hc.original);
                }
            }
            layer.contours.push_back(hc);
        }
        m_layers.push_back(layer);
        qDebug() << "  生成轮廓数:" << layer.contours.size();
    }

    qDebug() << "切片完成，共生成" << m_layers.size() << "层轮廓";
    return !m_layers.empty();
}

ContourGroups SlicePathGenerator::groupContours() const {
    ContourGroups result;
    if (m_layers.empty()) return result;

    std::vector<Quantity_Color> colorPalette = {
        Quantity_Color(0.0, 1.0, 0.0, Quantity_TOC_RGB),   // 绿色
        Quantity_Color(1.0, 0.0, 0.0, Quantity_TOC_RGB),   // 红色
        Quantity_Color(1.0, 1.0, 0.0, Quantity_TOC_RGB),   // 黄色
        Quantity_Color(1.0, 0.0, 1.0, Quantity_TOC_RGB),   // 品红
        Quantity_Color(0.0, 1.0, 1.0, Quantity_TOC_RGB),   // 青色
        Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB)    // 灰色
    };

    // 1. 外轮廓组
    std::vector<GroupedContour> outerGroup;
    for (size_t li = 0; li < m_layers.size(); ++li) {
        const LayerData& layer = m_layers[li];
        double z = layer.z;
        for (size_t ci = 0; ci < layer.contours.size(); ++ci) {
            const LayerContour& lc = layer.contours[ci];
            if (lc.isOuter) {
                GroupedContour gc;
                gc.z = z;
                gc.contour = &lc;
                gc.groupId = 0;
                gc.indexInGroup = -1;
                outerGroup.push_back(gc);
            }
        }
    }

    std::sort(outerGroup.begin(), outerGroup.end(),
              [](const GroupedContour& a, const GroupedContour& b) {
                  return a.z > b.z;
              });
    for (size_t i = 0; i < outerGroup.size(); ++i) {
        outerGroup[i].indexInGroup = static_cast<int>(i);
    }

    if (!outerGroup.empty()) {
        result.groups.push_back(outerGroup);
        result.groupColors.push_back(Quantity_Color(0.0, 0.0, 1.0, Quantity_TOC_RGB)); // 蓝色
    }

    // 2. 内轮廓分组
    struct InnerItem {
        int layerIdx;
        int contourIdx;
        const LayerContour* ptr;
        double z;
    };
    std::vector<InnerItem> innerItems;
    for (size_t li = 0; li < m_layers.size(); ++li) {
        const LayerData& layer = m_layers[li];
        for (size_t ci = 0; ci < layer.contours.size(); ++ci) {
            const LayerContour& lc = layer.contours[ci];
            if (!lc.isOuter) {
                innerItems.push_back({static_cast<int>(li), static_cast<int>(ci), &lc, layer.z});
            }
        }
    }

    if (!innerItems.empty()) {
        std::vector<int> parent(innerItems.size());
        for (size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);
        std::function<int(int)> find = [&](int x) {
            return parent[x] == x ? x : parent[x] = find(parent[x]);
        };
        auto unite = [&](int a, int b) {
            int ra = find(a), rb = find(b);
            if (ra != rb) parent[ra] = rb;
        };

        const double mergeThreshold = 5.0;
        for (size_t i = 0; i < innerItems.size(); ++i) {
            for (size_t j = i + 1; j < innerItems.size(); ++j) {
                const InnerItem& itemA = innerItems[i];
                const InnerItem& itemB = innerItems[j];
                if (std::abs(itemA.layerIdx - itemB.layerIdx) == 1) {
                    if (areInnerContoursConnected(*itemA.ptr, *itemB.ptr, mergeThreshold)) {
                        unite(static_cast<int>(i), static_cast<int>(j));
                    }
                }
            }
        }

        std::unordered_map<int, std::vector<int>> groupMap;
        for (size_t i = 0; i < innerItems.size(); ++i) {
            groupMap[find(static_cast<int>(i))].push_back(static_cast<int>(i));
        }

        int innerGroupId = 1;
        for (const auto& entry : groupMap) {
            const std::vector<int>& indices = entry.second;
            std::vector<GroupedContour> groupContours;
            for (int idx : indices) {
                const InnerItem& item = innerItems[idx];
                GroupedContour gc;
                gc.z = item.z;
                gc.contour = item.ptr;
                gc.groupId = innerGroupId;
                gc.indexInGroup = -1;
                groupContours.push_back(gc);
            }
            std::sort(groupContours.begin(), groupContours.end(),
                      [](const GroupedContour& a, const GroupedContour& b) {
                          return a.z > b.z;
                      });
            for (size_t k = 0; k < groupContours.size(); ++k) {
                groupContours[k].indexInGroup = static_cast<int>(k);
            }

            result.groups.push_back(groupContours);
            int colorIdx = (innerGroupId - 1) % colorPalette.size();
            result.groupColors.push_back(colorPalette[colorIdx]);
            ++innerGroupId;
        }
    }

    return result;
}

// ==================== 新增：刀具路径生成实现 ====================

void SlicePathGenerator::discretizeContour(const Contour2D& contour, double z, double spacing,
                                           std::vector<Point3D>& outPoints, bool reverseDir) {
    if (contour.size() < 2) return;

    std::vector<Point2D> working = contour;
    if (reverseDir) {
        std::reverse(working.begin(), working.end());
    }

    bool isClosed = (working.size() > 1 &&
                     std::hypot(working.front().x - working.back().x,
                                working.front().y - working.back().y) < 1e-6);
    if (isClosed) {
        working.pop_back();
    }

    for (size_t i = 0; i < working.size(); ++i) {
        const Point2D& p1 = working[i];
        const Point2D& p2 = working[(i + 1) % working.size()];
        double segLen = std::hypot(p2.x - p1.x, p2.y - p1.y);
        int numSteps = std::max(1, static_cast<int>(segLen / spacing));
        for (int s = 0; s < numSteps; ++s) {
            double t = static_cast<double>(s) / numSteps;
            double x = p1.x + t * (p2.x - p1.x);
            double y = p1.y + t * (p2.y - p1.y);
            outPoints.emplace_back(x, y, z);
        }
    }
    if (!outPoints.empty() && outPoints.front().distanceTo(outPoints.back()) > 1e-6) {
        outPoints.push_back(outPoints.front());
    }
}

Point2D SlicePathGenerator::computeContourCentroid(const Contour2D& cont) const {
    if (cont.empty()) return Point2D(0,0);
    double sumX = 0, sumY = 0;
    for (const auto& p : cont) {
        sumX += p.x;
        sumY += p.y;
    }
    return Point2D(sumX / cont.size(), sumY / cont.size());
}

int SlicePathGenerator::findClosestPointOnContour(const std::vector<Point3D>& points, const Point2D& ref) {
    if (points.empty()) return -1;
    int bestIdx = 0;
    double bestDistSq = std::numeric_limits<double>::max();
    for (size_t i = 0; i < points.size(); ++i) {
        double dx = points[i].x - ref.x;
        double dy = points[i].y - ref.y;
        double d2 = dx*dx + dy*dy;
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

bool SlicePathGenerator::generateSpiralPoints(const Point3D& center, double startRadius, double endRadius,
                                              double startZ, double endZ, double angleStep,
                                              std::vector<Point3D>& outPoints) {
    if (startRadius <= 0 || endRadius < 0 || startZ == endZ) return false;
    double totalAngle = 2 * M_PI * 2;
    double radiusStep = (endRadius - startRadius) / (totalAngle / angleStep);
    double zStep = (endZ - startZ) / (totalAngle / angleStep);
    double angle = 0.0;
    double radius = startRadius;
    double z = startZ;
    while ((z - endZ) * (startZ - endZ) > 0) {
        double x = center.x + radius * cos(angle);
        double y = center.y + radius * sin(angle);
        outPoints.emplace_back(x, y, z);
        angle += angleStep;
        radius += radiusStep;
        z += zStep;
    }
    outPoints.emplace_back(center.x + endRadius * cos(totalAngle),
                           center.y + endRadius * sin(totalAngle),
                           endZ);
    return !outPoints.empty();
}

Point2D SlicePathGenerator::computeMaxInscribedCircleCenter(const Contour2D& contour, double& outRadius) {
    using namespace Clipper2Lib;

    qDebug() << "              【内切圆计算】开始偏置迭代，轮廓点数:" << contour.size();

    Path64 path;
    path.reserve(contour.size());
    for (const auto& pt : contour) {
        path.push_back(Point64(static_cast<int64_t>(pt.x * 1e6),
                               static_cast<int64_t>(pt.y * 1e6)));
    }
    if (path.size() > 1 && path.front() == path.back()) path.pop_back();

    double stepMM = m_params.stepover;
    if (stepMM <= 1e-9) stepMM = 0.1;
    int64_t step = static_cast<int64_t>(stepMM * 1e6);

    int maxIter = m_params.maxOffsetPasses;
    if (maxIter <= 0) maxIter = 300;

    qDebug() << "              【内切圆计算】步长:" << stepMM << "mm, 最大迭代次数:" << maxIter;

    Paths64 current = {path};
    double offset = 0.0;
    int iter = 0;

    while (!current.empty() && iter < maxIter) {
        offset += step;
        Paths64 next = InflatePaths(current, -step, JoinType::Round, EndType::Polygon, 2.0);

        if (iter % 50 == 0) {
            qDebug() << "                迭代" << iter << ": 当前偏移量=" << offset * 1e-6
                     << "mm, 轮廓数=" << current.size() << ", 下一轮轮廓数=" << next.size();
        }

        current = std::move(next);
        ++iter;
    }

    if (iter >= maxIter) {
        qWarning() << "              【内切圆计算】达到最大迭代次数" << maxIter << "，停止偏置";
    }

    offset -= step;
    Paths64 lastValid = InflatePaths(Paths64{path}, -offset, JoinType::Round, EndType::Polygon, 2.0);
    if (lastValid.empty()) {
        qWarning() << "              【内切圆计算】未能获取有效偏置环，回退到质心";
        outRadius = 0.0;
        return computeContourCentroid(contour);
    }

    const Path64& inner = lastValid[0];
    double sumX = 0.0, sumY = 0.0;
    for (const auto& pt : inner) {
        sumX += pt.x * 1e-6;
        sumY += pt.y * 1e-6;
    }
    Point2D center(sumX / inner.size(), sumY / inner.size());
    outRadius = offset * 1e-6;

    qDebug() << "              【内切圆计算】完成，圆心:(" << center.x << "," << center.y
             << "), 半径:" << outRadius << "mm, 迭代次数:" << iter;

    return center;
}

Point2D SlicePathGenerator::computeInwardNormalAtPoint(const Contour2D& contour, size_t idx) {
    size_t n = contour.size();
    if (n < 3) return Point2D(0,0);
    const Point2D& p = contour[idx];
    const Point2D& prev = contour[(idx + n - 1) % n];
    const Point2D& next = contour[(idx + 1) % n];

    double tx = next.x - prev.x;
    double ty = next.y - prev.y;
    double len = std::sqrt(tx*tx + ty*ty);
    if (len < 1e-9) return Point2D(1,0);
    tx /= len; ty /= len;

    double nx = ty;
    double ny = -tx;

    Point2D test(p.x + nx * 0.01, p.y + ny * 0.01);
    if (!pointInPolygon(test, contour)) {
        nx = -nx;
        ny = -ny;
    }
    return Point2D(nx, ny);
}

bool SlicePathGenerator::generateSpiralEntry(const Contour2D& innerContour, double layerZ,
                                             double startZ, int contourId, bool isOuter,
                                             std::vector<ToolpathPoint>& outSpiral)
{
    qDebug() << "            【螺旋下刀】开始生成，内轮廓点数:" << innerContour.size()
             << ", 起始Z(安全高度):" << startZ << ", 目标Z(切削层):" << layerZ;

    // 1. 计算最大内切圆圆心及半径
    double maxRadius = 0.0;
    Point2D center = computeMaxInscribedCircleCenter(innerContour, maxRadius);
    if (maxRadius < 1e-6) {
        qWarning() << "            【螺旋下刀】内切圆半径为0，无法生成螺旋，返回失败";
        return false;
    }
    qDebug() << "            【螺旋下刀】内切圆圆心:(" << center.x << "," << center.y
             << "), 最大半径:" << maxRadius;

    // 2. 确定螺旋半径：取参数螺旋半径与内切圆半径的较小值（并留10%安全余量）
    double desiredSpiralR = m_params.spiralRadius;
    if (maxRadius < desiredSpiralR) {
        double newRadius = maxRadius * 0.9;
        qDebug() << "            【螺旋下刀】螺旋半径" << desiredSpiralR
                 << "大于内切圆半径，调整为" << newRadius;
        desiredSpiralR = newRadius;
    }
    double spiralR = std::min(desiredSpiralR, maxRadius * 0.9);
    qDebug() << "            【螺旋下刀】实际螺旋半径:" << spiralR;

    // 3. 寻找切入点：离圆心最近的点（保证螺旋终点自然过渡到轮廓上）
    std::vector<Point3D> discPts;
    discretizeContour(innerContour, layerZ, m_params.pointSpacing, discPts, false);
    if (discPts.empty()) {
        qWarning() << "            【螺旋下刀】离散化内轮廓失败";
        return false;
    }

    int bestIdx = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (size_t i = 0; i < discPts.size(); ++i) {
        double dx = discPts[i].x - center.x;
        double dy = discPts[i].y - center.y;
        double d2 = dx*dx + dy*dy;
        if (d2 < bestDist) {
            bestDist = d2;
            bestIdx = static_cast<int>(i);
        }
    }
    Point3D cutStart = discPts[bestIdx];
    qDebug() << "            【螺旋下刀】切入点:(" << cutStart.x << "," << cutStart.y << ")";

    // 4. 计算切入点处的内法线方向，作为螺旋起始方位角
    Point2D normal = computeInwardNormalAtPoint(innerContour, bestIdx);
    double startAngle = std::atan2(normal.y, normal.x);
    qDebug() << "            【螺旋下刀】法线方向:(" << normal.x << "," << normal.y
             << "), 起始角度(弧度):" << startAngle;

    // 5. 计算螺旋参数（确保向下移动，zStep 为负）
    double slopeRad = m_params.spiralSlope * M_PI / 180.0;
    double totalAngle = 2.0 * M_PI * 1.5;   // 1.5圈
    double zDiff = std::abs(startZ - layerZ);
    double pitch = 2.0 * M_PI * std::tan(slopeRad) * spiralR;  // 每转下降高度（近似）
    if (std::abs(pitch) < 1e-9) pitch = zDiff / 1.5;          // 若螺旋角为0，则按圈数分配

    double angleStep = 0.1;                                   // 弧度步长
    // 关键修正：确保 zStep 为负值（向下）
    double zStep = -std::abs(pitch * angleStep / (2.0 * M_PI));
    double radiusStep = spiralR * angleStep / totalAngle;      // 每步半径衰减

    qDebug() << "            【螺旋下刀】螺距:" << pitch << ", 每步Z增量:" << zStep
             << ", 每步半径衰减:" << radiusStep;

    // 6. 生成螺旋点（从安全高度逐渐下降到切削层，半径逐渐减小到0）
    double angle = startAngle;
    double radius = spiralR;
    double z = startZ;
    int iter = 0;
    const int maxIter = 10000;

    // 修正循环条件：z > layerZ（因为 zStep 为负，z 逐渐减小）
    while (z > layerZ + 1e-6 && iter < maxIter) {
        double x = center.x + radius * std::cos(angle);
        double y = center.y + radius * std::sin(angle);
        outSpiral.emplace_back(x, y, z, ToolpathPointType::SpiralPoint, contourId, 0, isOuter);

        angle += angleStep;
        radius = std::max(0.0, radius - radiusStep);
        z += zStep;  // zStep 为负，z 减小
        ++iter;

        if (iter % 1000 == 0) {
            qDebug() << "              【螺旋下刀】迭代中... 当前z=" << z << ", radius=" << radius;
        }
    }

    if (iter >= maxIter) {
        qWarning() << "            【螺旋下刀】超过最大迭代次数，终止生成";
        return false;
    }

    // 确保终点在切削层且位于切入点
    outSpiral.emplace_back(cutStart.x, cutStart.y, layerZ, ToolpathPointType::SpiralPoint, contourId, 0, isOuter);

    qDebug() << "            【螺旋下刀】生成完成，共" << outSpiral.size() << "个点";
    return true;
}

bool SlicePathGenerator::generateArcEntry(const Point3D& cutStart, const Point2D& inwardNormal,
                                          double arcRadius, double arcAngleDeg,
                                          double layerZ, int contourId, int passIdx, bool isOuter,
                                          std::vector<ToolpathPoint>& outArc)
{
    qDebug() << "            【圆弧进刀】半径:" << arcRadius << ", 角度:" << arcAngleDeg;

    double arcAngle = arcAngleDeg * M_PI / 180.0;
    Point2D tangent(-inwardNormal.y, inwardNormal.x);
    Point2D center(cutStart.x + inwardNormal.x * arcRadius,
                   cutStart.y + inwardNormal.y * arcRadius);
    Point2D startPt(center.x - tangent.x * arcRadius,
                    center.y - tangent.y * arcRadius);

    int steps = std::max(5, static_cast<int>(arcAngle * arcRadius / 0.2));
    qDebug() << "            【圆弧进刀】离散步数:" << steps;

    for (int i = 0; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double theta = t * arcAngle;
        double cosT = std::cos(theta);
        double sinT = std::sin(theta);
        double dx = startPt.x - center.x;
        double dy = startPt.y - center.y;
        double x = center.x + dx * cosT - dy * sinT;
        double y = center.y + dx * sinT + dy * cosT;

        ToolpathPointType type = (i == 0) ? ToolpathPointType::PlungeStart : ToolpathPointType::CutPoint;
        outArc.emplace_back(x, y, layerZ, type, contourId, passIdx, isOuter);
    }

    qDebug() << "            【圆弧进刀】生成完成，共" << outArc.size() << "个点";
    return true;
}

bool SlicePathGenerator::buildToolpathForContour(const LayerContour& lc, double layerZ, int contourId,
                                                 bool isOuter, ContourToolpath& result)
{
    qDebug() << "      【构建轮廓路径】轮廓" << contourId << ", 类型:" << (isOuter ? "外轮廓" : "内轮廓")
             << ", 层高:" << layerZ << ", 偏置圈数:" << lc.offsets.size();

    result.contourId = contourId;
    result.isOuter = isOuter;
    result.layerZ = layerZ;
    result.passes.clear();

    if (lc.offsets.empty()) {
        qWarning() << "      【构建轮廓路径】轮廓" << contourId << "无偏置数据";
        return false;
    }

    bool reverseDir = false;
    if (m_params.climbMilling) {
        reverseDir = isOuter ? false : true;
    } else {
        reverseDir = isOuter ? true : false;
    }
    qDebug() << "        【加工方向】" << (reverseDir ? "逆铣" : "顺铣");

    const Contour2D& innerContour = lc.offsets.back();

    qDebug() << "        【计算内切圆】正在计算最大内切圆圆心...";
    double maxInscribedRadius = 0.0;
    Point2D inscribedCenter = computeMaxInscribedCircleCenter(innerContour, maxInscribedRadius);
    if (maxInscribedRadius < 1e-6) {
        inscribedCenter = computeContourCentroid(innerContour);
        qDebug() << "        【计算内切圆】计算失败，使用轮廓质心代替。半径:" << maxInscribedRadius;
    } else {
        qDebug() << "        【计算内切圆】圆心:(" << inscribedCenter.x << "," << inscribedCenter.y
                 << "), 半径:" << maxInscribedRadius;
    }

    for (size_t passIdx = 0; passIdx < lc.offsets.size(); ++passIdx) {
        const Contour2D& offsetCont = lc.offsets[passIdx];
        qDebug() << "        【偏置圈" << passIdx << "】开始处理...";

        std::vector<Point3D> rawPoints;
        discretizeContour(offsetCont, layerZ, m_params.pointSpacing, rawPoints, reverseDir);
        if (rawPoints.empty()) {
            qWarning() << "          【偏置圈" << passIdx << "】离散化后无点，跳过";
            continue;
        }
        qDebug() << "          【偏置圈" << passIdx << "】离散化得到" << rawPoints.size() << "个点";

        int entryLocal = findClosestPointOnContour(rawPoints, inscribedCenter);
        if (entryLocal < 0) entryLocal = 0;
        qDebug() << "          【偏置圈" << passIdx << "】切入点索引:" << entryLocal
                 << ", 坐标:(" << rawPoints[entryLocal].x << "," << rawPoints[entryLocal].y << ")";

        std::vector<Point3D> reordered;
        int n = static_cast<int>(rawPoints.size());
        for (int i = 0; i < n; ++i) {
            int idx = (entryLocal + i) % n;
            reordered.push_back(rawPoints[idx]);
        }

        OffsetPassToolpath pass;
        pass.passIndex = static_cast<int>(passIdx);
        pass.geometry = offsetCont;
        pass.entryIdx = 0;
        pass.exitIdx = static_cast<int>(reordered.size()) - 1;

        // 圆弧进刀（偏置圈数 > 1 时）
        if (lc.offsets.size() > 1 && m_params.useArcEntry && m_params.entryArcRadius > 1e-6) {
            qDebug() << "          【偏置圈" << passIdx << "】启用圆弧进刀，半径:" << m_params.entryArcRadius;
            Point3D cutStart = reordered.front();
            size_t localIdx = 0;
            double minD = std::numeric_limits<double>::max();
            for (size_t i = 0; i < offsetCont.size(); ++i) {
                double dx = offsetCont[i].x - cutStart.x;
                double dy = offsetCont[i].y - cutStart.y;
                double d2 = dx*dx + dy*dy;
                if (d2 < minD) { minD = d2; localIdx = i; }
            }
            Point2D normal = computeInwardNormalAtPoint(offsetCont, localIdx);
            if (reverseDir) { normal.x = -normal.x; normal.y = -normal.y; }
            std::vector<ToolpathPoint> arcPts;
            if (generateArcEntry(cutStart, normal, m_params.entryArcRadius, m_params.entryArcAngle,
                                 layerZ, contourId, static_cast<int>(passIdx), isOuter, arcPts)) {
                pass.spiralPoints.insert(pass.spiralPoints.begin(), arcPts.begin(), arcPts.end());
                qDebug() << "            【圆弧进刀】生成" << arcPts.size() << "个圆弧点";
            }
        }

        // 螺旋下刀（仅第一圈）
        if (passIdx == 0) {
            std::vector<ToolpathPoint> spiralPts;
            if (m_params.useSpiralEntry) {
                qDebug() << "          【偏置圈0】启用螺旋下刀，半径:" << m_params.spiralRadius << ", 角度:" << m_params.spiralSlope;
                if (generateSpiralEntry(innerContour, layerZ, m_params.safeZ, contourId, isOuter, spiralPts)) {
                    qDebug() << "            【螺旋下刀】生成" << spiralPts.size() << "个点";
                } else {
                    qWarning() << "            【螺旋下刀】生成失败，改用垂直下刀";
                    ToolpathPoint plungeStart(reordered.front().x, reordered.front().y, m_params.safeZ,
                                              ToolpathPointType::PlungeStart, contourId, 0, isOuter);
                    ToolpathPoint plungeEnd(reordered.front().x, reordered.front().y, layerZ,
                                            ToolpathPointType::PlungeEnd, contourId, 0, isOuter);
                    spiralPts.push_back(plungeStart);
                    spiralPts.push_back(plungeEnd);
                }
            } else {
                qDebug() << "          【偏置圈0】使用垂直下刀";
                ToolpathPoint plungeStart(reordered.front().x, reordered.front().y, m_params.safeZ,
                                          ToolpathPointType::PlungeStart, contourId, 0, isOuter);
                ToolpathPoint plungeEnd(reordered.front().x, reordered.front().y, layerZ,
                                        ToolpathPointType::PlungeEnd, contourId, 0, isOuter);
                spiralPts.push_back(plungeStart);
                spiralPts.push_back(plungeEnd);
            }
            pass.spiralPoints = spiralPts;
            pass.spiralStartIdx = 0;
            pass.spiralEndIdx = static_cast<int>(pass.spiralPoints.size()) - 1;
        } else {
            Point3D cutStart = reordered.front();
            ToolpathPoint rapid(cutStart.x, cutStart.y, m_params.safeZ, ToolpathPointType::RapidMove,
                                contourId, static_cast<int>(passIdx), isOuter);
            ToolpathPoint plungeStart(cutStart.x, cutStart.y, m_params.safeZ,
                                      ToolpathPointType::PlungeStart, contourId, static_cast<int>(passIdx), isOuter);
            ToolpathPoint plungeEnd(cutStart.x, cutStart.y, layerZ,
                                    ToolpathPointType::PlungeEnd, contourId, static_cast<int>(passIdx), isOuter);
            pass.spiralPoints.push_back(rapid);
            pass.spiralPoints.push_back(plungeStart);
            pass.spiralPoints.push_back(plungeEnd);
            pass.spiralStartIdx = 0;
            pass.spiralEndIdx = 2;
        }

        // 切削点
        for (size_t i = 0; i < reordered.size(); ++i) {
            ToolpathPointType type = ToolpathPointType::CutPoint;
            if (i == 0) type = ToolpathPointType::CutStart;
            else if (i == static_cast<int>(reordered.size()) - 1) type = ToolpathPointType::CutEnd;
            ToolpathPoint tp(reordered[i].x, reordered[i].y, reordered[i].z, type,
                             contourId, static_cast<int>(passIdx), isOuter);
            pass.points.push_back(tp);
        }

        // 抬刀
        Point3D lastCut = reordered.back();
        ToolpathPoint retractStart(lastCut.x, lastCut.y, lastCut.z, ToolpathPointType::RetractStart,
                                   contourId, static_cast<int>(passIdx), isOuter);
        ToolpathPoint retractEnd(lastCut.x, lastCut.y, m_params.safeZ, ToolpathPointType::RetractEnd,
                                 contourId, static_cast<int>(passIdx), isOuter);
        pass.points.push_back(retractStart);
        pass.points.push_back(retractEnd);

        result.passes.push_back(std::move(pass));
        qDebug() << "          【偏置圈" << passIdx << "】处理完成，切削点数:" << reordered.size();
    }

    if (result.passes.empty()) {
        qWarning() << "      【构建轮廓路径】轮廓" << contourId << "未生成任何偏置圈路径";
        return false;
    }

    result.finalRetractStart = result.passes.back().points[result.passes.back().points.size() - 2];
    result.finalRetractEnd   = result.passes.back().points.back();

    qDebug() << "      【构建轮廓路径】轮廓" << contourId << "完成，共" << result.passes.size() << "个偏置圈";
    return true;
}

bool SlicePathGenerator::buildToolpathForGroup(const std::vector<GroupedContour>& groupContours,
                                               int groupId, const Quantity_Color& color,
                                               ContourGroupToolpath& groupResult)
{
    groupResult.groupId = groupId;
    groupResult.color = color;
    groupResult.contours.clear();

    qDebug() << "  【构建组路径】组" << groupId << "，轮廓数：" << groupContours.size();

    int nextContourId = 0;
    for (const auto& gc : groupContours) {
        const LayerContour* lc = gc.contour;
        if (!lc) continue;

        qDebug() << "    【构建组路径】轮廓ID:" << nextContourId << ", 外轮廓:" << lc->isOuter
                 << ", Z高度:" << gc.z << ", 偏置圈数:" << lc->offsets.size();

        ContourToolpath ct;
        if (buildToolpathForContour(*lc, gc.z, nextContourId++, lc->isOuter, ct)) {
            groupResult.contours.push_back(std::move(ct));
            qDebug() << "    【构建组路径】轮廓" << (nextContourId - 1) << "路径生成成功";
        } else {
            qWarning() << "    【构建组路径】轮廓" << (nextContourId - 1) << "路径生成失败";
        }
    }

    qDebug() << "  【构建组路径】组" << groupId << "完成，成功生成" << groupResult.contours.size() << "个轮廓路径";
    return !groupResult.contours.empty();
}

bool SlicePathGenerator::generateToolpath()
{
    qDebug() << "【生成刀具路径】开始...";

    if (m_layers.empty()) {
        qWarning() << "【生成刀具路径】错误：没有切片数据，请先调用 generateContoursOnly()";
        return false;
    }

    m_paramsCopy = m_params;

    qDebug() << "【生成刀具路径】正在进行轮廓分组...";
    ContourGroups groups = groupContours();
    if (groups.groups.empty()) {
        qWarning() << "【生成刀具路径】没有可用的轮廓分组";
        return false;
    }
    qDebug() << "【生成刀具路径】轮廓分组完成，共" << groups.groups.size() << "组";

    m_completeToolpath.groups.clear();
    m_completeToolpath.safeZ = m_params.safeZ;
    m_completeToolpath.cuttingHeight = m_params.cuttingHeight;

    std::vector<std::pair<int, size_t>> order;
    for (size_t i = 0; i < groups.groups.size(); ++i) {
        order.emplace_back(groups.groups[i].empty() ? -1 : groups.groups[i][0].groupId, i);
    }
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
        if (a.first == 0 && b.first != 0) return true;
        if (a.first != 0 && b.first == 0) return false;
        return a.first < b.first;
    });

    for (const auto& [gid, idx] : order) {
        if (gid < 0) continue;
        const auto& groupContours = groups.groups[idx];
        if (groupContours.empty()) continue;

        Quantity_Color color = (gid < static_cast<int>(groups.groupColors.size())) ?
                                   groups.groupColors[gid] :
                                   Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB);

        qDebug() << "【生成刀具路径】正在处理组" << gid << (gid == 0 ? "(外轮廓)" : "(内轮廓)") << "，轮廓数量：" << groupContours.size();

        ContourGroupToolpath groupPath;
        if (buildToolpathForGroup(groupContours, gid, color, groupPath)) {
            m_completeToolpath.groups.push_back(std::move(groupPath));
            qDebug() << "【生成刀具路径】组" << gid << "路径生成成功";
        } else {
            qWarning() << "【生成刀具路径】组" << gid << "路径生成失败";
        }
    }

    qDebug() << "【生成刀具路径】完成，共生成" << m_completeToolpath.groups.size() << "个组的刀具路径";
    return !m_completeToolpath.groups.empty();
}

std::vector<Point3D> SlicePathGenerator::generateContinuousToolpath(const CompleteToolpath& toolpath) const
{
    qDebug() << "【生成连续路径】开始，共有" << toolpath.groups.size() << "个轮廓组";
    std::vector<Point3D> fullPath;
    if (toolpath.groups.empty()) {
        qWarning() << "【生成连续路径】轮廓组为空，返回空路径";
        return fullPath;
    }

    double safeZ = toolpath.safeZ;

    // 辅助函数：获取轮廓第一个偏置圈的切入点
    auto getEntryPoint = [](const ContourToolpath& contour) -> Point3D {
        if (contour.passes.empty()) return Point3D(0,0,0, Point3D::MoveType::NormalCut, -1, false);
        const auto& firstPass = contour.passes.front();
        if (firstPass.entryIdx >= 0 && firstPass.entryIdx < (int)firstPass.points.size()) {
            const ToolpathPoint& tp = firstPass.points[firstPass.entryIdx];
            return Point3D(tp.x, tp.y, tp.z, Point3D::MoveType::NormalCut, tp.contourId, tp.isOuter);
        }
        return Point3D(0,0,0, Point3D::MoveType::NormalCut, -1, false);
    };

    // 辅助函数：获取轮廓最后一个偏置圈的最后一个切削点
    auto getLastCutPoint = [](const ContourToolpath& contour) -> Point3D {
        if (contour.passes.empty()) return Point3D(0,0,0, Point3D::MoveType::NormalCut, -1, false);
        const auto& lastPass = contour.passes.back();
        for (int i = (int)lastPass.points.size() - 1; i >= 0; --i) {
            const ToolpathPoint& tp = lastPass.points[i];
            if (tp.type == ToolpathPointType::CutPoint || tp.type == ToolpathPointType::CutEnd) {
                return Point3D(tp.x, tp.y, tp.z, Point3D::MoveType::NormalCut, tp.contourId, tp.isOuter);
            }
        }
        return Point3D(0,0,0, Point3D::MoveType::NormalCut, -1, false);
    };

    // 辅助函数：添加安全高度上的水平快速移动（RapidMove）
    auto addRapidMove = [&](const Point3D& from, const Point3D& to) {
        // 确保起点和终点都在安全高度
        if (std::abs(from.z - safeZ) > 1e-6 || std::abs(to.z - safeZ) > 1e-6) {
            qWarning() << "【生成连续路径】RapidMove 必须在安全高度进行，已强制修正Z坐标";
        }
        Point3D p1 = from;
        Point3D p2 = to;
        p1.z = safeZ;
        p2.z = safeZ;
        p1.moveType = Point3D::MoveType::RapidMove;
        p2.moveType = Point3D::MoveType::RapidMove;
        fullPath.push_back(p1);
        fullPath.push_back(p2);
    };

    // 辅助函数：添加垂直下刀（NormalEntry）
    auto addPlunge = [&](const Point3D& top, const Point3D& bottom) {
        // 确保起点Z = safeZ，终点Z = 切削层Z，且X、Y不变
        Point3D p1 = top;
        Point3D p2 = bottom;
        p1.z = safeZ;
        p1.moveType = Point3D::MoveType::NormalEntry;
        p2.moveType = Point3D::MoveType::NormalEntry;
        fullPath.push_back(p1);
        fullPath.push_back(p2);
    };

    // 辅助函数：添加垂直抬刀（ExitMove）
    auto addRetract = [&](const Point3D& bottom, const Point3D& top) {
        // 确保起点Z为切削层，终点Z为安全高度，且X、Y不变
        Point3D p1 = bottom;
        Point3D p2 = top;
        p2.z = safeZ;
        p2.x = p1.x;
        p2.y = p1.y;
        p1.moveType = Point3D::MoveType::ExitMove;
        p2.moveType = Point3D::MoveType::ExitMove;
        fullPath.push_back(p1);
        fullPath.push_back(p2);
    };

    // 辅助函数：添加非切削点（螺旋、下刀等）
    auto addNonCutPoints = [&](const OffsetPassToolpath& pass) {
        for (const auto& tp : pass.spiralPoints) {
            Point3D::MoveType mt = Point3D::MoveType::NormalEntry;
            switch (tp.type) {
            case ToolpathPointType::SpiralPoint:
                mt = Point3D::MoveType::Spiral;
                break;
            case ToolpathPointType::RapidMove:
                mt = Point3D::MoveType::RapidMove;
                break;
            case ToolpathPointType::PlungeStart:
            case ToolpathPointType::PlungeEnd:
                mt = Point3D::MoveType::NormalEntry;
                break;
            default:
                mt = Point3D::MoveType::NormalCut;
                break;
            }
            fullPath.emplace_back(tp.x, tp.y, tp.z, mt, tp.contourId, tp.isOuter);
        }
    };

    // 辅助函数：添加切削点
    auto addCutPoints = [&](const OffsetPassToolpath& pass) {
        for (const auto& tp : pass.points) {
            if (tp.type == ToolpathPointType::RetractStart || tp.type == ToolpathPointType::RetractEnd) {
                continue;
            }
            fullPath.emplace_back(tp.x, tp.y, tp.z, Point3D::MoveType::NormalCut, tp.contourId, tp.isOuter);
        }
    };

    // ========== 1. 处理外轮廓组（groupId == 0） ==========
    qDebug() << "【生成连续路径】开始处理外轮廓组 (groupId == 0)";
    for (const auto& group : toolpath.groups) {
        if (group.groupId != 0) continue;

        for (size_t cIdx = 0; cIdx < group.contours.size(); ++cIdx) {
            const auto& contour = group.contours[cIdx];
            if (contour.passes.empty()) continue;

            for (size_t pIdx = 0; pIdx < contour.passes.size(); ++pIdx) {
                const auto& pass = contour.passes[pIdx];

                if (pIdx == 0 && cIdx == 0) {
                    // 第一个外轮廓的第一圈：直接添加螺旋/下刀点
                    addNonCutPoints(pass);
                } else if (pIdx > 0) {
                    // 同一轮廓内后续偏置圈：抬刀 → 水平快速移动 → 下刀
                    Point3D prevLastCut = getLastCutPoint(contour);
                    Point3D safePrev(prevLastCut.x, prevLastCut.y, safeZ,
                                     Point3D::MoveType::ExitMove, prevLastCut.contourId, prevLastCut.isOuter);
                    addRetract(prevLastCut, safePrev);

                    Point3D aboveEntry(pass.points[pass.entryIdx].x, pass.points[pass.entryIdx].y, safeZ,
                                       Point3D::MoveType::RapidMove, pass.points[pass.entryIdx].contourId, pass.points[pass.entryIdx].isOuter);
                    Point3D lastSafePos = fullPath.empty() ? aboveEntry : fullPath.back();
                    addRapidMove(lastSafePos, aboveEntry);
                    addNonCutPoints(pass);
                } else {
                    // 其他外轮廓的第一圈：从上一个轮廓的抬刀点水平移动过来（已在上层处理）
                    addNonCutPoints(pass);
                }

                addCutPoints(pass);

                // 该圈结束后的抬刀（若包含在pass.points中则跳过，但我们的数据在addCutPoints中已跳过Retract段，故手动添加最终抬刀）
                // 注意：最后一个偏置圈的抬刀会在循环外处理
            }

            // 轮廓结束后抬刀到安全高度
            Point3D lastCut = getLastCutPoint(contour);
            if (lastCut.x != 0 || lastCut.y != 0 || lastCut.z != 0) {
                Point3D safeLast(lastCut.x, lastCut.y, safeZ,
                                 Point3D::MoveType::ExitMove, lastCut.contourId, lastCut.isOuter);
                addRetract(lastCut, safeLast);
            }
        }
    }

    // ========== 2. 处理内轮廓组（groupId >= 1） ==========
    qDebug() << "【生成连续路径】开始处理内轮廓组 (groupId >= 1)";
    for (const auto& group : toolpath.groups) {
        if (group.groupId == 0) continue;

        for (size_t cIdx = 0; cIdx < group.contours.size(); ++cIdx) {
            const auto& contour = group.contours[cIdx];
            if (contour.passes.empty()) continue;

            // 移动到该轮廓第一圈切入点正上方
            if (cIdx == 0) {
                Point3D entry = getEntryPoint(contour);
                Point3D currentPos = fullPath.empty() ? Point3D(0,0,safeZ, Point3D::MoveType::RapidMove, -1, false) : fullPath.back();
                // 确保当前点在安全高度
                if (std::abs(currentPos.z - safeZ) > 1e-6) {
                    Point3D safeCurrent(currentPos.x, currentPos.y, safeZ,
                                        Point3D::MoveType::ExitMove, currentPos.contourId, currentPos.isOuter);
                    addRetract(currentPos, safeCurrent);
                    currentPos = safeCurrent;
                }
                Point3D aboveEntry(entry.x, entry.y, safeZ,
                                   Point3D::MoveType::RapidMove, entry.contourId, entry.isOuter);
                addRapidMove(currentPos, aboveEntry);
            }

            for (size_t pIdx = 0; pIdx < contour.passes.size(); ++pIdx) {
                const auto& pass = contour.passes[pIdx];

                if (pIdx > 0) {
                    // 内轮廓后续偏置圈：抬刀 → 水平快速移动 → 下刀
                    Point3D prevLastCut = getLastCutPoint(contour);
                    Point3D safePrev(prevLastCut.x, prevLastCut.y, safeZ,
                                     Point3D::MoveType::ExitMove, prevLastCut.contourId, prevLastCut.isOuter);
                    addRetract(prevLastCut, safePrev);

                    Point3D aboveEntry(pass.points[pass.entryIdx].x, pass.points[pass.entryIdx].y, safeZ,
                                       Point3D::MoveType::RapidMove, pass.points[pass.entryIdx].contourId, pass.points[pass.entryIdx].isOuter);
                    Point3D lastSafePos = fullPath.back();
                    addRapidMove(lastSafePos, aboveEntry);
                    addNonCutPoints(pass);
                } else {
                    addNonCutPoints(pass);
                }

                addCutPoints(pass);
            }

            // 轮廓结束后抬刀
            Point3D lastCut = getLastCutPoint(contour);
            if (lastCut.x != 0 || lastCut.y != 0 || lastCut.z != 0) {
                Point3D safeLast(lastCut.x, lastCut.y, safeZ,
                                 Point3D::MoveType::ExitMove, lastCut.contourId, lastCut.isOuter);
                addRetract(lastCut, safeLast);
            }
        }
    }

    // 不再添加任何圆弧连接，直接结束
    qDebug() << "【生成连续路径】完成，总路径点数：" << fullPath.size();
    return fullPath;
}
