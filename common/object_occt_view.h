#ifndef OBJECT_OCCT_VIEW_H
#define OBJECT_OCCT_VIEW_H

#include <QWidget>
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Shape.hxx>
#include <StlAPI_Reader.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <Prs3d_LineAspect.hxx>
#include <AIS_ColoredShape.hxx>

#include <V3d_DirectionalLight.hxx>
#include <V3d_AmbientLight.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <Quantity_Color.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <AIS_ConnectedInteractive.hxx>
#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_Transform.hxx>

#include <gp_Pnt.hxx>
#include <AIS_TextLabel.hxx>
#include <TCollection_ExtendedString.hxx>

#include <algorithm>
#include <vector>
#include <memory>
#include <map>

#include "detected_object.h"
#include "stl_slicer.h"
#include "occwindow.h"

using Handle_AIS_Shape = opencascade::handle<AIS_Shape>;
struct MaxIncircleInfo;

class ObjectOcctView : public QWidget
{
    Q_OBJECT

public:
    explicit ObjectOcctView(QWidget* parent = nullptr);
    ~ObjectOcctView() override;

    Handle_AIS_Shape m_stlShape;

    // 光照和背景设置
    void setGradientBackground(const Quantity_Color& topColor, const Quantity_Color& bottomColor);
    void setSolidBackground(const Quantity_Color& color);
    void setLightIntensity(double intensity);
    void setAmbientLight(double intensity);
    void enableShadows(bool enable);
    void setLightPosition(double x, double y, double z);
    void setLightDirection(double x, double y, double z);
    void setTopDownLighting();

    // 模型操作
    bool loadModel(const std::string& filepath);
    void hideCoordinateSystem();
    void displayCoordinateSystem(const gp_Trsf& transform, double axisLength);
    void updateDetectedObjects(const std::vector<std::shared_ptr<DetectedObject>>& objects,
                               const gp_Trsf& coordTransform = gp_Trsf());
    void drawContinuousPath(const std::vector<Point3D>& continuousPath,
                            const std::shared_ptr<DetectedObject>& obj,
                            const gp_Trsf& coordTransform = gp_Trsf());
    // 切片和路径显示
    void drawContourGroups(const ContourGroups& groups,
                           const std::shared_ptr<DetectedObject>& obj,
                           const gp_Trsf& coordTransform = gp_Trsf(),
                           const std::vector<MaxIncircleInfo>* incircleInfo = nullptr);
    void drawRapidPaths(const std::vector<Point3D>& rapidpaths,
                        const std::shared_ptr<DetectedObject>& obj);
    // 显示毛坯形状
    void displayStockShape(int stockType,
                           double length, double width, double height,   // 方体
                           double diameter, double cylHeight,            // 圆柱
                           double centerX, double centerY, double centerZ,
                           double angle,
                           const gp_Trsf& coordTransform);
    // 清除操作
    void clearSlices();
    void clearPaths();
    void clearAll();

    void setShowPaths(bool show);

    void clearMaxIncircles();   // 清除所有内切圆白点及路径
    // 外轮廓：显示所有内切圆切点（白点）
    void displayMaxIncircles(const std::vector<MaxIncircleInfo>& circles,
                             const gp_Trsf& transform);


    void displayIncirclePath(const std::vector<MaxIncircleInfo>& allCircles,
                             const gp_Trsf& transform,
                             double safeZ,
                             double maxDirectDist,
                             const std::map<int, int>& contourIdToGroupId,
                             const gp_Pnt* prevOuterEnd = nullptr);

    void drawLiftTravelPlunge(const gp_Pnt& from, const gp_Pnt& to, double safeZ);

    // 绘制外轮廓及内凹环的完整加工路径（蓝点→抬刀→紫红点→绿点）
    void drawOuterContourPath(const std::vector<MaxIncircleInfo>& outerCircles,
                              const std::vector<MaxIncircleInfo>& concaveCircles,
                              const gp_Trsf& transform,
                              double safeZ,
                              double maxDirectDist = 10.0);
    void drawOuterBluesPath(const std::vector<MaxIncircleInfo>& outerCircles,
                            const gp_Trsf& transform,
                            double safeZ,
                            double maxDirectDist);
    //显示每一个点
    void displayAllContourPoints(const ContourGroups& groups, const gp_Trsf& transform);


