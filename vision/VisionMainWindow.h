#ifndef VISIONMAINWINDOW_H
#define VISIONMAINWINDOW_H

#include <QMainWindow>
#include <QToolButton>
#include <QMenu>

class VisionWidget;

class VisionMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit VisionMainWindow(QWidget *parent = nullptr);
    ~VisionMainWindow();

private:
    void init_ui();

    // 工具栏按钮构建辅助
    QToolButton* createToolButton(const QIcon &icon, const QString &tip, QMenu *menu = nullptr);

    // 槽函数（与设置相关）
    void onNewCoordinate();
    void onEditCoordinate();
    void onDeleteCoordinate();

    void onNewTool();
    void onEditTool();
    void onDeleteTool();

    void onNewStock();
    void onEditStock();
    void onDeleteStock();

    void onSelectComponent();

private:
    VisionWidget* vision_wid = nullptr;

    // 原加工动作
    QAction* action1;
    QAction* action2;
    QAction* action3;
    QAction* action4;
    QAction* action5;

    // 下拉菜单
    QMenu* menuCoord = nullptr;
    QMenu* menuTool = nullptr;
    QMenu* menuStock = nullptr;
};

#endif // VISIONMAINWINDOW_H
