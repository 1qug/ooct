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

// ========= 矩形边界过滤 =========
static const double RECT_EDGE_TOL = 1.0;  // 1 微米
// ========= 基于离散点采样的毛坯区域裁剪（矩形/圆形）=========

// 判断点是否在矩形区域内
static bool pointInRect(double x, double y,
                        double left, double right, double bottom, double top) {
    return (x >= left && x <= right && y >= bottom && y <= top);
}
// 计算多边形凸包面积（Andrew 单调链算法）
static double convexHullArea(const Contour2D& poly) {
    if (poly.size() < 3) return 0.0;
    std::vector<Point2D> pts = poly;
    // 去重与排序
    std::sort(pts.begin(), pts.end(), [](const Point2D& a, const Point2D& b) {
        return (a.x < b.x) || (std::abs(a.x - b.x) < 1e-9 && a.y < b.y);
    });
    pts.erase(std::unique(pts.begin(), pts.end(), [](const Point2D& a, const Point2D& b) {
                  return std::abs(a.x - b.x) < 1e-9 && std::abs(a.y - b.y) < 1e-9;
              }), pts.end());

    if (pts.size() < 3) return 0.0;

    auto cross = [](const Point2D& o, const Point2D& a, const Point2D& b) -> double {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };

    std::vector<Point2D> hull(2 * pts.size());
    int k = 0;
    for (int i = 0; i < (int)pts.size(); ++i) {
        while (k >= 2 && cross(hull[k-2], hull[k-1], pts[i]) <= 0) k--;
        hull[k++] = pts[i];
    }
    for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i) {
        while (k >= t && cross(hull[k-2], hull[k-1], pts[i]) <= 0) k--;
        hull[k++] = pts[i];
    }
    hull.resize(k - 1);

    // 计算面积
    double area = 0.0;
    for (size_t i = 0; i < hull.size(); ++i) {
        size_t j = (i + 1) % hull.size();
        area += hull[i].x * hull[j].y - hull[j].x * hull[i].y;
    }
    return std::abs(area) * 0.5;
}
// 判断点是否在圆形区域内
static bool pointInCircle(double x, double y, double cx, double cy, double r) {
    double dx = x - cx, dy = y - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

// 计算线段与矩形边界的交点（共4条边，返回最近交点，若无则返回 {false, {0,0}}）
static std::pair<bool, Point2D> intersectLineWithRect(
    const Point2D& p1, const Point2D& p2,
    double left, double right, double bottom, double top)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double minDist = 1e18;
    Point2D bestPt(0,0);
    bool found = false;

    auto checkSide = [&](bool vertical, double val, double start, double end) {
        double t = vertical ? (val - p1.x) / dx : (val - p1.y) / dy;
        if (t >= 0.0 && t <= 1.0) {
            double other = vertical ? p1.y + t * dy : p1.x + t * dx;
            if (other >= start && other <= end) {
                Point2D pt(vertical ? val : p1.x + t * dx,
                           vertical ? other : val);
                double dist = std::sqrt((pt.x - p1.x)*(pt.x - p1.x) + (pt.y - p1.y)*(pt.y - p1.y));
                if (dist < minDist) {
                    minDist = dist;
                    bestPt = pt;
                    found = true;
                }
            }
        }
    };

    // 四条边：左、右、下、上
    checkSide(true, left,   bottom, top);
    checkSide(true, right,  bottom, top);
    checkSide(false, bottom, left,  right);
    checkSide(false, top,    left,  right);
    return {found, bestPt};
}

// 计算线段与圆形边界的交点 (返回第一个从内到外或外到内的交点，t in [0,1])
static std::pair<bool, Point2D> intersectLineWithCircle(
    const Point2D& p1, const Point2D& p2,
    double cx, double cy, double r)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double fx = p1.x - cx;
    double fy = p1.y - cy;
    double a = dx * dx + dy * dy;
    double b = 2.0 * (fx * dx + fy * dy);
    double c = (fx * fx + fy * fy) - r * r;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0) return {false, Point2D(0,0)};

    double sqrtDisc = std::sqrt(disc);
    double t1 = (-b - sqrtDisc) / (2.0 * a);
    double t2 = (-b + sqrtDisc) / (2.0 * a);

    // 选择在 [0,1] 内的最近交点
    double bestT = -1.0;
    if (t1 >= 0.0 && t1 <= 1.0) bestT = t1;
    if (t2 >= 0.0 && t2 <= 1.0 && (bestT < 0 || t2 < bestT)) bestT = t2;
    if (bestT < 0) return {false, Point2D(0,0)};

    return {true, Point2D(p1.x + bestT * dx, p1.y + bestT * dy)};
}
// 判断一个整数点（1e6精度）是否在 clipRegion 中（支持多个凸/凹多边形）
static bool pointInClipRegion(int64_t x, int64_t y, const Paths64& clipRegion) {
    for (const auto& poly : clipRegion) {
        if (poly.size() < 3) continue;
        bool inside = false;
        size_t n = poly.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const auto& pi = poly[i];
            const auto& pj = poly[j];
            if (((pi.y > y) != (pj.y > y)) &&
                (x < (pj.x - pi.x) * (y - pi.y) / (double)(pj.y - pi.y) + pi.x))
                inside = !inside;
        }
        if (inside) return true;   // 只要在任意一个多边形内即可
    }
    return false;
}
static std::vector<Contour2D> clipOpenRingToRegion(const Contour2D& openRing,
                                                   const Paths64& clipRegion,
                                                   double sampleStep = 0.04)
{
    std::vector<Contour2D> result;
    if (openRing.size() < 2) return result;

    // 1. 沿折线密集采样（正确处理开放路径）
    std::vector<Point2D> sampled;
    sampled.reserve(openRing.size() * 2);

    for (size_t i = 0; i + 1 < openRing.size(); ++i) {   // 仅遍历实际存在的线段
        const Point2D& p1 = openRing[i];
        const Point2D& p2 = openRing[i + 1];
        sampled.push_back(p1);
        double dx = p2.x - p1.x, dy = p2.y - p1.y;
        double segLen = std::sqrt(dx*dx + dy*dy);
        if (segLen > sampleStep) {
            int divs = static_cast<int>(std::ceil(segLen / sampleStep));
            for (int j = 1; j < divs; ++j) {
                double t = (double)j / divs;
                sampled.push_back(Point2D(p1.x + t*dx, p1.y + t*dy));
            }
        }
    }
    sampled.push_back(openRing.back());   // 补上最后一个点

    // 2. 提取在 clipRegion 内的连续子段
    Contour2D currentFrag;
    bool lastInside = false;
    for (size_t i = 0; i < sampled.size(); ++i) {
        const Point2D& pt = sampled[i];
        int64_t ix = static_cast<int64_t>(pt.x * 1e6);
        int64_t iy = static_cast<int64_t>(pt.y * 1e6);
        bool inside = pointInClipRegion(ix, iy, clipRegion);

        if (i == 0) {
            if (inside) currentFrag.push_back(pt);
            lastInside = inside;
            continue;
        }

        if (inside && !lastInside) {
            currentFrag.push_back(pt);
        } else if (!inside && lastInside) {
            if (currentFrag.size() >= 2)
                result.push_back(std::move(currentFrag));
            currentFrag.clear();
        } else if (inside) {
            currentFrag.push_back(pt);
        }
        lastInside = inside;
    }
    if (currentFrag.size() >= 2)
        result.push_back(std::move(currentFrag));

    return result;
}
// 主裁剪函数：对轮廓密集采样，只保留矩形/圆形区域内部的点片段
static std::vector<Contour2D> clipContourByStockRegion(
    const Contour2D& contour,
    const SliceParams& params,
    double sampleSpacing = 0.1)   // 单位 mm，决定“过于细致”的程度
{
    std::vector<Contour2D> result;
    if (contour.size() < 2) return result;

    // 确定区域（矩形或圆形）
    bool isRect = (params.maxAllowedLength > 0.0 && params.maxAllowedWidth > 0.0);
    double left = 0, right = 0, bottom = 0, top = 0;
    double circleCx = 0, circleCy = 0, circleR = 0;
    if (isRect) {
        double halfL = params.maxAllowedLength * 0.5;
        double halfW = params.maxAllowedWidth  * 0.5;
        left   = params.stockCenterX - halfL;
        right  = params.stockCenterX + halfL;
        bottom = params.stockCenterY - halfW;
        top    = params.stockCenterY + halfW;
    } else if (params.maxAllowedRadius > 0.0) {
        circleCx = params.stockCenterX;
        circleCy = params.stockCenterY;
        circleR = params.maxAllowedRadius;
    } else {
        // 无限制，直接返回原轮廓
        return {contour};
    }

    auto isInside = [&](const Point2D& p) -> bool {
        if (isRect)
            return pointInRect(p.x, p.y, left, right, bottom, top);
        else
            return pointInCircle(p.x, p.y, circleCx, circleCy, circleR);
    };

    // 对轮廓进行等距采样，生成密集点序列
    std::vector<Point2D> sampled;
    sampled.reserve(contour.size() * 2);
    for (size_t i = 0; i < contour.size(); ++i) {
        const Point2D& p1 = contour[i];
        const Point2D& p2 = contour[(i+1) % contour.size()];
        sampled.push_back(p1);
        double segLen = std::sqrt((p2.x-p1.x)*(p2.x-p1.x) + (p2.y-p1.y)*(p2.y-p1.y));
        if (segLen > sampleSpacing) {
            int numDiv = static_cast<int>(std::ceil(segLen / sampleSpacing));
            for (int j = 1; j < numDiv; ++j) {
                double t = static_cast<double>(j) / numDiv;
                sampled.push_back(Point2D(p1.x + t * (p2.x-p1.x),
                                          p1.y + t * (p2.y-p1.y)));
            }
        }
    }
    // 如果原始轮廓是闭合的，采样序列也保持闭合（末尾不加重复点，后续处理时考虑环形）
    if (contour.size() > 2 && contour.front() == contour.back()) {
        // 已经闭合，采样后也闭合
        if (sampled.front() != sampled.back())
            sampled.push_back(sampled.front());
    }

    // 提取内部连续片段
    Contour2D currentFrag;
    bool lastInside = false;
    Point2D lastPoint(0,0);

    for (size_t i = 0; i < sampled.size(); ++i) {
        const Point2D& pt = sampled[i];
        bool inside = isInside(pt);

        if (i == 0) {
            if (inside) currentFrag.push_back(pt);
            lastInside = inside;
            lastPoint = pt;
            continue;
        }

        // 处理线段穿越边界
        if (inside && !lastInside) {
            // 从外到内：计算交点并加入片段
            auto [ok, inter] = isRect ? intersectLineWithRect(lastPoint, pt, left, right, bottom, top)
                                      : intersectLineWithCircle(lastPoint, pt, circleCx, circleCy, circleR);
            if (ok) currentFrag.push_back(inter);
            currentFrag.push_back(pt);
        } else if (!inside && lastInside) {
            // 从内到外：计算交点，结束当前片段
            auto [ok, inter] = isRect ? intersectLineWithRect(lastPoint, pt, left, right, bottom, top)
                                      : intersectLineWithCircle(lastPoint, pt, circleCx, circleCy, circleR);
            if (ok) currentFrag.push_back(inter);
            if (currentFrag.size() >= 2)
                result.push_back(currentFrag);
            currentFrag.clear();
        } else if (inside) {
            // 都在内部，直接添加
            currentFrag.push_back(pt);
        }

        lastInside = inside;
        lastPoint = pt;
    }

    // 处理循环闭合情况：如果采样序列是闭合的并且片段未结束，可能需要连接首尾
    if (!currentFrag.empty() && sampled.front() == sampled.back() && isInside(sampled.front())) {
        // 如果片段未结束但回到起点，尝试与首片段合并
        // 简化：直接将当前片段与已有的 first fragment 合并（假设只有一个外环裁剪后可能变为单个片段）
        // 这里简单地将当前片段加入结果，并依靠后续处理合并不连续的片段。
        if (currentFrag.size() >= 2)
            result.push_back(currentFrag);
    } else if (!currentFrag.empty() && currentFrag.size() >= 2) {
        result.push_back(currentFrag);
    }

    return result;
}
static bool isOnRectEdge(const Point64& pt,
                         double left, double right, double bottom, double top)
{
    double x = pt.x * 1e-6;
    double y = pt.y * 1e-6;
    return (std::abs(x - left)  < RECT_EDGE_TOL ||
            std::abs(x - right) < RECT_EDGE_TOL ||
            std::abs(y - bottom) < RECT_EDGE_TOL ||
            std::abs(y - top)    < RECT_EDGE_TOL);
}

