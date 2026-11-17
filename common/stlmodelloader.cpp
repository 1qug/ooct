#include "stlmodelloader.h"
#include <fstream>
#include <iostream>
// #include <algorithm>
#include <glm/glm.hpp>
#include <sstream>
#include <memory>
#include <QMessageBox>
#include <QWidget>
#include <QFileInfo>

#include <ShapeAnalysis_FreeBounds.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <TopTools_IndexedMapOfShape.hxx>


// 全局默认颜色（RGBA：灰色不透明）
const float DEFAULT_COLOR[4] = {0.8f, 0.8f, 0.8f, 1.0f};


// STL二进制文件头部结构
#pragma pack(push, 1)
struct STLHeader {
    char comment[80];
    uint32_t numTriangles;
};

struct STLTriangle {
    float normal[3];
    float v1[3], v2[3], v3[3];
    uint16_t attribute;
};

#pragma pack(pop)


STLModelLoader::STLModelLoader() {
    //initializeOpenGLFunctions();
}

STLModelLoader::~STLModelLoader()
{
    // if (m_stlObject != nullptr) {  // 安全检查：避免重复释放俄
    //     delete m_stlObject;        // 销毁堆对象
    //     m_stlObject = nullptr;     // 置空，避免悬垂指针
    // }

}

// 验证BRep几何精度，而不是三角化精度
void verifyGeometricPrecision(const TopoDS_Shape& original,
                              const TopoDS_Shape& scaled,
                              double scaleFactor) {
    // 1. 验证拓扑结构是否一致
    TopExp_Explorer origFaceExplorer(original, TopAbs_FACE);
    TopExp_Explorer scaledFaceExplorer(scaled, TopAbs_FACE);

    int origFaceCount = 0, scaledFaceCount = 0;
    while (origFaceExplorer.More()) { origFaceCount++; origFaceExplorer.Next(); }
    while (scaledFaceExplorer.More()) { scaledFaceCount++; scaledFaceExplorer.Next(); }

    qDebug() << "面数量 - 原始:" << origFaceCount << "缩放后:" << scaledFaceCount;

    // 2. 验证边数量
    TopExp_Explorer origEdgeExplorer(original, TopAbs_EDGE);
    TopExp_Explorer scaledEdgeExplorer(scaled, TopAbs_EDGE);

    int origEdgeCount = 0, scaledEdgeCount = 0;
    while (origEdgeExplorer.More()) { origEdgeCount++; origEdgeExplorer.Next(); }
    while (scaledEdgeExplorer.More()) { scaledEdgeCount++; scaledEdgeExplorer.Next(); }

    qDebug() << "边数量 - 原始:" << origEdgeCount << "缩放后:" << scaledEdgeCount;

    // 3. 验证顶点数量（几何顶点，不是三角化顶点）
    TopExp_Explorer origVertexExplorer(original, TopAbs_VERTEX);
    TopExp_Explorer scaledVertexExplorer(scaled, TopAbs_VERTEX);

    int origVertexCount = 0, scaledVertexCount = 0;
    while (origVertexExplorer.More()) { origVertexCount++; origVertexExplorer.Next(); }
    while (scaledVertexExplorer.More()) { scaledVertexCount++; scaledVertexExplorer.Next(); }

    qDebug() << "几何顶点数量 - 原始:" << origVertexCount << "缩放后:" << scaledVertexCount;
}

/**
 * @brief 计算 Vertex 集合的法向量（基于三角面片索引）
 * @param vertices 顶点集合（输入：position；输出：normal）
 * @param indices 三角面片索引（每 3 个一组，对应 vertices 的索引）
 */
void STLModelLoader::calculateVertexNormals(std::vector<VertexTWO>& vertices, const std::vector<unsigned int>& indices) {
    for (auto& v : vertices) {
        v.normal[0] = 0.0f;
        v.normal[1] = 0.0f;
        v.normal[2] = 0.0f;
    }

    if (vertices.empty() || indices.size() < 3) return;

    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int idx0 = indices[i];
        unsigned int idx1 = indices[i+1];
        unsigned int idx2 = indices[i+2];

        if (idx0 >= vertices.size() || idx1 >= vertices.size() || idx2 >= vertices.size()) {
            continue;
        }

        VertexTWO& v0 = vertices[idx0];
        VertexTWO& v1 = vertices[idx1];
        VertexTWO& v2 = vertices[idx2];

        float edge1[3] = {
            v1.position[0] - v0.position[0],
            v1.position[1] - v0.position[1],
            v1.position[2] - v0.position[2]
        };
        float edge2[3] = {
            v2.position[0] - v0.position[0],
            v2.position[1] - v0.position[1],
            v2.position[2] - v0.position[2]
        };

        float faceNormal[3] = {
            edge1[1] * edge2[2] - edge1[2] * edge2[1],
            edge1[2] * edge2[0] - edge1[0] * edge2[2],
            edge1[0] * edge2[1] - edge1[1] * edge2[0]
        };

        float normalLen = sqrt(faceNormal[0]*faceNormal[0] + faceNormal[1]*faceNormal[1] + faceNormal[2]*faceNormal[2]);
        if (normalLen < 1e-6f) continue;
        float invLen = 1.0f / normalLen;
        faceNormal[0] *= invLen;
        faceNormal[1] *= invLen;
        faceNormal[2] *= invLen;

        v0.normal[0] += faceNormal[0];
        v0.normal[1] += faceNormal[1];
        v0.normal[2] += faceNormal[2];

        v1.normal[0] += faceNormal[0];
        v1.normal[1] += faceNormal[1];
        v1.normal[2] += faceNormal[2];

        v2.normal[0] += faceNormal[0];
        v2.normal[1] += faceNormal[1];
        v2.normal[2] += faceNormal[2];
    }

    for (auto& v : vertices) {
        float len = sqrt(v.normal[0]*v.normal[0] + v.normal[1]*v.normal[1] + v.normal[2]*v.normal[2]);
        if (len < 1e-6f) {
            v.normal[0] = 0.0f;
            v.normal[1] = 1.0f;
            v.normal[2] = 0.0f;
        } else {
            float invLen = 1.0f / len;
            v.normal[0] *= invLen;
            v.normal[1] *= invLen;
            v.normal[2] *= invLen;
        }
    }
}




