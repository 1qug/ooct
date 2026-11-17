#include "object_occt_view.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QVBoxLayout>
#include <algorithm>
#include <set>
#include <cmath>
#include <Aspect_DisplayConnection.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>

#include <AIS_Point.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Quantity_Color.hxx>
#include <gp_Pnt.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <AIS_Line.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <Geom_CartesianPoint.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <gce_MakeCirc.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <GC_MakeSegment.hxx>

#include <V3d_ListOfLight.hxx>
#include <gp_Dir.hxx>
#include <AIS_Shape.hxx>

#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>


ObjectOcctView::ObjectOcctView(QWidget* parent)
    : QWidget(parent)
{
    occtWindow = new OcctWindow();
    container = QWidget::createWindowContainer((QWindow*)occtWindow, this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(container, 1);

    // 关键：安装事件过滤器，拦截容器的鼠标事件
    container->installEventFilter(this);
    container->setFocusPolicy(Qt::StrongFocus);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // 初始化预设颜色
    m_colors.push_back(Quantity_Color(119/255.0f, 136/255.0f, 153/255.0f, Quantity_TOC_RGB));
    m_colors.push_back(Quantity_Color(72/255.0f, 61/255.0f, 139/255.0f, Quantity_TOC_RGB));
    m_colors.push_back(Quantity_Color(40/255.0f, 162/255.0f, 250/255.0f, Quantity_TOC_RGB));
    m_colors.push_back(Quantity_Color(46/255.0f, 139/255.0f, 87/255.0f, Quantity_TOC_RGB));
    m_colors.push_back(Quantity_Color(243/255.0f, 217/255.0f, 6/255.0f, Quantity_TOC_RGB));
    m_colors.push_back(Quantity_Color(106/255.0f, 250/255.0f, 34/255.0f, Quantity_TOC_RGB));

    initializeOcct();
}

ObjectOcctView::~ObjectOcctView()
{
}


void ObjectOcctView::initializeOcct()
{
    Handle(Aspect_DisplayConnection) displayConnection = new Aspect_DisplayConnection();
    occtWindow->initOcctViewer(displayConnection);

    m_view = occtWindow->getView();
    m_context = occtWindow->getContext();
    m_viewer = occtWindow->getViewer();

    // 再次确保关闭降级渲染
    m_view->SetComputedMode(Standard_False);

    m_viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);

    setGradientBackground(Quantity_Color(80.0f/255.0f, 82.0f/255.0f, 95.0f/255.0f, Quantity_TOC_RGB),
                          Quantity_Color(131.0f/255.0f, 137.0f/255.0f, 153.0f/255.0f, Quantity_TOC_RGB));
    setTopDownLighting();
}
void ObjectOcctView::setGradientBackground(const Quantity_Color &topColor, const Quantity_Color &bottomColor)
{
    if (m_view.IsNull()) return;
    m_bgTopColor = topColor;
    m_bgBottomColor = bottomColor;
    m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER, Standard_False);
    updateView();
}

void ObjectOcctView::setSolidBackground(const Quantity_Color &color)
{
    if (m_view.IsNull()) return;
    m_bgTopColor = color;
    m_bgBottomColor = color;
    m_view->SetBackgroundColor(color);
    updateView();
}

void ObjectOcctView::setLightIntensity(double intensity)
{
    if (m_viewer.IsNull()) return;
    m_lightIntensity = qBound(0.0, intensity, 2.0);
    V3d_ListOfLight lights = m_viewer->ActiveLights();
    for (V3d_ListOfLight::Iterator it(lights); it.More(); it.Next()) {
        Handle(V3d_Light) light = it.Value();
        if (light->Type() == V3d_DIRECTIONAL) {
            light->SetIntensity(m_lightIntensity);
        }
    }
    updateView();
}

void ObjectOcctView::setAmbientLight(double intensity)
{
    if (m_viewer.IsNull()) return;
    m_ambientLight = qBound(0.0, intensity, 1.0);
    V3d_ListOfLight lights = m_viewer->ActiveLights();
    for (V3d_ListOfLight::Iterator it(lights); it.More(); it.Next()) {
        Handle(V3d_Light) light = it.Value();
        if (light->Type() == V3d_AMBIENT) {
            light->SetIntensity(m_ambientLight);
        }
    }
    updateView();
}

void ObjectOcctView::enableShadows(bool enable)
{
    if (m_view.IsNull()) return;
    m_shadowsEnabled = enable;
    Graphic3d_RenderingParams& params = m_view->ChangeRenderingParams();
    params.IsShadowEnabled = enable;
    updateView();
}

void ObjectOcctView::setLightPosition(double x, double y, double z)
{
    m_lightDirection = gp_Dir(x, y, z);
}

void ObjectOcctView::setLightDirection(double x, double y, double z)
{
    if (m_viewer.IsNull()) return;
    m_lightDirection = gp_Dir(x, y, z);
    V3d_ListOfLight lights = m_viewer->ActiveLights();
    for (V3d_ListOfLight::Iterator it(lights); it.More(); it.Next()) {
        Handle(V3d_Light) light = it.Value();
        if (light->Type() == V3d_DIRECTIONAL) {
            Handle(V3d_DirectionalLight) dirLight = Handle(V3d_DirectionalLight)::DownCast(light);
            if (!dirLight.IsNull()) {
                dirLight->SetDirection(m_lightDirection.X(), m_lightDirection.Y(), m_lightDirection.Z());
            }
        }
    }
    updateView();
}

void ObjectOcctView::setTopDownLighting()
{
    if (m_viewer.IsNull()) return;

    Graphic3d_RenderingParams& params = m_view->ChangeRenderingParams();
    int previousMsaa = params.NbMsaaSamples;
    bool previousAA = params.IsAntialiasingEnabled;
    params.NbMsaaSamples = 0;
    params.IsAntialiasingEnabled = false;

    Handle(V3d_DirectionalLight) keyLight = new V3d_DirectionalLight(V3d_Zneg, Quantity_NOC_WHITE, Standard_True);
    keyLight->SetDirection(0.0, 0.0, -1.0);
    keyLight->SetIntensity(1.2);
    m_viewer->AddLight(keyLight);

    Handle(V3d_DirectionalLight) fillLight = new V3d_DirectionalLight(V3d_Zpos, Quantity_NOC_WHITE, Standard_True);
    fillLight->SetDirection(0.0, 0.0, 1.0);
    fillLight->SetIntensity(0.3);
    m_viewer->AddLight(fillLight);

    Handle(V3d_DirectionalLight) sideLight = new V3d_DirectionalLight(V3d_Xpos, Quantity_NOC_WHITE, Standard_True);
    sideLight->SetDirection(1.0, 0.0, -0.5);
    sideLight->SetIntensity(0.2);
    m_viewer->AddLight(sideLight);

    Handle(V3d_AmbientLight) ambientLight = new V3d_AmbientLight();
    ambientLight->SetIntensity(0.2);
    m_viewer->AddLight(ambientLight);

    m_viewer->SetLightOn();

    params.NbMsaaSamples = previousMsaa;
    params.IsAntialiasingEnabled = previousAA;

    updateView();
    m_lightDirection = gp_Dir(0.0, 0.0, -1.0);
}

bool ObjectOcctView::loadModel(const std::string& filepath)
{
    try {
        std::string extension = filepath.substr(filepath.find_last_of(".") + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        TopoDS_Shape shape;

        if (extension == "stl" || extension == "stla" || extension == "stlb") {
            StlAPI_Reader stlReader;
            if (!stlReader.Read(shape, filepath.c_str())) {
                qWarning() << "Failed to read STL file:" << filepath.c_str();
                return false;
            }
        } else if (extension == "stp" || extension == "step") {
            STEPControl_Reader stepReader;
            IFSelect_ReturnStatus status = stepReader.ReadFile(filepath.c_str());
            if (status != IFSelect_RetDone) {
                qWarning() << "Failed to read STEP file:" << filepath.c_str();
                return false;
            }
            stepReader.TransferRoots();
            shape = stepReader.OneShape();
            if (shape.IsNull()) {
                qWarning() << "STEP file contains no valid shape:" << filepath.c_str();
                return false;
            }
        } else if (extension == "igs" || extension == "iges") {
            IGESControl_Reader igesReader;
            IFSelect_ReturnStatus status = igesReader.ReadFile(filepath.c_str());
            if (status != IFSelect_RetDone) {
                qWarning() << "Failed to read IGES file:" << filepath.c_str();
                return false;
            }
            igesReader.TransferRoots();
            shape = igesReader.OneShape();
            if (shape.IsNull()) {
                qWarning() << "IGES file contains no valid shape:" << filepath.c_str();
                return false;
            }
        } else {
            qWarning() << "Unsupported file format:" << extension.c_str();
            return false;
        }

        if (!m_stlShape.IsNull()) {
            m_context->Remove(m_stlShape, false);
        }
        m_stlShape = new AIS_Shape(shape);
        m_stlShape->SetColor(Quantity_Color(25.0/255.0, 35.0/255.0, 45.0/255.0, Quantity_TOC_RGB));


        Handle(Prs3d_Drawer) aDrawer=m_stlShape->Attributes();
        aDrawer->SetFaceBoundaryDraw(true);

        Handle(Prs3d_LineAspect) lineAspect=new Prs3d_LineAspect(Quantity_NOC_BLACK,Aspect_TOL_SOLID,1.0);
        aDrawer->SetFaceBoundaryAspect(lineAspect);

        m_context->Display(m_stlShape, true);
        m_context->SetDisplayMode(m_stlShape,AIS_Shaded,true);
        m_view->FitAll();
        update();
        return true;
    } catch (const Standard_Failure& e) {
        qCritical() << "OCCT error loading CAD file:" << e.GetMessageString();
        return false;
    }
}

void ObjectOcctView::hideCoordinateSystem()
{
    for (const auto& shape : m_coordAxesShapes) {
        if (!shape.IsNull()) {
            m_context->Remove(shape, false);
        }
    }
    m_coordAxesShapes.clear();
    updateView();
}

void ObjectOcctView::displayCoordinateSystem(const gp_Trsf& transform, double axisLength)
{
    hideCoordinateSystem();

    Quantity_Color red(1.0, 0.0, 0.0, Quantity_TOC_RGB);
    Quantity_Color green(0.0, 1.0, 0.0, Quantity_TOC_RGB);
    Quantity_Color blue(0.0, 0.0, 1.0, Quantity_TOC_RGB);

    gp_Pnt origin(0, 0, 0);
    gp_Pnt xEnd(axisLength, 0, 0);
    gp_Pnt yEnd(0, axisLength, 0);
    gp_Pnt zEnd(0, 0, axisLength);
    origin.Transform(transform);
    xEnd.Transform(transform);
    yEnd.Transform(transform);
    zEnd.Transform(transform);

    auto createArrowAxis = [&](const gp_Pnt& start, const gp_Pnt& end, const Quantity_Color& color) -> Handle(AIS_Shape) {
        gp_Vec dirVec(start, end);
        double length = dirVec.Magnitude();
        if (length < 1e-6) return nullptr;
        double arrowHeight = length * 0.12;
        double arrowRadius = arrowHeight * 0.4;
        double shaftLength = length - arrowHeight;
        double shaftRadius = arrowRadius * 0.3;
        gp_Dir dir(dirVec);
        gp_Ax2 shaftAxis(start, dir);
        gp_Pnt coneBase = start.Translated(dirVec.Normalized() * shaftLength);
        BRepPrimAPI_MakeCylinder shaftMaker(shaftAxis, shaftRadius, shaftLength);
        TopoDS_Shape shaftShape = shaftMaker.Shape();
        gp_Ax2 coneAxis(coneBase, dir);
        BRepPrimAPI_MakeCone coneMaker(coneAxis, arrowRadius, 0.0, arrowHeight);
        TopoDS_Shape coneShape = coneMaker.Shape();
        BRepAlgoAPI_Fuse fuse(shaftShape, coneShape);
        if (!fuse.IsDone()) return nullptr;
        Handle(AIS_Shape) arrowShape = new AIS_Shape(fuse.Shape());
        arrowShape->SetColor(color);
        return arrowShape;
    };

    Handle(AIS_Shape) xAxis = createArrowAxis(origin, xEnd, red);
    Handle(AIS_Shape) yAxis = createArrowAxis(origin, yEnd, green);
    Handle(AIS_Shape) zAxis = createArrowAxis(origin, zEnd, blue);

    if (!xAxis.IsNull()) {
        m_context->Display(xAxis, false);
        m_coordAxesShapes.push_back(xAxis);
    }
    if (!yAxis.IsNull()) {
        m_context->Display(yAxis, false);
        m_coordAxesShapes.push_back(yAxis);
    }
    if (!zAxis.IsNull()) {
        m_context->Display(zAxis, false);
        m_coordAxesShapes.push_back(zAxis);
    }

    BRepPrimAPI_MakeSphere sphereMaker(origin, axisLength * 0.02);
    Handle(AIS_Shape) sphereShape = new AIS_Shape(sphereMaker.Shape());
    sphereShape->SetColor(Quantity_NOC_WHITE);
    m_context->Display(sphereShape, false);
    m_coordAxesShapes.push_back(sphereShape);

    updateView();
}

void ObjectOcctView::updateDetectedObjects(const std::vector<std::shared_ptr<DetectedObject>>& objects,
                                           const gp_Trsf& coordTransform)
{
    // 清除原有检测物体
    for (const auto& shape : m_detectedObjectShapes) {
        m_context->Remove(shape, false);
    }
    m_detectedObjectShapes.clear();

    // 逐个转换并添加
    for (const auto& obj : objects) {
        Handle(AIS_Shape) occtShape = DetectedObjectOcctAdapter::convertToOcctShape(obj);
        if (!occtShape.IsNull()) {
            occtShape->SetLocalTransformation(coordTransform);

            // ★ 统一风格：偏黑的深钢色 + 黑色面边界
            //    RGB(25, 35, 45) 为深灰蓝色，接近暗钢色调
            occtShape->SetColor(Quantity_Color(25.0/255.0, 35.0/255.0, 45.0/255.0, Quantity_TOC_RGB));
            occtShape->SetTransparency(0);
            Handle(Prs3d_Drawer) aDrawer = occtShape->Attributes();
            aDrawer->SetFaceBoundaryDraw(Standard_True);               // 显示面边界

            Handle(Prs3d_LineAspect) lineAspect = new Prs3d_LineAspect(
                Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0);            // 黑色实线，线宽1.0
            aDrawer->SetFaceBoundaryAspect(lineAspect);

            // 添加到上下文并设为着色模式
            m_context->Display(occtShape, false);
            m_context->SetDisplayMode(occtShape, AIS_Shaded, false);

            m_detectedObjectShapes.push_back(occtShape);
        }
    }
    updateView();
}
// ==================== 绘制连续刀具路径 ====================
void ObjectOcctView::drawContinuousPath(const std::vector<Point3D>& continuousPath,
                                        const std::shared_ptr<DetectedObject>& obj,
                                        const gp_Trsf& coordTransform)
{
    clearPaths();
    if (continuousPath.size() < 2) return;

    if (m_view.IsNull() || m_context.IsNull()) {
        qWarning() << "drawContinuousPath: OCCT 上下文或视图无效";
        return;
    }
    if (!obj) {
        qWarning() << "drawContinuousPath: 毛坯对象为空";
        return;
    }

    // 毛坯变换
    gp_Trsf blankTrsf;
    double angleRad = obj->getAngle() * M_PI / 180.0;
    if (std::abs(angleRad) > 1e-6) {
        gp_Trsf rot;
        rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angleRad);
        blankTrsf = rot;
    }
    gp_Trsf trans;
    trans.SetTranslation(gp_Vec(obj->getCenter().x, obj->getCenter().y, obj->getCenter().z));
    blankTrsf = trans * blankTrsf;
    gp_Trsf finalTransform = coordTransform * blankTrsf;

    auto isValid = [](const Point3D& p) -> bool {
        return !std::isnan(p.x) && !std::isnan(p.y) && !std::isnan(p.z) &&
               std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    };

    // 颜色映射（同前）
    auto getStyle = [](MoveType type, Quantity_Color& color,
                       Aspect_TypeOfLine& style, Standard_Real& width) {
        switch (static_cast<int>(type)) {   // 直接转整数比较，避免枚举顺序问题
        case 0: // NormalCut
            color = Quantity_Color(0.0, 1.0, 0.0, Quantity_TOC_RGB);   // 绿色实线
            style = Aspect_TOL_SOLID; width = 1.8; break;
        case 1: // Spiral
            color = Quantity_Color(1.0, 0.5, 0.0, Quantity_TOC_RGB);   // 橙色实线
            style = Aspect_TOL_SOLID; width = 1.8; break;
        case 2: // RapidMove
            color = Quantity_Color(1.0, 0.0, 0.0, Quantity_TOC_RGB);   // 红色虚线
            style = Aspect_TOL_DASH; width = 1.2; break;
        case 3: // NormalEntry
            color = Quantity_Color(1.0, 1.0, 0.0, Quantity_TOC_RGB);   // 黄色虚线
            style = Aspect_TOL_DASH; width = 1.2; break;
        case 4: // CuttingHeight
            color = Quantity_Color(0.0, 0.0, 1.0, Quantity_TOC_RGB);   // 蓝色实线
            style = Aspect_TOL_SOLID; width = 1.2; break;
        case 5: // ExitMove
            color = Quantity_Color(0.5, 0.0, 0.5, Quantity_TOC_RGB);   // 紫色虚线
            style = Aspect_TOL_DASH; width = 1.2; break;
        case 6: // RapidLine
            color = Quantity_Color(1.0, 0.5, 0.0, Quantity_TOC_RGB);   // 橙色实线
            style = Aspect_TOL_SOLID; width = 1.5; break;
        default:
            color = Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB);
            style = Aspect_TOL_SOLID; width = 1.0; break;
        }
    };

    for (size_t i = 0; i < continuousPath.size() - 1; ++i) {
        const Point3D& p1 = continuousPath[i];
        const Point3D& p2 = continuousPath[i+1];

        if (!isValid(p1) || !isValid(p2)) continue;

        Quantity_Color color;
        Aspect_TypeOfLine lineStyle;
        Standard_Real width;
        getStyle(p1.moveType, color, lineStyle, width);

        // ★★★ 调试输出 ★★★
        qDebug().noquote()
            << QString("Seg%1 | from(%2,%3,%4) type=%5 → to(%6,%7,%8)")
                   .arg(i, 4)
                   .arg(p1.x, 8, 'f', 2).arg(p1.y, 8, 'f', 2).arg(p1.z, 8, 'f', 2)
                   .arg((int)p1.moveType)
                   .arg(p2.x, 8, 'f', 2).arg(p2.y, 8, 'f', 2).arg(p2.z, 8, 'f', 2)
            << "| Color RGB:"
            << color.Red() << color.Green() << color.Blue();

        gp_Pnt gp1(p1.x, p1.y, p1.z);
        gp_Pnt gp2(p2.x, p2.y, p2.z);
        Handle(AIS_Shape) seg = createLineWithStyle(gp1, gp2, color, lineStyle, width);
        if (!seg.IsNull()) {
            seg->SetLocalTransformation(finalTransform);
            m_context->Display(seg, Standard_False);
            m_pathShapes.push_back(seg);
        }
    }

    setAuxiliaryVisibility(m_showAuxiliary);
    updateView();
}