static bool isRectEdge(const Point64& p1, const Point64& p2,
                       double left, double right, double bottom, double top)
{
    if (!isOnRectEdge(p1, left, right, bottom, top)) return false;
    if (!isOnRectEdge(p2, left, right, bottom, top)) return false;
    double dx = (p1.x - p2.x) * 1e-6;
    double dy = (p1.y - p2.y) * 1e-6;
    if (std::abs(dx) > 1e-6 && std::abs(dy) > 1e-6) return false;
    return true;
}

static std::vector<Contour2D> filterRectEdges(const Path64& poly,
                                              double left, double right,
                                              double bottom, double top)
{
    std::vector<Contour2D> result;
    if (poly.size() < 2) return result;

    std::vector<Point64> pts;
    if (poly.front() == poly.back()) {
        pts.assign(poly.begin(), poly.end() - 1);
    } else {
        pts = poly;
    }

    size_t n = pts.size();
    std::vector<bool> keep(n, false);

    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        if (!isRectEdge(pts[i], pts[j], left, right, bottom, top)) {
            keep[i] = true;
            keep[j] = true;
        }
    }

    Contour2D current;
    for (size_t i = 0; i < n; ++i) {
        if (keep[i]) {
            current.emplace_back(pts[i].x * 1e-6, pts[i].y * 1e-6);
        } else {
            if (current.size() >= 2) result.push_back(std::move(current));
            current.clear();
        }
    }
    if (current.size() >= 2) result.push_back(std::move(current));
    return result;
}

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

// ---------- 构建毛坯裁剪区域（矩形或近似圆）----------
static Paths64 buildClipRegion(const SliceParams& params) {
    Paths64 clips;
    // 矩形毛坯
    if (params.maxAllowedLength > 0.0 && params.maxAllowedWidth > 0.0) {
        double halfL = params.maxAllowedLength * 0.5;
        double halfW = params.maxAllowedWidth  * 0.5;
        double cx = params.stockCenterX;
        double cy = params.stockCenterY;

        Path64 rect;
        rect.push_back(Point64(static_cast<int64_t>((cx - halfL) * 1e6),
                               static_cast<int64_t>((cy - halfW) * 1e6)));
        rect.push_back(Point64(static_cast<int64_t>((cx + halfL) * 1e6),
                               static_cast<int64_t>((cy - halfW) * 1e6)));
        rect.push_back(Point64(static_cast<int64_t>((cx + halfL) * 1e6),
                               static_cast<int64_t>((cy + halfW) * 1e6)));
        rect.push_back(Point64(static_cast<int64_t>((cx - halfL) * 1e6),
                               static_cast<int64_t>((cy + halfW) * 1e6)));
        rect.push_back(rect.front()); // 闭合
        clips.push_back(rect);
    }
    // 圆形毛坯（用 72 边形近似）
    else if (params.maxAllowedRadius > 0.0) {
        const int numSides = 72;
        double cx = params.stockCenterX;
        double cy = params.stockCenterY;
        double r  = params.maxAllowedRadius;

        Path64 circle;
        for (int i = 0; i < numSides; ++i) {
            double angle = 2.0 * M_PI * i / numSides;
            double x = cx + r * cos(angle);
            double y = cy + r * sin(angle);
            circle.push_back(Point64(static_cast<int64_t>(x * 1e6),
                                     static_cast<int64_t>(y * 1e6)));
        }
        circle.push_back(circle.front());
        clips.push_back(circle);
    }
    return clips;
}
//+=========================================================+
// ---- 最大内切圆辅助函数 ----

// 点到线段的最短距离及垂足
static double pointToSegmentDist(const Point2D& p, const Point2D& a, const Point2D& b,
                                 Point2D& proj) {
    double dx = b.x - a.x, dy = b.y - a.y;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-12) {
        proj = a;
        return std::sqrt((p.x-a.x)*(p.x-a.x) + (p.y-a.y)*(p.y-a.y));
    }
    double t = ((p.x - a.x)*dx + (p.y - a.y)*dy) / len2;
    if      (t < 0.0)    proj = a;
    else if (t > 1.0)    proj = b;
    else                 proj = Point2D(a.x + t*dx, a.y + t*dy);
    return std::sqrt((p.x-proj.x)*(p.x-proj.x) + (p.y-proj.y)*(p.y-proj.y));
}

// 点到闭合多边形边界的最短距离
static double minDistToPolygon(const Point2D& pt, const Contour2D& poly) {
    double minD = 1e100;
    for (size_t i = 0; i+1 < poly.size(); ++i) {
        Point2D dummy;
        double d = pointToSegmentDist(pt, poly[i], poly[i+1], dummy);
        if (d < minD) minD = d;
    }
    return minD;
}

// 基于两次网格搜索求多边形最大内切圆 (返回圆心和半径)
static bool computeMaxIncircle(const Contour2D& polygon, double gridStep,
                               Point2D& bestCenter, double& bestRadius) {
    if (polygon.size() < 3) return false;

    // 边界框
    double minX = polygon[0].x, maxX = minX, minY = polygon[0].y, maxY = minY;
    for (const auto& p : polygon) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    Point2D centerC;
    double radiusC = -1.0;

    // 粗搜索
    for (double x = minX; x <= maxX; x += gridStep) {
        for (double y = minY; y <= maxY; y += gridStep) {
            Point2D pt(x, y);
            if (!SlicePathGenerator::pointInPolygon(pt, polygon)) continue;
            double d = minDistToPolygon(pt, polygon);
            if (d > radiusC) {
                radiusC = d;
                centerC = pt;
            }
        }
    }
    if (radiusC <= 0.0) return false;

    // 细搜索 (以粗搜最优为中心, 范围 ±gridStep, 步长 gridStep/10)
    double fineStep = gridStep * 0.1;
    double lx = std::max(minX, centerC.x - gridStep);
    double rx = std::min(maxX, centerC.x + gridStep);
    double ly = std::max(minY, centerC.y - gridStep);
    double ry = std::min(maxY, centerC.y + gridStep);

    for (double x = lx; x <= rx; x += fineStep) {
        for (double y = ly; y <= ry; y += fineStep) {
            Point2D pt(x, y);
            if (!SlicePathGenerator::pointInPolygon(pt, polygon)) continue;
            double d = minDistToPolygon(pt, polygon);
            if (d > radiusC) {
                radiusC = d;
                centerC = pt;
            }
        }
    }

    bestCenter = centerC;
    bestRadius = radiusC;
    return true;
}