/**
 * @brief 将 STP 文件转换为 Vertex 顶点集合
 * @param stpFilePath STP 文件路径（const std::string&）
 * @param outVertices 输出：Vertex 顶点集合
 * @param outIndices 输出：三角面片索引（可选，用于渲染）
 * @param customColor 自定义颜色（可选，默认灰色）
 * @return bool：转换成功返回 true，失败返回 false
 */
bool STLModelLoader::stpToVertices(const std::string& stpFilePath,
                   std::vector<VertexTWO>& outVertices,
                   std::vector<unsigned int>& outIndices,TopoDS_Shape shape,
                    float proportion =1,
                   const float customColor[4] = DEFAULT_COLOR) {
    // 输出版本信息
    qDebug() << "[OCC版本信息] 主版本：" << OCC_VERSION_MAJOR;
    qDebug() << "[OCC版本信息] 次版本：" << OCC_VERSION_MINOR;
    qDebug() << "[OCC版本信息] 修订版：" << OCC_VERSION_MAINTENANCE;
    qDebug() << "[OCC版本信息] 完整版本：" << OCC_VERSION_STRING;

    outVertices.clear();
    outIndices.clear();
    stp_triangles.clear();
    // // 1. 读取 STP 文件
    // STEPControl_Reader reader;
    // IFSelect_ReturnStatus status = reader.ReadFile(stpFilePath.c_str());
    // if (status != IFSelect_RetDone) {
    //     qDebug() << "[STP转换失败] 读取文件失败：" << stpFilePath.c_str();
    //     return false;
    // }

    // reader.TransferRoots();
    // TopoDS_Shape shape = reader.OneShape();
    // if (shape.IsNull()) {
    //     qDebug() << "[STP转换失败] 模型为空";
    //     return false;
    // }
    double accuracy=proportion*0.005;
    qDebug() << "动态三角化精度:" << accuracy;


    // 2. 三角化
    BRepMesh_IncrementalMesh mesher(shape, accuracy);
    if (!mesher.IsDone()) {
        qDebug() << "[STP转换失败] 三角化失败";
        return false;
    }

    // 3. 遍历所有面
    TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
    unsigned int globalVertexIndex = 0;

    int totalTriangles = 0;

    // 初始化模型范围
    stpmodel_min_z = std::numeric_limits<double>::max();
    stpmodel_max_z = std::numeric_limits<double>::lowest();

    while (faceExplorer.More()) {
        TopoDS_Face face = TopoDS::Face(faceExplorer.Current());
        TopLoc_Location loc;

        // 提取三角化数据
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
        if (triangulation.IsNull()) {
            faceExplorer.Next();
            continue;
        }

        // ===================== 修正：正确的索引范围 =====================
        const Poly_ArrayOfNodes& nodes = triangulation->InternalNodes();

        // 遍历顶点 - 在 OpenCASCADE 7.6.3 中索引从 0 开始
        // Standard_Integer numNodes = triangulation->NbNodes();
        // for (Standard_Integer i = 1; i <= numNodes; ++i) {
        //     gp_Pnt occPnt = triangulation->Node(i);


        Standard_Integer numNodes = triangulation->NbNodes();
        for (Standard_Integer i = 0; i < numNodes; ++i) {
            // 使用正确的索引方式
            gp_Pnt occPnt = nodes.Value(i);

            // 应用坐标变换
            occPnt.Transform(loc.Transformation());

            VertexTWO vertex;

            // 填充 position
            vertex.position[0] = static_cast<float>(occPnt.X());
            vertex.position[1] = static_cast<float>(occPnt.Y());
            vertex.position[2] = static_cast<float>(occPnt.Z());

            // 填充 color
            vertex.color[0] = customColor[0];
            vertex.color[1] = customColor[1];
            vertex.color[2] = customColor[2];
            vertex.color[3] = customColor[3];

            // 法线初始化
            vertex.normal[0] = 0.0f;
            vertex.normal[1] = 0.0f;
            vertex.normal[2] = 0.0f;

            outVertices.push_back(vertex);
        }

        // 提取三角索引 - 三角形索引也需要从 0 开始
        // Standard_Integer numTriangles = triangulation->NbTriangles();
        // for (Standard_Integer i = 1; i <= numTriangles; ++i) {
        //     Poly_Triangle occTri = triangulation->Triangle(i);


        const Poly_Array1OfTriangle& occTriangles = triangulation->Triangles();
        Standard_Integer numTriangles = triangulation->NbTriangles();
        for (Standard_Integer i = 0; i < numTriangles; ++i) {
            Poly_Triangle occTri = occTriangles.Value(i+1); // Poly_Array1OfTriangle 仍然从 1 开始
            Standard_Integer idx1, idx2, idx3;
            occTri.Get(idx1, idx2, idx3);

            // 注意：三角形中的顶点索引也是从 1 开始的
            outIndices.push_back(static_cast<unsigned int>(globalVertexIndex + idx1 -1));
            outIndices.push_back(static_cast<unsigned int>(globalVertexIndex + idx2 -1));
            outIndices.push_back(static_cast<unsigned int>(globalVertexIndex + idx3 -1));

            // 获取顶点坐标（注意：索引从1开始）
            gp_Pnt p1 = nodes.Value(idx1-1);
            gp_Pnt p2 = nodes.Value(idx2-1);
            gp_Pnt p3 = nodes.Value(idx3-1);

            // 应用坐标变换
            p1.Transform(loc.Transformation());
            p2.Transform(loc.Transformation());
            p3.Transform(loc.Transformation());

            // 创建三角面片
            Point3D v0(p1.X(), p1.Y(), p1.Z());
            Point3D v1(p2.X(), p2.Y(), p2.Z());
            Point3D v2(p3.X(), p3.Y(), p3.Z());

            auto triangle = std::make_shared<Triangler>(v0, v1, v2);
            stp_triangles.push_back(triangle);
            // 更新模型范围
            stpmodel_min_z = std::min(stpmodel_min_z, triangle->min_z);
            stpmodel_max_z = std::max(stpmodel_max_z, triangle->max_z);

            totalTriangles++;

        }

        // 更新全局索引
        globalVertexIndex += static_cast<unsigned int>(numNodes);
        faceExplorer.Next();
    }

    // 计算法线
    calculateVertexNormals(outVertices, outIndices);

    qDebug() << "[STP转换成功] 顶点数：" << outVertices.size()
             << " 三角面片数：" << outIndices.size() / 3;

    return true;
}

