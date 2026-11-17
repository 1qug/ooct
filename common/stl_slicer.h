// stl_slicer.h
#ifndef STL_SLICER_H
#define STL_SLICER_H

#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <cmath>
#include <limits>
#include <algorithm>
#include <QDebug>
#include <Quantity_Color.hxx>
#include "thirdparty/clipper/clipper2/clipper.h"
#include "thirdparty/clipper/clipper2/clipper.engine.h"
#include "thirdparty/clipper/clipper2/clipper.core.h"
#include "thirdparty/clipper/clipper2/clipper.offset.h"
#include "thirdparty/clipper/clipper2/clipper.rectclip.h"

// 前向声明
struct VertexTWO;
class STLObject;

// ---------- 运动类型枚举 ----------
enum class MoveType {
    NormalCut = 0,      // 主切削路径（绿色实线）
    Spiral = 1,         // 螺旋下刀路径（橙色实线）
    RapidMove = 2,      // 安全高度的快速移动（红色虚线）
    NormalEntry = 3,    // 正常下刀（黄色虚线）
    CuttingHeight = 4,  // 慢速下刀部分（蓝色实线）
    ExitMove = 5,       // 抬刀到安全高度（紫色虚线）
    RapidLine = 6       // 层内快速连接（橙色实线）
};

// ---------- 基础几何类型 ----------
struct Point2D {
    double x, y;
    Point2D(double x = 0, double y = 0) : x(x), y(y) {}
    bool operator==(const Point2D& other) const {
        return std::abs(x - other.x) < 1e-9 && std::abs(y - other.y) < 1e-9;
    }
    bool operator!=(const Point2D& other) const {
        return !(*this == other);
    }
};

struct Point3D {
    double x, y, z;
    MoveType moveType = MoveType::NormalCut;