// 计算内切圆与多边形边/顶点的切点 (距离 == 半径 的点, 容差 1e-4 mm)
static std::vector<Point2D> getIncircleContactPoints(const Contour2D& polygon,
                                                     const Point2D& center, double radius) {
    std::vector<Point2D> points;
    const double tol = 1e-4;
    const size_t n = polygon.size();

    // 线段上的切点
    for (size_t i = 0; i+1 < n; ++i) {
        const Point2D& a = polygon[i];
        const Point2D& b = polygon[i+1];
        Point2D proj;
        double d = pointToSegmentDist(center, a, b, proj);
        if (std::abs(d - radius) <= tol) {
            points.push_back(proj);
        }
    }

    // 顶点上的切点 (避免重复)
    for (size_t i = 0; i+1 < n; ++i) {               // 多边形已闭合, 尾点==首点, 遍历 0~n-2
        const Point2D& v = polygon[i];
        double dist2 = (v.x-center.x)*(v.x-center.x) + (v.y-center.y)*(v.y-center.y);
        if (std::abs(std::sqrt(dist2) - radius) <= tol) {
            bool duplicate = false;
            for (const auto& p : points) {
                if (std::abs(p.x-v.x) < tol && std::abs(p.y-v.y) < tol) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) points.push_back(v);
        }
    }

    return points;
}
//+=========================================================+
std::vector<Contour2D> clipContourToRectangle(const Contour2D& contour,
                                              double centerX, double centerY,
                                              double length, double width,
                                              double minFragmentArea = 0.5)
{
    using namespace Clipper2Lib;
    if (contour.size() < 3 || length <= 0 || width <= 0)
        return {};

    double halfL = length * 0.5;
    double halfW = width * 0.5;
    double left   = centerX - halfL;
    double right  = centerX + halfL;
    double bottom = centerY - halfW;
    double top    = centerY + halfW;

    Path64 rect;
    rect.push_back(Point64(static_cast<int64_t>(left  * 1e6), static_cast<int64_t>(bottom * 1e6)));
    rect.push_back(Point64(static_cast<int64_t>(right * 1e6), static_cast<int64_t>(bottom * 1e6)));
    rect.push_back(Point64(static_cast<int64_t>(right * 1e6), static_cast<int64_t>(top    * 1e6)));
    rect.push_back(Point64(static_cast<int64_t>(left  * 1e6), static_cast<int64_t>(top    * 1e6)));
    rect.push_back(rect.front());

    Path64 subject;
    subject.reserve(contour.size());
    for (const auto& pt : contour) {
        subject.push_back(Point64(static_cast<int64_t>(pt.x * 1e6),
                                  static_cast<int64_t>(pt.y * 1e6)));
    }
    if (subject.size() > 1 && subject.front() != subject.back())
        subject.push_back(subject.front());

    Clipper64 clipper;
    clipper.AddSubject({subject});
    clipper.AddClip({rect});
    PolyTree64 solution;
    clipper.Execute(ClipType::Intersection, FillRule::NonZero, solution);

    std::vector<Contour2D> result;
    std::function<void(const PolyPath64&)> processNode = [&](const PolyPath64& node) {
        const Path64& poly = node.Polygon();
        if (!poly.empty()) {
            auto arcs = filterRectEdges(poly, left, right, bottom, top);
            for (auto& arc : arcs) {
                double area = SlicePathGenerator::polygonArea(arc);
                if (area >= minFragmentArea) {
                    result.push_back(std::move(arc));
                }
            }
        }
        for (const auto& child : node) {
            processNode(*child);
        }
    };
    for (const auto& child : solution) {
        processNode(*child);
    }

    return result;
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
static std::vector<Contour2D> customOffsetPolygon(const Contour2D& poly, double dist) {
    std::vector<Contour2D> result;
    if (poly.size() < 3) return result;

    // 1. 确保多边形为逆时针（面积为正）
    Contour2D ordered = poly;
    double area = SlicePathGenerator::polygonArea(ordered);
    if (area < 0) {
        std::reverse(ordered.begin(), ordered.end());
    }

    // 2. 计算每条边向内平移的直线
    struct Line { Point2D p; Point2D dir; };
    std::vector<Line> lines;
    size_t n = ordered.size();
    for (size_t i = 0; i < n - 1; ++i) {          // 跳过重复的闭合点
        const Point2D& a = ordered[i];
        const Point2D& b = ordered[i+1];
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-9) continue;
        // 左法线（逆时针多边形内部在左手边）
        Point2D normal(-dy / len, dx / len);
        // 向内平移起点
        Point2D start(a.x + normal.x * dist, a.y + normal.y * dist);
        lines.push_back({start, Point2D(dx, dy)});
    }

    if (lines.empty()) return result;

    // 3. 求相邻平移线的交点，构建新多边形
    Contour2D newPoly;
    size_t m = lines.size();
    for (size_t i = 0; i < m; ++i) {
        const Line& L1 = lines[i];
        const Line& L2 = lines[(i + 1) % m];
        double det = L1.dir.x * L2.dir.y - L1.dir.y * L2.dir.x;
        if (std::abs(det) < 1e-12) {
            // 平行，直接保留 L1 的起点（近似）
            newPoly.push_back(L1.p);
            continue;
        }
        double dx = L2.p.x - L1.p.x;
        double dy = L2.p.y - L1.p.y;
        double t = (dx * L2.dir.y - dy * L2.dir.x) / det;
        newPoly.push_back(Point2D(L1.p.x + t * L1.dir.x,
                                  L1.p.y + t * L1.dir.y));
    }

    // 闭合
    if (newPoly.size() > 1 && newPoly.front() != newPoly.back())
        newPoly.push_back(newPoly.front());

    // 舍弃面积过小的退化多边形
    double newArea = SlicePathGenerator::polygonArea(newPoly);
    if (std::abs(newArea) > 1e-6) {
        result.push_back(newPoly);
    }
    return result;
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

    // 手动连接容差逻辑（备用）
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
        lc.isOuter = (level % 2 == 0);   // 偶数层为外轮廓，奇数层为内轮廓
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

void SlicePathGenerator::populateLayerOffsetRings()
{
    m_layerOffsetRings.clear();
    m_layerOffsetRings.reserve(m_layers.size());

    for (size_t li = 0; li < m_layers.size(); ++li) {
        const LayerData& layer = m_layers[li];
        LayerOffsetRings layerRings;
        layerRings.z = layer.z;
        layerRings.layerId = static_cast<int>(li);

        for (const auto& hc : layer.contours) {
            // 闭合偏置
            for (size_t pass = 0; pass < hc.offsets.size(); ++pass) {
                OffsetRing ring;
                ring.contour   = hc.offsets[pass];
                ring.contourId = hc.id;
                ring.passIndex = static_cast<int>(pass);
                ring.area      = polygonArea(hc.offsets[pass]);

                // 外轮廓偏置环：通过凸包面积比区分内凹环与普通外环
                if (hc.isOuter) {
                    double area = std::abs(ring.area);
                    if (area > 1e-6) {
                        double hullArea = convexHullArea(ring.contour);
                        double ratio = hullArea / area;
                        // 凸包面积明显大于自身面积 → 凹多边形 → 内凹环
                        ring.isOuter = (ratio <= 1.2);
                    } else {
                        ring.isOuter = true; // 退化，保持外轮廓
                    }
                } else {
                    ring.isOuter = false;
                }

                layerRings.rings.push_back(ring);
            }

            // 开放片段
            for (size_t pass = 0; pass < hc.offsetSegments.size(); ++pass) {
                OffsetRing ring;
                ring.contour   = hc.offsetSegments[pass];
                ring.contourId = hc.id;
                ring.passIndex = static_cast<int>(pass) + static_cast<int>(hc.offsets.size());
                ring.isOuter   = hc.isOuter;
                ring.area      = polygonArea(hc.offsetSegments[pass]);
                layerRings.rings.push_back(ring);
            }
        }

        if (!layerRings.rings.empty()) {
            m_layerOffsetRings.push_back(layerRings);
        }
        if (!m_layerOffsetRings.empty()) {
            identifyRegions();   // 完成填充后进行区域识别
        }
    }
}
void SlicePathGenerator::identifyRegions() {
    if (m_layerOffsetRings.empty()) return;

    // 相似度阈值：面积/周长比值在 0.7 ~ 1.3 之间视为“相似”
    const double similarMin = -500.0;
    //const double similarMax = 10.0;

    // 用于独立区域质心聚类的距离阈值（刀具直径的 4 倍）
    const double clusterDist = m_params.toolDiameter * 4.0;
    const double clusterDistSq = clusterDist * clusterDist;

    // 按 contourId 收集所有内轮廓环
    std::map<int, std::vector<OffsetRing*>> groupsByContour;
    for (auto& layer : m_layerOffsetRings) {
        for (auto& ring : layer.rings) {
            if (!ring.isOuter && ring.contour.size() >= 3) {
                groupsByContour[ring.contourId].push_back(&ring);
            }
        }
    }

    for (auto& [contourId, rings] : groupsByContour) {
        std::vector<OffsetRing*> shared;
        std::vector<OffsetRing*> independent;

        OffsetRing* firstRing = nullptr;
        if (!rings.empty()) firstRing = rings.front();  // 第一条偏置圈（passIndex==0）

        if (!firstRing) continue;

        // 计算第一条环的参考面积 & 周长
        double refArea = SlicePathGenerator::polygonArea(firstRing->contour);
        if (refArea < 1e-9) refArea = 1.0;

        double refPerimeter = 0.0;
        for (size_t i = 0; i + 1 < firstRing->contour.size(); ++i) {
            double dx = firstRing->contour[i].x - firstRing->contour[i+1].x;
            double dy = firstRing->contour[i].y - firstRing->contour[i+1].y;
            refPerimeter += std::sqrt(dx*dx + dy*dy);
        }
        if (refPerimeter < 1e-9) refPerimeter = 1.0;

        // 遍历该轮廓的所有环，判断共享 / 独立
        for (auto* ring : rings) {
            if (ring == firstRing) {
                ring->regionId = 0;     // 第一条固定 A
                shared.push_back(ring);
                continue;
            }

            // 计算当前环的面积和周长
            double area = SlicePathGenerator::polygonArea(ring->contour);
            double perimeter = 0.0;
            const auto& cont = ring->contour;
            for (size_t i = 0; i + 1 < cont.size(); ++i) {
                double dx = cont[i].x - cont[i+1].x;
                double dy = cont[i].y - cont[i+1].y;
                perimeter += std::sqrt(dx*dx + dy*dy);
            }

            double areaRatio = area / refArea;
            //double periRatio = perimeter / refPerimeter;

            // qDebug()<<"areaRatio:"<<areaRatio<<"periRatio:"<<periRatio;
            // 同时满足面积比和周长比在阈值内，才视为“形状/面积差不多”
            if (areaRatio <= similarMin /*&& areaRatio <= similarMax*/)
            {
                ring->regionId = 0;        // 共享 A
                shared.push_back(ring);
            } else {
                independent.push_back(ring);
            }
        }

        // 如果没有独立区域，全部为 A
        if (independent.empty()) {
            qDebug().noquote() << QString("contour %1: 全部共享 → A").arg(contourId);
            continue;
        }

        // ========== 对独立区域进行质心聚类 ==========
        int n = static_cast<int>(independent.size());
        std::vector<Point2D> centers(n);
        for (int i = 0; i < n; ++i) {
            double cx = 0, cy = 0;
            const auto& cont = independent[i]->contour;
            for (const auto& p : cont) { cx += p.x; cy += p.y; }
            double inv = 1.0 / cont.size();
            centers[i] = Point2D(cx * inv, cy * inv);
        }

        std::vector<int> parent(n);
        for (int i = 0; i < n; ++i) parent[i] = i;
        auto find = [&](int x) {
            while (parent[x] != x) x = parent[parent[x]];
            return x;
        };
        auto unite = [&](int a, int b) {
            int ra = find(a), rb = find(b);
            if (ra != rb) parent[ra] = rb;
        };

        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) {
                double dx = centers[i].x - centers[j].x;
                double dy = centers[i].y - centers[j].y;
                if ((dx*dx + dy*dy) < clusterDistSq)
                    unite(i, j);
            }

        std::map<int, int> root2cluster;
        int numClusters = 0;
        for (int i = 0; i < n; ++i) {
            int root = find(i);
            if (root2cluster.find(root) == root2cluster.end())
                root2cluster[root] = numClusters++;
            independent[i]->regionId = root2cluster[root];
        }

        // 独立区域编号偏移 +1 → B, C, ...
        for (auto* ring : independent) ring->regionId += 1;

        // qDebug().noquote() << QString("contour %1: 共享环=%2, 独立区域=%3 组（字母 B~%4）")
        //                           .arg(contourId).arg(shared.size())
        //                           .arg(numClusters)
        //                           .arg(QChar('A' + numClusters));
    }
}


// ========== 辅助函数：根据 MaxIncircleInfo 查找对应的偏置环轮廓 ==========
static Contour2D getRingContour(const MaxIncircleInfo* info,
                                const std::vector<LayerOffsetRings>& layerOffsetRings) {
    for (const auto& layer : layerOffsetRings) {
        if (std::abs(layer.z - info->z) < 1e-6) {
            for (const auto& ring : layer.rings) {
                if (ring.contourId == info->contourId &&
                    ring.passIndex == info->passIndex &&
                    ring.isOuter == info->isOuter) {
                    return ring.contour;
                }
            }
        }
    }
    return {};
}

// ========== 辅助函数：点 target 到闭合轮廓 poly 的最近点（在轮廓上）==========
static Point2D closestPointOnContour(const Point2D& target, const Contour2D& poly) {
    double bestDist = 1e100;
    Point2D best = poly.front();
    for (size_t i = 0; i + 1 < poly.size(); ++i) {
        Point2D proj;
        double d = pointToSegmentDist(target, poly[i], poly[i + 1], proj);
        if (d < bestDist) {
            bestDist = d;
            best = proj;
        }
    }
    return best;
}