bool extractWiresFromSection(const TopoDS_Shape& sectionShape, std::vector<TopoDS_Wire>& wires)
{
    wires.clear();

    // 遍历所有边并构建线框
    TopTools_IndexedMapOfShape edgesMap;
    TopExp::MapShapes(sectionShape, TopAbs_EDGE, edgesMap);

    if (edgesMap.IsEmpty()) {
        return false;
    }

    // 使用自由边界提取算法来正确连接边
    Handle(TopTools_HSequenceOfShape) edgesSeq = new TopTools_HSequenceOfShape();

    for (int i = 1; i <= edgesMap.Extent(); i++) {
        edgesSeq->Append(edgesMap(i));
    }

    // 提取自由边界（线框）
    Handle(TopTools_HSequenceOfShape) wireSeq;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edgesSeq, 1e-6, false, wireSeq);

    if (wireSeq.IsNull() || wireSeq->IsEmpty()) {
        return false;
    }

    // 转换为线框
    for (int i = 1; i <= wireSeq->Length(); i++) {
        TopoDS_Wire wire = TopoDS::Wire(wireSeq->Value(i));

        // 修复线框
        ShapeFix_Wire fixer;
        fixer.Load(wire);
        fixer.FixReorder();
        fixer.FixConnected();
        fixer.FixSelfIntersection();

        if (fixer.IsReady()) {
            wires.push_back(fixer.Wire());
        } else {
            wires.push_back(wire);
        }
    }

    return !wires.empty();
}

bool manualOffsetContour(const TopoDS_Wire& originalWire, double offsetDistance, TopoDS_Wire& offsetWire, bool outwardOffset)
{
    try {
        // 手动实现简单的偏移：通过复制并平移每个顶点
        BRepBuilderAPI_MakePolygon polyMaker;

        // 遍历线框中的边
        BRepTools_WireExplorer explorer(originalWire);
        std::vector<gp_Pnt> points;

        for (; explorer.More(); explorer.Next()) {
            TopoDS_Edge edge = explorer.Current();

            // 获取边的几何信息
            Standard_Real first, last;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

            if (!curve.IsNull()) {
                // 在边的起点和终点采样点
                gp_Pnt pnt1 = curve->Value(first);
                gp_Pnt pnt2 = curve->Value(last);

                // 计算法向偏移（简化版本，实际应该计算法向量）
                // 这里使用Z轴偏移作为示例，实际应该根据轮廓的法向量计算
                gp_Vec offsetVec(0, 0, offsetDistance);
                gp_Pnt offsetPnt1 = pnt1.Translated(offsetVec);
                gp_Pnt offsetPnt2 = pnt2.Translated(offsetVec);

                points.push_back(offsetPnt1);
                points.push_back(offsetPnt2);
            }
        }

        // 移除重复点并构建多边形
        if (!points.empty()) {
            // 添加第一个点
            polyMaker.Add(points[0]);

            // 添加其余点
            for (size_t i = 1; i < points.size(); i++) {
                if (points[i].Distance(points[i-1]) > 1e-6) {
                    polyMaker.Add(points[i]);
                }
            }

            // 闭合多边形
            if (polyMaker.IsDone()) {
                offsetWire = polyMaker.Wire();
                return true;
            }
        }

        return false;

    } catch (Standard_Failure& e) {
        qDebug() << "Manual offset error:" << e.GetMessageString();
        return false;
    }
}


bool offsetContour(const TopoDS_Wire& originalWire, double offsetDistance, TopoDS_Wire& offsetWire, bool outwardOffset)
{
    try {
        // 根据偏移方向调整距离
        double actualOffset = outwardOffset ? offsetDistance : -offsetDistance;

        // 方法1: 使用 BRepOffsetAPI_MakeOffset 进行线框偏移
        BRepOffsetAPI_MakeOffset wireOffset;
        wireOffset.AddWire(originalWire);
        wireOffset.Perform(actualOffset);

        if (wireOffset.IsDone()) {
            TopoDS_Shape offsetShape = wireOffset.Shape();

            // 从偏移形状中提取线框
            TopExp_Explorer exp(offsetShape, TopAbs_WIRE);
            if (exp.More()) {
                offsetWire = TopoDS::Wire(exp.Current());
                return true;
            }
        }

        // 方法2: 如果线框偏移失败，尝试使用面偏移
        TopoDS_Face face = BRepBuilderAPI_MakeFace(originalWire);
        if (!face.IsNull()) {
            BRepOffsetAPI_MakeOffsetShape offsetMaker;
            offsetMaker.PerformByJoin(face, actualOffset, 1e-6);
                // BRepOffset_Skin,    // 偏移模式
                // Standard_False,     // 不缝合
                // Standard_False,     // 不保留角
                // GeomAbs_Arc,        // 拐角类型：圆角
                // Standard_True);     // 自交检查

            if (offsetMaker.IsDone()) {
                TopoDS_Shape offsetShape = offsetMaker.Shape();

                // 从偏移面中提取边界线框
                TopExp_Explorer exp(offsetShape, TopAbs_WIRE);
                if (exp.More()) {
                    offsetWire = TopoDS::Wire(exp.Current());
                    return true;
                }
            }
        }

        // 方法3: 使用简单的几何偏移（手动计算）
        return manualOffsetContour(originalWire, actualOffset, offsetWire, outwardOffset);

    } catch (Standard_Failure& e) {
        qDebug() << "Offset contour error:" << e.GetMessageString();
        return false;
    }
}