// void ObjectOcctView::drawContourGroups(const ContourGroups& groups,
//                                        const std::shared_ptr<DetectedObject>& obj,
//                                        const gp_Trsf& coordTransform,
//                                        const std::vector<MaxIncircleInfo>* incircleInfo)
// {
//     clearSlices();

//     // 计算从对象坐标系到世界坐标系的最终变换
//     gp_Trsf blankTrsf;
//     double angleRad = obj->getAngle() * M_PI / 180.0;
//     if (std::abs(angleRad) > 1e-6) {
//         gp_Trsf rot;
//         rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angleRad);
//         blankTrsf = rot;
//     }
//     gp_Trsf trans;
//     trans.SetTranslation(gp_Vec(obj->getCenter().x, obj->getCenter().y, obj->getCenter().z));
//     blankTrsf = trans * blankTrsf;
//     gp_Trsf finalTransform = coordTransform * blankTrsf;

//     // 获取毛坯矩形边界（如果有效）
//     bool useStockBounds = m_stockBoundsValid;
//     double stockLeft = m_stockLeft, stockRight = m_stockRight;
//     double stockBottom = m_stockBottom, stockTop = m_stockTop;

//     // ========== 构建全局连续偏置序号（每个 groupId 内跨区域连续，内→外递增）==========
//     std::map<int, std::map<std::pair<int,int>, int>> groupGlobalIndexMap; // key: (contourId, offIdx) -> globalIndex
//     {
//         // 收集每个 groupId 下的所有偏置环
//         struct RingKey {
//             int contourId;
//             int offIdx;
//             int regionId;
//         };
//         std::map<int, std::vector<RingKey>> groupRings;  // groupId -> rings

//         for (const auto& group : groups.groups) {
//             for (const auto& gc : group) {
//                 const LayerContour* lc = gc.contour;
//                 if (!lc) continue;
//                 double z = gc.z;
//                 for (size_t offIdx = 0; offIdx < lc->offsets.size(); ++offIdx) {
//                     int regionId = 0;
//                     if (incircleInfo) {
//                         for (const auto& info : *incircleInfo) {
//                             if (std::abs(info.z - z) < 1e-6 &&
//                                 info.contourId == lc->id &&
//                                 info.passIndex == static_cast<int>(offIdx)) {
//                                 regionId = info.regionId;
//                                 break;
//                             }
//                         }
//                     }
//                     groupRings[gc.groupId].push_back({lc->id, static_cast<int>(offIdx), regionId});
//                 }
//             }
//         }

//         // 对每个 groupId，跨区域连续编号
//         for (auto& [groupId, rings] : groupRings) {
//             // 按区域分组
//             std::map<int, std::vector<RingKey>> regionMap;
//             for (const auto& r : rings) {
//                 regionMap[r.regionId].push_back(r);
//             }

//             // 确定区域排列顺序：独占区域 (regionId > 0) 按 ID 升序，共享区域 A (regionId=0) 放在最后
//             std::vector<int> regionOrder;
//             for (const auto& [rid, _] : regionMap) {
//                 if (rid > 0) regionOrder.push_back(rid);
//             }
//             std::sort(regionOrder.begin(), regionOrder.end());
//             if (regionMap.count(0)) regionOrder.push_back(0);

//             int globalIdx = 0;  // 全局序号，跨区域递增
//             for (int rid : regionOrder) {
//                 auto& vec = regionMap[rid];
//                 // 区域内按 offIdx 降序（内环在前）
//                 std::sort(vec.begin(), vec.end(), [](const RingKey& a, const RingKey& b) {
//                     return a.offIdx > b.offIdx;
//                 });
//                 for (const auto& rk : vec) {
//                     groupGlobalIndexMap[groupId][{rk.contourId, rk.offIdx}] = globalIdx++;
//                 }
//             }
//         }
//     }
//     // ============================================================================

//     for (size_t gIdx = 0; gIdx < groups.groups.size(); ++gIdx) {
//         const auto& group = groups.groups[gIdx];
//         Quantity_Color groupColor = groups.groupColors[gIdx];

//         for (const auto& gc : group) {
//             const LayerContour* lc = gc.contour;
//             if (!lc) continue;
//             double z = gc.z;

//             // 外轮廓：只有没有偏置环时才显示原始轮廓
//             bool drawOriginal = true;
//             if (lc->isOuter && !lc->offsets.empty()) {
//                 drawOriginal = false;
//             }

//             // 绘制原始轮廓（灰色点线）
//             if (drawOriginal && !lc->original.empty()) {
//                 for (size_t i = 0; i < lc->original.size() - 1; ++i) {
//                     gp_Pnt p1(lc->original[i].x, lc->original[i].y, z);
//                     gp_Pnt p2(lc->original[i+1].x, lc->original[i+1].y, z);
//                     Handle(AIS_Shape) seg = createLineWithStyle(p1, p2, Quantity_NOC_GRAY50, Aspect_TOL_DOT, 1.0);
//                     seg->SetLocalTransformation(finalTransform);
//                     m_context->Display(seg, Standard_False);
//                     m_sliceShapes.push_back(seg);
//                 }
//             }

//             // 绘制偏置环（仅内部片段，offsets）
//             for (size_t offIdx = 0; offIdx < lc->offsets.size(); ++offIdx) {
//                 const Contour2D& cont = lc->offsets[offIdx];
//                 if (cont.size() < 2) continue;

//                 // 毛坯边界检查
//                 if (useStockBounds) {
//                     double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
//                     for (const auto& pt : cont) {
//                         minX = std::min(minX, pt.x);
//                         maxX = std::max(maxX, pt.x);
//                         minY = std::min(minY, pt.y);
//                         maxY = std::max(maxY, pt.y);
//                     }
//                     if (maxX < stockLeft || minX > stockRight || maxY < stockBottom || minY > stockTop)
//                         continue;
//                 }

//                 // 颜色和线型
//                 Quantity_Color color = groupColor;
//                 if (!lc->isOuter && offIdx > 0) {
//                     double factor = 1.0 - offIdx * 0.05;
//                     factor = std::max(0.3, factor);
//                     color = Quantity_Color(groupColor.Red() * factor,
//                                            groupColor.Green() * factor,
//                                            groupColor.Blue() * factor,
//                                            Quantity_TOC_RGB);
//                 }
//                 Aspect_TypeOfLine style = (offIdx == 0) ? Aspect_TOL_SOLID : Aspect_TOL_DASH;
//                 double width = (offIdx == 0) ? 1.8 : 1.2;

//                 // 绘制线段
//                 for (size_t i = 0; i < cont.size() - 1; ++i) {
//                     gp_Pnt p1(cont[i].x, cont[i].y, z);
//                     gp_Pnt p2(cont[i+1].x, cont[i+1].y, z);
//                     Handle(AIS_Shape) seg = createLineWithStyle(p1, p2, color, style, width);
//                     seg->SetLocalTransformation(finalTransform);
//                     m_context->Display(seg, Standard_False);
//                     m_sliceShapes.push_back(seg);
//                 }

//                 // 文本标签（带区域字母和全局连续序号）
//                 if (!cont.empty()) {
//                     gp_Pnt labelPos(cont[0].x, cont[0].y, z);
//                     labelPos.Transform(finalTransform);

//                     // 基本标签：indexInGroup-groupId.offIdx
//                     std::string labelStr = std::to_string(gc.indexInGroup) + "-" +
//                                            std::to_string(gc.groupId) + "." +
//                                            std::to_string(offIdx);

//                     // 获取区域 ID 并转换为字母（0→A，1→B…）
//                     int regionId = 0;
//                     if (incircleInfo) {
//                         for (const auto& info : *incircleInfo) {
//                             if (std::abs(info.z - z) < 1e-6 &&
//                                 info.contourId == lc->id &&
//                                 info.passIndex == static_cast<int>(offIdx)) {
//                                 regionId = info.regionId;
//                                 break;
//                             }
//                         }
//                     }
//                     if (regionId < 0) regionId = 0;
//                     char regionLetter = static_cast<char>('A' + regionId);

//                     // 全局连续序号（跨区域）
//                     int globalIdx = 0;
//                     auto itGroup = groupGlobalIndexMap.find(gc.groupId);
//                     if (itGroup != groupGlobalIndexMap.end()) {
//                         auto itIdx = itGroup->second.find({lc->id, static_cast<int>(offIdx)});
//                         if (itIdx != itGroup->second.end()) {
//                             globalIdx = itIdx->second;
//                         }
//                     }

//                     labelStr += "-" + std::string(1, regionLetter) + "-" + std::to_string(globalIdx);

//                     TCollection_ExtendedString text(labelStr.c_str());
//                     Handle(AIS_TextLabel) textLabel = new AIS_TextLabel();
//                     textLabel->SetText(text);
//                     textLabel->SetPosition(labelPos);
//                     textLabel->SetHeight(1.2);
//                     textLabel->SetColor(Quantity_NOC_BLACK);
//                     m_context->Display(textLabel, Standard_False);
//                     // 存入标签映射，供路径打印使用
//                     std::tuple<int, int, double, int> key{
//                         lc->id,
//                         static_cast<int>(offIdx),
//                         z,
//                         regionId
//                     };
//                     m_ringLabels[key] = labelStr;
//                     m_textLabels.push_back(textLabel);
//                 }
//             }
//         }
//     }

//     setAuxiliaryVisibility(m_showAuxiliary);
//     updateView();
// }

