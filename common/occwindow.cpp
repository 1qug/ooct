#include "occwindow.h"
#include <QDebug>
#include <QResizeEvent>

#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>

#ifdef Q_OS_LINUX
#include <Xw_Window.hxx>
#include <X11/Xlib.h>
#else
#include <WNT_Window.hxx>
#endif

OcctWindow::OcctWindow(QWindow* parent)
    : QWindow(parent)
{
    setSurfaceType(QWindow::OpenGLSurface);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);

    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(0);
    setFormat(format);
}

OcctWindow::~OcctWindow()
{
    if (!m_context.IsNull()) {
        m_context->RemoveAll(Standard_True);
        m_context.Nullify();
    }
    if (!m_view.IsNull()) {
        m_view.Nullify();
    }
    if (!m_viewer.IsNull()) {
        m_viewer.Nullify();
    }
}

void OcctWindow::initOcctViewer(const Handle(Aspect_DisplayConnection)& displayConnection)
{
    try {
        Handle(OpenGl_GraphicDriver) graphicDriver = new OpenGl_GraphicDriver(displayConnection, Standard_False);
        graphicDriver->ChangeOptions().contextDebug = Standard_False;
        graphicDriver->ChangeOptions().contextNoAccel = Standard_False;
        // 移除不存在的 useVBO 设置

        m_viewer = new V3d_Viewer(graphicDriver);
        m_viewer->SetDefaultLights();
        m_viewer->SetLightOn();
        m_viewer->SetDefaultBackgroundColor(Quantity_NOC_BLACK);

        m_context = new AIS_InteractiveContext(m_viewer);
        m_context->SetDisplayMode(AIS_Shaded, Standard_False);

        m_view = m_viewer->CreateView();

#ifdef Q_OS_LINUX
        Handle(Xw_Window) window = new Xw_Window(displayConnection, winId());
#else
        Handle(WNT_Window) window = new WNT_Window((Aspect_Handle)winId());
#endif

        m_view->SetWindow(window);
        if (!window->IsMapped()) {
            window->Map();
        }

        m_view->SetBackgroundColor(Quantity_NOC_BLACK);
        m_view->SetShadingModel(V3d_GOURAUD);
        m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);

        // 渲染参数优化（仅使用存在的成员）
        Graphic3d_RenderingParams& params = m_view->ChangeRenderingParams();
        params.NbMsaaSamples = 2;                // 降低采样数，提升性能
        params.IsAntialiasingEnabled = true;
        params.RenderResolutionScale = 0.8f;     // 可接受的分辨率缩放
        params.IsShadowEnabled = false;
        params.TransparencyMethod = Graphic3d_RTM_BLEND_OIT;

        // 核心：关闭计算模式，防止主动降级
        m_view->SetComputedMode(Standard_False);
        m_view->SetImmediateUpdate(Standard_False);

    } catch (Standard_Failure& e) {
        qDebug() << "Error initializing OCCT viewer:" << e.GetMessageString();
    }
}
void OcctWindow::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton) {
        m_selectionStart = event->pos();
        m_isSelecting = true;
        emit selectionStarted(m_selectionStart);
    }
    else if (event->button() == Qt::RightButton) {
        if (event->buttons() & Qt::MiddleButton) {
            m_isPanning = true;
            emit interactionStarted();
        } else {
            topView();
        }
    }
    else if (event->button() == Qt::MiddleButton) {
        if (event->buttons() & Qt::RightButton) {
            m_isPanning = true;
            emit interactionStarted();
        } else {
            m_isRotating = true;
            emit interactionStarted();
            // 移除原先的：m_view->SetComputedMode(Standard_True);
            m_view->StartRotation(event->pos().x(), event->pos().y());
        }
    }
    m_isDragging = true;
}

void OcctWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_isDragging) return;

    if (m_isSelecting) {
        // 发送框选矩形更新信号（Qt 坐标）
        QRect rect(m_selectionStart, event->pos());
        emit selectionUpdated(rect.normalized());
        // 不调用 OCCT 重绘，由上层绘制矩形
    }
    else if (m_isPanning) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();
        m_view->Pan(dx, -dy);
        requestUpdate();
    }
    else if (m_isRotating && (event->buttons() & Qt::MiddleButton)) {
        m_view->Rotation(event->pos().x(), event->pos().y());
        requestUpdate();
    }
    else if (event->buttons() == Qt::RightButton) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();
        m_view->Pan(dx, -dy);
        requestUpdate();
    }

    m_lastMousePos = event->pos();
}

void OcctWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        m_isSelecting = false;
        QRect rect(m_selectionStart, event->pos());
        emit selectionFinished(rect.normalized());
    }

    if (m_isRotating) {
        m_isRotating = false;
        m_view->SetComputedMode(Standard_False);
        emit interactionFinished();
        requestUpdate();
    }

    if (m_isPanning) {
        m_isPanning = false;
        emit interactionFinished();
    }

    m_isDragging = false;
}

void OcctWindow::wheelEvent(QWheelEvent* event)
{
    if (m_view.IsNull()) return;
    QPoint pos = event->position().toPoint();
    double factor = (event->angleDelta().y() > 0) ? 1.2 : 1.0 / 1.2;
    m_view->StartZoomAtPoint(pos.x(), pos.y());
    m_view->ZoomAtPoint(pos.x(), pos.y(), pos.x(), pos.y() * factor);
    requestUpdate();
}

void OcctWindow::resizeEvent(QResizeEvent* event)
{
    QWindow::resizeEvent(event);
    if (!m_view.IsNull()) {
        m_view->MustBeResized();
    }
}

bool OcctWindow::event(QEvent* event)
{
    if (event->type() == QEvent::UpdateRequest) {
        if (!m_view.IsNull()) {
            m_view->Redraw();
        }
        return true;
    }
    return QWindow::event(event);
}

// 视角控制函数保持不变...
void OcctWindow::fitAll() { if (!m_view.IsNull()) { m_view->FitAll(); requestUpdate(); } }
void OcctWindow::resetView() { if (!m_view.IsNull()) { m_view->Reset(); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::frontView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_Yneg); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::backView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_Ypos); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::leftView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_Xneg); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::rightView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_Xpos); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::topView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_Zpos); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::bottomView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_Zneg); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::zoomToRectangle(int x1, int y1, int x2, int y2) { if (!m_view.IsNull()) { m_view->WindowFitAll(x1, y1, x2, y2); requestUpdate(); } }
void OcctWindow::zoomAtCenter(double scaleFactor) {
    if (m_view.IsNull()) return;
    Standard_Integer w, h; m_view->Window()->Size(w, h);
    int cx = w/2, cy = h/2;
    m_view->StartZoomAtPoint(cx, cy);
    m_view->ZoomAtPoint(cx, cy, cx, cy*scaleFactor);
    requestUpdate();
}
void OcctWindow::zoomIn() { zoomAtCenter(0.8); }
void OcctWindow::zoomOut() { zoomAtCenter(1.2); }
void OcctWindow::axonometricView() { if (!m_view.IsNull()) { m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight); m_view->FitAll(); requestUpdate(); } }
void OcctWindow::refreshView() { requestUpdate(); }