void SlicePathGenerator::adjustInnerContactPoints(std::vector<MaxIncircleInfo>& infos) const {
    if (infos.empty()) return;

    // 1. 按 (contourId, regionId) 分组
    std::map<int, std::map<int, std::vector<MaxIncircleInfo*>>> contourRegionMap;
    for (auto& info : infos) {
        contourRegionMap[info.contourId][info.regionId].push_back(&info);
    }

    // 2. 逐个轮廓处理
    for (auto& [contourId, regionMap] : contourRegionMap) {

        // ---------- 先处理所有独占区域 (regionId > 0) ----------
        bool hasExclusive = false;
        Point2D exclusiveRefPt;          // 用于共享区域的参考点（取最后一个独占区域的最后一个点）

        for (auto& [regionId, vec] : regionMap) {
            if (regionId == 0) continue;
            hasExclusive = true;

            // 区域内排序：Z 降序，passIndex 升序（内轮廓偏置：passIndex 越大越靠内）
            std::sort(vec.begin(), vec.end(), [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
                if (std::abs(a->z - b->z) > 1e-6) return a->z > b->z;
                return a->passIndex < b->passIndex;
            });

            // 逐圈传递最近点（不在这里设置 isInnermost）
            Point2D prevPt;
            for (size_t i = 0; i < vec.size(); ++i) {
                auto* info = vec[i];
                Contour2D contour = getRingContour(info, m_layerOffsetRings);
                if (contour.empty()) continue;

                if (i == 0) {
                    prevPt = info->contactPoints.empty() ? contour.front() : info->contactPoints.front();
                } else {
                    prevPt = closestPointOnContour(prevPt, contour);
                    info->contactPoints = { prevPt };
                }
            }

            // ---- 标记该独占区域的最内层环（按 Z 层分组，passIndex 最大的环）----
            // 先按 Z 分组（容差 1e-6）
            std::vector<std::vector<MaxIncircleInfo*>> zGroups;
            for (auto* info : vec) {
                bool added = false;
                for (auto& group : zGroups) {
                    if (!group.empty() && std::abs(group[0]->z - info->z) < 1e-6) {
                        group.push_back(info);
                        added = true;
                        break;
                    }
                }
                if (!added) zGroups.push_back({info});
            }

            for (auto& group : zGroups) {
                // 找出该层 passIndex 最大的环（最内层）
                int maxPass = -1;
                for (auto* info : group) {
                    if (info->passIndex > maxPass) maxPass = info->passIndex;
                }
                for (auto* info : group) {
                    info->isInnermost = (info->passIndex == maxPass);
                }
            }

            // 记录最后一个独占区域最外圈的点（用于共享区域）
            if (regionId == std::max_element(regionMap.begin(), regionMap.end(),
                                             [](const auto& a, const auto& b) { return a.first < b.first; })->first) {
                exclusiveRefPt = prevPt;
            }
        }

        // ---------- 再处理共享区域 (regionId == 0) ----------
        auto itShared = regionMap.find(0);
        if (itShared != regionMap.end()) {
            auto& sharedVec = itShared->second;
            if (sharedVec.empty()) continue;

            // 排序：Z 降序，passIndex 升序
            std::sort(sharedVec.begin(), sharedVec.end(), [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
                if (std::abs(a->z - b->z) > 1e-6) return a->z > b->z;
                return a->passIndex < b->passIndex;
            });

            if (!hasExclusive) {
                // 没有独占区域，回退到原始单点传递逻辑
                Point2D prevPt;
                for (size_t i = 0; i < sharedVec.size(); ++i) {
                    auto* info = sharedVec[i];
                    Contour2D contour = getRingContour(info, m_layerOffsetRings);
                    if (contour.empty()) continue;

                    if (i == 0) {
                        prevPt = info->contactPoints.empty() ? contour.front() : info->contactPoints.front();
                    } else {
                        prevPt = closestPointOnContour(prevPt, contour);
                        info->contactPoints = { prevPt };
                    }
                }
            } else {
                // 有独占区域：所有共享环统一用离 exclusiveRefPt 最近的点
                for (auto* info : sharedVec) {
                    Contour2D contour = getRingContour(info, m_layerOffsetRings);
                    if (contour.empty()) continue;
                    Point2D bestPt = closestPointOnContour(exclusiveRefPt, contour);
                    info->contactPoints = { bestPt };
                }
            }
        }
    }
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
    qDebug() << "尺寸限制 - 矩形: 长=" << m_params.maxAllowedLength
             << ", 宽=" << m_params.maxAllowedWidth
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

            /*if (hc.isOuter) {
                Paths64 clipRegion = buildClipRegion(m_params);

                int maxPasses = m_params.maxOffsetPasses;
                for (int pass = 0; pass < maxPasses; ++pass) {
                    double currentOffset = baseOffset + pass * m_params.stepover;
                    auto offsetRes = offsetContour(hc.original, currentOffset);
                    if (offsetRes.empty()) break;

                    for (const auto& ring : offsetRes) {
                        Path64 subject;
                        for (const auto& pt : ring) {
                            subject.push_back(Point64(static_cast<int64_t>(pt.x * 1e6),
                                                      static_cast<int64_t>(pt.y * 1e6)));
                        }
                        if (subject.size() > 1 && subject.front() != subject.back())
                            subject.push_back(subject.front());

                        Clipper64 clipper;
                        clipper.AddSubject({subject});
                        clipper.AddClip(clipRegion);

                        Paths64 solutionInner;
                        clipper.Execute(ClipType::Intersection, FillRule::NonZero, solutionInner);

                        Paths64 solutionOuter;
                        clipper.Execute(ClipType::Difference, FillRule::NonZero, solutionOuter);

                        // ---------- 保存内部片段（仅保留圆弧段）----------
                        double clipLeft = 1e18, clipRight = -1e18, clipBottom = 1e18, clipTop = -1e18;
                        if (!clipRegion.empty() && !clipRegion.front().empty()) {
                            for (const auto& pt : clipRegion.front()) {
                                double x = pt.x * 1e-6;
                                double y = pt.y * 1e-6;
                                clipLeft   = std::min(clipLeft, x);
                                clipRight  = std::max(clipRight, x);
                                clipBottom = std::min(clipBottom, y);
                                clipTop    = std::max(clipTop, y);
                            }
                        }
                        const double eps = 1e-6;
                        auto isRectEdge = [=](double x1, double y1, double x2, double y2) -> bool {
                            return (std::abs(x1 - clipLeft)   < eps && std::abs(x2 - clipLeft)   < eps) ||
                                   (std::abs(x1 - clipRight)  < eps && std::abs(x2 - clipRight)  < eps) ||
                                   (std::abs(y1 - clipBottom) < eps && std::abs(y2 - clipBottom) < eps) ||
                                   (std::abs(y1 - clipTop)    < eps && std::abs(y2 - clipTop)    < eps);
                        };

                        for (const auto& poly : solutionInner) {
                            if (poly.size() < 2) continue;
                            Contour2D currentArc;
                            auto closeArc = [&]() {
                                if (currentArc.size() >= 2) {
                                    hc.offsets.push_back(std::move(currentArc));
                                }
                                currentArc.clear();
                            };
                            for (size_t i = 0; i < poly.size(); ++i) {
                                const auto& p0 = poly[i];
                                const auto& p1 = poly[(i+1) % poly.size()];
                                double x0 = p0.x * 1e-6, y0 = p0.y * 1e-6;
                                double x1 = p1.x * 1e-6, y1 = p1.y * 1e-6;
                                if (isRectEdge(x0, y0, x1, y1)) {
                                    closeArc();
                                } else {
                                    if (currentArc.empty()) currentArc.emplace_back(x0, y0);
                                    currentArc.emplace_back(x1, y1);
                                }
                            }
                            closeArc();
                        }

                        // ---------- 保存外部片段（原样保留）----------
                        for (const auto& poly : solutionOuter) {
                            if (poly.size() < 2) continue;
                            Contour2D frag;
                            for (const auto& pt : poly) {
                                frag.emplace_back(pt.x * 1e-6, pt.y * 1e-6);
                            }
                            if (frag.size() > 1 && frag.front() != frag.back())
                                frag.push_back(frag.front());
                            hc.offsetsOuter.push_back(std::move(frag));
                        }
                    }
                }
            }*/

            if (hc.isOuter)
            {
                Paths64 clipRegion = buildClipRegion(m_params);

                int maxPasses = m_params.maxOffsetPasses;
                for (int pass = 0; pass < maxPasses; ++pass) {
                    double currentOffset = baseOffset + pass * m_params.stepover;
                    auto offsetRes = offsetContour(hc.original, currentOffset);
                    if (offsetRes.empty()) break;

                    for (const auto& ring : offsetRes) {
                        Path64 subject;
                        for (const auto& pt : ring) {
                            subject.push_back(Point64(static_cast<int64_t>(pt.x * 1e6),
                                                      static_cast<int64_t>(pt.y * 1e6)));
                        }
                        if (subject.size() > 1 && subject.front() != subject.back())
                            subject.push_back(subject.front());

                        Clipper64 clipper;
                        clipper.AddSubject({subject});
                        clipper.AddClip(clipRegion);

                        Paths64 solutionInner;
                        clipper.Execute(ClipType::Intersection, FillRule::NonZero, solutionInner);

                        Paths64 solutionOuter;
                        clipper.Execute(ClipType::Difference, FillRule::NonZero, solutionOuter);

                        // ---------- 临时收集本环的内部弧段和外部片段 ----------
                        std::vector<Contour2D> tempInner, tempOuter;

                        // 计算矩形边界（用于滤除矩形边）
                        double clipLeft = 1e18, clipRight = -1e18, clipBottom = 1e18, clipTop = -1e18;
                        if (!clipRegion.empty() && !clipRegion.front().empty()) {
                            for (const auto& pt : clipRegion.front()) {
                                double x = pt.x * 1e-6;
                                double y = pt.y * 1e-6;
                                clipLeft   = std::min(clipLeft, x);
                                clipRight  = std::max(clipRight, x);
                                clipBottom = std::min(clipBottom, y);
                                clipTop    = std::max(clipTop, y);
                            }
                        }
                        const double eps = 1e-6;
                        auto isRectEdge = [=](double x1, double y1, double x2, double y2) -> bool {
                            return (std::abs(x1 - clipLeft)   < eps && std::abs(x2 - clipLeft)   < eps) ||
                                   (std::abs(x1 - clipRight)  < eps && std::abs(x2 - clipRight)  < eps) ||
                                   (std::abs(y1 - clipBottom) < eps && std::abs(y2 - clipBottom) < eps) ||
                                   (std::abs(y1 - clipTop)    < eps && std::abs(y2 - clipTop)    < eps);
                        };

                        // 处理内部交集：仅保留圆弧段，存入 tempInner
                        for (const auto& poly : solutionInner) {
                            if (poly.size() < 2) continue;
                            Contour2D currentArc;
                            auto closeArc = [&]() {
                                if (currentArc.size() >= 2) {
                                    tempInner.push_back(std::move(currentArc));
                                }
                                currentArc.clear();
                            };
                            for (size_t i = 0; i < poly.size(); ++i) {
                                const auto& p0 = poly[i];
                                const auto& p1 = poly[(i+1) % poly.size()];
                                double x0 = p0.x * 1e-6, y0 = p0.y * 1e-6;
                                double x1 = p1.x * 1e-6, y1 = p1.y * 1e-6;
                                if (isRectEdge(x0, y0, x1, y1)) {
                                    closeArc();
                                } else {
                                    if (currentArc.empty()) currentArc.emplace_back(x0, y0);
                                    currentArc.emplace_back(x1, y1);
                                }
                            }
                            closeArc();
                        }

                        // 处理外部差集：原样保留，存入 tempOuter
                        for (const auto& poly : solutionOuter) {
                            if (poly.size() < 2) continue;
                            Contour2D frag;
                            for (const auto& pt : poly) {
                                frag.emplace_back(pt.x * 1e-6, pt.y * 1e-6);
                            }
                            if (frag.size() > 1 && frag.front() != frag.back())
                                frag.push_back(frag.front());
                            tempOuter.push_back(std::move(frag));
                        }

                        // ---------- 合并相近的未封闭片段 ----------
                        auto mergeFragments = [](std::vector<Contour2D>& fragments, double mergeTol) {
                            if (fragments.size() < 2) return;
                            const double tolSq = mergeTol * mergeTol;
                            auto distSq = [](const Point2D& a, const Point2D& b) {
                                double dx = a.x - b.x, dy = a.y - b.y;
                                return dx*dx + dy*dy;
                            };
                            bool changed = true;
                            while (changed) {
                                changed = false;
                                for (size_t i = 0; i < fragments.size() && !changed; ++i) {
                                    if (fragments[i].size() < 2) continue;
                                    for (size_t j = i + 1; j < fragments.size() && !changed; ++j) {
                                        if (fragments[j].size() < 2) continue;
                                        const auto& a_head = fragments[i].front();
                                        const auto& a_tail = fragments[i].back();
                                        const auto& b_head = fragments[j].front();
                                        const auto& b_tail = fragments[j].back();

                                        // a尾 -> b头
                                        if (distSq(a_tail, b_head) < tolSq) {
                                            fragments[i].insert(fragments[i].end(),
                                                                fragments[j].begin() + 1, fragments[j].end());
                                            fragments.erase(fragments.begin() + j);
                                            changed = true;
                                            break;
                                        }
                                        // a头 -> b尾
                                        if (distSq(a_head, b_tail) < tolSq) {
                                            fragments[i].insert(fragments[i].begin(),
                                                                fragments[j].begin(), fragments[j].end() - 1);
                                            fragments.erase(fragments.begin() + j);
                                            changed = true;
                                            break;
                                        }
                                        // a尾 -> b尾（反转b）
                                        if (distSq(a_tail, b_tail) < tolSq) {
                                            std::reverse(fragments[j].begin(), fragments[j].end());
                                            fragments[i].insert(fragments[i].end(),
                                                                fragments[j].begin() + 1, fragments[j].end());
                                            fragments.erase(fragments.begin() + j);
                                            changed = true;
                                            break;
                                        }
                                        // a头 -> b头（反转b）
                                        if (distSq(a_head, b_head) < tolSq) {
                                            std::reverse(fragments[j].begin(), fragments[j].end());
                                            fragments[i].insert(fragments[i].begin(),
                                                                fragments[j].begin(), fragments[j].end() - 1);
                                            fragments.erase(fragments.begin() + j);
                                            changed = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        };

                        mergeFragments(tempInner, 0.01);   // 10微米合并阈值
                        mergeFragments(tempOuter, 0.01);

                        // 最终保存合并后的片段
                        for (auto& frag : tempInner) {
                            hc.offsets.push_back(std::move(frag));
                        }
                        for (auto& frag : tempOuter) {
                            hc.offsetsOuter.push_back(std::move(frag));
                        }
                    }
                }
            }
            else {
                // ==================== 内轮廓：保持原有闭合偏置逻辑 ====================
                double firstOffset = -baseOffset;
                auto firstRes = offsetContour(hc.original, firstOffset);
                if (!firstRes.empty()) {
                    for (const auto& ring : firstRes) {
                        if (std::abs(polygonArea(ring)) < 0.5) continue;
                        hc.offsets.push_back(ring);
                    }

                    if (m_params.stepover > 1e-6) {
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
                                    hc.offsets.push_back(nextRing);
                                    nextQueue.push_back(nextRing);
                                }
                            }
                            queue = std::move(nextQueue);
                            ++pass;
                        }
                    }
                } else {
                    hc.offsets.push_back(hc.original);
                }
            }
            layer.contours.push_back(hc);
        }
        m_layers.push_back(layer);
        qDebug() << "  生成轮廓数:" << layer.contours.size();
    }

    qDebug() << "切片完成，共生成" << m_layers.size() << "层轮廓";

    // 填充调试用的层偏置环结构
    populateLayerOffsetRings();
    return !m_layers.empty();
}

