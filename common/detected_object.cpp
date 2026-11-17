#include "detected_object.h"

#include <cmath>
#include <QDebug>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Quantity_Color.hxx>

// ============================================================================
// DetectedObject 基类实现
// ============================================================================
DetectedObject::~DetectedObject() {
    if (m_vao.isCreated()) m_vao.destroy();
    if (m_vbo.isCreated()) m_vbo.destroy();
}

std::vector<glm::vec3> DetectedObject::getCrossSection(float worldTargetZ) const {
    return {};
}

void DetectedObject::setDimensions(float w, float h) {
    (void)w; (void)h;
}

void DetectedObject::setRadius(float r) {
    (void)r;
}

float DetectedObject::getWidth() const  { return 0.0f; }
float DetectedObject::getHeight() const { return 0.0f; }
float DetectedObject::getRadius() const { return 0.0f; }

bool DetectedObject::isPointInPolygon(float x, float y,
                                      const std::vector<glm::vec3>& polygon) const {
    if (polygon.empty()) return false;
    bool inside = false;
    size_t n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const glm::vec3& pi = polygon[i];
        const glm::vec3& pj = polygon[j];
        if (((pi.y > y) != (pj.y > y)) &&
            (x < (pj.x - pi.x) * (y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// ============================================================================
// MetalBlock 实现
// ============================================================================
void MetalBlock::init(QOpenGLFunctions_3_3_Core* gl) {
    float halfW = m_width / 2.0f;
    float halfH = m_height / 2.0f;
    float halfD = getDepth() / 2.0f;

    std::vector<VertexTWO> vertices;

    // 8 个角点
    glm::vec3 corners[8] = {
        {-halfW, -halfH, -halfD}, { halfW, -halfH, -halfD},
        { halfW,  halfH, -halfD}, {-halfW,  halfH, -halfD},
        {-halfW, -halfH,  halfD}, { halfW, -halfH,  halfD},
        { halfW,  halfH,  halfD}, {-halfW,  halfH,  halfD}
    };

    // 6 个面的索引（每个面 2 个三角形）
    int faces[6][6] = {
        {0,1,2, 0,2,3},   // 底面
        {4,5,6, 4,6,7},   // 顶面
        {0,1,5, 0,5,4},   // 后面
        {2,3,7, 2,7,6},   // 前面
        {0,3,7, 0,7,4},   // 左面
        {1,2,6, 1,6,5}    // 右面
    };

    glm::vec3 normals[6] = {
        {0,0,-1}, {0,0,1}, {0,-1,0}, {0,1,0}, {-1,0,0}, {1,0,0}
    };

    glm::vec4 colors[6] = {
        {0.5f,0.5f,0.5f,0.8f}, {0.5f,0.5f,0.5f,0.8f}, {0.5f,0.5f,0.5f,0.8f},
        {0.5f,0.5f,0.5f,0.8f}, {0.5f,0.5f,0.5f,0.8f}, {0.5f,0.5f,0.5f,0.8f}
    };

    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i < 6; ++i) {
            VertexTWO v;
            glm::vec3 pos = corners[faces[face][i]];
            v.position[0] = pos.x;
            v.position[1] = pos.y;
            v.position[2] = pos.z;

            v.normal[0] = normals[face].x;
            v.normal[1] = normals[face].y;
            v.normal[2] = normals[face].z;

            v.color[0] = colors[face].r;
            v.color[1] = colors[face].g;
            v.color[2] = colors[face].b;
            v.color[3] = colors[face].a;

            vertices.push_back(v);
        }
    }

    setVertexCount(static_cast<int>(vertices.size()));
    setModelBbox(STLBoundingBoxCalculator::calculateFromVertices(vertices));

    auto& vao = getVAO();
    auto& vbo = getVBO();
    if (!vao.isCreated()) vao.create();
    if (!vbo.isCreated()) vbo.create();

    QOpenGLVertexArrayObject::Binder binder(&vao);
    vbo.bind();
    vbo.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(VertexTWO)));

    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, position));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, normal));
    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, color));
}