void ObjectOcctView::drawContourGroups(const ContourGroups& groups,
                                       const std::shared_ptr<DetectedObject>& obj,
                                       const gp_Trsf& coordTransform,
                                       const std::vector<MaxIncircleInfo>* incircleInfo)
{
    clearSlices();

    // 计算从对象坐标系到世界坐标系的最终变换
    gp_Trsf blankTrsf;
    double angleRad = obj->getAngle() * M_PI / 180.0;
    if (std::abs(angleRad) > 1e-6) {
        gp_Trsf rot;
        rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angleRad);
        blankTrsf = rot;
    }
    gp_Trsf trans;
    trans.SetTranslation(gp_Vec(obj->getCenter().x, obj->getCenter().y, obj->getCenter().z));
    blankTrsf = trans * blankTrsf;
    gp_Trsf finalTransform = coordTransform * blankTrsf;

    // 获取毛坯矩形边界（如果有效）
    bool useStockBounds = m_stockBoundsValid;
    double stockLeft = m_stockLeft, stockRight = m_stockRight;
    double stockBottom = m_stockBottom, stockTop = m_stockTop;

    // ========== 构建“层内跨区域连续”偏置序号（每个 groupId 内，按层独立，从0开始）==========
    struct RingKey {
        int contourId;
        int offIdx;
        int regionId;
    };

    // groupId -> z -> [(contourId,offIdx) -> index]
    std::map<int, std::map<double, std::map<std::pair<int,int>, int>>> groupLayerIndexMap;

    {
        // 收集所有偏置环，按 (groupId, z) 分组
        std::map<int, std::map<double, std::vector<RingKey>>> groupLayerRings;

        for (const auto& group : groups.groups) {
            for (const auto& gc : group) {
                const LayerContour* lc = gc.contour;
                if (!lc) continue;
                double z = gc.z;
                for (size_t offIdx = 0; offIdx < lc->offsets.size(); ++offIdx) {
                    int regionId = 0;
                    if (incircleInfo) {
                        for (const auto& info : *incircleInfo) {
                            if (std::abs(info.z - z) < 1e-6 &&
                                info.contourId == lc->id &&
                                info.passIndex == static_cast<int>(offIdx)) {
                                regionId = info.regionId;
                                break;
                            }
                        }
                    }
                    groupLayerRings[gc.groupId][z].push_back({lc->id, static_cast<int>(offIdx), regionId});
                }
            }
        }

        // 对每个 (groupId, z) 层，跨区域连续编号
        for (auto& [groupId, layerMap] : groupLayerRings) {
            for (auto& [z, rings] : layerMap) {
                // 按区域分组
                std::map<int, std::vector<RingKey>> regionMap;
                for (const auto& r : rings) {
                    regionMap[r.regionId].push_back(r);
                }

                // 区域排列顺序：独占区域 (regionId > 0) 按 ID 升序，共享区域 A (regionId=0) 放在最后
                std::vector<int> regionOrder;
                for (const auto& [rid, _] : regionMap) {
                    if (rid > 0) regionOrder.push_back(rid);
                }
                std::sort(regionOrder.begin(), regionOrder.end());
                if (regionMap.count(0)) regionOrder.push_back(0);

                int globalIdx = 0;  // 当前层的全局序号，跨区域自增
                for (int rid : regionOrder) {
                    auto& vec = regionMap[rid];
                    // 区域内按 offIdx 降序（内环在前）
                    std::sort(vec.begin(), vec.end(), [](const RingKey& a, const RingKey& b) {
                        return a.offIdx > b.offIdx;
                    });
                    for (const auto& rk : vec) {
                        groupLayerIndexMap[groupId][z][{rk.contourId, rk.offIdx}] = globalIdx++;
                    }
                }
            }
        }
    }
    // ========================================================================================

    for (size_t gIdx = 0; gIdx < groups.groups.size(); ++gIdx) {
        const auto& group = groups.groups[gIdx];
        Quantity_Color groupColor = groups.groupColors[gIdx];

        for (const auto& gc : group) {
            const LayerContour* lc = gc.contour;
            if (!lc) continue;
            double z = gc.z;

            // 外轮廓：只有没有偏置环时才显示原始轮廓
            bool drawOriginal = true;
            if (lc->isOuter && !lc->offsets.empty()) {
                drawOriginal = false;
            }

            // 绘制原始轮廓（灰色点线）
            if (drawOriginal && !lc->original.empty()) {
                for (size_t i = 0; i < lc->original.size() - 1; ++i) {
                    gp_Pnt p1(lc->original[i].x, lc->original[i].y, z);
                    gp_Pnt p2(lc->original[i+1].x, lc->original[i+1].y, z);
                    Handle(AIS_Shape) seg = createLineWithStyle(p1, p2, Quantity_NOC_GRAY50, Aspect_TOL_DOT, 1.0);
                    seg->SetLocalTransformation(finalTransform);
                    m_context->Display(seg, Standard_False);
                    m_sliceShapes.push_back(seg);
                }
            }

            // 绘制偏置环（仅内部片段，offsets）
            for (size_t offIdx = 0; offIdx < lc->offsets.size(); ++offIdx) {
                const Contour2D& cont = lc->offsets[offIdx];
                if (cont.size() < 2) continue;

                // 毛坯边界检查
                if (useStockBounds) {
                    double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
                    for (const auto& pt : cont) {
                        minX = std::min(minX, pt.x);
                        maxX = std::max(maxX, pt.x);
                        minY = std::min(minY, pt.y);
                        maxY = std::max(maxY, pt.y);
                    }
                    if (maxX < stockLeft || minX > stockRight || maxY < stockBottom || minY > stockTop)
                        continue;
                }

                // 颜色和线型
                Quantity_Color color = groupColor;
                if (!lc->isOuter && offIdx > 0) {
                    double factor = 1.0 - offIdx * 0.05;
                    factor = std::max(0.3, factor);
                    color = Quantity_Color(groupColor.Red() * factor,
                                           groupColor.Green() * factor,
                                           groupColor.Blue() * factor,
                                           Quantity_TOC_RGB);
                }
                Aspect_TypeOfLine style = (offIdx == 0) ? Aspect_TOL_SOLID : Aspect_TOL_DASH;
                double width = (offIdx == 0) ? 1.8 : 1.2;

                // 绘制线段
                for (size_t i = 0; i < cont.size() - 1; ++i) {
                    gp_Pnt p1(cont[i].x, cont[i].y, z);
                    gp_Pnt p2(cont[i+1].x, cont[i+1].y, z);
                    Handle(AIS_Shape) seg = createLineWithStyle(p1, p2, color, style, width);
                    seg->SetLocalTransformation(finalTransform);
                    m_context->Display(seg, Standard_False);
                    m_sliceShapes.push_back(seg);
                }

                // 文本标签（区域字母 + 层内连续全局序号）
                if (!cont.empty()) {
                    gp_Pnt labelPos(cont[0].x, cont[0].y, z);
                    labelPos.Transform(finalTransform);

                    // 基本标签：indexInGroup-groupId.offIdx
                    std::string labelStr = std::to_string(gc.indexInGroup) + "-" +
                                           std::to_string(gc.groupId) + "." +
                                           std::to_string(offIdx);

                    // 获取区域 ID 并转换为字母（0→A，1→B…）
                    int regionId = 0;
                    if (incircleInfo) {
                        for (const auto& info : *incircleInfo) {
                            if (std::abs(info.z - z) < 1e-6 &&
                                info.contourId == lc->id &&
                                info.passIndex == static_cast<int>(offIdx)) {
                                regionId = info.regionId;
                                break;
                            }
                        }
                    }
                    if (regionId < 0) regionId = 0;
                    char regionLetter = static_cast<char>('A' + regionId);

                    // 层内全局连续序号（每层从0开始）
                    int globalIdx = 0;
                    auto itGroup = groupLayerIndexMap.find(gc.groupId);
                    if (itGroup != groupLayerIndexMap.end()) {
                        auto itLayer = itGroup->second.find(z);
                        if (itLayer != itGroup->second.end()) {
                            auto itIdx = itLayer->second.find({lc->id, static_cast<int>(offIdx)});
                            if (itIdx != itLayer->second.end()) {
                                globalIdx = itIdx->second;
                            }
                        }
                    }

                    labelStr += "-" + std::string(1, regionLetter) + "-" + std::to_string(globalIdx);

                    TCollection_ExtendedString text(labelStr.c_str());
                    Handle(AIS_TextLabel) textLabel = new AIS_TextLabel();
                    textLabel->SetText(text);
                    textLabel->SetPosition(labelPos);
                    textLabel->SetHeight(1.2);
                    textLabel->SetColor(Quantity_NOC_BLACK);
                    m_context->Display(textLabel, Standard_False);
                    m_textLabels.push_back(textLabel);
                }
            }
        }
    }

    // 标签空间去重
    // const double labelMergeDist = 2.0;
    // std::vector<Handle(AIS_TextLabel)> keptLabels;
    // for (const auto& label : m_textLabels) {
    //     bool tooClose = false;
    //     for (const auto& kept : keptLabels) {
    //         if (label->Position().Distance(kept->Position()) < labelMergeDist) {
    //             m_context->Remove(label, Standard_False);
    //             tooClose = true;
    //             break;
    //         }
    //     }
    //     if (!tooClose) keptLabels.push_back(label);
    // }
    // m_textLabels = keptLabels;

    setAuxiliaryVisibility(m_showAuxiliary);
    updateView();
}

void ObjectOcctView::drawRapidPaths(const std::vector<Point3D>& rapidpaths,
                                    const std::shared_ptr<DetectedObject>& obj)
{
    if (rapidpaths.size() < 2) return;
    Quantity_Color rapidColor(1.0, 0.0, 0.0, Quantity_TOC_RGB);

    gp_Trsf blankTrsf;
    double angleRad = obj->getAngle() * M_PI / 180.0;
    if (std::abs(angleRad) > 1e-6) {
        gp_Trsf rot;
        rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angleRad);
        blankTrsf = rot;
    }
    gp_Trsf trans;
    trans.SetTranslation(gp_Vec(obj->getCenter().x, obj->getCenter().y, obj->getCenter().z));
    blankTrsf = trans * blankTrsf;

    for (size_t i = 0; i < rapidpaths.size() - 1; ++i) {
        gp_Pnt p1(rapidpaths[i].x, rapidpaths[i].y, rapidpaths[i].z);
        gp_Pnt p2(rapidpaths[i+1].x, rapidpaths[i+1].y, rapidpaths[i+1].z);
        Handle(AIS_Shape) seg = createLineWithStyle(p1, p2, rapidColor, Aspect_TOL_SOLID, 2.0);
        seg->SetLocalTransformation(blankTrsf);
        m_context->Display(seg, Standard_False);
        m_pathShapes.push_back(seg);
    }
    updateView();
}
// 在文件顶部需要包含的头文件（如果之前没包含）
static gp_Pnt toWorld(const MaxIncircleInfo& info, const gp_Trsf& trsf) {
    gp_Pnt p(info.contactPoints[0].x, info.contactPoints[0].y, info.z);
    p.Transform(trsf);
    return p;
}
// 构建一个中心位于原点、尺寸为 (lx,ly,lz) 的实心方体
static TopoDS_Shape BuildBox(double lx, double ly, double lz)
{
    // 八个顶点（中心在原点）
    gp_Pnt p1(-lx/2, -ly/2, -lz/2);
    gp_Pnt p2( lx/2, -ly/2, -lz/2);
    gp_Pnt p3( lx/2,  ly/2, -lz/2);
    gp_Pnt p4(-lx/2,  ly/2, -lz/2);
    gp_Pnt p5(-lx/2, -ly/2,  lz/2);
    gp_Pnt p6( lx/2, -ly/2,  lz/2);
    gp_Pnt p7( lx/2,  ly/2,  lz/2);
    gp_Pnt p8(-lx/2,  ly/2,  lz/2);

    // 辅助：用四个点创建闭合多边形并生成面
    auto makeFace = [](const gp_Pnt& a, const gp_Pnt& b,
                       const gp_Pnt& c, const gp_Pnt& d) -> TopoDS_Face {
        BRepBuilderAPI_MakePolygon poly;
        poly.Add(a); poly.Add(b); poly.Add(c); poly.Add(d);
        poly.Close();
        TopoDS_Wire wire = poly.Wire();
        BRepBuilderAPI_MakeFace faceMaker(wire);
        return faceMaker.Face();
    };

    // 六个面（注意法向方向应一致指向外侧）
    TopoDS_Face faces[6];
    faces[0] = makeFace(p1, p2, p3, p4); // 底面 (Z-)
    faces[1] = makeFace(p5, p6, p7, p8); // 顶面 (Z+)
    faces[2] = makeFace(p1, p2, p6, p5); // 前面 (Y-)
    faces[3] = makeFace(p2, p3, p7, p6); // 右面 (X+)
    faces[4] = makeFace(p3, p4, p8, p7); // 后面 (Y+)
    faces[5] = makeFace(p4, p1, p5, p8); // 左面 (X-)

    // 将所有面放入一个 Compound
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (int i = 0; i < 6; ++i) {
        builder.Add(compound, faces[i]);
    }

    return compound;
}

void ObjectOcctView::displayStockShape(int stockType,
                                       double length, double width, double height,
                                       double diameter, double cylHeight,
                                       double centerX, double centerY, double centerZ,
                                       double angle,
                                       const gp_Trsf& coordTransform)
{
    qDebug() << "displayStockShape called: type=" << stockType
             << "L=" << length << "W=" << width << "H=" << height
             << "Dia=" << diameter << "CylH=" << cylHeight
             << "Center=(" << centerX << "," << centerY << "," << centerZ << ")"
             << "Angle=" << angle;

    // 1. 移除旧毛坯
    if (!m_stockShape.IsNull()) {
        m_context->Remove(m_stockShape, Standard_False);
        m_stockShape.Nullify();
    }

    // 2. 根据类型创建 TopoDS_Shape
    TopoDS_Shape stockTopo;
    if (stockType == 0) {
        if (length <= 0 || width <= 0 || height <= 0) {
            qWarning() << "Invalid box dimensions";
            return;
        }
        stockTopo = BuildBox(length, width, height);
        if (stockTopo.IsNull()) {
            qWarning() << "BuildBox failed";
            return;
        }
    } else {
        // 圆柱创建保持不变
        if (diameter <= 0 || cylHeight <= 0) {
            qWarning() << "Invalid cylinder dimensions";
            return;
        }
        BRepPrimAPI_MakeCylinder cylMaker(gp_Ax2(gp_Pnt(0,0,-cylHeight/2.0), gp_Dir(0,0,1)),
                                          diameter / 2.0, cylHeight);
        if (!cylMaker.IsDone()) {
            qWarning() << "Cylinder creation failed";
            return;
        }
        stockTopo = cylMaker.Shape();
    }

    // 3. 创建 AIS_Shape，半透明灰色
    Handle(AIS_Shape) shape = new AIS_Shape(stockTopo);
    // 方案二：自定义浅灰（如 RGB 0.85, 0.85, 0.85）
    shape->SetColor(Quantity_Color(0.85, 0.85, 0.85, Quantity_TOC_RGB));

    shape->SetTransparency(0.9);   // 可适当增加透明度，更通透

    // 4. 毛坯本地变换（旋转+平移）
    gp_Trsf localTrsf;
    if (std::abs(angle) > 1e-6) {
        gp_Trsf rot;
        rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angle * M_PI / 180.0);
        localTrsf = rot;
    }
    gp_Trsf trans;
    trans.SetTranslation(gp_Vec(centerX, centerY, centerZ));
    localTrsf = trans * localTrsf;

    // 5. 组合坐标系变换并应用
    gp_Trsf finalTrsf = coordTransform * localTrsf;
    shape->SetLocalTransformation(finalTrsf);

    // 6. 显示
    m_context->Display(shape, Standard_False);
    m_context->SetDisplayMode(shape, AIS_Shaded, Standard_False);
    m_stockShape = shape;


    m_view->FitAll();
    m_view->Redraw();
    m_context->UpdateCurrentViewer();
    updateView();
}
Handle(AIS_Shape) ObjectOcctView::createLineWithStyle(const gp_Pnt& from, const gp_Pnt& to,
                                                      const Quantity_Color& color,
                                                      Aspect_TypeOfLine lineStyle,
                                                      Standard_Real width)
{
    BRepBuilderAPI_MakeEdge edgeMaker(from, to);
    if (!edgeMaker.IsDone()) return nullptr;
    TopoDS_Edge edge = edgeMaker.Edge();
    Handle(AIS_Shape) shape = new AIS_Shape(edge);
    Handle(Prs3d_LineAspect) lineAspect = new Prs3d_LineAspect(color, lineStyle, width);
    shape->Attributes()->SetWireAspect(lineAspect);
    shape->SetWidth(width);
    shape->SetDisplayMode(AIS_WireFrame);
    return shape;
}