bool generateUniformPointsAlongWire(const TopoDS_Wire& wire, std::vector<gp_Pnt>& points,
                                                    double pointDensity, double pointSpacing)
{
    points.clear();

    try {
        // 计算线框总长度
        double totalLength = 0.0;
        std::vector<double> edgeLengths;
        std::vector<Handle(Geom_Curve)> edgeCurves;
        std::vector<std::pair<double, double>> edgeParamRanges;

        // 遍历所有边，计算长度和存储曲线信息
        BRepTools_WireExplorer explorer(wire);
        for (; explorer.More(); explorer.Next()) {
            TopoDS_Edge edge = explorer.Current();

            Standard_Real first, last;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

            if (!curve.IsNull()) {
                // 计算边长度
                GCPnts_AbscissaPoint lengthCalculator;
                double edgeLength = GCPnts_AbscissaPoint::Length(GeomAdaptor_Curve(curve), first, last);

                totalLength += edgeLength;
                edgeLengths.push_back(edgeLength);
                edgeCurves.push_back(curve);
                edgeParamRanges.push_back(std::make_pair(first, last));
            }
        }

        if (totalLength < 1e-6) {
            qDebug() << "Wire length is too small:" << totalLength;
            return false;
        }

        // 计算总点数（基于密度或间距）
        int totalPoints;
        if (pointSpacing > 0.0) {
            // 使用固定间距
            totalPoints = static_cast<int>(totalLength / pointSpacing) + 1;
        } else {
            // 使用密度（点/单位长度）
            totalPoints = static_cast<int>(totalLength * pointDensity) + 1;
        }

        if (totalPoints < 2) totalPoints = 2; // 至少2个点

        // 计算实际间距
        double actualSpacing = totalLength / (totalPoints - 1);

        qDebug() << "Wire total length:" << totalLength << "Points:" << totalPoints
                 << "Spacing:" << actualSpacing;

        // 沿每条边生成点
        double currentDistance = 0.0;

        for (size_t edgeIdx = 0; edgeIdx < edgeCurves.size(); edgeIdx++) {
            const auto& curve = edgeCurves[edgeIdx];
            double first = edgeParamRanges[edgeIdx].first;
            double last = edgeParamRanges[edgeIdx].second;
            double edgeLength = edgeLengths[edgeIdx];

            // 计算这条边上的点数
            int pointsOnThisEdge = static_cast<int>(edgeLength / actualSpacing) + 1;
            if (pointsOnThisEdge < 1) pointsOnThisEdge = 1;

            // 计算这条边上的参数增量
            double paramIncrement = (last - first) / (pointsOnThisEdge - 1);

            // 生成这条边上的点
            for (int i = 0; i < pointsOnThisEdge; i++) {
                double param = first + i * paramIncrement;

                // 对于最后一条边的最后一个点，确保不重复
                if (edgeIdx == edgeCurves.size() - 1 && i == pointsOnThisEdge - 1) {
                    // 这是整个线框的终点，已经包含在第一条边的起点中
                    continue;
                }

                gp_Pnt point = curve->Value(param);
                points.push_back(point);
            }

            currentDistance += edgeLength;
        }

        // 确保起点和终点一致（闭合线框）
        if (!points.empty() && points.size() > 1) {
            double distance = points[0].Distance(points.back());
            if (distance < actualSpacing * 0.1) { // 如果首尾点很接近，移除尾点
                points.pop_back();
            }
        }

        qDebug() << "Generated" << points.size() << "points along wire";
        return !points.empty();

    } catch (Standard_Failure& e) {
        qDebug() << "Error generating points along wire:" << e.GetMessageString();
        return false;
    }
}

// void displayPoints(const std::vector<gp_Pnt>& points, const Quantity_Color& color)
// {
//     if (points.empty()) return;

//     try {
//         // 创建点集显示
//         Handle(Graphic3d_ArrayOfPoints) pointArray = new Graphic3d_ArrayOfPoints(points.size());

//         for (size_t i = 0; i < points.size(); i++) {
//             pointArray->AddVertex(points[i]);
//         }

//         // 创建点集交互对象
//         Handle(AIS_PointCloud) pointCloud = new AIS_PointCloud();
//         pointCloud->SetPoints(pointArray);
//         pointCloud->SetColor(color);

//         // 存储点云对象
//         m_pointClouds.push_back(pointCloud);

//         // 显示点集
//         m_context->Display(pointCloud, Standard_False);

//     } catch (Standard_Failure& e) {
//         qDebug() << "Error displaying points:" << e.GetMessageString();
//     }
// }



bool STLModelLoader::stpSlice_model(const TopoDS_Shape& shape,
                                    double sliceHeight,         // 切片高度
                                    double pointDensity,        // 点密度
                                    double pointSpacing){       // 点间距
    // 获取模型的边界框
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox, true);

    double xMin, yMin, zMin, xMax, yMax, zMax;
    bbox.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    qDebug() << "Model bounds: Z from" << zMin << "to" << zMax;

    int sliceCount = static_cast<int>((zMax - zMin) / sliceHeight);
    // int totalPoints = 0;

    std::vector<SliceSegment> segments;

    for (int i = 0; i <= sliceCount; i++) {
        double z = zMin + i * sliceHeight;
        z += 0.001;
        if(z<zMax &&z>zMin){
            // 创建切割平面
            gp_Pln slicePlane(gp_Pnt(0, 0, z), gp_Dir(0, 0, 1));

            // 执行截面计算
            BRepAlgoAPI_Section sectionMaker(shape, slicePlane);
            sectionMaker.Build();


            if (sectionMaker.IsDone()) {
                TopoDS_Shape sectionShape = sectionMaker.Shape();
                parseSectionToLayers(sectionShape, z);
                // if (!sectionShape.IsNull()) {

                //     // 提取线框
                //     std::vector<TopoDS_Wire> wires;
                //     if (extractWiresFromSection(sectionShape, wires)) {
                //         // 生成偏移轮廓
                //         // std::vector<TopoDS_Wire> contourWires;
                //         // if (offsetDistance == 0.0){
                //         //     // 无偏移：直接使用原始轮廓
                //         //     contourWires = wires;
                //         // }else if (offsetDistance > 0.0) {
                //         //     for (const auto& wire : wires) {
                //         //         TopoDS_Wire offsetWire;
                //         //         if (offsetContour(wire, offsetDistance, offsetWire, outwardOffset)) {
                //         //             contourWires.push_back(offsetWire);

                //         //         }
                //         //     }

                //         // }

                //         for (const auto& wire : wires) {
                //             // 从偏移轮廓生成均匀点
                //             std::vector<gp_Pnt> points;
                //             if (generateUniformPointsAlongWire(wire, points, pointDensity, pointSpacing)) {
                //                 // if(points.size()<2) return segments;
                //                 for(size_t i = 0; i < points.size() - 1; i++){

                //                     Point3D p1 = Point3D(points[i].X(), points[i].Y(), points[i].Z());
                //                     Point3D p2 = Point3D(points[i+1].X(), points[i+1].Y(), points[i+1].Z());
                //                     segments.emplace_back(p1, p2);
                //                 }
                //                 stp_layers[z] = segments;
                //                 qDebug() << "Slice" << i << "at Z =" << z
                //                          << "generated" << points.size() << "points";
                //             }
                //         }
                //     }
                // }

            }

        }

    }
    qDebug() <<"切片完成"<<stp_layers.size();
    return true;

}