ContourGroups SlicePathGenerator::groupContours() const {
    ContourGroups result;
    if (m_layers.empty()) return result;

    std::vector<Quantity_Color> colorPalette = {
        Quantity_Color(0.0, 1.0, 0.0, Quantity_TOC_RGB),
        Quantity_Color(1.0, 0.0, 0.0, Quantity_TOC_RGB),
        Quantity_Color(1.0, 1.0, 0.0, Quantity_TOC_RGB),
        Quantity_Color(1.0, 0.0, 1.0, Quantity_TOC_RGB),
        Quantity_Color(0.0, 1.0, 1.0, Quantity_TOC_RGB),
        Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB)
    };

    // 外轮廓组
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
        result.groupColors.push_back(Quantity_Color(0.0, 0.0, 1.0, Quantity_TOC_RGB));
    }

    // 内轮廓分组
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


//右上角
// std::vector<MaxIncircleInfo> SlicePathGenerator::computeIncircles(bool isOuter, double gridStep) {
//     std::vector<MaxIncircleInfo> results;
//     for (const auto& layer : m_layerOffsetRings) {
//         for (const auto& ring : layer.rings) {
//             if (ring.isOuter != isOuter) continue;
//             if (ring.contour.empty()) continue;

//             Point2D center;
//             double radius = 0.0;
//             std::vector<Point2D> contacts;

//             bool ok = computeMaxIncircle(ring.contour, gridStep, center, radius);
//             if (ok) {
//                 contacts = getIncircleContactPoints(ring.contour, center, radius);
//             }
//             if (!ok || contacts.empty()) {
//                 contacts.clear();
//                 contacts.push_back(ring.contour.front());
//                 center = ring.contour.front();
//                 radius = 0.0;
//             }

//             MaxIncircleInfo info;
//             info.layerId     = layer.layerId;
//             info.z           = layer.z;
//             info.contourId   = ring.contourId;
//             info.passIndex   = ring.passIndex;
//             info.isOuter     = isOuter;
//             info.center      = center;
//             info.radius      = radius;
//             info.contactPoints = contacts; // 保留完整切点
//             info.regionId    = isOuter ? 0 : ring.regionId;
//             info.area        = std::abs(ring.area);
//             results.push_back(info);
//         }
//     }

//     if (isOuter) {
//         // ---------- 边界参数 ----------
//         double stockLeft = 0, stockRight = 0, stockBottom = 0, stockTop = 0;
//         double circleCx = 0, circleCy = 0, circleR = 0;
//         bool isRect = (m_params.maxAllowedLength > 0.0 && m_params.maxAllowedWidth > 0.0);
//         if (isRect) {
//             double halfL = m_params.maxAllowedLength * 0.5;
//             double halfW = m_params.maxAllowedWidth  * 0.5;
//             stockLeft   = m_params.stockCenterX - halfL;
//             stockRight  = m_params.stockCenterX + halfL;
//             stockBottom = m_params.stockCenterY - halfW;
//             stockTop    = m_params.stockCenterY + halfW;
//         } else if (m_params.maxAllowedRadius > 0.0) {
//             circleCx = m_params.stockCenterX;
//             circleCy = m_params.stockCenterY;
//             circleR  = m_params.maxAllowedRadius;
//         }

//         const double boundaryTol = 0.5;
//         auto isOnBoundary = [&](double x, double y) -> bool {
//             if (isRect) {
//                 return std::abs(x - stockLeft)   < boundaryTol ||
//                        std::abs(x - stockRight)  < boundaryTol ||
//                        std::abs(y - stockBottom) < boundaryTol ||
//                        std::abs(y - stockTop)    < boundaryTol;
//             } else if (circleR > 0.0) {
//                 double dist = std::sqrt((x - circleCx)*(x - circleCx) + (y - circleCy)*(y - circleCy));
//                 return std::abs(dist - circleR) < boundaryTol;
//             }
//             return false;
//         };

//         auto ringTouchesBoundary = [&](const Contour2D& contour) -> bool {
//             for (const auto& pt : contour) {
//                 if (isOnBoundary(pt.x, pt.y)) return true;
//             }
//             return false;
//         };

//         // 按 contourId 分组
//         std::map<int, std::vector<size_t>> contourMap;
//         for (size_t i = 0; i < results.size(); ++i) {
//             contourMap[results[i].contourId].push_back(i);
//         }

//         const double smallRatio = 0.2;
//         for (auto& [cid, indices] : contourMap) {
//             double maxArea = 0.0;
//             for (size_t idx : indices) {
//                 maxArea = std::max(maxArea, results[idx].area);
//             }
//             if (indices.size() <= 1 || maxArea < 1e-6) {
//                 continue;
//             }

//             double threshold = maxArea * smallRatio;
//             for (size_t idx : indices) {
//                 double area = results[idx].area;
//                 bool small = (area < threshold);
//                 bool touches = false;

//                 const OffsetRing* ringPtr = nullptr;
//                 for (const auto& layer : m_layerOffsetRings) {
//                     if (std::abs(layer.z - results[idx].z) > 1e-6) continue;
//                     for (const auto& r : layer.rings) {
//                         if (r.contourId == cid && r.passIndex == results[idx].passIndex &&
//                             r.isOuter == isOuter) {
//                             ringPtr = &r;
//                             break;
//                         }
//                     }
//                     if (ringPtr) break;
//                 }

//                 if (ringPtr) {
//                     touches = ringTouchesBoundary(ringPtr->contour);
//                 }

//                 results[idx].isConcave = (small && !touches);
//             }
//         }

//         // 标记侧边凹入环的最内圈
//         for (auto& [cid, indices] : contourMap) {
//             int maxPass = -1;
//             for (size_t idx : indices) {
//                 if (results[idx].isConcave && results[idx].passIndex > maxPass) {
//                     maxPass = results[idx].passIndex;
//                 }
//             }
//             if (maxPass >= 0) {
//                 for (size_t idx : indices) {
//                     if (results[idx].isConcave && results[idx].passIndex == maxPass) {
//                         results[idx].isInnermost = true;
//                     }
//                 }
//             }
//         }

//         // ---- 调整外轮廓点的最终位置 ----
//         // 1. 所有蓝色点（非凹入环，area > 0）取环轮廓上“最右侧且 y 最大”的点
//         for (auto& info : results) {
//             if (info.isConcave || info.area <= 0 || info.contactPoints.empty()) continue;

//             Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
//             if (ringContour.empty()) {
//                 // 回退：取候选点中 x 最大的
//                 auto maxIt = std::max_element(info.contactPoints.begin(), info.contactPoints.end(),
//                                               [](const Point2D& a, const Point2D& b) { return a.x < b.x; });
//                 if (maxIt != info.contactPoints.end()) {
//                     info.contactPoints = { *maxIt };
//                 }
//                 continue;
//             }

//             // 找到最大 x 值
//             double maxX = -std::numeric_limits<double>::max();
//             for (const auto& pt : ringContour) {
//                 if (pt.x > maxX) maxX = pt.x;
//             }