void MetalBlock::render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) {
    if (!getVAO().isCreated()) {
        qDebug() << "MetalBlock VAO 未创建，正在初始化...";
        init(gl);
    }
    if (!shader->isLinked()) {
        qDebug() << "[错误] 着色器程序未链接";
        return;
    }

    shader->bind();

    QMatrix4x4 model;
    model.translate(getCenter().x, getCenter().y, getZ());
    model.rotate(getAngle(), 0, 0, 1);
    shader->setUniformValue("model", model);

    shader->setUniformValue("lightDir", 1.0f, 1.0f, 1.0f);
    shader->setUniformValue("lightAmbient", 0.2f, 0.2f, 0.2f);
    shader->setUniformValue("lightDiffuse", 0.8f, 0.8f, 0.8f);
    shader->setUniformValue("useLighting", true);

    gl->glDepthMask(GL_FALSE);
    QOpenGLVertexArrayObject::Binder binder(&getVAO());
    gl->glDrawArrays(GL_TRIANGLES, 0, getVertexCount());
    gl->glDepthMask(GL_TRUE);
}

bool MetalBlock::isPointInside(const glm::vec3& worldPoint) const {
    QMatrix4x4 invModel;
    invModel.rotate(-getAngle(), 0, 0, 1);
    invModel.translate(-getCenter().x, -getCenter().y, -getZ());
    QVector3D local = invModel.map(QVector3D(worldPoint.x, worldPoint.y, worldPoint.z));

    float halfW = m_width / 2.0f;
    float halfH = m_height / 2.0f;
    float halfD = getDepth() / 2.0f;
    return (local.x() >= -halfW - 1e-3f && local.x() <= halfW + 1e-3f) &&
           (local.y() >= -halfH - 1e-3f && local.y() <= halfH + 1e-3f) &&
           (local.z() >= -halfD - 1e-3f && local.z() <= halfD + 1e-3f);
}

std::vector<glm::vec3> MetalBlock::getCrossSection(float worldTargetZ) const {
    std::vector<glm::vec3> result;
    QMatrix4x4 invModel;
    invModel.rotate(-getAngle(), 0, 0, 1);
    invModel.translate(-getCenter().x, -getCenter().y, -getZ());
    float localZ = invModel.map(QVector3D(0, 0, worldTargetZ)).z();

    float halfD = getDepth() / 2.0f;
    if (localZ < -halfD - 1e-3f || localZ > halfD + 1e-3f) return result;

    float halfW = m_width / 2.0f;
    float halfH = m_height / 2.0f;
    std::vector<glm::vec3> localVerts = {
        {-halfW, -halfH, localZ},
        { halfW, -halfH, localZ},
        { halfW,  halfH, localZ},
        {-halfW,  halfH, localZ}
    };

    QMatrix4x4 model;
    model.translate(getCenter().x, getCenter().y, getZ());
    model.rotate(getAngle(), 0, 0, 1);
    for (const auto& lv : localVerts) {
        QVector3D w = model.map(QVector3D(lv.x, lv.y, lv.z));
        result.emplace_back(w.x(), w.y(), w.z());
    }
    return result;
}