Handle(AIS_Shape) ObjectOcctView::createLineShape(const gp_Pnt& p1, const gp_Pnt& p2,
                                                  const Quantity_Color& color,
                                                  Aspect_TypeOfLine lineStyle,
                                                  Standard_Real width)
{
    return createLineWithStyle(p1, p2, color, lineStyle, width);
}

void ObjectOcctView::clearSlices()
{
    for (const auto& shape : m_sliceShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_sliceShapes.clear();
    for (const auto& label : m_textLabels) {
        m_context->Remove(label, Standard_False);
    }
    m_textLabels.clear();
    updateView();
}

void ObjectOcctView::clearPaths()
{
    for (const auto& shape : m_pathShapes) {
        m_context->Remove(shape, false);
    }
    m_pathShapes.clear();
    updateView();
}

void ObjectOcctView::clearAll()
{
    clearPaths();
    if (!m_stlShape.IsNull()) {
        m_context->Remove(m_stlShape, false);
        m_stlShape.Nullify();
    }
    for (const auto& shape : m_detectedObjectShapes) {
        m_context->Remove(shape, false);
    }
    m_detectedObjectShapes.clear();
    hideCoordinateSystem();
    clearSlices();
    // 清除毛坯
    if (!m_stockShape.IsNull()) {
        m_context->Remove(m_stockShape, Standard_False);
        m_stockShape.Nullify();
    }
    // ★ 清除内切圆相关形状
    for (auto& shape : m_incircleShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_incircleShapes.clear();
    for (auto& shape : m_incirclePathShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_incirclePathShapes.clear();
    for (auto& shape : m_connectionShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_connectionShapes.clear();
    for (auto& shape : m_contourPointShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_contourPointShapes.clear();
    for (auto& shape : m_outerPathShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_outerPathShapes.clear();
    updateView();
}

void ObjectOcctView::setShowPaths(bool show)
{
    m_showPaths = show;
    for (const auto& shape : m_pathShapes) {
        if (show)
            m_context->Display(shape, false);
        else
            m_context->Erase(shape, false);
    }
    update();
}

void ObjectOcctView::updateView()
{
    if (!m_view.IsNull()) {
        occtWindow->requestUpdate();
    }
}

void ObjectOcctView::setAuxiliaryVisibility(bool visible)
{
    m_showAuxiliary = visible;
    for (const auto& shape : m_sliceShapes) {
        if (visible)
            m_context->Display(shape, Standard_False);
        else
            m_context->Erase(shape, Standard_False);
    }
    for (const auto& label : m_textLabels) {
        if (visible)
            m_context->Display(label, Standard_False);
        else
            m_context->Erase(label, Standard_False);
    }
    occtWindow->requestUpdate();
}

// ================= 事件过滤器 =================
bool ObjectOcctView::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == container) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            mousePressEvent(mouseEvent);
            // 中键、右键操作由上层完全处理，不传递给底层
            if (mouseEvent->button() == Qt::MiddleButton || mouseEvent->button() == Qt::RightButton) {
                return true;
            }
            // 左键框选也由上层处理
            return true;
        }
        case QEvent::MouseMove:
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            mouseMoveEvent(mouseEvent);
            // 如果正在旋转、平移或框选，完全拦截
            if (m_isRotating || m_isPanning || m_isSelecting) {
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            mouseReleaseEvent(mouseEvent);
            if (mouseEvent->button() == Qt::MiddleButton || mouseEvent->button() == Qt::RightButton) {
                return true;
            }
            return true;
        }
        case QEvent::Wheel:
        {
            QWheelEvent* wEvent = static_cast<QWheelEvent*>(event);
            wheelEvent(wEvent);
            return true;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}
void ObjectOcctView::mousePressEvent(QMouseEvent* event)
{
    if (m_view.IsNull()) return;

    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton) {
        m_selectionStart = event->pos();
        m_isSelecting = true;
        m_selectionRect = QRect(m_selectionStart, QSize());
        update();
    }
    else if (event->button() == Qt::RightButton) {
        if (event->buttons() & Qt::MiddleButton) {
            m_isPanning = true;
        }
        // 右键单击切换视图，由 release 处理
    }
    else if (event->button() == Qt::MiddleButton) {
        if (event->buttons() & Qt::RightButton) {
            m_isPanning = true;
        } else {
            m_isRotating = true;
            // 关键修改：强制关闭降级渲染，保持模型不透明
            m_view->SetComputedMode(Standard_False);
            setAuxiliaryVisibility(false);
            m_view->StartRotation(event->pos().x(), event->pos().y());
        }
    }
}
void ObjectOcctView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_view.IsNull()) return;

    if (m_isSelecting) {
        m_selectionRect = QRect(m_selectionStart, event->pos()).normalized();
        update();
        return;
    }

    // 旋转：优先级最高，且只在中键单独按下时执行
    if (m_isRotating && (event->buttons() & Qt::MiddleButton)) {
        m_view->Rotation(event->pos().x(), event->pos().y());
        occtWindow->requestUpdate();
    }
    else if (m_isPanning) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();
        m_view->Pan(dx, -dy);
        occtWindow->requestUpdate();
    }
    else if (event->buttons() == Qt::RightButton) {
        // 仅当右键单独按下且不是旋转/平移状态才执行
        if (!m_isRotating && !m_isPanning) {
            int dx = event->pos().x() - m_lastMousePos.x();
            int dy = event->pos().y() - m_lastMousePos.y();
            m_view->Pan(dx, -dy);
            occtWindow->requestUpdate();
        }
    }

    m_lastMousePos = event->pos();
}
void ObjectOcctView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        m_isSelecting = false;
        if (m_selectionRect.width() > 5 && m_selectionRect.height() > 5) {
            handleAreaSelection(m_selectionRect);
        }
        m_selectionRect = QRect();
        update();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        m_isRotating = false;
        setAuxiliaryVisibility(true);
        // 确保旋转结束后仍然保持完全渲染
        m_view->SetComputedMode(Standard_False);
        occtWindow->requestUpdate();
    }
    if (event->button() == Qt::RightButton) {
        m_isPanning = false;
        if (!m_isRotating && !m_isSelecting && !(event->buttons() & Qt::MiddleButton)) {
            occtWindow->topView();
        }
    }

    // 双击或状态复位时同样加固
    if (event->buttons() == Qt::NoButton) {
        m_isPanning = false;
        m_isRotating = false;
        m_view->SetComputedMode(Standard_False);
    }
}
void ObjectOcctView::wheelEvent(QWheelEvent* event)
{
    if (m_view.IsNull()) return;

    // 获取鼠标在视图中的位置
    QPoint pos = event->position().toPoint();

    // 计算缩放因子
    double scaleFactor = (event->angleDelta().y() > 0) ? 0.9 : 1.1;  // 更平滑的缩放步长

    // 将缩放中心设置为鼠标位置
    m_view->StartZoomAtPoint(pos.x(), pos.y());
    m_view->ZoomAtPoint(pos.x(), pos.y(), pos.x(), pos.y() * scaleFactor);

    occtWindow->requestUpdate();
}

void ObjectOcctView::paintEvent(QPaintEvent* event)
{
    // 仅当需要时重绘 OCCT（由 requestUpdate 触发）
    if (!m_view.IsNull()) {
        m_view->Redraw();
    }

    // 绘制框选矩形（仅当正在框选时）
    if (m_isSelecting && !m_selectionRect.isNull()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(QColor(100, 150, 255), 1, Qt::DashLine));
        painter.setBrush(QColor(100, 150, 255, 50));
        painter.drawRect(m_selectionRect);
    }
}
void ObjectOcctView::resizeEvent(QResizeEvent* event)
{
    if (!m_view.IsNull()) {
        m_view->MustBeResized();
    }
    QWidget::resizeEvent(event);
}

void ObjectOcctView::handleAreaSelection(const QRect& rect)
{
    if (m_view.IsNull()) return;

    Standard_Integer winWidth, winHeight;
    m_view->Window()->Size(winWidth, winHeight);

    int x1 = rect.left();
    int y1 = winHeight - rect.bottom();
    int x2 = rect.right();
    int y2 = winHeight - rect.top();

    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    occtWindow->zoomToRectangle(x1, y1, x2, y2);
}

// ================== 辅助函数（放在文件任意全局区）==================
static gp_Pnt closestPointOnRing(const std::vector<gp_Pnt>& ring, const gp_Pnt& target)
{
    double bestDist = std::numeric_limits<double>::max();
    gp_Pnt best = ring.front();
    for (const auto& p : ring) {
        double dx = p.X() - target.X();
        double dy = p.Y() - target.Y();
        double dz = p.Z() - target.Z();
        double d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < bestDist) {
            bestDist = d2;
            best = p;
        }
    }
    return best;
}