// 函数：解析横截面并存入 stp_layers（适配 Point3D 版本）
void STLModelLoader::parseSectionToLayers(const TopoDS_Shape& sectionShape, double sliceZ) {
    if (sectionShape.IsNull()) {
        qWarning() << "警告：横截面为空，跳过存储";
        return;
    }

    // std::vector<SliceSegment> currentSegments;
    std::vector<Point3D> all_points;

    // TopoDS_Shape connectedSection = connectSectionEdges(sectionShape);

    // 1. 遍历横截面中的所有边（TopoDS_Edge）
    TopExp_Explorer edgeExplorer(sectionShape, TopAbs_EDGE);
    while (edgeExplorer.More()) {
        TopoDS_Edge edge = TopoDS::Edge(edgeExplorer.Current());
        if (edge.IsNull()) {
            edgeExplorer.Next();
            continue;
        }
        AdaptiveCurveSampler sampler;

        // auto points1 = sampler.UniformSampling(edge, 20); // 20个点
        // auto segments=sampler.PrintSamplingResults(points1, "均匀采样(点数)");

        auto points2 = sampler.UniformSamplingByLength(edge, 0.1); // 0.1mm间隔
        auto segments=sampler.PrintSamplingResults(points2, "均匀采样(弧长)");

        // auto points3 = sampler.DeflectionSampling(edge, 0.1); // 0.1mm偏差
        // auto segments=sampler.PrintSamplingResults(points3, "均匀偏差采样");

        // auto points4 = sampler.TangentialDeflectionSampling(edge, 0.1, 0.1); // 线性和角度偏差
        // auto segments=sampler.PrintSamplingResults(points4, "切向偏差采样");

        if(!segments.empty()){
            all_points.insert(all_points.end(),segments.begin(),segments.end());
        }


        edgeExplorer.Next();
    }

    if(!all_points.empty()){
        stp_layers[sliceZ] = all_points;
        qDebug() << "成功存储切片：高度=" << sliceZ
                 << "，线段数=" << all_points.size();
    } else {
        qWarning() << "警告：切片高度=" << sliceZ << " 无有效线段，未存储";

    }



    // qDebug() << "=== 截面形状调试信息 ===";
    // qDebug() << "sectionShape 是否为空：" << (sectionShape.IsNull() ? "是" : "否");
    // qDebug() << "sectionShape 拓扑类型：" << sectionShape.ShapeType(); // 输出拓扑类型（Wire/Face/Solid等）

    // // 2. 统计 sectionShape 中所有拓扑类型的数量（关键：看是否有 Wire）
    // auto countShapeType = [&](TopAbs_ShapeEnum type) -> int {
    //     TopExp_Explorer explorer(sectionShape, type);
    //     int count = 0;
    //     while (explorer.More()) { count++; explorer.Next(); }
    //     return count;
    // };
    // qDebug() << "sectionShape 中 Wire 数量：" << countShapeType(TopAbs_WIRE);
    // qDebug() << "sectionShape 中 Edge 数量：" << countShapeType(TopAbs_EDGE);
    // qDebug() << "sectionShape 中 Face 数量：" << countShapeType(TopAbs_FACE);

}

// 获取容器中指定索引的对象（只读，返回 nullptr 表示索引无效）
const std::shared_ptr<DetectedObject>& getObject(const std::vector<std::shared_ptr<DetectedObject>>& objects,size_t index)
{
    // 检查索引是否有效（在容器范围内）
    if (index < objects.size()) {
        return objects[index];
    }
    // 返回一个空指针引用（需在类中定义一个静态空指针作为默认返回值）
    static const std::shared_ptr<DetectedObject> emptyPtr = nullptr;
    return emptyPtr;
}