// ============================================================================
// AnnulusOuter 实现
// ============================================================================
void AnnulusOuter::init(QOpenGLFunctions_3_3_Core* gl) {
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    float topZ = getDepth() / 2.0f;
    float bottomZ = -topZ;

    std::vector<VertexTWO> vertices;
    glm::vec4 sideColor(0.5f, 0.5f, 0.5f, 1.0f);

    // 侧面
    for (int i = 0; i < m_segments; ++i) {
        float a1 = (i / static_cast<float>(m_segments)) * twoPi;
        float a2 = ((i + 1) / static_cast<float>(m_segments)) * twoPi;
        float x1 = m_radius * cosf(a1), y1 = m_radius * sinf(a1);
        float x2 = m_radius * cosf(a2), y2 = m_radius * sinf(a2);
        glm::vec3 n1(cosf(a1), sinf(a1), 0.0f);
        glm::vec3 n2(cosf(a2), sinf(a2), 0.0f);

        // 三角形 1
        vertices.push_back({{x1, y1, topZ}, {n1.x, n1.y, n1.z}, {sideColor.r, sideColor.g, sideColor.b, sideColor.a}});
        vertices.push_back({{x2, y2, topZ}, {n2.x, n2.y, n2.z}, {sideColor.r, sideColor.g, sideColor.b, sideColor.a}});
        vertices.push_back({{x1, y1, bottomZ}, {n1.x, n1.y, n1.z}, {sideColor.r, sideColor.g, sideColor.b, sideColor.a}});
        // 三角形 2
        vertices.push_back({{x1, y1, bottomZ}, {n1.x, n1.y, n1.z}, {sideColor.r, sideColor.g, sideColor.b, sideColor.a}});
        vertices.push_back({{x2, y2, topZ}, {n2.x, n2.y, n2.z}, {sideColor.r, sideColor.g, sideColor.b, sideColor.a}});
        vertices.push_back({{x2, y2, bottomZ}, {n2.x, n2.y, n2.z}, {sideColor.r, sideColor.g, sideColor.b, sideColor.a}});
    }

    // 顶面（中心+边缘）
    glm::vec4 topColor(0.6f, 0.6f, 0.6f, 1.0f);
    glm::vec3 topNormal(0.0f, 0.0f, 1.0f);
    vertices.push_back({{0.0f, 0.0f, topZ}, {0,0,1}, {topColor.r, topColor.g, topColor.b, topColor.a}});
    for (int i = 0; i <= m_segments; ++i) {
        float a = (i / static_cast<float>(m_segments)) * twoPi;
        float x = m_radius * cosf(a), y = m_radius * sinf(a);
        vertices.push_back({{x, y, topZ}, {0,0,1}, {topColor.r, topColor.g, topColor.b, topColor.a}});
    }

    // 底面（中心+边缘）
    glm::vec4 bottomColor(0.4f, 0.4f, 0.4f, 1.0f);
    glm::vec3 bottomNormal(0.0f, 0.0f, -1.0f);
    vertices.push_back({{0.0f, 0.0f, bottomZ}, {0,0,-1}, {bottomColor.r, bottomColor.g, bottomColor.b, bottomColor.a}});
    for (int i = 0; i <= m_segments; ++i) {
        float a = (i / static_cast<float>(m_segments)) * twoPi;
        float x = m_radius * cosf(a), y = m_radius * sinf(a);
        vertices.push_back({{x, y, bottomZ}, {0,0,-1}, {bottomColor.r, bottomColor.g, bottomColor.b, bottomColor.a}});
    }

    setVertexCount(static_cast<int>(vertices.size()));
    setModelBbox(STLBoundingBoxCalculator::calculateFromVertices(vertices));

    auto& vao = getVAO();
    auto& vbo = getVBO();
    if (!vao.isCreated()) vao.create();
    if (!vbo.isCreated()) vbo.create();

    QOpenGLVertexArrayObject::Binder binder(&vao);
    vbo.bind();
    vbo.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(VertexTWO)));

    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, position));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, normal));
    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, color));
}

void AnnulusOuter::render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) {
    if (!getVAO().isCreated()) {
        qDebug() << "AnnulusOuter VAO 未创建，正在初始化...";
        init(gl);
    }
    if (!shader->isLinked()) {
        qDebug() << "[错误] 着色器程序未链接";
        return;
    }

    shader->bind();

    QMatrix4x4 model;
    model.translate(getCenter().x, getCenter().y, getZ());
    model.rotate(getAngle(), 0, 0, 1);
    shader->setUniformValue("model", model);

    shader->setUniformValue("lightDir", 1.0f, 1.0f, 1.0f);
    shader->setUniformValue("lightAmbient", 0.2f, 0.2f, 0.2f);
    shader->setUniformValue("lightDiffuse", 0.8f, 0.8f, 0.8f);
    shader->setUniformValue("useLighting", true);

    gl->glDepthMask(GL_FALSE);
    QOpenGLVertexArrayObject::Binder binder(&getVAO());

    int sideVertexCount = m_segments * 6;
    gl->glDrawArrays(GL_TRIANGLES, 0, sideVertexCount);
    gl->glDrawArrays(GL_TRIANGLE_FAN, sideVertexCount, m_segments + 2);
    gl->glDrawArrays(GL_TRIANGLE_FAN, sideVertexCount + m_segments + 2, m_segments + 2);

    gl->glDepthMask(GL_TRUE);
}

bool AnnulusOuter::isPointInside(const glm::vec3& worldPoint) const {
    QMatrix4x4 invModel;
    invModel.rotate(-getAngle(), 0, 0, 1);
    invModel.translate(-getCenter().x, -getCenter().y, -getZ());
    QVector3D local = invModel.map(QVector3D(worldPoint.x, worldPoint.y, worldPoint.z));

    float halfD = getDepth() / 2.0f;
    if (local.z() < -halfD - 1e-3f || local.z() > halfD + 1e-3f) return false;
    float dist = std::sqrt(local.x() * local.x() + local.y() * local.y());
    return dist <= m_radius + 1e-3f;
}