//             // 在所有轮廓点中（不只是右侧点集）选出 x 接近 maxX 且 y 最大的点
//             const double eps = 1e-6;
//             Point2D bestPt = ringContour[0];
//             bool found = false;

//             for (const auto& pt : ringContour) {
//                 if (std::abs(pt.x - maxX) <= eps) {
//                     if (!found || pt.y > bestPt.y) {
//                         bestPt = pt;
//                         found = true;
//                     }
//                 }
//             }

//             if (!found) {
//                 // 安全回退
//                 bestPt = ringContour.front();
//             }

//             info.contactPoints = { bestPt };
//         }

//         // 2. 凹入环链：绿点向紫红点靠拢
//         std::map<int, std::vector<MaxIncircleInfo*>> concaveGroups;
//         for (auto& info : results) {
//             if (info.isConcave) {
//                 concaveGroups[info.contourId].push_back(&info);
//             }
//         }

//         for (auto& [cid, group] : concaveGroups) {
//             std::sort(group.begin(), group.end(),
//                       [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
//                           return a->passIndex > b->passIndex;
//                       });

//             if (group.empty()) continue;

//             MaxIncircleInfo* innermost = group.front();
//             if (!innermost->isInnermost) {
//                 innermost->contactPoints.resize(1);
//             }
//             Point2D refPt = innermost->contactPoints[0];

//             for (size_t i = 1; i < group.size(); ++i) {
//                 MaxIncircleInfo* curInfo = group[i];
//                 Contour2D ringContour = getRingContour(curInfo, m_layerOffsetRings);
//                 if (ringContour.empty()) {
//                     curInfo->contactPoints.resize(1);
//                     refPt = curInfo->contactPoints[0];
//                     continue;
//                 }
//                 Point2D greenPt = closestPointOnContour(refPt, ringContour);
//                 curInfo->contactPoints = { greenPt };
//                 refPt = greenPt;
//             }
//         }

//         for (auto& info : results) {
//             if (info.contactPoints.size() > 1) {
//                 info.contactPoints.resize(1);
//             }
//         }
//     }

//     // 去重
//     {
//         std::vector<MaxIncircleInfo> unique;
//         for (const auto& info : results) {
//             bool duplicate = false;
//             for (const auto& u : unique) {
//                 if (u.contourId == info.contourId &&
//                     u.passIndex == info.passIndex &&
//                     std::abs(u.z - info.z) < 0.01 &&
//                     u.isOuter == info.isOuter) {
//                     duplicate = true;
//                     break;
//                 }
//             }
//             if (!duplicate) unique.push_back(info);
//         }
//         results = std::move(unique);
//     }

//     if (!isOuter) {
//         adjustInnerContactPoints(results);
//     }

//     return results;
// }

//除以2
// std::vector<MaxIncircleInfo> SlicePathGenerator::computeIncircles(bool isOuter, double gridStep) {
//     std::vector<MaxIncircleInfo> results;
//     for (const auto& layer : m_layerOffsetRings) {
//         for (const auto& ring : layer.rings) {
//             if (ring.isOuter != isOuter) continue;
//             if (ring.contour.empty()) continue;

//             Point2D center;
//             double radius = 0.0;
//             std::vector<Point2D> contacts;

//             bool ok = computeMaxIncircle(ring.contour, gridStep, center, radius);
//             if (ok) {
//                 contacts = getIncircleContactPoints(ring.contour, center, radius);
//             }
//             if (!ok || contacts.empty()) {
//                 contacts.clear();
//                 contacts.push_back(ring.contour.front());
//                 center = ring.contour.front();
//                 radius = 0.0;
//             }

//             MaxIncircleInfo info;
//             info.layerId     = layer.layerId;
//             info.z           = layer.z;
//             info.contourId   = ring.contourId;
//             info.passIndex   = ring.passIndex;
//             info.isOuter     = isOuter;
//             info.center      = center;
//             info.radius      = radius;
//             info.contactPoints = contacts;
//             info.regionId    = isOuter ? 0 : ring.regionId;
//             info.area        = std::abs(ring.area);
//             results.push_back(info);
//         }
//     }

//     if (isOuter) {
//         // ---------- 边界参数 ----------
//         double stockLeft = 0, stockRight = 0, stockBottom = 0, stockTop = 0;
//         double circleCx = 0, circleCy = 0, circleR = 0;
//         bool isRect = (m_params.maxAllowedLength > 0.0 && m_params.maxAllowedWidth > 0.0);
//         if (isRect) {
//             double halfL = m_params.maxAllowedLength * 0.5;
//             double halfW = m_params.maxAllowedWidth  * 0.5;
//             stockLeft   = m_params.stockCenterX - halfL;
//             stockRight  = m_params.stockCenterX + halfL;
//             stockBottom = m_params.stockCenterY - halfW;
//             stockTop    = m_params.stockCenterY + halfW;
//         } else if (m_params.maxAllowedRadius > 0.0) {
//             circleCx = m_params.stockCenterX;
//             circleCy = m_params.stockCenterY;
//             circleR  = m_params.maxAllowedRadius;
//         }

//         const double boundaryTol = 0.5;
//         auto isOnBoundary = [&](double x, double y) -> bool {
//             if (isRect) {
//                 return std::abs(x - stockLeft)   < boundaryTol ||
//                        std::abs(x - stockRight)  < boundaryTol ||
//                        std::abs(y - stockBottom) < boundaryTol ||
//                        std::abs(y - stockTop)    < boundaryTol;
//             } else if (circleR > 0.0) {
//                 double dist = std::sqrt((x - circleCx)*(x - circleCx) + (y - circleCy)*(y - circleCy));
//                 return std::abs(dist - circleR) < boundaryTol;
//             }
//             return false;
//         };

//         auto ringTouchesBoundary = [&](const Contour2D& contour) -> bool {
//             for (const auto& pt : contour) {
//                 if (isOnBoundary(pt.x, pt.y)) return true;
//             }
//             return false;
//         };

//         // 按 contourId 分组
//         std::map<int, std::vector<size_t>> contourMap;
//         for (size_t i = 0; i < results.size(); ++i) {
//             contourMap[results[i].contourId].push_back(i);
//         }

//         const double smallRatio = 0.2;
//         for (auto& [cid, indices] : contourMap) {
//             double maxArea = 0.0;
//             for (size_t idx : indices) {
//                 maxArea = std::max(maxArea, results[idx].area);
//             }
//             if (indices.size() <= 1 || maxArea < 1e-6) continue;

//             double threshold = maxArea * smallRatio;
//             for (size_t idx : indices) {
//                 double area = results[idx].area;
//                 bool small = (area < threshold);
//                 bool touches = false;

//                 const OffsetRing* ringPtr = nullptr;
//                 for (const auto& layer : m_layerOffsetRings) {
//                     if (std::abs(layer.z - results[idx].z) > 1e-6) continue;
//                     for (const auto& r : layer.rings) {
//                         if (r.contourId == cid && r.passIndex == results[idx].passIndex &&
//                             r.isOuter == isOuter) {
//                             ringPtr = &r;
//                             break;
//                         }
//                     }
//                     if (ringPtr) break;
//                 }

//                 if (ringPtr) touches = ringTouchesBoundary(ringPtr->contour);
//                 results[idx].isConcave = (small && !touches);
//             }
//         }

//         // 标记侧边凹入环的最内圈
//         for (auto& [cid, indices] : contourMap) {
//             int maxPass = -1;
//             for (size_t idx : indices) {
//                 if (results[idx].isConcave && results[idx].passIndex > maxPass)
//                     maxPass = results[idx].passIndex;
//             }
//             if (maxPass >= 0) {
//                 for (size_t idx : indices) {
//                     if (results[idx].isConcave && results[idx].passIndex == maxPass)
//                         results[idx].isInnermost = true;
//                 }
//             }
//         }

//         // ---- 调整外轮廓点的最终位置 ----
//         // 1. 蓝色点：右侧边 y = maxY/2 处
//         for (auto& info : results) {
//             if (info.isConcave || info.area <= 0 || info.contactPoints.empty()) continue;

//             Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
//             if (ringContour.empty()) {
//                 // 回退：取候选点中 x 最大的
//                 auto maxIt = std::max_element(info.contactPoints.begin(), info.contactPoints.end(),
//                                               [](const Point2D& a, const Point2D& b) { return a.x < b.x; });
//                 if (maxIt != info.contactPoints.end()) info.contactPoints = { *maxIt };
//                 continue;
//             }

//             // 找到最大 x 值
//             double maxX = -std::numeric_limits<double>::max();
//             for (const auto& pt : ringContour) if (pt.x > maxX) maxX = pt.x;

//             const double eps = 1e-6;
//             std::vector<Point2D> rightmostPts;
//             for (const auto& pt : ringContour)
//                 if (std::abs(pt.x - maxX) <= eps) rightmostPts.push_back(pt);

//             double maxY = -std::numeric_limits<double>::max();
//             for (const auto& pt : rightmostPts) if (pt.y > maxY) maxY = pt.y;

//             double targetY = maxY * 0.5;
//             Point2D bestPt = rightmostPts.empty() ? ringContour.front() : rightmostPts[0];
//             double bestDist = std::numeric_limits<double>::max();
//             for (const auto& pt : rightmostPts) {
//                 double dy = pt.y - targetY;
//                 double dist = dy * dy;
//                 if (dist < bestDist) { bestDist = dist; bestPt = pt; }
//             }
//             info.contactPoints = { bestPt };
//         }

//         // 2. 凹入环链：绿点向紫红点靠拢
//         std::map<int, std::vector<MaxIncircleInfo*>> concaveGroups;
//         for (auto& info : results)
//             if (info.isConcave) concaveGroups[info.contourId].push_back(&info);

//         for (auto& [cid, group] : concaveGroups) {
//             std::sort(group.begin(), group.end(),
//                       [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
//                           return a->passIndex > b->passIndex;
//                       });
//             if (group.empty()) continue;

//             MaxIncircleInfo* innermost = group.front();
//             if (!innermost->isInnermost) innermost->contactPoints.resize(1);
//             Point2D refPt = innermost->contactPoints[0];

//             for (size_t i = 1; i < group.size(); ++i) {
//                 MaxIncircleInfo* curInfo = group[i];
//                 Contour2D ringContour = getRingContour(curInfo, m_layerOffsetRings);
//                 if (ringContour.empty()) {
//                     curInfo->contactPoints.resize(1);
//                     refPt = curInfo->contactPoints[0];
//                     continue;
//                 }
//                 Point2D greenPt = closestPointOnContour(refPt, ringContour);
//                 curInfo->contactPoints = { greenPt };
//                 refPt = greenPt;
//             }
//         }

//         // 强制保留一个点
//         for (auto& info : results)
//             if (info.contactPoints.size() > 1) info.contactPoints.resize(1);
//     }

//     // 去重
//     {
//         std::vector<MaxIncircleInfo> unique;
//         for (const auto& info : results) {
//             bool duplicate = false;
//             for (const auto& u : unique) {
//                 if (u.contourId == info.contourId &&
//                     u.passIndex == info.passIndex &&
//                     std::abs(u.z - info.z) < 0.01 &&
//                     u.isOuter == info.isOuter) {
//                     duplicate = true;
//                     break;
//                 }
//             }
//             if (!duplicate) unique.push_back(info);
//         }
//         results = std::move(unique);
//     }