std::unique_ptr<STLObject> STLModelLoader::loadSTLModel(
    const std::string& filepath,
    std::shared_ptr<DetectedObject> blanktemp,
    float spacing)
{
    std::vector<unsigned int> indices;
    std::vector<VertexTWO> vertices;
    std::vector<VertexTWO> finalVertices;
    TopoDS_Shape transformedShape;
    vision_BoundingBox modelBbox;

    QFileInfo fileInfo(filepath.c_str());
    QString ext = fileInfo.suffix().toLower();
    qDebug() << "文件扩展名:" << ext;

    auto blank = blanktemp;

    // ---------- STL 分支 ----------
    if (ext == "stl") {
        if (STLUtils::isASCIISTL(filepath)) {
            if (!parseASCIISTL(filepath, vertices, indices)) {
                std::cerr << "Failed to parse ASCII STL: " << filepath << std::endl;
                return nullptr;
            }
        } else {
            if (!parseBinarySTL(filepath, vertices, indices)) {
                std::cerr << "Failed to parse binary STL: " << filepath << std::endl;
                return nullptr;
            }
        }

        finalVertices = vertices;

        // 补偿偏移量，让模型对齐基准面 Z=0
        if (!finalVertices.empty()) {
            float sumX = 0.0f, sumY = 0.0f;
            for (const auto& v : finalVertices) {
                sumX += v.position[0];
                sumY += v.position[1];
            }
            float centerX = sumX / finalVertices.size();
            float centerY = sumY / finalVertices.size();

            float maxZ = -INFINITY;
            for (const auto& v : finalVertices) {
                if (v.position[2] > maxZ) maxZ = v.position[2];
            }

            float offsetX = 0 - centerX;
            float offsetY = 0 - centerY;
            float offsetZ = 0 - maxZ;

            for (auto& v : finalVertices) {
                v.position[0] += offsetX;
                v.position[1] += offsetY;
                v.position[2] += offsetZ;
            }
        }

        modelBbox = STLBoundingBoxCalculator::calculateFromVertices(finalVertices);

        if (blank) {
            bool isInside = STLUtils::isBBoxInsideBlank(modelBbox, blank);
            if (!isInside) {
                float fitScale = 0.9f * STLUtils::calculateFitScale(modelBbox, blank);
                for (auto& v : finalVertices) {
                    v.position[0] *= fitScale;
                    v.position[1] *= fitScale;
                    v.position[2] *= fitScale;
                }
                if (!stp_triangles.empty()) {
                    auto scalePoint = [fitScale](Point3D& point) {
                        point.x *= fitScale;
                        point.y *= fitScale;
                        point.z *= fitScale;
                    };
                    for (auto& t : stp_triangles) {
                        scalePoint(t->v1);
                        scalePoint(t->v2);
                        scalePoint(t->v3);
                        t->min_z = std::min({t->v1.z, t->v2.z, t->v3.z});
                        t->max_z = std::max({t->v1.z, t->v2.z, t->v3.z});
                    }
                }
                modelBbox = STLBoundingBoxCalculator::calculateFromVertices(finalVertices);
                qDebug() << "STL模型缩放完成，比例:" << fitScale;
            }
        }
    }
    // ---------- STP/STEP 分支 ----------
    else if (ext == "stp" || ext == "step") {
        try {
            ModelTransformer* modelTransformer = new ModelTransformer();
            TopoDS_Shape originalShape = modelTransformer->loadModelFile(filepath);
            modelTransformer->loadAndTransformSTP(originalShape, 0.5, transformedShape);
            delete modelTransformer;   // 手动删除，避免内存泄漏

            if (!transformedShape.IsNull()) {
                Bnd_Box bbox;
                BRepBndLib::Add(transformedShape, bbox, Standard_True);
                Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
                bbox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
                modelBbox.max = glm::vec3(xMax, yMax, zMax);
                modelBbox.min = glm::vec3(xMin, yMin, zMin);
                modelBbox.calculateProperties();
                qDebug() << "包围盒中心" << modelBbox.center.x << modelBbox.center.y << modelBbox.center.z;
                qDebug() << "包围盒尺寸" << modelBbox.size.x << modelBbox.size.y << modelBbox.size.z;

                // 第一次三角化（未缩放）
                stpToVertices(filepath, vertices, indices, transformedShape);
                finalVertices = vertices;
                qDebug() << "第一次三角化后 stp_triangles 数量:" << stp_triangles.size();

                if (blank) {
                    bool isInside = STLUtils::isBBoxInsideBlank(modelBbox, blank);
                    if (!isInside) {
                        qDebug() << "模型包围盒超出毛坯范围，自动缩放适配";
                        float fitScale = 0.9f * STLUtils::calculateFitScale(modelBbox, blank);

                        gp_Trsf scaleTransform;
                        scaleTransform.SetScale(gp_Pnt(0, 0, 0), fitScale);
                        BRepBuilderAPI_Transform transformTool(transformedShape, scaleTransform, Standard_True);
                        transformedShape = transformTool.Shape();

                        // 重新计算缩放后的包围盒
                        Bnd_Box bbox2;
                        BRepBndLib::Add(transformedShape, bbox2, Standard_True);
                        Standard_Real sxMin, syMin, szMin, sxMax, syMax, szMax;
                        bbox2.Get(sxMin, syMin, szMin, sxMax, syMax, szMax);
                        modelBbox.max = glm::vec3(sxMax, syMax, szMax);
                        modelBbox.min = glm::vec3(sxMin, syMin, szMin);
                        modelBbox.calculateProperties();

                        // 缩放后重新三角化
                        stpToVertices(filepath, finalVertices, indices, transformedShape, fitScale);
                        qDebug() << "缩放后三角化 stp_triangles 数量:" << stp_triangles.size();
                    } else {
                        qDebug() << "包含在毛坯里，不用缩放";
                    }
                }
            }
        } catch (Standard_Failure& e) {
            qDebug() << "提取顶点时发生错误:" << e.GetMessageString();
            return nullptr;
        }
    }

    // ---------- 创建 STLObject 并返回 ----------
    auto pObj = std::make_unique<STLObject>(finalVertices, indices);

    if (ext == "stl") {
        pObj->setClassName("stl_model");
    } else if (ext == "stp" || ext == "step") {
        pObj->setClassName("stp_model");
        pObj->setStpShape(transformedShape);
        pObj->setTriangles(stp_triangles);
    }

    if (blank) {
        pObj->blank = blank;
        pObj->setCenter(blank->getCenter().x,
                        blank->getCenter().y,
                        blank->getCenter().z);
        pObj->setDepth(blank->getDepth() - spacing);
        pObj->setAngle(blank->getAngle());
        pObj->setSTLModelBbox(modelBbox);
        qDebug() << "有毛坯，返回 STLObject";
    } else {
        pObj->setCenter(0, 0, 0);
        pObj->setDepth(0.0f);
        pObj->setAngle(0.0f);
        qDebug() << "没有毛坯，创建临时 STLObject";
    }

    qDebug() << "STL vertices size:" << vertices.size()
             << "stlname:" << filepath.c_str()
             << "indices size:" << indices.size();

    return pObj;
}
void checkGLError(const char* location) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* errorStr = "";
        switch (error) {
        case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
        default: errorStr = "Unknown GL error"; break;
        }
        qWarning() << "OpenGL error at" << location << ":" << errorStr;
    }
}


