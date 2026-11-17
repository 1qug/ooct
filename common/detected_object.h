#ifndef DETECTED_OBJECT_H
#define DETECTED_OBJECT_H

#include <string>
#include <vector>
#include <memory>

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <glm/glm.hpp>

// OpenCASCADE 相关头文件
#include <AIS_Shape.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <BRepMesh_IncrementalMesh.hxx>


// 项目内部头文件
#include "stl_utils.h"
#include "stl_slicer.h"       // 包含 Triangler、Point3D 等

// 前向声明
struct vision_BoundingBox;
struct VertexTWO;

// ============================================================================
// 基类：DetectedObject（所有可检测物体的抽象接口）
// ============================================================================
class DetectedObject {
public:
    virtual ~DetectedObject();

    // 纯虚函数，子类必须实现
    virtual void init(QOpenGLFunctions_3_3_Core* gl) = 0;
    virtual void render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) = 0;
    virtual bool isPointInside(const glm::vec3& worldPoint) const = 0;

    // 虚函数，可被子类重写
    virtual std::vector<glm::vec3> getCrossSection(float worldTargetZ) const;
    virtual void setDimensions(float w, float h);
    virtual void setRadius(float r);

    virtual float getWidth() const;
    virtual float getHeight() const;
    virtual float getRadius() const;

    // 属性访问（只读）
    const std::string& getClassName() const { return m_className; }
    const std::string& getName() const      { return m_name; }
    const Point3D& getCenter() const        { return m_center; }
    float getDepth() const                  { return m_depth; }
    float getZ() const                      { return m_center.z - m_depth / 2.0f; }
    float getPlaneDepth() const             { return m_planeDepth; }
    float getAngle() const                  { return m_angle; }
    const vision_BoundingBox& getModelBbox() const { return m_modelBbox; }

    // 属性修改
    void setClassName(const std::string& cls) { m_className = cls; }
    void setName(const std::string& name)     { m_name = name; }
    void setCenter(float x, float y, float z) { m_center = Point3D(x, y, z); }
    void setDepth(float depth)                { m_depth = depth; }
    void setPlaneDepth(float pd)              { m_planeDepth = pd; }
    void setAngle(float angle)                { m_angle = angle; }
    void setModelBbox(const vision_BoundingBox& bbox) { m_modelBbox = bbox; }

    // 辅助工具函数
    bool isPointInPolygon(float x, float y, const std::vector<glm::vec3>& polygon) const;

    // 索引属性（可选）
    int index = 0;

protected:
    // 子类通过以下接口访问 OpenGL 资源
    QOpenGLVertexArrayObject& getVAO() { return m_vao; }
    QOpenGLBuffer& getVBO()            { return m_vbo; }
    int getVertexCount() const         { return m_vertexCount; }
    void setVertexCount(int count)     { m_vertexCount = count; }

private:
    std::string m_className;
    std::string m_name;
    Point3D     m_center;
    float       m_depth      = 0.0f;
    float       m_planeDepth = 0.0f;
    float       m_angle      = 0.0f;
    vision_BoundingBox m_modelBbox;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    int m_vertexCount = 0;
};

// ============================================================================
// 具体物体类：金属方块（长方体毛坯）
// ============================================================================
class MetalBlock : public DetectedObject {
public:
    // 实现纯虚函数
    void init(QOpenGLFunctions_3_3_Core* gl) override;
    void render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) override;
    bool isPointInside(const glm::vec3& worldPoint) const override;
    std::vector<glm::vec3> getCrossSection(float worldTargetZ) const override;

    // 重写尺寸设置
    void setDimensions(float w, float h) override { m_width = w; m_height = h; }

    // 属性访问
    float getWidth() const override  { return m_width; }
    float getHeight() const override { return m_height; }

private:
    float m_width  = 0.0f;
    float m_height = 0.0f;
};

