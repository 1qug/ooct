// stl_utils.cpp
#include "stl_utils.h"
#include <fstream>
#include <algorithm>
#include <string>
#include <QDebug>
#include <cstdint>
#include <sstream>
#include <QRegularExpression>
#include "detected_object.h"

bool STLUtils::isASCIISTL(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        qWarning() << "Cannot open file for format detection:" << filepath.c_str();
        return false;
    }

    // 读取文件开头80字节来判断格式
    char header[80] = {0};
    file.read(header, 80);

    // 检查是否为ASCII格式的关键词
    std::string headerStr(header, 80);
    std::transform(headerStr.begin(), headerStr.end(), headerStr.begin(), ::tolower);

    // ASCII STL通常以"solid"开头
    if (headerStr.find("solid") == 0) {
        // 进一步验证：检查后面是否有"facet"或"vertex"等关键词
        if (headerStr.find("facet") != std::string::npos ||
            headerStr.find("vertex") != std::string::npos) {
            qDebug() << "Detected ASCII STL format";
            return true;
        }
    }

    // 二进制STL：第80-84字节应该是三角形数量
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();

    if (fileSize >= 84) {
        file.seekg(80);
        uint32_t numTriangles;
        file.read(reinterpret_cast<char*>(&numTriangles), 4);

        // 验证三角形数量是否合理
        std::streamsize expectedSize = 84 + numTriangles * 50;
        if (fileSize == expectedSize) {
            qDebug() << "Detected binary STL format with" << numTriangles << "triangles";
            return false;
        }
    }

    qWarning() << "Cannot determine STL format, assuming binary";
    return false;
}

bool STLUtils::isValidSTL(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // 最小文件大小检查（二进制STL至少84字节：80+4）
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    if (fileSize < 84) {
        return false;
    }

    // 二进制STL额外检查：文件大小与三角形数量匹配
    if (!isASCIISTL(filepath)) {
        file.seekg(80);
        uint32_t numTriangles = 0;
        file.read(reinterpret_cast<char*>(&numTriangles), sizeof(uint32_t));

        // 二进制STL总大小 = 80(头部) + 4(数量) + 50*N(每个三角形50字节：12(法线)+3*12(顶点)+2(属性))
        std::streamsize expectedSize = 84 + 50 * numTriangles;
        if (fileSize != expectedSize) {
            return false;
        }
    }

    return true;
}

// 判断包围盒8个顶点是否全在毛坯内
bool STLUtils::isBBoxInsideBlank(const vision_BoundingBox& modelBbox, const std::shared_ptr<DetectedObject>& blank) {
    if (!blank) return false;

    // 获取模型包围盒的8个顶点
    auto bboxVertices = modelBbox.getVertices();

    // 检查每个顶点是否在毛坯内（调用毛坯的isPointInside方法）
    for (const auto& vert : bboxVertices) {
        if (!blank->isPointInside(vert)) {
            return false;
        }
    }
    return true;
}