std::vector<glm::vec3> AnnulusOuter::getCrossSection(float worldTargetZ) const {
    std::vector<glm::vec3> result;
    QMatrix4x4 invModel;
    invModel.rotate(-getAngle(), 0, 0, 1);
    invModel.translate(-getCenter().x, -getCenter().y, -getZ());
    float localZ = invModel.map(QVector3D(0, 0, worldTargetZ)).z();

    float halfD = getDepth() / 2.0f;
    if (localZ < -halfD - 1e-3f || localZ > halfD + 1e-3f) return result;

    const int samples = 36;
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    std::vector<glm::vec3> localVerts;
    for (int i = 0; i < samples; ++i) {
        float theta = (i / static_cast<float>(samples)) * twoPi;
        localVerts.emplace_back(m_radius * cosf(theta), m_radius * sinf(theta), localZ);
    }

    QMatrix4x4 model;
    model.translate(getCenter().x, getCenter().y, getZ());
    model.rotate(getAngle(), 0, 0, 1);
    for (const auto& lv : localVerts) {
        QVector3D w = model.map(QVector3D(lv.x, lv.y, lv.z));
        result.emplace_back(w.x(), w.y(), w.z());
    }
    return result;
}

// ============================================================================
// STLObject 实现
// ============================================================================
STLObject::STLObject(const std::vector<VertexTWO>& vertices,
                     const std::vector<unsigned int>& indices)
    : m_vertices(vertices), m_indices(indices) {
    setName("STL_Model");
}

STLObject::~STLObject() {
    if (getVAO().isCreated()) getVAO().destroy();
    if (getVBO().isCreated()) getVBO().destroy();
    if (m_ebo.isCreated()) m_ebo.destroy();
}

void STLObject::init(QOpenGLFunctions_3_3_Core* gl) {
    if (!gl) {
        qWarning("STLObject::init: gl context is null!");
        return;
    }

    auto& vao = getVAO();
    auto& vbo = getVBO();
    if (!vao.isCreated()) vao.create();
    if (!vbo.isCreated()) vbo.create();

    QOpenGLVertexArrayObject::Binder binder(&vao);
    vbo.bind();
    vbo.allocate(m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(VertexTWO)));

    if (!m_ebo.isCreated()) m_ebo.create();
    m_ebo.bind();
    m_ebo.allocate(m_indices.data(), static_cast<int>(m_indices.size() * sizeof(unsigned int)));

    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, position));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, normal));
    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexTWO),
                              (void*)offsetof(VertexTWO, color));

    setVertexCount(static_cast<int>(m_indices.size()));
}

void STLObject::render(QOpenGLFunctions_3_3_Core* gl, QOpenGLShaderProgram* shader) {
    QOpenGLVertexArrayObject::Binder binder(&getVAO());
    shader->bind();

    QMatrix4x4 model;
    model.translate(getCenter().x, getCenter().y, getCenter().z);
    model.rotate(getAngle(), 0, 0, 1);
    shader->setUniformValue("model", model);

    shader->setUniformValue("lightDir", 1.0f, 1.0f, 1.0f);
    shader->setUniformValue("lightAmbient", 0.2f, 0.2f, 0.2f);
    shader->setUniformValue("lightDiffuse", 0.8f, 0.8f, 0.8f);
    shader->setUniformValue("useLighting", true);

    gl->glDrawElements(GL_TRIANGLES, getVertexCount(), GL_UNSIGNED_INT, nullptr);
}

bool STLObject::isPointInside(const glm::vec3& worldPoint) const {
    (void)worldPoint;
    return false; // STL 对象暂不支持
}

// ============================================================================
// DetectedObjectOcctAdapter 实现
// ============================================================================
Handle(AIS_Shape) DetectedObjectOcctAdapter::convertToOcctShape(const std::shared_ptr<DetectedObject>& obj) {
    if (!obj) return nullptr;

    if (auto block = std::dynamic_pointer_cast<MetalBlock>(obj))
        return convertMetalBlock(block);
    if (auto annulus = std::dynamic_pointer_cast<AnnulusOuter>(obj))
        return convertAnnulusOuter(annulus);
    if (auto stlObj = std::dynamic_pointer_cast<STLObject>(obj))
        return convertSTLObject(stlObj);

    qWarning() << "未知的 DetectedObject 类型";
    return nullptr;
}