    Point3D(double x = 0, double y = 0, double z = 0, MoveType type = MoveType::NormalCut)
        : x(x), y(y), z(z), moveType(type) {}
    double distanceTo(const Point3D& other) const {
        double dx = x - other.x, dy = y - other.y, dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

// 切片线段（用于中间计算）
struct SliceSegment {
    Point3D p1, p2;
    SliceSegment(const Point3D& a, const Point3D& b) : p1(a), p2(b) {}
};

// 三角形面片
class Triangler {
public:
    Point3D v1, v2, v3;
    double min_z, max_z;
    Triangler(const Point3D& a, const Point3D& b, const Point3D& c)
        : v1(a), v2(b), v3(c) {
        min_z = std::min({a.z, b.z, c.z});
        max_z = std::max({a.z, b.z, c.z});
    }
};

// 2D轮廓
using Contour2D = std::vector<Point2D>;

// ---------- 层数据结构 ----------
struct LayerContour {
    Contour2D original;                      // 原始切片轮廓（未偏置）
    std::vector<Contour2D> offsets;          // 内部偏置片段（交集部分，闭合或接近闭合）
    std::vector<Contour2D> offsetsOuter;     // ★ 外部偏置片段（差集部分，在裁剪区域外）
    std::vector<Contour2D> offsetSegments;   // 原字段保留，如不需要可忽略
    int level;                               // 嵌套层级（0 = 最外层）
    bool isOuter;                            // 是否为外轮廓（相对于毛坯）
    bool isClosed;                           // 是否为封闭轮廓
    int id;                                  // 全局唯一编号（从0开始）
    double area;                             // 多边形面积

    LayerContour() : level(0), isOuter(false), isClosed(false), id(-1), area(0.0) {}
};

struct LayerData {
    double z;                                   // 层高
    std::vector<LayerContour> contours;         // 所有封闭轮廓
    std::vector<Point3D> pathPoints;            // 本层完整路径点（调试用）
    std::vector<SliceSegment> rawSegments;      // 原始切片线段（调试用）
};

// 表示一个单独的轮廓（带层高和原始指针，便于查找）
struct GroupedContour {
    double z;                // 所在层高度
    const LayerContour* contour;  // 指向原始轮廓数据
    int groupId;             // 所属组ID
    int indexInGroup;        // 组内序号（从底部开始0,1,2...）
};

// 编组结果
struct ContourGroups {
    std::vector<std::vector<GroupedContour>> groups;  // 每组内的轮廓列表
    std::vector<Quantity_Color> groupColors;          // 每组对应的颜色
};

// ---------- 切片参数 ----------
struct SliceParams {
    double layerHeight = 1.0;           // 层高
    double topZ = 0.0;                  // 顶部高度
    double bottomZ = -10.0;             // 底部高度
    double sideMargin = 0.0;            // 侧向余量（正值向外，负值向内）
    double toolDiameter = 6.0;          // 刀具直径
    bool climbMilling = true;           // 顺铣/逆铣（仅用于方向参考）
    double safeZ = 5.0;                 // 安全高度
    double cuttingHeight = 2.0;         // 慢速下刀高度
    double stepover = 2.0;              // 步距

    double pointSpacing = 0.2;          // 离散点间距（调试用）

    double maxAllowedLength = 0.0;      // 矩形毛坯允许的最大长度（X方向）
    double maxAllowedWidth  = 0.0;      // 矩形毛坯允许的最大宽度（Y方向）
    double maxAllowedRadius = 0.0;      // 圆形毛坯允许的最大半径

    double stockCenterX = 0.0;
    double stockCenterY = 0.0;

    int maxOffsetPasses = 300;          // 最多偏置次数（内轮廓）
};

//======================================

// 单个偏置环
struct OffsetRing {
    Contour2D contour;   // 偏置环顶点（可以是开放或闭合）
    int contourId;       // 所属轮廓的全局ID（对应 LayerContour::id）
    int passIndex;       // 该轮廓内的偏置序号（0表示第一个偏置环）
    bool isOuter;        // 是否为外轮廓
    double area;         // 环的面积（可选）
    int regionId = -1;   // 所属较宽区域编号（-1 表示未分配）
};

// 一层中所有偏置环的集合
struct LayerOffsetRings {
    double z;                          // 层高
    int layerId;                       // 层序号（0,1,2...）
    std::vector<OffsetRing> rings;     // 该层所有偏置环（含开放片段）
};
//+===============================================+
// 最大内切圆信息
struct MaxIncircleInfo {
    int layerId=-1;                         // 层 ID
    double z=0.0;                            // 层高 (mm)
    int contourId=-1;                       // 所属轮廓 ID
    int passIndex=-1;                       // 偏置序号
    bool isOuter;
    Point2D center;                      // 圆心 (mm)
    double radius=0.0;                       // 半径 (mm)
    std::vector<Point2D> contactPoints;  // 切点 (mm)
    int regionId = -1;   // 从对应偏置环传递过来的区域编号
    bool isInnermost = false;   // 是否为该独占区域最内层环
    bool isConcave = false;   // 是否为侧边凹入产生的环（面积明显偏小）
    double area = 0.0;        // 偏置环的面积（绝对值），用于大小比较
};
// 内轮廓路径连接段
struct InnerPathSegment {
    std::vector<Point2D> points;  // 段上的点序列（用于绘制多段线）
    double z=0.0;                     // 该段所在层的 Z 高度
    bool arc=true;                     // true: 沿轮廓的弧段（点已按轮廓采样）; false: 直线段
};

struct InnerPathConnection {
    std::vector<InnerPathSegment> segments; // 一组内轮廓的完整连接路径
};
//+==============================================
// ---------- 主类：切片与路径生成器 ----------
class SlicePathGenerator {
public:
    SlicePathGenerator();
    explicit SlicePathGenerator(const std::vector<std::shared_ptr<Triangler>>& triangles);

    // 工厂方法：从 STLObject 创建（自动提取三角面片）
    static std::unique_ptr<SlicePathGenerator> fromSTLObject(const STLObject* obj);

    // 辅助函数
    static double polygonArea(const Contour2D& poly);
    static bool pointInPolygon(const Point2D& pt, const Contour2D& poly);

    // 参数设置
    void setParams(const SliceParams& params) { m_params = params; }
    void setTriangles(const std::vector<std::shared_ptr<Triangler>>& tris) { m_triangles = tris; }

    // 结果获取
    const std::vector<LayerData>& getLayers() const { return m_layers; }
    std::vector<Point3D> getAllPathPoints() const;

    // 只计算到偏置轮廓，不生成刀具路径
    bool generateContoursOnly();

    // 轮廓分组
    ContourGroups groupContours() const;

    const std::vector<LayerOffsetRings>& getLayerOffsetRings() const { return m_layerOffsetRings; }
    // 计算所有内轮廓偏置环的最大内切圆 (gridStep: 网格搜索步长 mm)
    std::vector<MaxIncircleInfo> computeInnerIncircles(double gridStep = 0.2);
    std::vector<MaxIncircleInfo> computeIncircles(bool isOuter, double gridStep = 0.2);
private:
    std::vector<std::shared_ptr<Triangler>> m_triangles;
    SliceParams m_params;
    std::vector<LayerData> m_layers;

    // 核心步骤函数
    std::vector<SliceSegment> sliceAtZ(double z) const;
    std::vector<Contour2D> connectSegmentsToContours(const std::vector<SliceSegment>& segments) const;
    std::vector<Contour2D> offsetContour(const Contour2D& contour, double distance) const;
    std::vector<LayerContour> buildContourHierarchy(const std::vector<Contour2D>& rawContours) const;

    void populateLayerOffsetRings();
    void identifyRegions();   // 对内轮廓的偏置环进行区域聚类
    void adjustInnerContactPoints(std::vector<MaxIncircleInfo>& infos) const;

    std::vector<LayerOffsetRings> m_layerOffsetRings;  // 拆分后的偏置环容器
};

#endif // STL_SLICER_H