bool STLModelLoader::parseBinarySTL(const std::string& filepath,
                                    std::vector<VertexTWO>& vertices,
                                    std::vector<unsigned int>& indices) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        qWarning() << "Cannot open binary STL file:" << filepath.c_str();
        return false;
    }

    // 检查文件大小
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < 84) {
        qWarning() << "STL file too small:" << fileSize << "bytes";
        return false;
    }

    // 跳过80字节头部
    char header[80];
    file.read(header, 80);

    // 读取三角形数量
    uint32_t numTriangles;
    file.read(reinterpret_cast<char*>(&numTriangles), sizeof(uint32_t));

    // 验证文件大小
    std::streamsize expectedSize = 84 + numTriangles * 50;
    if (fileSize != expectedSize) {
        qWarning() << "STL file size mismatch. Expected:" << expectedSize
                   << "Actual:" << fileSize;
        return false;
    }

    if (numTriangles == 0 || numTriangles > 1000000) {
        qWarning() << "Invalid number of triangles:" << numTriangles;
        return false;
    }

    // 读取所有三角形
    vertices.reserve(numTriangles * 3);
    indices.reserve(numTriangles * 3);

    for (uint32_t i = 0; i < numTriangles; ++i) {
        STLTriangle triangle;
        file.read(reinterpret_cast<char*>(&triangle), sizeof(STLTriangle));

        if (file.fail()) {
            qWarning() << "Failed to read triangle" << i;
            return false;
        }

        // 创建三个顶点
        for (int j = 0; j < 3; ++j) {
            VertexTWO vertex;
            const float* position = (j == 0) ? triangle.v1 : (j == 1) ? triangle.v2 : triangle.v3;

            // // 应用缩放因子到顶点坐标
            vertex.position[0] = position[0];
            vertex.position[1] = position[1];
            vertex.position[2] = position[2];


            vertex.normal[0] = triangle.normal[0];
            vertex.normal[1] = triangle.normal[1];
            vertex.normal[2] = triangle.normal[2];

            vertex.color[0] = 0.7f;
            vertex.color[1] = 0.5f;
            vertex.color[2] = 0.8f;
            vertex.color[3] = 1.0f;

            vertices.push_back(vertex);
            indices.push_back(i * 3 + j);
        }
    }

    file.close();

    // 二进制STL解析后（parseBinarySTL函数末尾）
    for (const auto& idx : indices) {
        if (idx >= vertices.size()) {
            qCritical() << "Invalid index" << idx << " (vertices size:" << vertices.size() << ")";
            return false;
        }
    }


    qDebug() << "Successfully parsed binary STL:" << numTriangles << "triangles";
    return true;
}

bool STLModelLoader::parseASCIISTL(const std::string& filepath,
                                   std::vector<VertexTWO>& vertices,
                                   std::vector<unsigned int>& indices) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        qWarning() << "Cannot open ASCII STL file:" << filepath.c_str();
        return false;
    }

    std::string line;
    std::vector<glm::vec3> currentNormals;
    std::vector<glm::vec3> currentVertices;
    int vertexCount = 0;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "facet") {
            std::string normalKeyword;
            float nx, ny, nz;
            iss >> normalKeyword >> nx >> ny >> nz;

            if (normalKeyword == "normal") {
                currentNormals.push_back(glm::vec3(nx, ny, nz));
            }
        }
        else if (keyword == "vertex") {
            float x, y, z;
            iss >> x >> y >> z;

            // // 应用缩放因子到顶点坐标
            // x *= scaleFactor;
            // y *= scaleFactor;
            // z *= scaleFactor;
            currentVertices.push_back(glm::vec3(x, y, z));

            // 每3个顶点组成一个三角形
            if (currentVertices.size() == 3) {
                if (currentNormals.empty()) {
                    // 计算面法线
                    glm::vec3 v1 = currentVertices[1] - currentVertices[0];
                    glm::vec3 v2 = currentVertices[2] - currentVertices[0];
                    glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
                    currentNormals.push_back(normal);
                }

                glm::vec3 normal = currentNormals.back();

                // 创建三个顶点
                for (int i = 0; i < 3; ++i) {
                    VertexTWO vertex;
                    vertex.position[0] = currentVertices[i].x;
                    vertex.position[1] = currentVertices[i].y;
                    vertex.position[2] = currentVertices[i].z;

                    vertex.normal[0] = normal.x;
                    vertex.normal[1] = normal.y;
                    vertex.normal[2] = normal.z;

                    // 设置颜色（灰紫色）
                    vertex.color[0] = 0.7f;
                    vertex.color[1] = 0.5f;
                    vertex.color[2] = 0.8f;
                    vertex.color[3] = 1.0f;

                    vertices.push_back(vertex);
                    indices.push_back(vertexCount++);
                }

                currentVertices.clear();
            }
        }
        else if (keyword == "endfacet") {
            currentNormals.pop_back(); // 移除当前面的法线
        }
    }

    file.close();

    if (vertices.empty()) {
        qWarning() << "No vertices found in ASCII STL file:" << filepath.c_str();
        return false;
    }


    // 二进制STL解析后（parseBinarySTL函数末尾）
    for (const auto& idx : indices) {
        if (idx >= vertices.size()) {
            qCritical() << "Invalid index" << idx << " (vertices size:" << vertices.size() << ")";
            return false;
        }
    }


    qDebug() << "Successfully parsed ASCII STL:" << vertices.size() << "vertices,"
             << indices.size() / 3 << "triangles";
    return true;
}




// 均匀采样（按点数）
std::vector<AdaptiveCurveSampler::SamplePoint>
AdaptiveCurveSampler::UniformSampling(const TopoDS_Edge& edge, int numPoints) {
    std::vector<SamplePoint> results;

    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

    if (curve.IsNull()) {
        std::cerr << "错误: 无法获取曲线几何" << std::endl;
        return results;
    }

    // 创建几何适配器
    GeomAdaptor_Curve curveAdaptor(curve, first, last);

    // 正确的均匀采样器使用方法
    GCPnts_UniformAbscissa uniformSampler;
    uniformSampler.Initialize(curveAdaptor, numPoints);

    if (!uniformSampler.IsDone()) {
        std::cerr << "错误: 均匀采样失败" << std::endl;
        return results;
    }

    int pointCount = uniformSampler.NbPoints();
    std::cout << "均匀采样: 生成 " << pointCount << " 个点" << std::endl;

    for (int i = 1; i <= pointCount; i++) {
        double param = uniformSampler.Parameter(i);
        gp_Pnt point = curve->Value(param);

        SamplePoint sample;
        sample.point = point;
        sample.parameter = param;

        double curvature = 0.0;
        CalculateCurvature(curve, param, curvature);
        sample.curvature = curvature;

        results.push_back(sample);
    }

    return results;
}

