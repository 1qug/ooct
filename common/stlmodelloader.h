#ifndef STL_MODEL_LOADER_H
#define STL_MODEL_LOADER_H

#include <string>
#include "detected_object.h"
#include <QOpenGLFunctions_3_3_Core>
#include <memory>
// #include <algorithm>
// #include <limits>
#include "stl_utils.h"


// OCC 基础核心头文件（必须放在最前面）
#include <Standard.hxx>
#include <Standard_Integer.hxx>
#include <gp.hxx>
#include <gp_Pnt.hxx>
#include <TopLoc_Location.hxx>

// OCC 拓扑结构头文件
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs.hxx>

// OCC 三角化头文件
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <Poly_Triangle.hxx>

// OCC STEP 读取头文件
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>


#include <BRepAlgoAPI_Section.hxx>


#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Wire.hxx>
#include <ShapeAnalysis_Wire.hxx>
#include <ShapeExtend_WireData.hxx>
#include <ShapeBuild_Edge.hxx>
#include <TopTools_ListOfShape.hxx>
#include <GCPnts_AbscissaPoint.hxx>

// 截面相关的辅助功能（如设置精度、提取结果）
#include <Precision.hxx>       // 精度设置
#include <BRepBuilderAPI_MakeWire.hxx> // 构建截面轮廓线
#include <TopoDS_Wire.hxx>
#include <BRepTools_WireExplorer.hxx>

// 边界采样
#include <GCPnts_UniformAbscissa.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <Precision.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Edge.hxx>
#include <Geom_Curve.hxx>

#include <Standard_Version.hxx>         // OCC 版本相关（可选，防止版本兼容问题）




#include <TopoDS_AlertWithShape.hxx>
#include <AIS_TexturedShape.hxx>
#include <STEPControl_Reader.hxx>
#include <StlAPI_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <StlAPI.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <RWStl.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Face.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <Poly_Triangulation.hxx>
#include <TShort_HArray1OfShortReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <V3d_AmbientLight.hxx>
#include <V3d_DirectionalLight.hxx>
#include <AIS_Triangulation.hxx>
#include <BRepTopAdaptor_FClass2d.hxx>
#include <GeomAPI_IntCS.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <BRepTools.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <ShapeFix_Wire.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>

#include <BRepTools_WireExplorer.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <ShapeFix_Wire.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepAlgoAPI_Cut.hxx>







// 添加 Geom_Line 的头文件（OCC 7.6 兼容）
#include <Geom_Line.hxx>

// 同时确保已包含其他核心几何头文件（避免连锁错误）
#include <Geom_Curve.hxx>  // Geom_Line 是 Geom_Curve 的子类
#include <gp_Pnt.hxx>      // 点类型
#include <gp_Dir.hxx>      // 方向类型（创建直线需要）
#include <gp_Pln.hxx>
#include <gp_Ax3.hxx>






class VisionWidget;
class STLUtils;

// struct STLLoadResult {
//     std::shared_ptr<DetectedObject> object;
//     // std::vector<SliceSegment> allSegments;
//     // std::map<double, std::vector<std::vector<Point3D>>> all_toolpaths;
//     std::vector<Vertex> vertices;

// };







class STLModelLoader : protected QOpenGLFunctions_3_3_Core {
public:
    STLModelLoader();
    // 析构函数（核心：释放 stlObject 内存）
    ~STLModelLoader();

    // std::vector<Vertex> vertices;                                         //顶点数据
    // std::vector<SliceSegment> allSegments;                                //切片线段
    // std::map<double, std::vector<std::vector<Point3D>>> all_toolpaths;    //所有路径

    //STLObject* m_stlObject=nullptr;
    std::vector<std::shared_ptr<Triangler>> stp_triangles;
    // std::map<double, std::vector<SliceSegment>> stp_layers;
    std::map<double, std::vector<Point3D>> stp_layers;