//     if (!isOuter) {
//         adjustInnerContactPoints(results);
//     }

//     return results;
// }

// std::vector<MaxIncircleInfo> SlicePathGenerator::computeIncircles(bool isOuter, double gridStep) {
//     std::vector<MaxIncircleInfo> results;
//     // 1. 遍历所有层的所有偏置环，计算最大内切圆
//     for (const auto& layer : m_layerOffsetRings) {
//         for (const auto& ring : layer.rings) {
//             if (ring.isOuter != isOuter) continue;
//             if (ring.contour.empty()) continue;

//             Point2D center;
//             double radius = 0.0;
//             std::vector<Point2D> contacts;

//             bool ok = computeMaxIncircle(ring.contour, gridStep, center, radius);
//             if (ok) {
//                 contacts = getIncircleContactPoints(ring.contour, center, radius);
//             }
//             if (!ok || contacts.empty()) {
//                 contacts.clear();
//                 contacts.push_back(ring.contour.front());
//                 center = ring.contour.front();
//                 radius = 0.0;
//             }

//             MaxIncircleInfo info;
//             info.layerId     = layer.layerId;
//             info.z           = layer.z;
//             info.contourId   = ring.contourId;
//             info.passIndex   = ring.passIndex;
//             info.isOuter     = isOuter;
//             info.center      = center;
//             info.radius      = radius;
//             info.contactPoints = contacts;
//             info.regionId    = isOuter ? 0 : ring.regionId;
//             info.area        = std::abs(ring.area);
//             results.push_back(info);
//         }
//     }

//     // 2. 仅处理外轮廓的修饰：凹入识别、边界残片选点、蓝点定位等
//     if (isOuter) {
//         // ---------- 毛坯边界参数 ----------
//         double stockLeft   = 0, stockRight = 0, stockBottom = 0, stockTop = 0;
//         double circleCx = 0, circleCy = 0, circleR = 0;
//         bool isRect = (m_params.maxAllowedLength > 0.0 && m_params.maxAllowedWidth > 0.0);
//         if (isRect) {
//             double halfL = m_params.maxAllowedLength * 0.5;
//             double halfW = m_params.maxAllowedWidth  * 0.5;
//             stockLeft   = m_params.stockCenterX - halfL;
//             stockRight  = m_params.stockCenterX + halfL;
//             stockBottom = m_params.stockCenterY - halfW;
//             stockTop    = m_params.stockCenterY + halfW;
//         } else if (m_params.maxAllowedRadius > 0.0) {
//             circleCx = m_params.stockCenterX;
//             circleCy = m_params.stockCenterY;
//             circleR  = m_params.maxAllowedRadius;
//         }

//         const double boundaryTol = 0.5;
//         auto isOnBoundary = [&](double x, double y) -> bool {
//             if (isRect) {
//                 return std::abs(x - stockLeft)   < boundaryTol ||
//                        std::abs(x - stockRight)  < boundaryTol ||
//                        std::abs(y - stockBottom) < boundaryTol ||
//                        std::abs(y - stockTop)    < boundaryTol;
//             } else if (circleR > 0.0) {
//                 double dist = std::sqrt((x - circleCx)*(x - circleCx) + (y - circleCy)*(y - circleCy));
//                 return std::abs(dist - circleR) < boundaryTol;
//             }
//             return false;
//         };

//         auto ringTouchesBoundary = [&](const Contour2D& contour) -> bool {
//             for (const auto& pt : contour) {
//                 if (isOnBoundary(pt.x, pt.y)) return true;
//             }
//             return false;
//         };

//         // ---------- 凹入环识别 ----------
//         std::map<int, std::vector<size_t>> contourMap;
//         for (size_t i = 0; i < results.size(); ++i) {
//             contourMap[results[i].contourId].push_back(i);
//         }

//         const double smallRatio = 0.2;
//         for (auto& [cid, indices] : contourMap) {
//             double maxArea = 0.0;
//             for (size_t idx : indices) {
//                 maxArea = std::max(maxArea, results[idx].area);
//             }
//             if (indices.size() <= 1 || maxArea < 1e-6) continue;

//             double threshold = maxArea * smallRatio;
//             for (size_t idx : indices) {
//                 double area = results[idx].area;
//                 bool small = (area < threshold);
//                 bool touches = false;

//                 const OffsetRing* ringPtr = nullptr;
//                 for (const auto& layer : m_layerOffsetRings) {
//                     if (std::abs(layer.z - results[idx].z) > 1e-6) continue;
//                     for (const auto& r : layer.rings) {
//                         if (r.contourId == cid && r.passIndex == results[idx].passIndex &&
//                             r.isOuter == isOuter) {
//                             ringPtr = &r;
//                             break;
//                         }
//                     }
//                     if (ringPtr) break;
//                 }

//                 if (ringPtr) touches = ringTouchesBoundary(ringPtr->contour);
//                 results[idx].isConcave = (small && !touches);
//             }
//         }

//         // 标记侧边凹入环的最内圈
//         for (auto& [cid, indices] : contourMap) {
//             int maxPass = -1;
//             for (size_t idx : indices) {
//                 if (results[idx].isConcave && results[idx].passIndex > maxPass)
//                     maxPass = results[idx].passIndex;
//             }
//             if (maxPass >= 0) {
//                 for (size_t idx : indices) {
//                     if (results[idx].isConcave && results[idx].passIndex == maxPass)
//                         results[idx].isInnermost = true;
//                 }
//             }
//         }

//         // ========== 处理面积为零的边界残片（全部保留一个蓝点，正确左右判定） ==========
//         for (auto& info : results) {
//             if (info.isConcave || info.area > 0) continue;
//             if (info.contactPoints.empty()) continue;

//             Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
//             if (ringContour.empty()) {
//                 info.contactPoints.resize(1);
//                 continue;
//             }

//             // 可靠左右判定：使用残片包围盒中心 X 与毛坯中心比较
//             double minX = ringContour[0].x, maxX = minX;
//             for (const auto& pt : ringContour) {
//                 if (pt.x < minX) minX = pt.x;
//                 if (pt.x > maxX) maxX = pt.x;
//             }
//             double midX = (minX + maxX) * 0.5;
//             bool isLeft = (midX < m_params.stockCenterX);

//             // 根据顺逆铣确定目标 X
//             double targetX;
//             if (m_params.climbMilling) {
//                 // 顺铣：左侧残片选最右点（靠近工件），右侧残片选最左点
//                 targetX = isLeft ? std::numeric_limits<double>::max()
//                                  : -std::numeric_limits<double>::max();
//             } else {
//                 // 逆铣：左侧残片选最左点，右侧残片选最右点
//                 targetX = isLeft ? -std::numeric_limits<double>::max()
//                                  : std::numeric_limits<double>::max();
//             }

//             // 挑选最佳点：优先满足 targetX，同 X 时选 y 最小
//             Point2D chosenPt = ringContour[0];
//             double bestDist = std::abs(chosenPt.x - targetX);
//             for (const auto& pt : ringContour) {
//                 double d = std::abs(pt.x - targetX);
//                 if (d < bestDist - 1e-9) {
//                     bestDist = d;
//                     chosenPt = pt;
//                 } else if (std::abs(d - bestDist) < 1e-9 && pt.y < chosenPt.y) {
//                     chosenPt = pt;
//                 }
//             }
//             info.contactPoints = { chosenPt };
//         }

//         // ========== 主体蓝点（area > 0）定位到最右侧 y = maxY/2 ==========
//         for (auto& info : results) {
//             if (info.isConcave || info.area <= 0 || info.contactPoints.empty()) continue;

//             Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
//             if (ringContour.empty()) {
//                 auto maxIt = std::max_element(info.contactPoints.begin(), info.contactPoints.end(),
//                                               [](const Point2D& a, const Point2D& b) { return a.x < b.x; });
//                 if (maxIt != info.contactPoints.end()) info.contactPoints = { *maxIt };
//                 continue;
//             }

//             double maxX = -std::numeric_limits<double>::max();
//             for (const auto& pt : ringContour) if (pt.x > maxX) maxX = pt.x;

//             const double eps = 1e-6;
//             std::vector<Point2D> rightmostPts;
//             for (const auto& pt : ringContour)
//                 if (std::abs(pt.x - maxX) <= eps) rightmostPts.push_back(pt);

//             double maxY = -std::numeric_limits<double>::max();
//             for (const auto& pt : rightmostPts) if (pt.y > maxY) maxY = pt.y;

//             double targetY = maxY * 0.5;
//             Point2D bestPt = rightmostPts.empty() ? ringContour.front() : rightmostPts[0];
//             double bestDist = std::numeric_limits<double>::max();
//             for (const auto& pt : rightmostPts) {
//                 double dy = pt.y - targetY;
//                 double dist = dy * dy;
//                 if (dist < bestDist) { bestDist = dist; bestPt = pt; }
//             }
//             info.contactPoints = { bestPt };
//         }

//         // ========== 凹入环链：绿点向紫红点靠拢 ==========
//         std::map<int, std::vector<MaxIncircleInfo*>> concaveGroups;
//         for (auto& info : results)
//             if (info.isConcave) concaveGroups[info.contourId].push_back(&info);

//         for (auto& [cid, group] : concaveGroups) {
//             std::sort(group.begin(), group.end(),
//                       [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
//                           return a->passIndex > b->passIndex;
//                       });
//             if (group.empty()) continue;

//             MaxIncircleInfo* innermost = group.front();
//             if (!innermost->isInnermost) innermost->contactPoints.resize(1);
//             Point2D refPt = innermost->contactPoints[0];

//             for (size_t i = 1; i < group.size(); ++i) {
//                 MaxIncircleInfo* curInfo = group[i];
//                 Contour2D ringContour = getRingContour(curInfo, m_layerOffsetRings);
//                 if (ringContour.empty()) {
//                     curInfo->contactPoints.resize(1);
//                     refPt = curInfo->contactPoints[0];
//                     continue;
//                 }
//                 Point2D greenPt = closestPointOnContour(refPt, ringContour);
//                 curInfo->contactPoints = { greenPt };
//                 refPt = greenPt;
//             }
//         }

//         // 强制保留一个点
//         for (auto& info : results)
//             if (info.contactPoints.size() > 1) info.contactPoints.resize(1);
//     }

//     // 3. 去重
//     {
//         std::vector<MaxIncircleInfo> unique;
//         for (const auto& info : results) {
//             bool duplicate = false;
//             for (const auto& u : unique) {
//                 if (u.contourId == info.contourId &&
//                     u.passIndex == info.passIndex &&
//                     std::abs(u.z - info.z) < 0.01 &&
//                     u.isOuter == info.isOuter) {
//                     duplicate = true;
//                     break;
//                 }
//             }
//             if (!duplicate && !info.contactPoints.empty()) {
//                 unique.push_back(info);
//             }
//         }
//         results = std::move(unique);
//     }

//     // 4. 内轮廓额外调整切点位置
//     if (!isOuter) {
//         adjustInnerContactPoints(results);
//     }