// ============================================================================
// 具体物体类：圆柱体（圆形毛坯）
// ============================================================================
class AnnulusOuter : public DetectedObject {
public:
    // 实现纯虚函数
    void init(QOpenGLFunctions_3_3_Core* gl) override;
    void render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) override;
    bool isPointInside(const glm::vec3& worldPoint) const override;
    std::vector<glm::vec3> getCrossSection(float worldTargetZ) const override;

    // 重写半径设置
    void setRadius(float r) override { m_radius = r; }

    // 属性访问
    float getRadius() const override { return m_radius; }
    int   getSegments() const        { return m_segments; }

private:
    float m_radius   = 0.0f;
    int   m_segments = 36;
};

// ============================================================================
// 具体物体类：STL/STP 模型对象
// ============================================================================
class STLObject : public DetectedObject {
public:
    // 构造函数
    STLObject(const std::vector<VertexTWO>& vertices,
              const std::vector<unsigned int>& indices);

    ~STLObject() override;

    // 实现纯虚函数
    void init(QOpenGLFunctions_3_3_Core* gl) override;
    void render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) override;
    bool isPointInside(const glm::vec3& worldPoint) const override;

    // STP 形状存储（仅用于 STP 模型）
    void setStpShape(const TopoDS_Shape& shape) { m_stpShape = shape; }
    TopoDS_Shape getStpShape() const            { return m_stpShape; }

    // 三角面片存储（用于 STP 切片）
    void setTriangles(const std::vector<std::shared_ptr<Triangler>>& tris) { m_triangles = tris; }
    const std::vector<std::shared_ptr<Triangler>>& getTriangles() const    { return m_triangles; }

    // 顶点/索引访问（统一接口）
    const std::vector<VertexTWO>& getVertices() const { return m_vertices; }
    const std::vector<unsigned int>& getIndices() const { return m_indices; }

    // 包围盒便捷设置
    void setSTLModelBbox(const vision_BoundingBox& bbox) { setModelBbox(bbox); }

    // 公开数据成员（为兼容现有代码保留，建议逐步改为访问函数）
    std::shared_ptr<DetectedObject> blank;

private:
    // 几何数据
    std::vector<VertexTWO> m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<std::shared_ptr<Triangler>> m_triangles;   // STP 三角面片
    TopoDS_Shape m_stpShape;                               // STP 原始形状

    // OpenGL 资源
    QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer};
};

// ============================================================================
// OpenCASCADE 适配器类（将 DetectedObject 转换为 AIS_Shape 以在 OCCT 视图中显示）
// ============================================================================
class DetectedObjectOcctAdapter {
public:
    // 将任意物体转换为 AIS_Shape
    static Handle(AIS_Shape) convertToOcctShape(const std::shared_ptr<DetectedObject>& obj);

    // 创建横截面形状
    static Handle(AIS_Shape) createCrossSectionShape(const std::shared_ptr<DetectedObject>& obj,
                                                     float worldTargetZ);

    // 从 STL 数据创建 OCCT 形状（供内部使用）
    static TopoDS_Shape createShapeFromSTLData(const std::vector<VertexTWO>& vertices,
                                               const std::vector<unsigned int>& indices);

private:
    // 具体转换函数
    static Handle(AIS_Shape) convertMetalBlock(const std::shared_ptr<MetalBlock>& block);
    static Handle(AIS_Shape) convertAnnulusOuter(const std::shared_ptr<AnnulusOuter>& annulus);
    static Handle(AIS_Shape) convertSTLObject(const std::shared_ptr<STLObject>& stlObj);

    // 辅助几何构建
    static Handle(AIS_Shape) createPolygonShape(const std::vector<glm::vec3>& polygon,
                                                const Quantity_Color& color);
    static gp_Pnt vertexToGpPnt(const VertexTWO& vertex);
    static TopoDS_Face createTriangle(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3);
};

#endif // DETECTED_OBJECT_H