Handle(AIS_Shape) DetectedObjectOcctAdapter::convertMetalBlock(const std::shared_ptr<MetalBlock>& block) {
    try {
        double halfW = block->getWidth() / 2.0;
        double halfH = block->getHeight() / 2.0;
        double halfD = block->getDepth() / 2.0;

        TopoDS_Shape base = BRepPrimAPI_MakeBox(
                                gp_Pnt(-halfW, -halfH, -halfD),
                                gp_Pnt( halfW,  halfH,  halfD)).Shape();

        gp_Trsf trsf;
        if (std::abs(block->getAngle()) > 1e-6) {
            gp_Ax1 axis(gp_Pnt(0,0,0), gp_Dir(0,0,1));
            gp_Trsf rot;
            rot.SetRotation(axis, block->getAngle() * M_PI / 180.0);
            trsf = rot;
        }
        gp_Trsf trans;
        trans.SetTranslation(gp_Vec(block->getCenter().x, block->getCenter().y, block->getZ()));
        trsf = trans * trsf;

        BRepBuilderAPI_Transform transformer(base, trsf);
        TopoDS_Shape transformed = transformer.Shape();

        Bnd_Box bbox;
        BRepBndLib::Add(transformed, bbox);
        double x1,y1,z1,x2,y2,z2;
        bbox.Get(x1,y1,z1,x2,y2,z2);
        vision_BoundingBox vb;
        vb.min = glm::vec3(x1,y1,z1);
        vb.max = glm::vec3(x2,y2,z2);
        vb.calculateProperties();
        block->setModelBbox(vb);

        Handle(AIS_Shape) ais = new AIS_Shape(transformed);
        ais->SetColor(Quantity_NOC_GRAY30);
        ais->SetTransparency(0.3);
        ais->SetDisplayMode(1);
        return ais;
    } catch (Standard_Failure& e) {
        qCritical() << "转换 MetalBlock 失败:" << e.GetMessageString();
        return nullptr;
    }
}

Handle(AIS_Shape) DetectedObjectOcctAdapter::convertAnnulusOuter(const std::shared_ptr<AnnulusOuter>& annulus) {
    try {
        double r = annulus->getRadius();
        double h = annulus->getDepth();
        gp_Ax2 axis(gp_Pnt(0,0,-h/2), gp_Dir(0,0,1));
        TopoDS_Shape base = BRepPrimAPI_MakeCylinder(axis, r, h).Shape();

        gp_Trsf trsf;
        if (std::abs(annulus->getAngle()) > 1e-6) {
            gp_Ax1 rotAxis(gp_Pnt(0,0,0), gp_Dir(0,0,1));
            gp_Trsf rot;
            rot.SetRotation(rotAxis, annulus->getAngle() * M_PI / 180.0);
            trsf = rot;
        }
        gp_Trsf trans;
        trans.SetTranslation(gp_Vec(annulus->getCenter().x, annulus->getCenter().y, annulus->getZ()));
        trsf = trans * trsf;

        BRepBuilderAPI_Transform transformer(base, trsf);
        TopoDS_Shape transformed = transformer.Shape();

        Bnd_Box bbox;
        BRepBndLib::Add(transformed, bbox);
        double x1,y1,z1,x2,y2,z2;
        bbox.Get(x1,y1,z1,x2,y2,z2);
        vision_BoundingBox vb;
        vb.min = glm::vec3(x1,y1,z1);
        vb.max = glm::vec3(x2,y2,z2);
        vb.calculateProperties();
        annulus->setModelBbox(vb);

        Handle(AIS_Shape) ais = new AIS_Shape(transformed);
        ais->SetColor(Quantity_NOC_GRAY30);
        ais->SetTransparency(0.3);
        ais->SetDisplayMode(1);
        return ais;
    } catch (Standard_Failure& e) {
        qCritical() << "转换 AnnulusOuter 失败:" << e.GetMessageString();
        return nullptr;
    }
}

