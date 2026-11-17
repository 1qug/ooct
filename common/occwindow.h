#ifndef OCCWINDOW_H
#define OCCWINDOW_H

#include <QWindow>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <AIS_InteractiveContext.hxx>
#include <V3d_Viewer.hxx>
#include <V3d_View.hxx>

class OcctWindow : public QWindow
{
    Q_OBJECT

public:
    explicit OcctWindow(QWindow* parent = nullptr);
    ~OcctWindow();

    void initOcctViewer(const Handle(Aspect_DisplayConnection)& displayConnection);

    Handle(V3d_View) getView() const { return m_view; }
    Handle(AIS_InteractiveContext) getContext() const { return m_context; }
    Handle(V3d_Viewer) getViewer() const { return m_viewer; }

    // 视角控制
    void fitAll();
    void resetView();
    void frontView();
    void backView();
    void leftView();
    void rightView();
    void topView();
    void bottomView();
    void zoomToRectangle(int x1, int y1, int x2, int y2);
    void zoomAtCenter(double scaleFactor);
    void zoomIn();
    void zoomOut();
    void axonometricView();
    void refreshView();

    void requestUpdate() { QWindow::requestUpdate(); }

signals:
    void selectionStarted(const QPoint& pos);
    void selectionUpdated(const QRect& rect);
    void selectionFinished(const QRect& rect);
    void interactionStarted();      // 旋转/平移开始时发出
    void interactionFinished();     // 结束时发出

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

private:
    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(AIS_InteractiveContext) m_context;

    // 交互状态
    QPoint m_lastMousePos;
    QPoint m_selectionStart;
    bool m_isDragging = false;
    bool m_isSelecting = false;
    bool m_isPanning = false;
    bool m_isRotating = false;
};

#endif // OCCWINDOW_H
