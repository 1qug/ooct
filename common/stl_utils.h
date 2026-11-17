// stl_utils.h

#ifndef STL_UTILS_H
#define STL_UTILS_H

#include <string>
#include <vector>
#include <glm/glm.hpp>


#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_Pnt.hxx>
#include <gp_GTrsf.hxx>
#include <TopoDS_Shape.hxx>
#include <QMessageBox>
#include <QString>
#include <QDebug>
// 顶点结构
struct VertexTWO {
    float position[3];
    float normal[3];
    float color[4];
};


struct vision_BoundingBox {
    glm::vec3 min;    // 最小顶点 (x_min, y_min, z_min)
    glm::vec3 max;    // 最大顶点 (x_max, y_max, z_max)
    glm::vec3 center; // 中心点
    glm::vec3 size;   // 尺寸 (width, depth, height)

    vision_BoundingBox() {
        reset();
    }

    void reset() {
        min = glm::vec3(std::numeric_limits<float>::max());
        max = glm::vec3(std::numeric_limits<float>::lowest());
        center = glm::vec3(0.0f);
        size = glm::vec3(0.0f);
    }

    // 更新包围盒以包含一个点
    void expand(const glm::vec3& point) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);

        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    // 计算中心点和尺寸
    void calculateProperties() {

        // 计算中心点
        center.x = (min.x + max.x) / 2.0f;
        center.y = (min.y + max.y) / 2.0f;
        center.z = (min.z + max.z) / 2.0f;

        // 计算尺寸
        size.x = max.x - min.x;
        size.y = max.y - min.y;
        size.z = max.z - min.z;

    }

    // 获取包围盒的8个顶点
    std::vector<glm::vec3> getVertices() const {
        return {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, max.z),
            glm::vec3(min.x, max.y, max.z)
        };
    }
};



class DetectedObject;

class STLUtils {
public:
    // 判断STL文件是否为ASCII格式
    static bool isASCIISTL(const std::string& filepath);

    // 验证STL文件有效性（格式+大小检查）
    static bool isValidSTL(const std::string& filepath);

    // 判断包围盒所有顶点是否在毛坯内
    static bool isBBoxInsideBlank(const vision_BoundingBox& modelBbox, const std::shared_ptr<DetectedObject>& blank);

    // 计算模型包围盒适配毛坯的缩放比例
    static float calculateFitScale(const vision_BoundingBox& modelBbox, const std::shared_ptr<DetectedObject>& blank);

};

// STL包围盒计算工具类
class STLBoundingBoxCalculator {
public:

    // 从顶点数据计算包围盒
    static vision_BoundingBox calculateFromVertices(const std::vector<VertexTWO>& vertices);

    // 从文件直接计算包围盒（不加载完整模型）
    static vision_BoundingBox calculateFromFile(const std::string& filepath, float scaleFactor = 1.0f);

    static std::vector<VertexTWO> createBoundingBoxMesh(const vision_BoundingBox& bbox, const glm::vec4& color);

private:
    static void calculateFromBinaryFile(const std::string& filepath, vision_BoundingBox& bbox, float scaleFactor);
    static void calculateFromASCIIFile(const std::string& filepath, vision_BoundingBox& bbox, float scaleFactor);

};


class BlankdataUtils{
    public:
    // 解析字典 集合的数据
        static std::vector<std::map<std::string, std::string>> parseDetections(const std::vector<std::string>& detections);
};
class ObjectFactory {
public:
    static std::unique_ptr<DetectedObject> createObject(const std::map<std::string, std::string>& data);
};



// 工具类：STP模型加载与几何变换
class ModelTransformer {
public:
    ModelTransformer(){};
    //加载
    TopoDS_Shape loadModelFile(const std::string& stpPath){
        // 1. 加载STP文件
        TopoDS_Shape shape;
        if (!loadSTPFile(stpPath.c_str(), shape)) {
            qDebug()<<"STP加载失败";
        }
        return shape;
    }


    //平移
    bool loadAndTransformSTP(const TopoDS_Shape& originalShape, double scaleFactor, TopoDS_Shape& outTransformedShape) {
        outTransformedShape.Nullify();

        try {

            // 2. 计算模型中心点（包围盒中心）
            gp_Pnt modelCenter;
            if (!calculateModelCenter(originalShape, modelCenter)) {
                QMessageBox::critical(nullptr, "Error", "Failed to calculate model center!");
                return false;
            }
            qDebug() << "Original model center: (" << modelCenter.X() << ","
                     << modelCenter.Y() << "," << modelCenter.Z() << ")";

            // 3. 构建变换矩阵：平移（中心→原点）
            gp_Trsf transform;

            // 平移（向量 = 原点 - 模型中心）
            gp_Vec translateVec = gp_Vec(modelCenter, gp_Pnt(0, 0, 0));
            transform.SetTranslation(translateVec);


            // 4. 应用变换到模型
            BRepBuilderAPI_Transform transformTool(originalShape, transform, Standard_True);
            if (!transformTool.IsDone()) {
                QMessageBox::critical(nullptr, "Error", "Model transformation failed!");
                return false;
            }
            outTransformedShape = transformTool.Shape();

            // 验证变换后中心点
            gp_Pnt transformedCenter;
            calculateModelCenter(outTransformedShape, transformedCenter);
            qDebug() << "Transformed model center: (" << transformedCenter.X() << ","
                     << transformedCenter.Y() << "," << transformedCenter.Z() << ")";

            return true;

        } catch (Standard_Failure& e) {
            QMessageBox::critical(nullptr, "Error",
                                  QString("Process failed: %1").arg(e.GetMessageString()));
            return false;
        }
    }





private:
    // 加载STP文件
    bool loadSTPFile(const QString& stpPath, TopoDS_Shape& outShape) {
        STEPControl_Reader reader;
        IFSelect_ReturnStatus status = reader.ReadFile(stpPath.toUtf8().constData());
        if (status != IFSelect_RetDone) {
            QMessageBox::critical(nullptr, "Error", "Failed to read STP file!");
            return false;
        }

        reader.TransferRoots();
        outShape = reader.OneShape();
        if (outShape.IsNull()) {
            QMessageBox::critical(nullptr, "Error", "STP file has no valid geometry!");
            return false;
        }

        qDebug() << "STP file loaded successfully!";
        return true;
    }

    // 计算模型中心点（修正BRepBndLib::Add参数）
    bool calculateModelCenter(const TopoDS_Shape& shape, gp_Pnt& outCenter) {
        if (shape.IsNull()) return false;

        Bnd_Box bbox;
        // 修正：使用正确的BRepBndLib::Add重载版本
        BRepBndLib::Add(shape, bbox, Standard_True); // 使用三角化提高精度

        // 计算包围盒中心
        if (bbox.IsVoid()) {
            return false;
        }

        Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
        bbox.Get(xMin, yMin, zMin, xMax, yMax, zMax); // 获取6个边界值

        outCenter = gp_Pnt(
            (xMin + xMax) / 2.0,
            (yMin + yMax) / 2.0,
            // (zMin + zMax) / 2.0
            zMax
            );

        return true;
    }
};

#endif // STL_UTILS_H