    // 加载STL文件并转换为DetectedObject
    std::unique_ptr<STLObject> loadSTLModel(const std::string& filepath,
                                            std::shared_ptr<DetectedObject> blanktemp,
                                            float spacing);
    // 批量加载多个STL文件
    std::vector<std::shared_ptr<DetectedObject>> loadSTLModels(const std::vector<std::string>& filepaths);

private:

    double stpmodel_min_z, stpmodel_max_z;
    bool detect_contours;

    // STP文件解析
    bool stpToVertices(const std::string& stpFilePath,
                       std::vector<VertexTWO>& outVertices,
                       std::vector<unsigned int>& outIndices,TopoDS_Shape TransformedShape,
                       float proportion,const float customColor[4]);

    // STL文件解析
    bool parseBinarySTL(const std::string& filepath,
                        std::vector<VertexTWO>& vertices,
                        std::vector<unsigned int>& indices);

    bool parseASCIISTL(const std::string& filepath,
                       std::vector<VertexTWO>& vertices,
                       std::vector<unsigned int>& indices);

    // bool isASCIISTL(const std::string& filepath);

    // 计算顶点法线（如果STL文件不包含法线）
    void calculateVertexNormals(std::vector<VertexTWO>& vertices,
                                const std::vector<unsigned int>& indices);

    // 计算横截面
    bool stpSlice_model(const TopoDS_Shape& shape,
                        double sliceHeight, double pointDensity, double pointSpacing);


    void parseSectionToLayers(const TopoDS_Shape& sectionShape, double sliceZ);



};

//------------------采集OCCT边界点-----------------


class AdaptiveCurveSampler {
public:
    struct SamplePoint {
        gp_Pnt point;
        double parameter;
        double curvature;
    };

    // 将单个 SamplePoint 转换为 Point3D
    Point3D SamplePointToPoint3D(const SamplePoint& sample) {
        return Point3D(sample.point.X(), sample.point.Y(), sample.point.Z());
    }

    // 结构转换
    std::vector<Point3D> ConvertToSequentialSegments(
        const std::vector<SamplePoint>& samples) {

        // std::vector<SliceSegment> segments;


        // if (samples.size() < 2) {
        //     return segments; // 至少需要2个点才能形成线段
        // }

        // segments.reserve(samples.size() - 1);

        // for (size_t i = 0; i < samples.size() - 1; i++) {
        //     Point3D p1 = SamplePointToPoint3D(samples[i]);
        //     Point3D p2 = SamplePointToPoint3D(samples[i + 1]);

        //     // std::cout << "点 " << i << ": 参数=" << samples[i].parameter
        //     //           << ", 坐标=(" << samples[i].point.X() << ", " << samples[i].point.Y() << ", " << samples[i].point.Z() << ")"
        //     //           << ", 曲率=" << samples[i].curvature << std::endl;
        //     if(sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2))<1.5){
        //         segments.emplace_back(p1, p2);
        //     }

        // }

        // return segments;


        std::vector<Point3D> allpoints;
        for(const auto& point:samples){
            allpoints.push_back(SamplePointToPoint3D(point));
        }
        return allpoints;
    }



    // 均匀采样（按点数）
    std::vector<SamplePoint> UniformSampling(const TopoDS_Edge& edge, int numPoints);

    // 均匀采样（按弧长）
    std::vector<SamplePoint> UniformSamplingByLength(const TopoDS_Edge& edge, double linearDeflection);

    // 均匀偏差采样
    std::vector<SamplePoint> DeflectionSampling(const TopoDS_Edge& edge, double deflection);

    // 切向偏差采样（修正版本）
    std::vector<SamplePoint> TangentialDeflectionSampling(const TopoDS_Edge& edge,
                                                          double linearDeflection,
                                                          double angularDeflection);

    std::vector<Point3D> PrintSamplingResults(const std::vector<SamplePoint>& points, const std::string& methodName);

private:
    void CalculateCurvature(Handle(Geom_Curve) curve, double param, double& curvature);
};

#endif // STL_MODEL_LOADER_H