// 计算缩放比例使模型包围盒与毛坯相切
float STLUtils::calculateFitScale(const vision_BoundingBox& modelBbox, const std::shared_ptr<DetectedObject>& blank) {
    if (!blank) return 1.0f;

    // 处理金属块（正方体）毛坯
    if (blank->getClassName()== "metal block") {
        auto metalBlock = std::dynamic_pointer_cast<MetalBlock>(blank);
        if (!metalBlock) return 1.0f;

        // 获取毛坯参数（宽、高、深度）
        float blankWidth = metalBlock->getWidth();
        float blankHeight = metalBlock->getHeight();
        float blankDepth = metalBlock->getDepth();

        // 计算模型包围盒在X、Y方向的半尺寸（从中心到边缘的最大距离）
        glm::vec3 bboxCenter = modelBbox.center;
        float halfWidthModel = std::max(bboxCenter.x - modelBbox.min.x, modelBbox.max.x - bboxCenter.x);
        float halfHeightModel = std::max(bboxCenter.y - modelBbox.min.y, modelBbox.max.y - bboxCenter.y);

        // 计算Z方向尺寸
        float zSize = modelBbox.size.z;

        // 计算各方向缩放比例（毛坯半尺寸 / 模型半尺寸）
        float scaleX = (blankWidth / 2.0f) / halfWidthModel;
        float scaleY = (blankHeight / 2.0f) / halfHeightModel;
        float scaleZ = blankDepth / zSize;

        // 取最小值确保模型完全适配毛坯
        return std::min({scaleX, scaleY, scaleZ});
    }

    // 处理圆环（圆柱体）毛坯
    if (blank->getClassName() == "annulus" || blank->getClassName() == "cylinder") {
        auto annulus = std::dynamic_pointer_cast<AnnulusOuter>(blank);
        if (!annulus) return 1.0f;

        // 获取毛坯参数（半径和深度）
        float blankRadius = annulus->getRadius();
        float blankDepth = annulus->getDepth();

        // 计算模型包围盒在XY平面到中心的最大距离（适配圆半径）
        glm::vec3 bboxCenter = modelBbox.center;
        float maxXYDistance = 0.0f;
        auto bboxVertices = modelBbox.getVertices();
        for (const auto& vert : bboxVertices) {
            float dx = vert.x - bboxCenter.x;
            float dy = vert.y - bboxCenter.y;
            maxXYDistance = std::max(maxXYDistance, std::sqrt(dx*dx + dy*dy));
        }

        // 计算Z方向尺寸
        float zSize = modelBbox.size.z;

        // 计算XY和Z方向的缩放比例（取最小值）
        float scaleXY = blankRadius / maxXYDistance;
        float scaleZ = blankDepth / zSize;
        return std::min(scaleXY, scaleZ);
    }

    // 未知类型返回默认缩放
    return 1.0f;
}

// 解析函数
std::vector<std::map<std::string, std::string>> BlankdataUtils::parseDetections(const std::vector<std::string>& detections) {
    std::vector<std::map<std::string, std::string>> result;
    for (const auto& det : detections) {
        std::map<std::string, std::string> data;
        QString qdet = QString::fromStdString(det);

        // 关键修复：移除所有干扰符号（单引号、双引号、大括号）
        qdet = qdet.remove("'").remove("\"").remove("{").remove("}").trimmed();

        // 按逗号+空格分割键值对（处理如 "name: metal block, center_x: -1" 的格式）
        QStringList parts = qdet.split(QRegularExpression(",\\s*"));

        for (const QString& part : parts) {
            // 按第一个冒号分割键和值（避免值中含冒号的情况）
            int colonPos = part.indexOf(':');
            if (colonPos == -1) continue; // 跳过格式错误的部分

            QString key = part.left(colonPos).trimmed();
            QString val = part.mid(colonPos + 1).trimmed();

            // qDebug() << "解析到键:" << key << "，值:" << val;

            data[key.toStdString()] = val.toStdString();
        }
        result.push_back(data);
    }
    return result;
}
// ============= ObjectFactory::createObject 方法实现 =============

std::unique_ptr<DetectedObject> ObjectFactory::createObject(const std::map<std::string, std::string>& data) {
    if (data.find("cls_name") == data.end()) return nullptr;
    std::string cls_name = data.at("cls_name");

    if (cls_name == "metal block") {
        std::unique_ptr<MetalBlock> block = std::make_unique<MetalBlock>();
        block->setClassName(cls_name);
        block->setName("方体");
        // 字符串转float
        block->setCenter(std::stof(data.at("center_x")), std::stof(data.at("center_y")),std::stof(data.at("z")));
        // block->z=std::stof(data.at("z"));
        block->setDepth(std::stof(data.at("depth")));
        block->setPlaneDepth(std::stof(data.at("plane_depth")));
        block->setAngle(std::stof(data.at("angle")));
        block->setDimensions(std::stof(data.at("width")),std::stof(data.at("height")));
        return block;
    }
    else if (cls_name == "cylinder"  || cls_name == "annulus"){   //cls_name == "annulus_outer"
        std::unique_ptr<AnnulusOuter> annulus = std::make_unique<AnnulusOuter>();
        annulus->setClassName(cls_name);
        annulus->setName("圆柱体");
        annulus->setCenter(std::stof(data.at("center_x")), std::stof(data.at("center_y")),std::stof(data.at("z")));
        // annulus->z=std::stof(data.at("z"));
        annulus->setDepth(std::stof(data.at("depth")));
        annulus->setPlaneDepth(std::stof(data.at("plane_depth")));
        annulus->setRadius(std::stof(data.at("radius")));
        return annulus;
    }

    return nullptr; // 未知物体类型
}