Handle(AIS_Shape) DetectedObjectOcctAdapter::convertSTLObject(const std::shared_ptr<STLObject>& stlObj) {
    try {
        TopoDS_Shape shape;
        if (stlObj->getClassName() == "stp_model") {
            shape = stlObj->getStpShape();
            if (shape.IsNull()) {
                shape = createShapeFromSTLData(stlObj->getVertices(), stlObj->getIndices());
            }
        } else {
            shape = createShapeFromSTLData(stlObj->getVertices(), stlObj->getIndices());
        }

        if (shape.IsNull()) return nullptr;

        gp_Trsf trsf;
        if (std::abs(stlObj->getAngle()) > 1e-6) {
            gp_Ax1 axis(gp_Pnt(0,0,0), gp_Dir(0,0,1));
            gp_Trsf rot;
            rot.SetRotation(axis, stlObj->getAngle() * M_PI / 180.0);
            trsf = rot;
        }
        gp_Trsf trans;
        trans.SetTranslation(gp_Vec(stlObj->getCenter().x, stlObj->getCenter().y, stlObj->getCenter().z));
        trsf = trans * trsf;

        Handle(AIS_Shape) ais = new AIS_Shape(shape);
        ais->SetLocalTransformation(trsf);
        ais->SetColor(Quantity_NOC_STEELBLUE);
        ais->SetTransparency(0.3);
        ais->SetDisplayMode(1);

        if (stlObj->getClassName() == "stp_model") {
            BRepMesh_IncrementalMesh(shape, 0.01);
        }
        return ais;
    } catch (Standard_Failure& e) {
        qCritical() << "转换 STLObject 失败:" << e.GetMessageString();
        return nullptr;
    }
}

TopoDS_Shape DetectedObjectOcctAdapter::createShapeFromSTLData(
    const std::vector<VertexTWO>& vertices,
    const std::vector<unsigned int>& indices) {
    try {
        BRep_Builder builder;
        TopoDS_Compound comp;
        builder.MakeCompound(comp);

        for (size_t i = 0; i < indices.size(); i += 3) {
            if (indices[i] >= vertices.size() ||
                indices[i+1] >= vertices.size() ||
                indices[i+2] >= vertices.size()) continue;

            gp_Pnt p1 = vertexToGpPnt(vertices[indices[i]]);
            gp_Pnt p2 = vertexToGpPnt(vertices[indices[i+1]]);
            gp_Pnt p3 = vertexToGpPnt(vertices[indices[i+2]]);
            TopoDS_Face face = createTriangle(p1, p2, p3);
            if (!face.IsNull()) builder.Add(comp, face);
        }
        return comp;
    } catch (Standard_Failure& e) {
        qCritical() << "createShapeFromSTLData 失败:" << e.GetMessageString();
        return TopoDS_Shape();
    }
}

Handle(AIS_Shape) DetectedObjectOcctAdapter::createCrossSectionShape(
    const std::shared_ptr<DetectedObject>& obj, float worldTargetZ) {
    auto pts = obj->getCrossSection(worldTargetZ);
    if (pts.empty()) return nullptr;
    return createPolygonShape(pts, Quantity_NOC_RED);
}

Handle(AIS_Shape) DetectedObjectOcctAdapter::createPolygonShape(
    const std::vector<glm::vec3>& polygon, const Quantity_Color& color) {
    if (polygon.size() < 3) return nullptr;
    try {
        BRep_Builder builder;
        TopoDS_Compound comp;
        builder.MakeCompound(comp);
        for (size_t i = 0; i < polygon.size(); ++i) {
            size_t j = (i + 1) % polygon.size();
            gp_Pnt p1(polygon[i].x, polygon[i].y, polygon[i].z);
            gp_Pnt p2(polygon[j].x, polygon[j].y, polygon[j].z);
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(p1, p2);
            if (!edge.IsNull()) builder.Add(comp, edge);
        }
        Handle(AIS_Shape) ais = new AIS_Shape(comp);
        ais->SetColor(color);
        ais->SetWidth(2.0);
        return ais;
    } catch (Standard_Failure& e) {
        qCritical() << "createPolygonShape 失败:" << e.GetMessageString();
        return nullptr;
    }
}

gp_Pnt DetectedObjectOcctAdapter::vertexToGpPnt(const VertexTWO& vertex) {
    return gp_Pnt(vertex.position[0], vertex.position[1], vertex.position[2]);
}

TopoDS_Face DetectedObjectOcctAdapter::createTriangle(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3) {
    try {
        TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(p1, p2);
        TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(p2, p3);
        TopoDS_Edge e3 = BRepBuilderAPI_MakeEdge(p3, p1);
        BRepBuilderAPI_MakeWire wireMaker;
        wireMaker.Add(e1);
        wireMaker.Add(e2);
        wireMaker.Add(e3);
        if (!wireMaker.IsDone()) return TopoDS_Face();
        return BRepBuilderAPI_MakeFace(wireMaker.Wire());
    } catch (Standard_Failure& e) {
        qWarning() << "createTriangle 失败:" << e.GetMessageString();
        return TopoDS_Face();
    }
}