void ObjectOcctView::clearMaxIncircles() {
    for (auto& shape : m_incircleShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_incircleShapes.clear();

    for (auto& shape : m_incirclePathShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_incirclePathShapes.clear();

    for (auto& shape : m_outerIncirclePathShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_outerIncirclePathShapes.clear();
    for (auto& shape : m_contourPointShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_contourPointShapes.clear();


    updateView();
}

// ==================点绘制==================

void ObjectOcctView::displayMaxIncircles(const std::vector<MaxIncircleInfo>& circles,
                                         const gp_Trsf& transform)
{
    Handle(AIS_InteractiveContext) ctx = getContext();
    if (ctx.IsNull()) return;

    // -------- 去重：每个 (contourId, passIndex, z, isOuter) 只保留第一个 --------
    std::vector<MaxIncircleInfo> uniqueCircles;
    std::set<std::tuple<int, int, double, bool>> seenKeys;
    for (const auto& info : circles) {
        auto key = std::make_tuple(info.contourId, info.passIndex, info.z, info.isOuter);
        if (seenKeys.insert(key).second) {
            uniqueCircles.push_back(info);
        }
    }

    const double sphereRadius = 0.3;
    for (const auto& info : uniqueCircles) {
        // 颜色规则：
        //   外轮廓：内凹环(绿/紫红) 或 普通外环(蓝)
        //   内轮廓：最内层(紫红) 或 其他(绿)
        Quantity_Color color;
        if (info.isOuter) {
            if (info.isConcave) {
                color = info.isInnermost ? Quantity_Color(1.0, 0.0, 1.0, Quantity_TOC_RGB)   // 紫红
                                         : Quantity_NOC_GREEN;                               // 绿
            } else {
                color = Quantity_NOC_BLUE;   // 蓝（普通外环 + 边界残片）
            }
        } else {
            color = info.isInnermost ? Quantity_Color(1.0, 0.0, 1.0, Quantity_TOC_RGB)
                                     : Quantity_NOC_GREEN;
        }

        // 每个 info 只取第一个接触点（computeIncircles 已确保 contactPoints 仅含一个点）
        const std::vector<Point2D>& pts = info.contactPoints;
        if (pts.empty()) continue;

        // 外轮廓强制只绘制第一个点（安全措施）
        std::vector<Point2D> singlePt;
        const std::vector<Point2D>* drawPts = &pts;
        if (info.isOuter && !pts.empty()) {
            singlePt.push_back(pts.front());
            drawPts = &singlePt;
        }

        // 为每个点绘制一个半透明小球
        for (const auto& pt2d : *drawPts) {
            gp_Pnt worldPt(pt2d.x, pt2d.y, info.z);
            worldPt.Transform(transform);
            TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(worldPt, sphereRadius);
            Handle(AIS_Shape) aisSphere = new AIS_Shape(sphere);
            aisSphere->SetColor(color);
            aisSphere->SetMaterial(Graphic3d_NOM_PLASTIC);
            aisSphere->SetTransparency(0.0);
            ctx->Display(aisSphere, Standard_True);
            m_incircleShapes.push_back(aisSphere);
        }
    }
    ctx->UpdateCurrentViewer();
}
void ObjectOcctView::displayIncirclePath(
    const std::vector<MaxIncircleInfo>& allCircles,
    const gp_Trsf& transform,
    double safeZ,
    double maxDirectDist,
    const std::map<int, int>& contourIdToGroupId,
    const gp_Pnt* prevOuterEnd)
{
    Handle(AIS_InteractiveContext) ctx = getContext();
    if (ctx.IsNull() || allCircles.empty()) return;

    // 清除旧的内轮廓路径
    for (auto& shape : m_incirclePathShapes)
        ctx->Remove(shape, Standard_False);
    m_incirclePathShapes.clear();

    // 仅保留内轮廓 (isOuter == false) 且包含切点
    std::vector<const MaxIncircleInfo*> innerPts;
    for (const auto& c : allCircles) {
        if (!c.isOuter && !c.contactPoints.empty())
            innerPts.push_back(&c);
    }
    if (innerPts.empty()) {
        qDebug() << "内轮廓内切圆点数为0，不生成内轮廓路径";
        return;
    }

    // ---------- 基础几何构建工具 ----------
    auto makeLine = [](const gp_Pnt& a, const gp_Pnt& b) -> TopoDS_Edge {
        if (a.Distance(b) < 1e-9) return TopoDS_Edge();
        GC_MakeSegment seg(a, b);
        return seg.IsDone() ? BRepBuilderAPI_MakeEdge(seg.Value()) : TopoDS_Edge();
    };

    enum SegType { UP = 0, TRAVEL = 1, DOWN = 2, OTHER = 3 };
    std::vector<TopoDS_Edge> edgesUp, edgesTravel, edgesDown, edgesOther;

    auto addSeg = [&](const TopoDS_Edge& e, SegType t) {
        if (!e.IsNull()) {
            switch (t) {
            case UP:     edgesUp.push_back(e);      break;
            case TRAVEL: edgesTravel.push_back(e);  break;
            case DOWN:   edgesDown.push_back(e);    break;
            default:     edgesOther.push_back(e);   break;
            }
        }
    };
    auto addLine = [&](const gp_Pnt& a, const gp_Pnt& b, SegType t) {
        addSeg(makeLine(a, b), t);
    };
    auto addUpTravelDown = [&](const gp_Pnt& from, const gp_Pnt& to) {
        gp_Pnt up(from.X(), from.Y(), safeZ);
        gp_Pnt down(to.X(), to.Y(), safeZ);
        addLine(from, up, UP);
        addLine(up, down, TRAVEL);
        addLine(down, to, DOWN);
    };

    auto toWorld = [&](const MaxIncircleInfo& info) -> gp_Pnt {
        const Point2D& pt = info.contactPoints.front();
        return gp_Pnt(pt.x, pt.y, info.z).Transformed(transform);
    };

    // ---------- 按 groupId 组织数据 ----------
    std::map<int, std::map<int, std::vector<const MaxIncircleInfo*>>> groupRegions;
    for (auto* p : innerPts) {
        int gid = 0;
        auto it = contourIdToGroupId.find(p->contourId);
        if (it != contourIdToGroupId.end()) gid = it->second;
        else continue;

        groupRegions[gid][p->regionId].push_back(p);
    }

    // 每个区域内部排序：Z降序，同Z按passIndex降序（内→外，紫红点在前）
    for (auto& [gid, regionMap] : groupRegions) {
        for (auto& [rid, vec] : regionMap) {
            std::sort(vec.begin(), vec.end(),
                      [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
                          if (std::abs(a->z - b->z) > 1e-6) return a->z > b->z;
                          return a->passIndex > b->passIndex;
                      });
        }
    }

    // ---------- 获取有序的 groupId 列表 ----------
    std::vector<int> sortedGroupIds;
    for (const auto& [gid, _] : groupRegions)
        sortedGroupIds.push_back(gid);
    std::sort(sortedGroupIds.begin(), sortedGroupIds.end());

    gp_Pnt prevGroupLastPt;
    bool hasPrevGroup = false;

    for (int gid : sortedGroupIds) {
        auto& regionMap = groupRegions[gid];

        // 区域顺序：独占区域 (regionId>0) 升序，再共享区域 (regionId==0)
        std::vector<int> exclusiveIds;
        int sharedId = -1;
        for (auto& [rid, _] : regionMap) {
            if (rid > 0) exclusiveIds.push_back(rid);
            else if (rid == 0) sharedId = 0;
        }
        std::sort(exclusiveIds.begin(), exclusiveIds.end());
        std::vector<int> orderedRegions = exclusiveIds;
        if (sharedId != -1) orderedRegions.push_back(sharedId);

        if (orderedRegions.empty()) continue;

        gp_Pnt prevPt;
        int prevRid = -1;

        for (size_t regionIdx = 0; regionIdx < orderedRegions.size(); ++regionIdx) {
            int rid = orderedRegions[regionIdx];
            const auto& points = regionMap[rid];
            if (points.empty()) continue;

            gp_Pnt startPt = toWorld(*points.front());

            // ---------- 前驱连接 ----------
            if (regionIdx == 0 && gid == sortedGroupIds.front() && prevOuterEnd != nullptr) {
                // 外轮廓终点 → 第一个内腔的第一个区域（独占 B 紫红点）
                qDebug() << "=== 外轮廓终点 -> 内腔 groupId=1 独占区域B 紫红点 抬刀路径 ===";
                qDebug() << "  外轮廓终点:" << prevOuterEnd->X() << prevOuterEnd->Y() << prevOuterEnd->Z();
                qDebug() << "  内腔起点:" << startPt.X() << startPt.Y() << startPt.Z();
                addUpTravelDown(*prevOuterEnd, startPt);
            }
            else if (regionIdx == 0 && hasPrevGroup) {
                // 上一个内腔终点 → 当前内腔第一个区域起点（抬刀）
                addUpTravelDown(prevGroupLastPt, startPt);
            }
            else if (regionIdx > 0) {
                // 同一内腔的区域间连接
                if (prevRid > 0 && rid > 0) {
                    // 两个都是独占区域 → 抬刀
                    addUpTravelDown(prevPt, startPt);
                } else {
                    // 独占→共享 或 共享→共享 → 直线
                    addLine(prevPt, startPt, OTHER);
                }
            }

            // ---------- 区域内直线连接 ----------
            for (size_t i = 0; i + 1 < points.size(); ++i) {
                addLine(toWorld(*points[i]), toWorld(*points[i+1]), OTHER);
            }

            prevPt = toWorld(*points.back());
            prevRid = rid;

            if (regionIdx == orderedRegions.size() - 1) {
                prevGroupLastPt = prevPt;
                hasPrevGroup = true;
            }
        }
    }

    // ---------- 显示所有线段 ----------
    auto displayEdges = [&](const std::vector<TopoDS_Edge>& edges,
                            Quantity_Color color,
                            Aspect_TypeOfLine style) {
        if (edges.empty()) return;
        TopoDS_Compound comp;
        BRep_Builder b;
        b.MakeCompound(comp);
        for (const auto& e : edges) b.Add(comp, e);
        Handle(AIS_Shape) shape = new AIS_Shape(comp);
        shape->SetDisplayMode(0);
        shape->SetColor(color);
        Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(color, style, 2.0);
        shape->Attributes()->SetLineAspect(aspect);
        ctx->Display(shape, Standard_True);
        m_incirclePathShapes.push_back(shape);
    };

    displayEdges(edgesUp,     Quantity_NOC_PURPLE, Aspect_TOL_DASH);
    displayEdges(edgesTravel, Quantity_NOC_RED,    Aspect_TOL_DASH);
    displayEdges(edgesDown,   Quantity_NOC_YELLOW, Aspect_TOL_DASH);
    displayEdges(edgesOther,  Quantity_NOC_ORANGE, Aspect_TOL_SOLID);

    ctx->UpdateCurrentViewer();
    qDebug() << "内轮廓路径生成完成";
}

void ObjectOcctView::drawLiftTravelPlunge(const gp_Pnt& from, const gp_Pnt& to, double safeZ)
{
    if (m_view.IsNull() || m_context.IsNull()) return;

    gp_Pnt upFrom(from.X(), from.Y(), safeZ);
    gp_Pnt upTo(to.X(), to.Y(), safeZ);

    // UP: from -> upFrom  (紫色虚线)
    Handle(AIS_Shape) segUp = createLineWithStyle(from, upFrom, Quantity_NOC_PURPLE, Aspect_TOL_DASH, 2.0);
    // TRAVEL: upFrom -> upTo (红色虚线)
    Handle(AIS_Shape) segTravel = createLineWithStyle(upFrom, upTo, Quantity_NOC_RED, Aspect_TOL_DASH, 2.0);
    // DOWN: upTo -> to (黄色虚线)
    Handle(AIS_Shape) segDown = createLineWithStyle(upTo, to, Quantity_NOC_YELLOW, Aspect_TOL_DASH, 2.0);

    if (!segUp.IsNull()) {
        m_context->Display(segUp, false);
        m_connectionShapes.push_back(segUp);
    }
    if (!segTravel.IsNull()) {
        m_context->Display(segTravel, false);
        m_connectionShapes.push_back(segTravel);
    }
    if (!segDown.IsNull()) {
        m_context->Display(segDown, false);
        m_connectionShapes.push_back(segDown);
    }
    updateView();
}


void ObjectOcctView::drawOuterContourPath(const std::vector<MaxIncircleInfo>& outerCircles,
                                          const std::vector<MaxIncircleInfo>& concaveCircles,
                                          const gp_Trsf& transform,
                                          double safeZ,
                                          double maxDirectDist)
{
    Handle(AIS_InteractiveContext) ctx = getContext();
    if (ctx.IsNull()) return;

    // ---- 安全创建直线/圆弧边的辅助函数（复用同一模式） ----
    auto makeSafeLine = [](const gp_Pnt& a, const gp_Pnt& b) -> TopoDS_Edge {
        if (a.Distance(b) < 1e-9) return TopoDS_Edge();
        GC_MakeSegment segMaker(a, b);
        if (segMaker.IsDone())
            return BRepBuilderAPI_MakeEdge(segMaker.Value());
        return TopoDS_Edge();
    };

    auto addStraight = [&](const gp_Pnt& a, const gp_Pnt& b,
                           TopoDS_Compound& comp, std::vector<TopoDS_Edge>& edges) {
        TopoDS_Edge e = makeSafeLine(a, b);
        if (!e.IsNull()) {
            BRep_Builder().Add(comp, e);
            edges.push_back(e);
        }
    };

    enum EdgeType { UP, TRAVEL, DOWN, OTHER };
    auto makeCompound = [&](const std::vector<TopoDS_Edge>& edges, EdgeType type) -> Handle(AIS_Shape) {
        if (edges.empty()) return nullptr;
        TopoDS_Compound comp;
        BRep_Builder b;
        b.MakeCompound(comp);
        for (const auto& e : edges) b.Add(comp, e);

        Handle(AIS_Shape) shape = new AIS_Shape(comp);
        shape->SetDisplayMode(0);
        Quantity_Color color;
        Aspect_TypeOfLine style;
        switch (type) {
        case UP:     color = Quantity_NOC_PURPLE; style = Aspect_TOL_DASH;   break;
        case TRAVEL: color = Quantity_NOC_RED;    style = Aspect_TOL_DASH;   break;
        case DOWN:   color = Quantity_NOC_YELLOW; style = Aspect_TOL_DASH;   break;
        default:     color = Quantity_NOC_ORANGE; style = Aspect_TOL_SOLID;  break;
        }
        shape->SetColor(color);
        Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(color, style, 2.0);
        shape->Attributes()->SetLineAspect(aspect);
        return shape;
    };

    // ---------- 步骤1: 处理外轮廓蓝点（同腔体连接，可复用原逻辑） ----------
    // 这里简化：按原腔体分组连接外轮廓蓝点（仅生成 OTHER 线），保留最外侧最低点以供后续连接
    // 实际可调用修改后的 displayIncirclePath 逻辑，但为避免修改，此处仅生成基本线框。
    // （外轮廓蓝点连接已在 displayIncirclePath 中完成，此处仅复制关键部分或忽略，聚焦抬刀段）
    // 为完整起见，可注释说明：外轮廓内部连接由 displayIncirclePath 负责，本函数只处理抬刀跨段。

    // ---------- 步骤2: 外轮廓最外侧最低点 → 内凹环紫红点 ----------
    if (outerCircles.empty() || concaveCircles.empty()) return;

    // 找出外轮廓的“最外最低”偏置环上的最后一个蓝点（假设路径最后点为最外最低）
    const MaxIncircleInfo* lastOuter = &outerCircles.back();
    // 通常按层高排序后，最后元素即为最低最外，可加入更严格查找
    double lowestZ = -1e100;
    for (const auto& info : outerCircles) {
        if (info.z > lowestZ) {
            lowestZ = info.z;
            lastOuter = &info;
        }
    }
    // 取外轮廓路径的最后一个切点（假设每个info的contactPoints非空且最后一点为终点）
    if (lastOuter->contactPoints.empty()) return;
    gp_Pnt outerEndPt = gp_Pnt(lastOuter->contactPoints.back().x,
                               lastOuter->contactPoints.back().y,
                               lastOuter->z).Transformed(transform);

    // 找出内凹环的“紫红点”（假设为 concaveCircles 中最高最内侧的第一个切点）
    const MaxIncircleInfo* firstConcave = &concaveCircles.front();
    double highestZ = 1e100;
    for (const auto& info : concaveCircles) {
        if (info.z < highestZ) {
            highestZ = info.z;
            firstConcave = &info;
        }
    }
    if (firstConcave->contactPoints.empty()) return;
    gp_Pnt concaveStartPt = gp_Pnt(firstConcave->contactPoints[0].x,
                                   firstConcave->contactPoints[0].y,
                                   firstConcave->z).Transformed(transform);

    // 抬刀、平移、下刀
    std::vector<TopoDS_Edge> upEdges, travelEdges, downEdges;
    gp_Pnt upPt(outerEndPt.X(), outerEndPt.Y(), safeZ);
    gp_Pnt downPt(concaveStartPt.X(), concaveStartPt.Y(), safeZ);

    TopoDS_Compound upComp, travelComp, downComp;
    BRep_Builder().MakeCompound(upComp);
    BRep_Builder().MakeCompound(travelComp);
    BRep_Builder().MakeCompound(downComp);
    addStraight(outerEndPt, upPt, upComp, upEdges);
    addStraight(upPt, downPt, travelComp, travelEdges);
    addStraight(downPt, concaveStartPt, downComp, downEdges);

    Handle(AIS_Shape) upShape = makeCompound(upEdges, UP);
    Handle(AIS_Shape) travelShape = makeCompound(travelEdges, TRAVEL);
    Handle(AIS_Shape) downShape = makeCompound(downEdges, DOWN);
    if (!upShape.IsNull()) ctx->Display(upShape, Standard_True);
    if (!travelShape.IsNull()) ctx->Display(travelShape, Standard_True);
    if (!downShape.IsNull()) ctx->Display(downShape, Standard_True);

    // ---------- 步骤3: 内凹环绿点连接（高→低、内→外） ----------
    // 此处仅生成内凹环内部的 OTHER 线段（橙色实线）
    std::vector<TopoDS_Edge> otherEdges;
    TopoDS_Compound otherComp;
    BRep_Builder().MakeCompound(otherComp);
    for (size_t i = 0; i + 1 < concaveCircles.size(); ++i) {
        const auto& info1 = concaveCircles[i];
        const auto& info2 = concaveCircles[i+1];
        if (info1.contactPoints.empty() || info2.contactPoints.empty()) continue;
        gp_Pnt p1(info1.contactPoints.back().x, info1.contactPoints.back().y, info1.z);
        gp_Pnt p2(info2.contactPoints[0].x, info2.contactPoints[0].y, info2.z);
        p1.Transform(transform);
        p2.Transform(transform);
        addStraight(p1, p2, otherComp, otherEdges);
    }
    Handle(AIS_Shape) otherShape = makeCompound(otherEdges, OTHER);
    if (!otherShape.IsNull()) ctx->Display(otherShape, Standard_True);

    ctx->UpdateCurrentViewer();
}

// ========== 新增：外轮廓层内蓝点连接 + 凹入环抬刀连接 ==========
// void ObjectOcctView::drawOuterBluesPath(
//     const std::vector<MaxIncircleInfo>& outerCircles,
//     const gp_Trsf& transform,
//     double safeZ,
//     double maxDirectDist)
// {
//     Handle(AIS_InteractiveContext) ctx = getContext();
//     if (ctx.IsNull()) return;

//     for (auto& shape : m_outerPathShapes) {
//         ctx->Remove(shape, Standard_False);
//     }
//     m_outerPathShapes.clear();

//     auto toWorld = [&](const MaxIncircleInfo& info) -> gp_Pnt {
//         const Point2D& pt = info.contactPoints.front();
//         return gp_Pnt(pt.x, pt.y, info.z).Transformed(transform);
//     };

//     // 组织数据：按 (z, contourId)
//     std::map<double, std::map<int, std::vector<const MaxIncircleInfo*>>> layerContourMap;
//     for (const auto& c : outerCircles) {
//         if (c.contactPoints.empty()) continue;
//         layerContourMap[c.z][c.contourId].push_back(&c);
//     }

//     std::vector<TopoDS_Edge> edgesCut, edgesUp, edgesTravel, edgesDown;

//     auto makeLine = [](const gp_Pnt& a, const gp_Pnt& b) -> TopoDS_Edge {
//         if (a.Distance(b) < 1e-9) return TopoDS_Edge();
//         GC_MakeSegment seg(a, b);
//         return seg.IsDone() ? BRepBuilderAPI_MakeEdge(seg.Value()) : TopoDS_Edge();
//     };

//     auto addLiftMovePlunge = [&](const gp_Pnt& from, const gp_Pnt& to) {
//         gp_Pnt upFrom(from.X(), from.Y(), safeZ);
//         gp_Pnt upTo(to.X(), to.Y(), safeZ);
//         auto e1 = makeLine(from, upFrom);
//         auto e2 = makeLine(upFrom, upTo);
//         auto e3 = makeLine(upTo, to);
//         if (!e1.IsNull()) edgesUp.push_back(e1);
//         if (!e2.IsNull()) edgesTravel.push_back(e2);
//         if (!e3.IsNull()) edgesDown.push_back(e3);
//     };

//     for (auto itZ = layerContourMap.rbegin(); itZ != layerContourMap.rend(); ++itZ) {
//         double z = itZ->first;
//         for (auto& [cid, ptrs] : itZ->second) {
//             if (ptrs.empty()) continue;

//             // 分离蓝点与凹环点
//             std::vector<const MaxIncircleInfo*> bluePts, concavePts;
//             for (auto* p : ptrs) {
//                 if (p->isConcave) concavePts.push_back(p);
//                 else bluePts.push_back(p);
//             }

//             // 蓝点按 passIndex 升序（外→内）
//             std::sort(bluePts.begin(), bluePts.end(),
//                       [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
//                           return a->passIndex < b->passIndex;
//                       });

//             gp_Pnt prevPt;
//             bool first = true;

//             // 1. 连接所有蓝点（外→内）
//             for (auto* bp : bluePts) {
//                 gp_Pnt curr = toWorld(*bp);
//                 if (first) {
//                     prevPt = curr;
//                     first = false;
//                     continue;
//                 }
//                 double dist = prevPt.Distance(curr);
//                 if (dist <= maxDirectDist) {
//                     auto e = makeLine(prevPt, curr);
//                     if (!e.IsNull()) edgesCut.push_back(e);
//                 } else {
//                     addLiftMovePlunge(prevPt, curr);
//                 }
//                 prevPt = curr;
//             }

//             // 2. 如果有凹环点，抬刀进入最内点并连接所有凹环
//             if (!concavePts.empty()) {
//                 // 凹环点按 passIndex 降序（内→外）
//                 std::sort(concavePts.begin(), concavePts.end(),
//                           [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
//                               return a->passIndex > b->passIndex;
//                           });

//                 // 找到最内点（优先 isInnermost）
//                 const MaxIncircleInfo* innermost = nullptr;
//                 for (auto* c : concavePts) {
//                     if (c->isInnermost) { innermost = c; break; }
//                 }
//                 if (!innermost) innermost = concavePts.front();

//                 gp_Pnt entryPt = toWorld(*innermost);

//                 if (!first) {
//                     addLiftMovePlunge(prevPt, entryPt);
//                 } else {
//                     // 该层没有蓝点，从凹环开始
//                     first = false;
//                 }

//                 // 连接所有凹环点（内→外，橙色实线）
//                 for (size_t k = 0; k + 1 < concavePts.size(); ++k) {
//                     auto e = makeLine(toWorld(*concavePts[k]),
//                                       toWorld(*concavePts[k+1]));
//                     if (!e.IsNull()) edgesCut.push_back(e);
//                 }

//                 prevPt = toWorld(*concavePts.back());
//             }

//             // 注意：根据需求，凹环处理后该轮廓即结束，不再连回蓝点
//         }
//     }

//     // 显示线段
//     auto displayCompound = [&](const std::vector<TopoDS_Edge>& edges,
//                                Quantity_Color color, Aspect_TypeOfLine style) {
//         if (edges.empty()) return;
//         TopoDS_Compound comp;
//         BRep_Builder b;
//         b.MakeCompound(comp);
//         for (const auto& e : edges) b.Add(comp, e);
//         Handle(AIS_Shape) shape = new AIS_Shape(comp);
//         shape->SetDisplayMode(AIS_WireFrame);
//         shape->SetColor(color);
//         Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(color, style, 2.0);
//         shape->Attributes()->SetLineAspect(aspect);
//         ctx->Display(shape, Standard_True);
//         m_outerPathShapes.push_back(shape);
//     };

//     displayCompound(edgesCut,    Quantity_NOC_ORANGE, Aspect_TOL_SOLID);
//     displayCompound(edgesUp,     Quantity_NOC_PURPLE, Aspect_TOL_DASH);
//     displayCompound(edgesTravel, Quantity_NOC_RED,    Aspect_TOL_DASH);
//     displayCompound(edgesDown,   Quantity_NOC_YELLOW, Aspect_TOL_DASH);

//     ctx->UpdateCurrentViewer();
// }

void ObjectOcctView::drawOuterBluesPath(
    const std::vector<MaxIncircleInfo>& outerCircles,
    const gp_Trsf& transform,
    double safeZ,
    double maxDirectDist)
{
    Handle(AIS_InteractiveContext) ctx = getContext();
    if (ctx.IsNull()) return;

    for (auto& shape : m_outerPathShapes) {
        ctx->Remove(shape, Standard_False);
    }
    m_outerPathShapes.clear();

    auto toWorld = [&](const MaxIncircleInfo& info) -> gp_Pnt {
        const Point2D& pt = info.contactPoints.front();
        return gp_Pnt(pt.x, pt.y, info.z).Transformed(transform);
    };

    std::map<double, std::map<int, std::vector<const MaxIncircleInfo*>>> layerContourMap;
    for (const auto& c : outerCircles) {
        if (c.contactPoints.empty()) continue;
        layerContourMap[c.z][c.contourId].push_back(&c);
    }

    std::vector<TopoDS_Edge> edgesCut;
    struct LiftMove { gp_Pnt from; gp_Pnt to; };
    std::vector<LiftMove> liftMoves;

    auto makeLine = [](const gp_Pnt& a, const gp_Pnt& b) -> TopoDS_Edge {
        if (a.Distance(b) < 1e-9) return TopoDS_Edge();
        GC_MakeSegment seg(a, b);
        return seg.IsDone() ? BRepBuilderAPI_MakeEdge(seg.Value()) : TopoDS_Edge();
    };

    gp_Pnt lastEndPt;
    bool hasPrev = false;

    for (auto itZ = layerContourMap.rbegin(); itZ != layerContourMap.rend(); ++itZ) {
        for (auto& [cid, ptrs] : itZ->second) {
            if (ptrs.empty()) continue;

            std::vector<const MaxIncircleInfo*> bluePts, concavePts;
            for (auto* p : ptrs) {
                if (p->isConcave) concavePts.push_back(p);
                else bluePts.push_back(p);
            }

            std::sort(bluePts.begin(), bluePts.end(),
                      [](auto* a, auto* b) { return a->passIndex < b->passIndex; });
            std::sort(concavePts.begin(), concavePts.end(),
                      [](auto* a, auto* b) { return a->passIndex > b->passIndex; });

            gp_Pnt firstPt;
            bool validFirst = false;
            if (!bluePts.empty()) {
                firstPt = toWorld(*bluePts.front());
                validFirst = true;
            } else if (!concavePts.empty()) {
                auto* innermost = concavePts.front();
                for (auto* c : concavePts)
                    if (c->isInnermost) { innermost = c; break; }
                firstPt = toWorld(*innermost);
                validFirst = true;
            }
            if (!validFirst) continue;

            if (hasPrev) {
                liftMoves.push_back({lastEndPt, firstPt});
                qDebug() << "跨轮廓抬刀:" << lastEndPt.X() << lastEndPt.Y() << lastEndPt.Z()
                         << "->" << firstPt.X() << firstPt.Y() << firstPt.Z();
            }

            gp_Pnt prevPt = firstPt;
            bool firstBlue = true;
            for (auto* bp : bluePts) {
                gp_Pnt curr = toWorld(*bp);
                if (firstBlue) { firstBlue = false; continue; }
                double dist = prevPt.Distance(curr);
                if (dist <= maxDirectDist) {
                    auto e = makeLine(prevPt, curr);
                    if (!e.IsNull()) edgesCut.push_back(e);
                } else {
                    liftMoves.push_back({prevPt, curr});
                    qDebug() << "蓝点长距离抬刀:" << prevPt.X() << prevPt.Y() << prevPt.Z()
                             << "->" << curr.X() << curr.Y() << curr.Z() << "dist:" << dist;
                }
                prevPt = curr;
            }

            if (!concavePts.empty()) {
                auto* innermost = concavePts.front();
                for (auto* c : concavePts)
                    if (c->isInnermost) { innermost = c; break; }
                gp_Pnt entryPt = toWorld(*innermost);

                if (!bluePts.empty()) {
                    liftMoves.push_back({prevPt, entryPt});
                    qDebug() << "凹环入口抬刀:" << prevPt.X() << prevPt.Y() << prevPt.Z()
                             << "->" << entryPt.X() << entryPt.Y() << entryPt.Z();
                }

                for (size_t k = 0; k + 1 < concavePts.size(); ++k) {
                    auto e = makeLine(toWorld(*concavePts[k]),
                                      toWorld(*concavePts[k+1]));
                    if (!e.IsNull()) edgesCut.push_back(e);
                }
                prevPt = toWorld(*concavePts.back());
            }

            lastEndPt = prevPt;
            hasPrev = true;
        }
    }

    qDebug() << "总抬刀组数:" << liftMoves.size();

    std::vector<TopoDS_Edge> edgesUp, edgesTravel, edgesDown;
    for (size_t i = 0; i < liftMoves.size(); ++i) {
        const auto& lm = liftMoves[i];
        gp_Pnt upFrom(lm.from.X(), lm.from.Y(), safeZ);
        gp_Pnt upTo(lm.to.X(), lm.to.Y(), safeZ);

        auto eUp = makeLine(lm.from, upFrom);
        auto eTravel = makeLine(upFrom, upTo);
        auto eDown = makeLine(upTo, lm.to);

        qDebug().noquote()
            << QString("Lift[%1] from(%2,%3,%4) to(%5,%6,%7) safeZ=%8")
                   .arg(i).arg(lm.from.X(),0,'f',3).arg(lm.from.Y(),0,'f',3).arg(lm.from.Z(),0,'f',3)
                   .arg(lm.to.X(),0,'f',3).arg(lm.to.Y(),0,'f',3).arg(lm.to.Z(),0,'f',3)
                   .arg(safeZ,0,'f',3);
        qDebug() << "  eUp null?" << eUp.IsNull() << " eTravel null?" << eTravel.IsNull() << " eDown null?" << eDown.IsNull();

        if (!eUp.IsNull()) edgesUp.push_back(eUp);
        if (!eTravel.IsNull()) edgesTravel.push_back(eTravel);
        if (!eDown.IsNull()) edgesDown.push_back(eDown);
    }

    qDebug() << "最终边数: Cut" << edgesCut.size() << "Up" << edgesUp.size() << "Travel" << edgesTravel.size() << "Down" << edgesDown.size();

    auto displayCompound = [&](const std::vector<TopoDS_Edge>& edges,
                               Quantity_Color color, Aspect_TypeOfLine style,
                               Standard_Real width = 2.0) {  // 默认 2.0
        if (edges.empty()) return;
        TopoDS_Compound comp;
        BRep_Builder b;
        b.MakeCompound(comp);
        for (const auto& e : edges) b.Add(comp, e);
        Handle(AIS_Shape) shape = new AIS_Shape(comp);
        shape->SetDisplayMode(AIS_WireFrame);
        shape->SetColor(color);
        Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(color, style, width);
        shape->Attributes()->SetLineAspect(aspect);
        ctx->Display(shape, Standard_True);
        m_outerPathShapes.push_back(shape);
    };

    displayCompound(edgesCut,    Quantity_NOC_ORANGE, Aspect_TOL_SOLID);          // 橙色 2.0
    displayCompound(edgesUp,     Quantity_NOC_PURPLE, Aspect_TOL_DASH, 4.0);     // 紫色 4.0
    displayCompound(edgesTravel, Quantity_NOC_RED,    Aspect_TOL_DASH, 4.0);     // 红色 4.0
    displayCompound(edgesDown,   Quantity_NOC_YELLOW, Aspect_TOL_DASH, 4.0);     // 黄色 4.0

    ctx->UpdateCurrentViewer();
}



void ObjectOcctView::displayAllContourPoints(const ContourGroups& groups,
                                             const gp_Trsf& transform)
{
    // 清除上一轮显示的点
    for (auto& shape : m_contourPointShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_contourPointShapes.clear();

    const double pointRadius = 0.15;
    Quantity_Color white(1.0, 1.0, 1.0, Quantity_TOC_RGB);

    for (const auto& group : groups.groups) {
        for (const auto& gc : group) {
            const LayerContour* lc = gc.contour;
            if (!lc) continue;
            double z = gc.z;

            // 只绘制裁切后的内部轮廓片段（offsets）
            for (const auto& off : lc->offsets) {
                for (const auto& pt : off) {
                    gp_Pnt worldPt(pt.x, pt.y, z);
                    worldPt.Transform(transform);

                    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(worldPt, pointRadius);
                    Handle(AIS_Shape) aisSphere = new AIS_Shape(sphere);
                    aisSphere->SetColor(white);
                    aisSphere->SetTransparency(0.0);
                    m_context->Display(aisSphere, Standard_False);
                    m_contourPointShapes.push_back(aisSphere);
                }
            }

            // 移除了 original 和 offsetsOuter 的遍历
        }
    }

    m_context->UpdateCurrentViewer();
    updateView();
}

// void ObjectOcctView::drawLayeredPath(
//     const std::vector<MaxIncircleInfo>& outerCircles,
//     const std::vector<MaxIncircleInfo>& innerCircles,
//     const gp_Trsf& transform,
//     double safeZ,
//     double maxDirectDist,
//     const std::map<int, int>& contourIdToGroupId)
// {
//     // 清除旧的外/内路径对象及抬刀连接
//     for (auto& shape : m_outerPathShapes) {
//         m_context->Remove(shape, Standard_False);
//     }
//     m_outerPathShapes.clear();
//     for (auto& shape : m_incirclePathShapes) {
//         m_context->Remove(shape, Standard_False);
//     }
//     m_incirclePathShapes.clear();
//     for (auto& shape : m_connectionShapes) {
//         m_context->Remove(shape, Standard_False);
//     }
//     m_connectionShapes.clear();

//     // 将点按层高 z 分组
//     std::map<double, std::vector<const MaxIncircleInfo*>> layerOuter, layerInner;
//     for (const auto& info : outerCircles) {
//         if (!info.contactPoints.empty())
//             layerOuter[info.z].push_back(&info);
//     }
//     for (const auto& info : innerCircles) {
//         if (!info.contactPoints.empty())
//             layerInner[info.z].push_back(&info);
//     }

//     // 获取所有层高并降序排序
//     std::set<double, std::greater<double>> allZ;
//     for (auto& [z, _] : layerOuter) allZ.insert(z);
//     for (auto& [z, _] : layerInner) allZ.insert(z);

//     gp_Pnt lastOuterEnd, lastInnerEnd;
//     bool hasLastOuter = false;
//     bool hasLastInner = false;

//     // 逐层处理：从高到低
//     for (double z : allZ) {
//         // ---------------- 本层外轮廓 ----------------
//         auto itOuter = layerOuter.find(z);
//         if (itOuter != layerOuter.end()) {
//             if (hasLastInner && !hasLastOuter) {
//                 gp_Pnt firstPt = toWorld(*itOuter->second.front(), transform);
//                 drawLiftTravelPlunge(lastInnerEnd, firstPt, safeZ);
//             }
//             lastOuterEnd = drawSingleLayerOuterPath(z, itOuter->second,
//                                                     hasLastOuter ? lastOuterEnd : gp_Pnt(),
//                                                     hasLastOuter,
//                                                     safeZ, maxDirectDist, transform);
//             hasLastOuter = true;
//             hasLastInner = false;
//         }

//         // ---------------- 本层内轮廓 ----------------
//         auto itInner = layerInner.find(z);
//         if (itInner != layerInner.end()) {
//             if (hasLastOuter) {
//                 gp_Pnt firstPt = toWorld(*itInner->second.front(), transform);
//                 drawLiftTravelPlunge(lastOuterEnd, firstPt, safeZ);
//             }
//             lastInnerEnd = drawSingleLayerInnerPath(z, itInner->second,
//                                                     hasLastInner ? lastInnerEnd : gp_Pnt(),
//                                                     hasLastInner,
//                                                     safeZ,
//                                                     contourIdToGroupId, transform);
//             hasLastInner = true;
//             hasLastOuter = false;
//         }
//     }

//     // ========== 外轮廓绿点 → 内轮廓 groupId=1 独占区域B紫红点（逐层配对）==========
//     std::vector<const MaxIncircleInfo*> targetPurples;
//     for (const auto& info : innerCircles) {
//         if (info.isOuter) continue;
//         if (!info.isInnermost) continue;
//         if (info.regionId != 1) continue;
//         if (info.contactPoints.empty()) continue;
//         auto it = contourIdToGroupId.find(info.contourId);
//         if (it == contourIdToGroupId.end() || it->second != 1) continue;
//         targetPurples.push_back(&info);
//     }

//     std::sort(targetPurples.begin(), targetPurples.end(),
//               [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
//                   return a->z > b->z;
//               });

//     for (const auto* purple : targetPurples) {
//         const MaxIncircleInfo* outerGreen = nullptr;
//         for (const auto& info : outerCircles) {
//             if (!info.isOuter) continue;
//             if (!info.isConcave || info.isInnermost) continue;
//             if (info.contactPoints.empty()) continue;
//             if (std::abs(info.z - purple->z) > 1e-6) continue;

//             if (!outerGreen || info.passIndex < outerGreen->passIndex)
//                 outerGreen = &info;
//         }

//         if (outerGreen) {
//             gp_Pnt fromWorld(outerGreen->contactPoints[0].x, outerGreen->contactPoints[0].y, outerGreen->z);
//             gp_Pnt toWorld(purple->contactPoints[0].x, purple->contactPoints[0].y, purple->z);
//             fromWorld.Transform(transform);
//             toWorld.Transform(transform);
//             drawLiftTravelPlunge(fromWorld, toWorld, safeZ);
//             qDebug() << "外轮廓绿点→内腔B紫红点抬刀 (z =" << purple->z << ")";
//         } else {
//             qDebug() << "未找到同层外轮廓绿点，紫红点层 z =" << purple->z;
//         }
//     }

//     m_context->UpdateCurrentViewer();
//     updateView();
// }

void ObjectOcctView::drawLayeredPath(
    const std::vector<MaxIncircleInfo>& outerCircles,
    const std::vector<MaxIncircleInfo>& innerCircles,
    const gp_Trsf& transform,
    double safeZ,
    double maxDirectDist,
    const std::map<int, int>& contourIdToGroupId)
{
    // 清除旧的外/内路径对象及抬刀连接
    for (auto& shape : m_outerPathShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_outerPathShapes.clear();
    for (auto& shape : m_incirclePathShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_incirclePathShapes.clear();
    for (auto& shape : m_connectionShapes) {
        m_context->Remove(shape, Standard_False);
    }
    m_connectionShapes.clear();

    // 将点按层高 z 分组（外轮廓和内轮廓分别存放指针）
    std::map<double, std::vector<const MaxIncircleInfo*>> layerOuter, layerInner;
    for (const auto& info : outerCircles) {
        if (!info.contactPoints.empty())
            layerOuter[info.z].push_back(&info);
    }
    for (const auto& info : innerCircles) {
        if (!info.contactPoints.empty())
            layerInner[info.z].push_back(&info);
    }

    // 获取所有有数据的层高并降序排序
    std::set<double, std::greater<double>> allZ;
    for (auto& [z, _] : layerOuter) allZ.insert(z);
    for (auto& [z, _] : layerInner) allZ.insert(z);

    qDebug() << "======== 开始分层路径连接 (从高到低) ========";
    qDebug() << "总层数:" << allZ.size();

    gp_Pnt lastOuterEnd, lastInnerEnd;
    bool hasLastOuter = false;
    bool hasLastInner = false;

    int layerIndex = 0;
    for (double z : allZ) {
        layerIndex++;
        qDebug().noquote() << QString("--- 层 %1 (Z=%2) ---").arg(layerIndex).arg(z, 0, 'f', 3);

        // ---------------- 本层外轮廓 ----------------
        auto itOuter = layerOuter.find(z);
        if (itOuter != layerOuter.end()) {
            size_t outerCount = itOuter->second.size();
            qDebug() << "  外轮廓点数:" << outerCount;

            // 分类统计：蓝点、绿点（凹入非最内）、紫红点（凹入最内）
            int blueCount = 0, greenCount = 0, purpleCount = 0;
            for (auto* p : itOuter->second) {
                if (p->isConcave) {
                    if (p->isInnermost) purpleCount++;
                    else greenCount++;
                } else {
                    blueCount++;
                }
            }
            qDebug() << "    蓝点:" << blueCount << " 绿点:" << greenCount << " 紫红点:" << purpleCount;

            // 连接前一层内轮廓到本层外轮廓的抬刀（若需要）
            if (hasLastInner && !hasLastOuter) {
                gp_Pnt firstPt = toWorld(*itOuter->second.front(), transform);
                drawLiftTravelPlunge(lastInnerEnd, firstPt, safeZ);
                qDebug() << "    抬刀连接: 内轮廓终点 -> 本层外轮廓起点";
            } else if (hasLastOuter) {
                qDebug() << "    连续外轮廓，无需额外抬刀";
            }

            // 绘制本层外轮廓路径
            lastOuterEnd = drawSingleLayerOuterPath(z, itOuter->second,
                                                    hasLastOuter ? lastOuterEnd : gp_Pnt(),
                                                    hasLastOuter,
                                                    safeZ, maxDirectDist, transform);
            hasLastOuter = true;
            hasLastInner = false;
            qDebug() << "    外轮廓处理完成";
        } else {
            qDebug() << "  本层无外轮廓";
        }

        // ---------------- 本层内轮廓 ----------------
        auto itInner = layerInner.find(z);
        if (itInner != layerInner.end()) {
            size_t innerCount = itInner->second.size();
            qDebug() << "  内轮廓点数:" << innerCount;

            // 连接外轮廓终点到内轮廓起点（抬刀）
            if (hasLastOuter) {
                gp_Pnt firstPt = toWorld(*itInner->second.front(), transform);
                drawLiftTravelPlunge(lastOuterEnd, firstPt, safeZ);
                qDebug() << "    抬刀连接: 外轮廓终点 -> 本层内轮廓起点";
            } else if (hasLastInner) {
                qDebug() << "    连续内轮廓，无需额外抬刀";
            }

            // 绘制本层内轮廓路径
            lastInnerEnd = drawSingleLayerInnerPath(z, itInner->second,
                                                    hasLastInner ? lastInnerEnd : gp_Pnt(),
                                                    hasLastInner,
                                                    safeZ,
                                                    contourIdToGroupId, transform);
            hasLastInner = true;
            hasLastOuter = false;
            qDebug() << "    内轮廓处理完成";
        } else {
            qDebug() << "  本层无内轮廓，跳过内轮廓连接";
        }
    }

    // ========== 外轮廓绿点 → 内轮廓 groupId=1 独占区域B紫红点（逐层配对）==========
    qDebug() << "======== 特殊连接：外轮廓绿点 -> 内腔B紫红点 ========";
    std::vector<const MaxIncircleInfo*> targetPurples;
    for (const auto& info : innerCircles) {
        if (info.isOuter) continue;
        if (!info.isInnermost) continue;
        if (info.regionId != 1) continue;
        if (info.contactPoints.empty()) continue;
        auto it = contourIdToGroupId.find(info.contourId);
        if (it == contourIdToGroupId.end() || it->second != 1) continue;
        targetPurples.push_back(&info);
    }

    std::sort(targetPurples.begin(), targetPurples.end(),
              [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) {
                  return a->z > b->z;
              });

    for (const auto* purple : targetPurples) {
        const MaxIncircleInfo* outerGreen = nullptr;
        for (const auto& info : outerCircles) {
            if (!info.isOuter) continue;
            if (!info.isConcave || info.isInnermost) continue;
            if (info.contactPoints.empty()) continue;
            if (std::abs(info.z - purple->z) > 1e-6) continue;

            if (!outerGreen || info.passIndex < outerGreen->passIndex)
                outerGreen = &info;
        }

        if (outerGreen) {
            gp_Pnt fromWorld(outerGreen->contactPoints[0].x, outerGreen->contactPoints[0].y, outerGreen->z);
            gp_Pnt toWorld(purple->contactPoints[0].x, purple->contactPoints[0].y, purple->z);
            fromWorld.Transform(transform);
            toWorld.Transform(transform);
            drawLiftTravelPlunge(fromWorld, toWorld, safeZ);
            qDebug() << "  绿点(z=" << outerGreen->z << ") -> 紫红点(z=" << purple->z << ") 抬刀连接完成";
        } else {
            qDebug() << "  未找到同层绿点，紫红点(z=" << purple->z << ") 未连接";
        }
    }

    m_context->UpdateCurrentViewer();
    updateView();
    qDebug() << "======== 分层路径连接完成 ========";
}




// void ObjectOcctView::drawSpiralPath(const gp_Pnt& from, const gp_Pnt& to,
//                                     double helixAngleDeg, double helixRadius, double /*safeZ*/)
// {
//     if (m_view.IsNull() || m_context.IsNull()) return;
//     if (from.Distance(to) < 1e-9) return;

//     // 高度差
//     double h = std::abs(to.Z() - from.Z());

//     // 导程 = 2π·R·tan(升角)
//     double tanA = std::tan(helixAngleDeg * M_PI / 180.0);
//     double lead = (tanA > 1e-9) ? (2.0 * M_PI * helixRadius * tanA) : 1e6;
//     double turns = h / lead;
//     turns = std::max(1.0, turns);      // 至少一圈，确保螺旋可见
//     const double maxTurns = 10.0;
//     if (turns > maxTurns) turns = maxTurns;

//     double totalAngleRad = 2.0 * M_PI * turns;
//     totalAngleRad = -totalAngleRad;    // 右旋（顺时针，从上往下看）

//     int steps = std::max(500, static_cast<int>(std::abs(totalAngleRad) / 0.02));

//     // 起始偏移方向：水平向右 (1,0)
//     gp_Dir startDir(1.0, 0.0, 0.0);

//     // 终止偏移方向：从起始方向旋转 totalAngle 弧度
//     double endAngle = totalAngleRad;   // 注意 totalAngleRad 已经是负值（右旋）
//     gp_Dir endDir(std::cos(endAngle), std::sin(endAngle), 0.0);

//     // 中心起点 = 起点 - R * 起始偏移方向
//     gp_Pnt centerStart(from.X() - helixRadius * startDir.X(),
//                        from.Y() - helixRadius * startDir.Y(),
//                        from.Z());

//     // 中心终点 = 终点 - R * 终止偏移方向
//     gp_Pnt centerEnd(to.X() - helixRadius * endDir.X(),
//                      to.Y() - helixRadius * endDir.Y(),
//                      to.Z());

//     std::vector<gp_Pnt> pts;
//     pts.reserve(steps + 1);

//     for (int i = 0; i <= steps; ++i) {
//         double t = static_cast<double>(i) / steps;
//         double angle = totalAngleRad * t;   // 当前旋转角

//         // 中心点线性插值
//         double cx = centerStart.X() + t * (centerEnd.X() - centerStart.X());
//         double cy = centerStart.Y() + t * (centerEnd.Y() - centerStart.Y());
//         double cz = centerStart.Z() + t * (centerEnd.Z() - centerStart.Z());

//         // 偏移向量 = R * (cos(angle), sin(angle))
//         double offX = helixRadius * std::cos(angle);
//         double offY = helixRadius * std::sin(angle);

//         pts.push_back(gp_Pnt(cx + offX, cy + offY, cz));
//     }

//     // 合并为一个 Compound 对象（提升性能）
//     TopoDS_Compound compound;
//     BRep_Builder builder;
//     builder.MakeCompound(compound);

//     for (size_t i = 1; i < pts.size(); ++i) {
//         TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(pts[i-1], pts[i]);
//         if (!edge.IsNull())
//             builder.Add(compound, edge);
//     }

//     Handle(AIS_Shape) shape = new AIS_Shape(compound);
//     shape->SetDisplayMode(AIS_WireFrame);
//     shape->SetColor(Quantity_NOC_MAGENTA1);
//     Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(Quantity_NOC_MAGENTA1, Aspect_TOL_DASH, 2.0);
//     shape->Attributes()->SetLineAspect(aspect);

//     m_context->Display(shape, Standard_False);
//     m_connectionShapes.push_back(shape);

//     m_context->UpdateCurrentViewer();
//     m_view->Redraw();

//     qDebug() << "螺旋线: from" << from.X() << from.Y() << from.Z()
//              << "to" << to.X() << to.Y() << to.Z()
//              << "升角" << helixAngleDeg << "半径" << helixRadius
//              << "高度差" << h << "圈数" << turns;
// }


void ObjectOcctView::drawSpiralPath(const gp_Pnt& from, const gp_Pnt& to,
                                    const gp_Pnt& fromCenter, const gp_Pnt& toCenter,
                                    double helixAngleDeg, double helixRadius, double /*safeZ*/)
{
    if (m_view.IsNull() || m_context.IsNull()) return;
    if (from.Distance(to) < 1e-9) return;

    // ---------- 基础参数 ----------
    double h = std::abs(to.Z() - from.Z());                     // 高度差
    double tanA = std::tan(helixAngleDeg * M_PI / 180.0);
    // 导程 = 2π·R·tan(升角)
    double lead = (tanA > 1e-9) ? (2.0 * M_PI * helixRadius * tanA) : 1e6;

    // 期望圈数（半径越大导程越大，圈数越多，符合要求）
    double desiredTurns = h / lead;
    desiredTurns = std::max(1.0, desiredTurns);                 // 至少1圈
    const double maxTurns = 20.0;                                // 上限可调整
    if (desiredTurns > maxTurns) desiredTurns = maxTurns;

    // ---------- 相位计算 ----------
    double startAngle = std::atan2(from.Y() - fromCenter.Y(), from.X() - fromCenter.X());
    double targetEndAngle = std::atan2(to.Y() - toCenter.Y(), to.X() - toCenter.X());

    // 从 startAngle 顺时针（负方向）到 targetEndAngle 的最小负角度差 diff
    double diff = targetEndAngle - startAngle;
    if (diff > 0) diff -= 2.0 * M_PI;          // 保证 diff ∈ (-2π, 0]

    // 需要增加整圈数 k，使得总旋转角度 total = diff - 2π·k 的绝对值 ≥ 2π·desiredTurns
    // 且 total 仍为负值（右旋）
    double desiredNegAngle = -2.0 * M_PI * desiredTurns;   // 负值，绝对值等于期望圈数对应的弧度
    // 计算满足 diff - 2π·k ≤ desiredNegAngle 的最小整数 k
    // 即 diff - 2π·k ≤ desiredNegAngle  →  -2π·k ≤ desiredNegAngle - diff
    // → k ≥ (desiredNegAngle - diff) / (-2π) = (diff - desiredNegAngle) / (2π)
    double kMin = (diff - desiredNegAngle) / (2.0 * M_PI);
    int k = static_cast<int>(std::ceil(kMin));
    // 确保 k 至少为 0（避免 total 变成正值）
    if (k < 0) k = 0;

    double totalAngleRad = diff - 2.0 * M_PI * k;

    // 如果 totalAngleRad 意外 > 0，则再减一圈强制右旋
    if (totalAngleRad > 0) totalAngleRad -= 2.0 * M_PI;

    // ---------- 生成螺旋点 ----------
    int steps = std::max(500, static_cast<int>(std::abs(totalAngleRad) / 0.02));
    std::vector<gp_Pnt> pts;
    pts.reserve(steps + 1);

    for (int i = 0; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double angle = startAngle + totalAngleRad * t;

        // 圆心线性插值
        double cx = fromCenter.X() + t * (toCenter.X() - fromCenter.X());
        double cy = fromCenter.Y() + t * (toCenter.Y() - fromCenter.Y());
        double cz = fromCenter.Z() + t * (toCenter.Z() - fromCenter.Z());

        double px = cx + helixRadius * std::cos(angle);
        double py = cy + helixRadius * std::sin(angle);
        double pz = cz;

        pts.push_back(gp_Pnt(px, py, pz));
    }

    // ---------- 合并为 Compound 显示 ----------
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (size_t i = 1; i < pts.size(); ++i) {
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(pts[i-1], pts[i]);
        if (!edge.IsNull()) builder.Add(compound, edge);
    }

    Handle(AIS_Shape) shape = new AIS_Shape(compound);
    shape->SetDisplayMode(AIS_WireFrame);
    shape->SetColor(Quantity_NOC_MAGENTA1);
    Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(Quantity_NOC_MAGENTA1, Aspect_TOL_SOLID, 3.0);
    shape->Attributes()->SetLineAspect(aspect);

    m_context->Display(shape, Standard_False);
    m_connectionShapes.push_back(shape);

    m_context->UpdateCurrentViewer();
    m_view->Redraw();

    qDebug() << "螺旋线: from" << from.X() << from.Y() << from.Z()
             << "to" << to.X() << to.Y() << to.Z()
             << "升角" << helixAngleDeg << "半径" << helixRadius
             << "高度差" << h << "期望圈数" << desiredTurns
             << "实际圈数" << std::abs(totalAngleRad) / (2 * M_PI);
}

gp_Pnt ObjectOcctView::drawSingleLayerOuterPath(
    double z, const std::vector<const MaxIncircleInfo*>& points,
    const gp_Pnt& prevEndPoint, bool hasPrev,
    double safeZ, double maxDirectDist, const gp_Trsf& transform)
{
    // 将传入的点按 contourId 分组（同一层可能有多个外轮廓）
    std::map<int, std::vector<const MaxIncircleInfo*>> contourMap;
    for (auto* p : points) contourMap[p->contourId].push_back(p);

    gp_Pnt lastPoint;
    bool firstContour = true;

    // 用于保存线条的临时集合
    std::vector<TopoDS_Edge> cutEdges, upEdges, travelEdges, downEdges;

    auto makeLine = [](const gp_Pnt& a, const gp_Pnt& b) -> TopoDS_Edge {
        if (a.Distance(b) < 1e-9) return TopoDS_Edge();
        GC_MakeSegment seg(a, b);
        return seg.IsDone() ? BRepBuilderAPI_MakeEdge(seg.Value()) : TopoDS_Edge();
    };

    auto addLiftMovePlunge = [&](const gp_Pnt& from, const gp_Pnt& to) {
        gp_Pnt upFrom(from.X(), from.Y(), safeZ);
        gp_Pnt upTo(to.X(), to.Y(), safeZ);
        auto e1 = makeLine(from, upFrom);
        auto e2 = makeLine(upFrom, upTo);
        auto e3 = makeLine(upTo, to);
        if (!e1.IsNull()) upEdges.push_back(e1);
        if (!e2.IsNull()) travelEdges.push_back(e2);
        if (!e3.IsNull()) downEdges.push_back(e3);
    };

    for (auto& [cid, ptrs] : contourMap) {
        // 分离蓝点和凹环点（沿用原逻辑）
        std::vector<const MaxIncircleInfo*> bluePts, concavePts;
        for (auto* p : ptrs) {
            if (p->isConcave) concavePts.push_back(p);
            else bluePts.push_back(p);
        }

        // 排序：蓝点 passIndex 升序（外→内），凹环点 passIndex 降序（内→外）
        std::sort(bluePts.begin(), bluePts.end(),
                  [](auto* a, auto* b) { return a->passIndex < b->passIndex; });
        std::sort(concavePts.begin(), concavePts.end(),
                  [](auto* a, auto* b) { return a->passIndex > b->passIndex; });

        // 确定本轮廓的第一个点
        gp_Pnt firstPt;
        bool validFirst = false;
        if (!bluePts.empty()) {
            firstPt = toWorld(*bluePts.front(), transform);
            validFirst = true;
        } else if (!concavePts.empty()) {
            auto* innermost = concavePts.front();
            for (auto* c : concavePts)
                if (c->isInnermost) { innermost = c; break; }
            firstPt = toWorld(*innermost, transform);
            validFirst = true;
        }
        if (!validFirst) continue;

        // 如果不是第一个轮廓，且没有前驱点（hasPrev == false），
        // 则需要在轮廓间执行抬刀连接（跨轮廓）
        if (!firstContour) {
            addLiftMovePlunge(lastPoint, firstPt);
        } else if (hasPrev) {
            // 同层第一个轮廓可能与上一层终点连接，此处由 drawLayeredPath 的层间抬刀处理，
            // 这里不需要额外连接，因为已经在外层保证了抬刀。
            // 但为了统一，如果 hasPrev 为 true 且 firstContour，说明上一层的终点应连接到本层起点。
            // 该连接已在 drawLayeredPath 中处理，此处跳过。
        }

        gp_Pnt prevPt = firstPt;
        bool firstBlue = true;

        // 连接所有蓝点（外→内）
        for (auto* bp : bluePts) {
            gp_Pnt curr = toWorld(*bp, transform);
            if (firstBlue) { firstBlue = false; continue; }
            double dist = prevPt.Distance(curr);
            if (dist <= maxDirectDist) {
                auto e = makeLine(prevPt, curr);
                if (!e.IsNull()) cutEdges.push_back(e);
            } else {
                addLiftMovePlunge(prevPt, curr);
            }
            prevPt = curr;
        }

        // 如果有凹环点，抬刀进入最内凹环点并连接所有凹环
        if (!concavePts.empty()) {
            auto* innermost = concavePts.front();
            for (auto* c : concavePts)
                if (c->isInnermost) { innermost = c; break; }
            gp_Pnt entryPt = toWorld(*innermost, transform);

            if (!bluePts.empty()) {
                addLiftMovePlunge(prevPt, entryPt);
            }
            for (size_t k = 0; k + 1 < concavePts.size(); ++k) {
                auto e = makeLine(toWorld(*concavePts[k], transform),
                                  toWorld(*concavePts[k+1], transform));
                if (!e.IsNull()) cutEdges.push_back(e);
            }
            prevPt = toWorld(*concavePts.back(), transform);
        }

        lastPoint = prevPt;
        firstContour = false;
    }

    // 将所有收集到的边显示为不同颜色的线段（复用原绘制方式）
    auto displayCompound = [&](const std::vector<TopoDS_Edge>& edges,
                               Quantity_Color color, Aspect_TypeOfLine style,
                               Standard_Real width = 2.0) {
        if (edges.empty()) return;
        TopoDS_Compound comp;
        BRep_Builder b;
        b.MakeCompound(comp);
        for (const auto& e : edges) b.Add(comp, e);
        Handle(AIS_Shape) shape = new AIS_Shape(comp);
        shape->SetDisplayMode(AIS_WireFrame);          // 强制线框模式
        shape->SetColor(color);
        Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(color, style, width);
        shape->Attributes()->SetWireAspect(aspect);    // 使用 SetWireAspect 而非 SetLineAspect
        m_context->Display(shape, Standard_True);
        m_outerPathShapes.push_back(shape);
    };

    displayCompound(cutEdges,    Quantity_NOC_ORANGE, Aspect_TOL_SOLID);
    displayCompound(upEdges,     Quantity_NOC_PURPLE, Aspect_TOL_DASH, 2.0);
    displayCompound(travelEdges, Quantity_NOC_RED,    Aspect_TOL_DASH, 2.0);
    displayCompound(downEdges,   Quantity_NOC_YELLOW, Aspect_TOL_DASH, 2.0);

    return lastPoint;   // 返回该层外轮廓的最终点
}
gp_Pnt ObjectOcctView::drawSingleLayerInnerPath(
    double z,
    const std::vector<const MaxIncircleInfo*>& points,
    const gp_Pnt& prevEndPoint,
    bool hasPrev,
    double safeZ,                       // ← 增加此参数
    const std::map<int, int>& contourToGroup,
    const gp_Trsf& transform)
{
    std::vector<TopoDS_Edge> cutEdges, upEdges, travelEdges, downEdges;

    auto makeLine = [](const gp_Pnt& a, const gp_Pnt& b) -> TopoDS_Edge {
        if (a.Distance(b) < 1e-9) return TopoDS_Edge();
        GC_MakeSegment seg(a, b);
        return seg.IsDone() ? BRepBuilderAPI_MakeEdge(seg.Value()) : TopoDS_Edge();
    };

    // 抬刀/平移/下刀
    auto addUpTravelDown = [&](const gp_Pnt& from, const gp_Pnt& to) {
        gp_Pnt up(from.X(), from.Y(), safeZ);     // ← 使用参数 safeZ
        gp_Pnt down(to.X(), to.Y(), safeZ);
        auto e1 = makeLine(from, up);
        auto e2 = makeLine(up, down);
        auto e3 = makeLine(down, to);
        if (!e1.IsNull()) upEdges.push_back(e1);
        if (!e2.IsNull()) travelEdges.push_back(e2);
        if (!e3.IsNull()) downEdges.push_back(e3);
    };

    // 按 groupId 和 regionId 组织点
    std::map<int, std::map<int, std::vector<const MaxIncircleInfo*>>> groupRegions;
    for (auto* p : points) {
        int gid = 0;
        auto it = contourToGroup.find(p->contourId);
        if (it != contourToGroup.end()) gid = it->second;
        else continue;
        groupRegions[gid][p->regionId].push_back(p);
    }

    gp_Pnt lastPt;
    bool firstGroup = true;

    for (auto& [gid, regionMap] : groupRegions) {
        // 区域排序：先独占区 (id>0) 升序，再共享区 (0)
        std::vector<int> order;
        for (auto& [rid, _] : regionMap) if (rid > 0) order.push_back(rid);
        std::sort(order.begin(), order.end());
        if (regionMap.count(0)) order.push_back(0);

        for (int rid : order) {
            auto& vec = regionMap[rid];
            if (vec.empty()) continue;

            // 区域内排序：z 降序，passIndex 降序
            std::sort(vec.begin(), vec.end(),
                      [](auto* a, auto* b) {
                          if (std::abs(a->z - b->z) > 1e-6) return a->z > b->z;
                          return a->passIndex > b->passIndex;
                      });

            gp_Pnt startPt = toWorld(*vec.front(), transform);

            if (!firstGroup) {
                if (rid > 0 && lastPt.Distance(startPt) > 1e-6) {
                    addUpTravelDown(lastPt, startPt);
                } else {
                    auto e = makeLine(lastPt, startPt);
                    if (!e.IsNull()) cutEdges.push_back(e);
                }
            }

            for (size_t i = 0; i + 1 < vec.size(); ++i) {
                auto e = makeLine(toWorld(*vec[i], transform),
                                  toWorld(*vec[i+1], transform));
                if (!e.IsNull()) cutEdges.push_back(e);
            }

            lastPt = toWorld(*vec.back(), transform);
            firstGroup = false;
        }
    }

    // 显示线段（颜色和线型与原 displayIncirclePath 一致）
    auto displayCompound = [&](const std::vector<TopoDS_Edge>& edges,
                               Quantity_Color color, Aspect_TypeOfLine style) {
        if (edges.empty()) return;
        TopoDS_Compound comp;
        BRep_Builder b;
        b.MakeCompound(comp);
        for (const auto& e : edges) b.Add(comp, e);
        Handle(AIS_Shape) shape = new AIS_Shape(comp);
        shape->SetDisplayMode(AIS_WireFrame);          // 强制线框模式
        shape->SetColor(color);
        Handle(Prs3d_LineAspect) aspect = new Prs3d_LineAspect(color, style, 2.0);
        shape->Attributes()->SetWireAspect(aspect);    // 改为 SetWireAspect
        m_context->Display(shape, Standard_True);
        m_incirclePathShapes.push_back(shape);
    };

    displayCompound(cutEdges,    Quantity_NOC_ORANGE, Aspect_TOL_SOLID);
    displayCompound(upEdges,     Quantity_NOC_PURPLE, Aspect_TOL_DASH);
    displayCompound(travelEdges, Quantity_NOC_RED,    Aspect_TOL_DASH);
    displayCompound(downEdges,   Quantity_NOC_YELLOW, Aspect_TOL_DASH);

    return lastPt;
}