// ============= STLBoundingBoxCalculator 方法实现 =============

// 从顶点计算包围盒
vision_BoundingBox STLBoundingBoxCalculator::calculateFromVertices(const std::vector<VertexTWO>& vertices) {
    vision_BoundingBox bbox;

    if (vertices.empty()) {
        return bbox;
    }

    // 初始化最小最大值
    bbox.min.x = bbox.min.y = bbox.min.z = std::numeric_limits<float>::max();
    bbox.max.x = bbox.max.y = bbox.max.z = std::numeric_limits<float>::lowest();

    // 遍历所有顶点，找到最小和最大值
    for (const auto& vertex : vertices) {
        // 更新X坐标
        if (vertex.position[0] < bbox.min.x) bbox.min.x = vertex.position[0];
        if (vertex.position[0] > bbox.max.x) bbox.max.x = vertex.position[0];

        // 更新Y坐标
        if (vertex.position[1] < bbox.min.y) bbox.min.y = vertex.position[1];
        if (vertex.position[1] > bbox.max.y) bbox.max.y = vertex.position[1];

        // 更新Z坐标
        if (vertex.position[2] < bbox.min.z) bbox.min.z = vertex.position[2];
        if (vertex.position[2] > bbox.max.z) bbox.max.z = vertex.position[2];
    }

    // 计算中心点
    bbox.center.x = (bbox.min.x + bbox.max.x) / 2.0f;
    bbox.center.y = (bbox.min.y + bbox.max.y) / 2.0f;
    bbox.center.z = (bbox.min.z + bbox.max.z) / 2.0f;

    // 计算尺寸
    bbox.size.x = bbox.max.x - bbox.min.x;
    bbox.size.y = bbox.max.y - bbox.min.y;
    bbox.size.z = bbox.max.z - bbox.min.z;

    return bbox;
}

// 从文件计算包围盒
vision_BoundingBox STLBoundingBoxCalculator::calculateFromFile(const std::string& filepath, float scaleFactor) {
    vision_BoundingBox bbox;

    // 先验证文件有效性
    if (!STLUtils::isValidSTL(filepath)) {
        return bbox; // 无效文件返回空包围盒
    }

    // 根据文件格式选择解析方式
    if (STLUtils::isASCIISTL(filepath)) {
        calculateFromASCIIFile(filepath, bbox, scaleFactor);
    } else {
        calculateFromBinaryFile(filepath, bbox, scaleFactor);
    }

    // 计算包围盒的中心和尺寸
    bbox.calculateProperties();
    return bbox;
}

// 从二进制STL文件计算包围盒
void STLBoundingBoxCalculator::calculateFromBinaryFile(const std::string& filepath, vision_BoundingBox& bbox, float scaleFactor) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    // 跳过80字节头部
    file.seekg(80);

    // 读取三角形数量
    uint32_t numTriangles = 0;
    file.read(reinterpret_cast<char*>(&numTriangles), sizeof(uint32_t));

    // 遍历每个三角形，提取顶点并扩展包围盒
    for (uint32_t i = 0; i < numTriangles; ++i) {
        // 跳过12字节法线（不需要用于包围盒计算）
        file.seekg(12, std::ios::cur);

        // 读取3个顶点的坐标
        for (int j = 0; j < 3; ++j) {
            float x, y, z;
            file.read(reinterpret_cast<char*>(&x), sizeof(float));
            file.read(reinterpret_cast<char*>(&y), sizeof(float));
            file.read(reinterpret_cast<char*>(&z), sizeof(float));

            // 应用缩放因子，扩展包围盒
            glm::vec3 point(x * scaleFactor, y * scaleFactor, z * scaleFactor);
            bbox.expand(point);
        }

        // 跳过2字节属性信息
        file.seekg(2, std::ios::cur);
    }

    file.close();
}

// 从ASCII STL文件计算包围盒
void STLBoundingBoxCalculator::calculateFromASCIIFile(const std::string& filepath, vision_BoundingBox& bbox, float scaleFactor) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        // 只处理"vertex"行（提取顶点坐标）
        if (keyword == "vertex") {
            float x, y, z;
            iss >> x >> y >> z;

            // 应用缩放因子，扩展包围盒
            glm::vec3 point(x * scaleFactor, y * scaleFactor, z * scaleFactor);
            bbox.expand(point);
        }
    }

    file.close();
}