    // 分层交替绘制外/内轮廓路径（从高到低，每层先外后内）
    void drawLayeredPath(const std::vector<MaxIncircleInfo>& outerCircles,
                         const std::vector<MaxIncircleInfo>& innerCircles,
                         const gp_Trsf& transform,
                         double safeZ,
                         double maxDirectDist,
                         const std::map<int, int>& contourIdToGroupId);

    void drawSpiralPath(const gp_Pnt& from, const gp_Pnt& to,
                        const gp_Pnt& fromCenter, const gp_Pnt& toCenter,
                        double helixAngleDeg, double helixRadius, double safeZ);
    // 如果当前没有公开 getContext(), 可以添加:
    Handle(AIS_InteractiveContext) getContext() const { return m_context; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;  // 事件过滤器

private:
    // OCCT核心对象
    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(AIS_InteractiveContext) m_context;
    Handle(AIS_Shape) m_stockShape;   // 存储毛坯的 AIS 对象
    OcctWindow* occtWindow;
    QWidget* container;
    std::vector<Handle(AIS_InteractiveObject)> m_outerIncirclePathShapes;

    // 显示对象容器
    std::vector<Handle_AIS_Shape> m_coordAxesShapes;
    std::vector<Handle_AIS_Shape> m_detectedObjectShapes;
    std::vector<Handle(AIS_InteractiveObject)> m_sliceShapes;
    std::vector<Handle(AIS_TextLabel)> m_textLabels;
    std::vector<Handle_AIS_Shape> m_pathShapes;

    std::vector<Quantity_Color> m_colors;

    // 背景与光照
    Quantity_Color m_bgTopColor = Quantity_NOC_BLACK;
    Quantity_Color m_bgBottomColor = Quantity_NOC_BLACK;
    double m_lightIntensity = 1.0;
    double m_ambientLight = 0.3;
    bool m_shadowsEnabled = false;
    gp_Dir m_lightDirection = gp_Dir(0, 0, 1);

    // 显示控制
    bool m_showPaths = true;
    bool m_showAuxiliary = true;

    // 交互状态（用于 Qt 绘制框选）
    QPoint m_lastMousePos;
    QPoint m_selectionStart;
    bool m_isSelecting = false;
    bool m_isPanning = false;
    bool m_isRotating = false;
    QRect m_selectionRect;

    // 辅助方法
    void initializeOcct();
    Handle(AIS_Shape) createLineWithStyle(const gp_Pnt& from, const gp_Pnt& to,
                                          const Quantity_Color& color,
                                          Aspect_TypeOfLine lineStyle,
                                          Standard_Real width = 1.5);
    Handle(AIS_Shape) createLineShape(const gp_Pnt& p1, const gp_Pnt& p2,
                                      const Quantity_Color& color,
                                      Aspect_TypeOfLine lineStyle,
                                      Standard_Real width);
    void updateView();
    void setAuxiliaryVisibility(bool visible);
    void handleAreaSelection(const QRect& rect);

    // 处理单层外轮廓路径，返回该层最后一个点（世界坐标）
    gp_Pnt drawSingleLayerOuterPath(double z,
                                    const std::vector<const MaxIncircleInfo*>& points,
                                    const gp_Pnt& prevEndPoint,
                                    bool hasPrev,
                                    double safeZ,
                                    double maxDirectDist,
                                    const gp_Trsf& transform);
    // 处理单层内轮廓路径，返回该层最后一个点（世界坐标）
    gp_Pnt drawSingleLayerInnerPath(double z,
                                    const std::vector<const MaxIncircleInfo*>& points,
                                    const gp_Pnt& prevEndPoint,
                                    bool hasPrev,
                                    double safeZ,
                                    const std::map<int, int>& contourToGroup,
                                    const gp_Trsf& transform);
private:
    double m_stockLeft   = 0.0;
    double m_stockRight  = 0.0;
    double m_stockBottom = 0.0;
    double m_stockTop    = 0.0;
    bool   m_stockBoundsValid = false;
    std::vector<Handle(AIS_InteractiveObject)> m_incircleShapes;   // 内切圆切点球
    std::vector<Handle(AIS_InteractiveObject)> m_incirclePathShapes; // 内切圆连接路径

    std::map<std::tuple<int, int, double, int>, std::string> m_ringLabels;
    std::vector<Handle(AIS_InteractiveObject)> m_connectionShapes;  // 外→内连接线段

    std::vector<Handle(AIS_InteractiveObject)> m_contourPointShapes;

    std::vector<Handle(AIS_Shape)> m_outerPathShapes;
};

#endif // OBJECT_OCCT_VIEW_H