//     // 最终确保每个 info 只有一个接触点
//     for (auto& info : results) {
//         if (info.contactPoints.size() > 1) {
//             info.contactPoints.resize(1);
//         }
//     }

//     return results;
// }



std::vector<MaxIncircleInfo> SlicePathGenerator::computeIncircles(bool isOuter, double gridStep) {
    std::vector<MaxIncircleInfo> results;
    // 1. 遍历所有层的所有偏置环，计算最大内切圆
    for (const auto& layer : m_layerOffsetRings) {
        for (const auto& ring : layer.rings) {
            if (ring.isOuter != isOuter) continue;
            if (ring.contour.empty()) continue;

            Point2D center;
            double radius = 0.0;
            std::vector<Point2D> contacts;

            bool ok = computeMaxIncircle(ring.contour, gridStep, center, radius);
            if (ok) {
                contacts = getIncircleContactPoints(ring.contour, center, radius);
            }
            if (!ok || contacts.empty()) {
                contacts.clear();
                contacts.push_back(ring.contour.front());
                center = ring.contour.front();
                radius = 0.0;
            }

            MaxIncircleInfo info;
            info.layerId     = layer.layerId;
            info.z           = layer.z;
            info.contourId   = ring.contourId;
            info.passIndex   = ring.passIndex;
            info.isOuter     = isOuter;
            info.center      = center;
            info.radius      = radius;
            info.contactPoints = contacts;
            info.regionId    = isOuter ? 0 : ring.regionId;
            info.area        = std::abs(ring.area);
            results.push_back(info);
        }
    }

    // 2. 仅处理外轮廓的修饰：凹入识别、边界残片选点、蓝点定位等
    if (isOuter) {
        // ---------- 毛坯边界参数 ----------
        double stockLeft   = 0, stockRight = 0, stockBottom = 0, stockTop = 0;
        double circleCx = 0, circleCy = 0, circleR = 0;
        bool isRect = (m_params.maxAllowedLength > 0.0 && m_params.maxAllowedWidth > 0.0);
        if (isRect) {
            double halfL = m_params.maxAllowedLength * 0.5;
            double halfW = m_params.maxAllowedWidth  * 0.5;
            stockLeft   = m_params.stockCenterX - halfL;
            stockRight  = m_params.stockCenterX + halfL;
            stockBottom = m_params.stockCenterY - halfW;
            stockTop    = m_params.stockCenterY + halfW;
        } else if (m_params.maxAllowedRadius > 0.0) {
            circleCx = m_params.stockCenterX;
            circleCy = m_params.stockCenterY;
            circleR  = m_params.maxAllowedRadius;
        }

        const double boundaryTol = 0.5;
        auto isOnBoundary = [&](double x, double y) -> bool {
            if (isRect) {
                return std::abs(x - stockLeft)   < boundaryTol ||
                       std::abs(x - stockRight)  < boundaryTol ||
                       std::abs(y - stockBottom) < boundaryTol ||
                       std::abs(y - stockTop)    < boundaryTol;
            } else if (circleR > 0.0) {
                double dist = std::sqrt((x - circleCx)*(x - circleCx) + (y - circleCy)*(y - circleCy));
                return std::abs(dist - circleR) < boundaryTol;
            }
            return false;
        };

        auto ringTouchesBoundary = [&](const Contour2D& contour) -> bool {
            for (const auto& pt : contour) {
                if (isOnBoundary(pt.x, pt.y)) return true;
            }
            return false;
        };

        // ---------- 凹入环识别 ----------
        std::map<int, std::vector<size_t>> contourMap;
        for (size_t i = 0; i < results.size(); ++i) {
            contourMap[results[i].contourId].push_back(i);
        }

        const double smallRatio = 0.2;
        for (auto& [cid, indices] : contourMap) {
            double maxArea = 0.0;
            for (size_t idx : indices) {
                maxArea = std::max(maxArea, results[idx].area);
            }
            if (indices.size() <= 1 || maxArea < 1e-6) continue;

            double threshold = maxArea * smallRatio;
            for (size_t idx : indices) {
                double area = results[idx].area;
                bool small = (area < threshold);
                bool touches = false;

                const OffsetRing* ringPtr = nullptr;
                for (const auto& layer : m_layerOffsetRings) {
                    if (std::abs(layer.z - results[idx].z) > 1e-6) continue;
                    for (const auto& r : layer.rings) {
                        if (r.contourId == cid && r.passIndex == results[idx].passIndex &&
                            r.isOuter == isOuter) {
                            ringPtr = &r;
                            break;
                        }
                    }
                    if (ringPtr) break;
                }

                if (ringPtr) touches = ringTouchesBoundary(ringPtr->contour);
                results[idx].isConcave = (small && !touches);
            }
        }

        // 标记侧边凹入环的最内圈
        for (auto& [cid, indices] : contourMap) {
            int maxPass = -1;
            for (size_t idx : indices) {
                if (results[idx].isConcave && results[idx].passIndex > maxPass)
                    maxPass = results[idx].passIndex;
            }
            if (maxPass >= 0) {
                for (size_t idx : indices) {
                    if (results[idx].isConcave && results[idx].passIndex == maxPass)
                        results[idx].isInnermost = true;
                }
            }
        }

        // ========== 主体蓝点（area > 0）定位到最右侧 y = maxY/2 ==========
        for (auto& info : results) {
            if (info.isConcave || info.area <= 0 || info.contactPoints.empty()) continue;

            Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
            if (ringContour.empty()) {
                auto maxIt = std::max_element(info.contactPoints.begin(), info.contactPoints.end(),
                                              [](const Point2D& a, const Point2D& b) { return a.x < b.x; });
                if (maxIt != info.contactPoints.end()) info.contactPoints = { *maxIt };
                continue;
            }

            double maxX = -std::numeric_limits<double>::max();
            for (const auto& pt : ringContour) if (pt.x > maxX) maxX = pt.x;

            const double eps = 1e-6;
            std::vector<Point2D> rightmostPts;
            for (const auto& pt : ringContour)
                if (std::abs(pt.x - maxX) <= eps) rightmostPts.push_back(pt);

            double maxY = -std::numeric_limits<double>::max();
            for (const auto& pt : rightmostPts) if (pt.y > maxY) maxY = pt.y;

            double targetY = maxY * 0.5;
            Point2D bestPt = rightmostPts.empty() ? ringContour.front() : rightmostPts[0];
            double bestDist = std::numeric_limits<double>::max();
            for (const auto& pt : rightmostPts) {
                double dy = pt.y - targetY;
                double dist = dy * dy;
                if (dist < bestDist) { bestDist = dist; bestPt = pt; }
            }
            info.contactPoints = { bestPt };
        }

        // ========== 凹入环链：绿点向紫红点靠拢 ==========
        std::map<int, std::vector<MaxIncircleInfo*>> concaveGroups;
        for (auto& info : results)
            if (info.isConcave) concaveGroups[info.contourId].push_back(&info);

        for (auto& [cid, group] : concaveGroups) {
            std::sort(group.begin(), group.end(),
                      [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
                          return a->passIndex > b->passIndex;
                      });
            if (group.empty()) continue;

            MaxIncircleInfo* innermost = group.front();
            if (!innermost->isInnermost) innermost->contactPoints.resize(1);
            Point2D refPt = innermost->contactPoints[0];

            for (size_t i = 1; i < group.size(); ++i) {
                MaxIncircleInfo* curInfo = group[i];
                Contour2D ringContour = getRingContour(curInfo, m_layerOffsetRings);
                if (ringContour.empty()) {
                    curInfo->contactPoints.resize(1);
                    refPt = curInfo->contactPoints[0];
                    continue;
                }
                Point2D greenPt = closestPointOnContour(refPt, ringContour);
                curInfo->contactPoints = { greenPt };
                refPt = greenPt;
            }
        }

        // ========== 【新增】边界残片通用起点选择（基于端点距离 + 顺逆铣） ==========
        for (auto& info : results) {
            // 只处理外轮廓且未被凹入环处理过的环，且面积不大于0（即边界残片或未进主体处理的环）
            if (!info.isOuter || info.isConcave || info.area > 0) continue;
            if (info.contactPoints.empty()) continue;

            Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
            if (ringContour.size() < 2) continue;

            const Point2D& p0 = ringContour.front();
            const Point2D& p1 = ringContour.back();
            double d0 = (p0.x - m_params.stockCenterX) * (p0.x - m_params.stockCenterX) +
                        (p0.y - m_params.stockCenterY) * (p0.y - m_params.stockCenterY);
            double d1 = (p1.x - m_params.stockCenterX) * (p1.x - m_params.stockCenterX) +
                        (p1.y - m_params.stockCenterY) * (p1.y - m_params.stockCenterY);

            Point2D chosen;
            if (m_params.climbMilling) {
                chosen = (d0 <= d1) ? p0 : p1;   // 顺铣近端
            } else {
                chosen = (d0 > d1) ? p0 : p1;    // 逆铣远端
            }
            info.contactPoints = { chosen };
        }

        // 强制保留一个点
        for (auto& info : results)
            if (info.contactPoints.size() > 1) info.contactPoints.resize(1);
    }

    // 3. 去重
    {
        std::vector<MaxIncircleInfo> unique;
        for (const auto& info : results) {
            bool duplicate = false;
            for (const auto& u : unique) {
                if (u.contourId == info.contourId &&
                    u.passIndex == info.passIndex &&
                    std::abs(u.z - info.z) < 0.01 &&
                    u.isOuter == info.isOuter) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate && !info.contactPoints.empty()) {
                unique.push_back(info);
            }
        }
        results = std::move(unique);
    }

    // 4. 内轮廓额外调整切点位置
    if (!isOuter) {
        adjustInnerContactPoints(results);
    }

    // ========== 最终强制调整：左上/右上象限的残片 → 强制取终点（back） ==========
    for (auto& info : results) {
        if (!info.isOuter || info.contactPoints.empty()) continue;
        if (info.isConcave || info.area > 0) continue;

        Contour2D ringContour = getRingContour(&info, m_layerOffsetRings);
        if (ringContour.size() < 2) continue;

        double minX = ringContour[0].x, maxX = minX;
        double minY = ringContour[0].y, maxY = minY;
        for (const auto& pt : ringContour) {
            if (pt.x < minX) minX = pt.x;
            if (pt.x > maxX) maxX = pt.x;
            if (pt.y < minY) minY = pt.y;
            if (pt.y > maxY) maxY = pt.y;
        }
        double midX = (minX + maxX) * 0.5;
        double midY = (minY + maxY) * 0.5;

        // 严格左上、右上象限判断
        bool isTopLeft  = (midX < m_params.stockCenterX) && (midY >= m_params.stockCenterY);
        bool isTopRight = (midX >= m_params.stockCenterX) && (midY >= m_params.stockCenterY);

        if (isTopLeft || isTopRight) {
            info.contactPoints = { ringContour.back() };
        }
    }

    // 最终确保每个 info 只有一个接触点
    for (auto& info : results) {
        if (info.contactPoints.size() > 1) {
            info.contactPoints.resize(1);
        }
    }

    return results;
}

std::vector<MaxIncircleInfo> SlicePathGenerator::computeInnerIncircles(double gridStep) {
    return computeIncircles(false, gridStep);
}