// 均匀采样（按弧长）
std::vector<AdaptiveCurveSampler::SamplePoint>
AdaptiveCurveSampler::UniformSamplingByLength(const TopoDS_Edge& edge, double linearDeflection) {
    std::vector<SamplePoint> results;

    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

    if (curve.IsNull()) {
        std::cerr << "错误: 无法获取曲线几何" << std::endl;
        return results;
    }

    GeomAdaptor_Curve curveAdaptor(curve, first, last);

    // 按弧长间隔采样
    GCPnts_UniformAbscissa uniformSampler;
    uniformSampler.Initialize(curveAdaptor, linearDeflection);

    if (!uniformSampler.IsDone()) {
        std::cerr << "错误: 均匀弧长采样失败" << std::endl;
        return results;
    }

    int pointCount = uniformSampler.NbPoints();
    std::cout << "均匀弧长采样: 生成 " << pointCount << " 个点" << std::endl;

    for (int i = 1; i <= pointCount; i++) {
        double param = uniformSampler.Parameter(i);
        gp_Pnt point = curve->Value(param);

        SamplePoint sample;
        sample.point = point;
        sample.parameter = param;

        double curvature = 0.0;
        CalculateCurvature(curve, param, curvature);
        sample.curvature = curvature;

        results.push_back(sample);
    }

    return results;
}


// 均匀偏差采样
std::vector<AdaptiveCurveSampler::SamplePoint>
AdaptiveCurveSampler::DeflectionSampling(const TopoDS_Edge& edge, double deflection) {
    std::vector<SamplePoint> results;

    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);

    if (curve.IsNull()) {
        std::cerr << "错误: 无法获取曲线几何" << std::endl;
        return results;
    }

    GeomAdaptor_Curve curveAdaptor(curve, first, last);

    // 正确的偏差采样器使用方法
    GCPnts_UniformDeflection deflectionSampler;
    deflectionSampler.Initialize(curveAdaptor, deflection);

    if (!deflectionSampler.IsDone()) {
        std::cerr << "错误: 偏差采样失败" << std::endl;
        return results;
    }

    int pointCount = deflectionSampler.NbPoints();
    std::cout << "均匀偏差采样: 生成 " << pointCount << " 个点" << std::endl;

    for (int i = 1; i <= pointCount; i++) {
        double param = deflectionSampler.Parameter(i);
        gp_Pnt point = curve->Value(param);

        SamplePoint sample;
        sample.point = point;
        sample.parameter = param;

        double curvature = 0.0;
        CalculateCurvature(curve, param, curvature);
        sample.curvature = curvature;

        results.push_back(sample);
    }

    return results;
}



// 切向偏差采样（修正版本）
std::vector<AdaptiveCurveSampler::SamplePoint>
AdaptiveCurveSampler::TangentialDeflectionSampling(const TopoDS_Edge& edge,
                                                   double linearDeflection,
                                                   double angularDeflection) {
    std::vector<SamplePoint> results;

    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    qDebug()<<"起始点"<<first<<"终点"<<last;
    if (curve.IsNull()) {
        std::cerr << "错误: 无法获取曲线几何" << std::endl;
        return results;
    }

    GeomAdaptor_Curve curveAdaptor(curve, first, last);

    // 正确的切向偏差采样器使用方法
    GCPnts_TangentialDeflection tangentialSampler;
    tangentialSampler.Initialize(curveAdaptor, angularDeflection, linearDeflection);

    // 检查是否初始化成功
    if (tangentialSampler.NbPoints() == 0) {
        std::cerr << "错误: 切向偏差采样失败" << std::endl;
        return results;
    }

    int pointCount = tangentialSampler.NbPoints();
    std::cout << "切向偏差采样: 生成 " << pointCount << " 个点" << std::endl;

    // 正确的参数获取方式
    for (int i = 1; i <= pointCount; i++) {
        double param = tangentialSampler.Parameter(i);
        gp_Pnt point = curve->Value(param);

        SamplePoint sample;
        sample.point = point;
        sample.parameter = param;

        double curvature = 0.0;
        CalculateCurvature(curve, param, curvature);
        sample.curvature = curvature;

        results.push_back(sample);
    }

    return results;
}

void AdaptiveCurveSampler::CalculateCurvature(Handle(Geom_Curve) curve, double param, double& curvature) {
    try {
        // 计算一阶和二阶导数
        gp_Pnt point;
        gp_Vec firstDeriv, secondDeriv;
        curve->D2(param, point, firstDeriv, secondDeriv);

        // 计算曲率: |firstDeriv × secondDeriv| / |firstDeriv|^3
        if (firstDeriv.Magnitude() > Precision::Confusion()) {
            gp_Vec crossProduct = firstDeriv.Crossed(secondDeriv);
            curvature = crossProduct.Magnitude() / pow(firstDeriv.Magnitude(), 3);
        } else {
            curvature = 0.0;
        }
    } catch (...) {
        curvature = 0.0;
    }
}

std::vector<Point3D> AdaptiveCurveSampler::PrintSamplingResults(const std::vector<SamplePoint>& points, const std::string& methodName) {
    std::cout << "\n=== " << methodName << " 结果 ===" << std::endl;
    std::cout << "总点数: " << points.size() << std::endl;


    // auto segments=ConvertToSequentialSegments(points);
    auto allpoints=ConvertToSequentialSegments(points);

    // for (size_t i = 0; i < points.size(); i++) {
    //     const SamplePoint& sp = points[i];

    //     std::cout << "点 " << i << ": 参数=" << sp.parameter
    //               << ", 坐标=(" << sp.point.X() << ", " << sp.point.Y() << ", " << sp.point.Z() << ")"
    //               << ", 曲率=" << sp.curvature << std::endl;
    // }


    // 统计信息
    if (!points.empty()) {
        double minCurvature = points[0].curvature;
        double maxCurvature = points[0].curvature;
        for (const auto& point : points) {
            if (point.curvature < minCurvature) minCurvature = point.curvature;
            if (point.curvature > maxCurvature) maxCurvature = point.curvature;
        }
        std::cout << "曲率范围: " << minCurvature << " 到 " << maxCurvature << std::endl;
    }
    // return segments;
    return allpoints;
}
