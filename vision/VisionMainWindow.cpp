#include "VisionMainWindow.h"
#include "VisionWidget.h"
#include "CreatePathDialog.h"
#include "public/GlobalMachiningData.h"

#include <QToolBar>
#include <QStatusBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QApplication>

VisionMainWindow::VisionMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    init_ui();
}

VisionMainWindow::~VisionMainWindow() {}

// 快捷创建带菜单的工具按钮
QToolButton* VisionMainWindow::createToolButton(const QIcon &icon, const QString &tip, QMenu *menu)
{
    QToolButton *btn = new QToolButton(this);
    btn->setIcon(icon);
    btn->setToolTip(tip);
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setStyleSheet("QToolButton::menu-indicator { image: none; }"); // 隐藏默认下拉箭头
    if (menu) {
        btn->setMenu(menu);
    }
    return btn;
}

void VisionMainWindow::init_ui()
{
    vision_wid = new VisionWidget;
    setCentralWidget(vision_wid);
    setStyleSheet("font-size: 12pt;");

    // ---------- 构建主工具栏 ----------
    QToolBar *toolBar = addToolBar("Main Toolbar");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolBar->setIconSize(QSize(24, 24));                    // 图标稍大
    toolBar->setStyleSheet(R"(
        QToolBar {
            spacing: 8px;
            padding: 5px;
        }
        QToolButton {
            font-size: 12pt;                                /* 字体放大为12pt */
        }
    )");

    // --- 坐标系按钮 ---
    menuCoord = new QMenu(this);
    QAction *actNewCoord = menuCoord->addAction(tr("新建坐标系"));
    QAction *actEditCoord = menuCoord->addAction(tr("编辑坐标系"));
    QAction *actDeleteCoord = menuCoord->addAction(tr("删除坐标系"));
    connect(actNewCoord, &QAction::triggered, this, &VisionMainWindow::onNewCoordinate);
    connect(actEditCoord, &QAction::triggered, this, &VisionMainWindow::onEditCoordinate);
    connect(actDeleteCoord, &QAction::triggered, this, &VisionMainWindow::onDeleteCoordinate);

    QToolButton *btnCoord = createToolButton(QIcon(":/resource/images/Settings/axis.png"), tr("坐标系"), menuCoord);
    btnCoord->setText(tr("坐标系"));
    btnCoord->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolBar->addWidget(btnCoord);

    // --- 刀具按钮 ---
    menuTool = new QMenu(this);
    QAction *actNewTool = menuTool->addAction(tr("新建刀具"));
    QAction *actEditTool = menuTool->addAction(tr("编辑刀具"));
    QAction *actDeleteTool = menuTool->addAction(tr("删除刀具"));
    connect(actNewTool, &QAction::triggered, this, &VisionMainWindow::onNewTool);
    connect(actEditTool, &QAction::triggered, this, &VisionMainWindow::onEditTool);
    connect(actDeleteTool, &QAction::triggered, this, &VisionMainWindow::onDeleteTool);

    QToolButton *btnTool = createToolButton(QIcon(":/resource/images/Settings/cutting.png"), tr("刀具"), menuTool);
    btnTool->setText(tr("刀具"));
    btnTool->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolBar->addWidget(btnTool);

    // --- 毛坯按钮 ---
    menuStock = new QMenu(this);
    QAction *actNewStock = menuStock->addAction(tr("新建毛坯"));
    QAction *actEditStock = menuStock->addAction(tr("编辑毛坯"));
    QAction *actDeleteStock = menuStock->addAction(tr("删除毛坯"));
    connect(actNewStock, &QAction::triggered, this, &VisionMainWindow::onNewStock);
    connect(actEditStock, &QAction::triggered, this, &VisionMainWindow::onEditStock);
    connect(actDeleteStock, &QAction::triggered, this, &VisionMainWindow::onDeleteStock);

    QToolButton *btnStock = createToolButton(QIcon(":/resource/images/Settings/scrisshair.png"), tr("毛坯"), menuStock);
    btnStock->setText(tr("毛坯"));
    btnStock->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolBar->addWidget(btnStock);

    // --- 选择部件按钮（直接点击） ---
    QToolButton *btnComponent = new QToolButton(this);
    btnComponent->setIcon(QIcon(":/resource/images/Settings/scrisshair.png"));
    btnComponent->setToolTip(tr("选择部件"));
    btnComponent->setText(tr("部件"));
    btnComponent->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    connect(btnComponent, &QToolButton::clicked, this, &VisionMainWindow::onSelectComponent);
    toolBar->addWidget(btnComponent);

    // ---------- 分隔线 ----------
    toolBar->addSeparator();

    // --- 原有加工动作 ---
    action1 = toolBar->addAction(QIcon(":/resource/images/加工机械.png"), tr("等高线粗加工"));
    action2 = toolBar->addAction(QIcon(":/resource/images/加工机械.png"), tr("等高线精加工"));
    action3 = toolBar->addAction(QIcon(":/resource/images/加工机械.png"), tr("平面加工"));
    action4 = toolBar->addAction(QIcon(":/resource/images/加工机械.png"), tr("投影线加工"));
    action5 = toolBar->addAction(QIcon(":/resource/images/加工机械.png"), tr("清角加工"));

    connect(action1, &QAction::triggered, vision_wid, &VisionWidget::onCreatePathButtonClicked);
    connect(action2, &QAction::triggered, vision_wid, &VisionWidget::onCreateFinishPathClicked);
    // action3～action5 可按需连接
}
// ======================== 槽函数实现 ========================
void VisionMainWindow::onNewCoordinate()
{
    CoordinateDialog dlg(this, tr("新建坐标系"));
    if (dlg.exec() == QDialog::Accepted) {
        GlobalMachiningData::instance().addCoordinate(dlg.getCoordinate());
    }
}

void VisionMainWindow::onEditCoordinate()
{
    const auto &coords = GlobalMachiningData::instance().getCoordinates();
    if (coords.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可编辑的坐标系"));
        return;
    }
    QStringList names;
    for (const auto &c : coords) names << c.name;
    bool ok;
    QString name = QInputDialog::getItem(this, tr("选择坐标系"), tr("坐标系:"), names, 0, false, &ok);
    if (!ok || name.isEmpty()) return;

    auto coord = GlobalMachiningData::instance().getCoordinateByName(name);
    CoordinateDialog dlg(this, tr("编辑坐标系"), &coord);
    if (dlg.exec() == QDialog::Accepted) {
        int idx = GlobalMachiningData::instance().getCoordinateIndexByName(name);
        GlobalMachiningData::instance().updateCoordinate(idx, dlg.getCoordinate());
    }
}

void VisionMainWindow::onDeleteCoordinate()
{
    const auto &coords = GlobalMachiningData::instance().getCoordinates();
    if (coords.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的坐标系"));
        return;
    }
    QStringList names;
    for (const auto &c : coords) names << c.name;
    bool ok;
    QString name = QInputDialog::getItem(this, tr("选择坐标系"), tr("坐标系:"), names, 0, false, &ok);
    if (!ok || name.isEmpty()) return;

    if (QMessageBox::question(this, tr("确认"), tr("确定删除坐标系 '%1' 吗?").arg(name))
        == QMessageBox::Yes) {
        int idx = GlobalMachiningData::instance().getCoordinateIndexByName(name);
        GlobalMachiningData::instance().deleteCoordinate(idx);
    }
}

void VisionMainWindow::onNewTool()
{
    ToolDialog dlg(this, tr("新建刀具"));
    if (dlg.exec() == QDialog::Accepted) {
        GlobalMachiningData::instance().addTool(dlg.getTool());
    }
}

void VisionMainWindow::onEditTool()
{
    const auto &tools = GlobalMachiningData::instance().getTools();
    if (tools.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可编辑的刀具"));
        return;
    }
    QStringList nums;
    for (const auto &t : tools) nums << t.toolNumber;
    bool ok;
    QString num = QInputDialog::getItem(this, tr("选择刀具"), tr("刀具号:"), nums, 0, false, &ok);
    if (!ok || num.isEmpty()) return;

    auto tool = GlobalMachiningData::instance().getToolByNumber(num);
    ToolDialog dlg(this, tr("编辑刀具"), &tool);
    if (dlg.exec() == QDialog::Accepted) {
        int idx = GlobalMachiningData::instance().getToolIndexByNumber(num);
        GlobalMachiningData::instance().updateTool(idx, dlg.getTool());
    }
}

void VisionMainWindow::onDeleteTool()
{
    const auto &tools = GlobalMachiningData::instance().getTools();
    if (tools.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的刀具"));
        return;
    }
    QStringList nums;
    for (const auto &t : tools) nums << t.toolNumber;
    bool ok;
    QString num = QInputDialog::getItem(this, tr("选择刀具"), tr("刀具号:"), nums, 0, false, &ok);
    if (!ok || num.isEmpty()) return;

    if (QMessageBox::question(this, tr("确认"), tr("确定删除刀具 '%1' 吗?").arg(num))
        == QMessageBox::Yes) {
        int idx = GlobalMachiningData::instance().getToolIndexByNumber(num);
        GlobalMachiningData::instance().deleteTool(idx);
    }
}

void VisionMainWindow::onNewStock()
{
    StockDialog dlg(this, tr("新建毛坯"));
    if (dlg.exec() == QDialog::Accepted) {
        GlobalMachiningData::instance().addStock(dlg.getStock());
    }
}

void VisionMainWindow::onEditStock()
{
    const auto &stocks = GlobalMachiningData::instance().getStocks();
    if (stocks.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可编辑的毛坯"));
        return;
    }
    QStringList names;
    for (const auto &s : stocks) names << GlobalMachiningData::instance().generateStockName(s);
    bool ok;
    QString name = QInputDialog::getItem(this, tr("选择毛坯"), tr("毛坯:"), names, 0, false, &ok);
    if (!ok || name.isEmpty()) return;

    auto stock = GlobalMachiningData::instance().getStockByName(name);
    StockDialog dlg(this, tr("编辑毛坯"), &stock);
    if (dlg.exec() == QDialog::Accepted) {
        int idx = GlobalMachiningData::instance().getStockIndexByName(name);
        GlobalMachiningData::instance().updateStock(idx, dlg.getStock());
    }
}

void VisionMainWindow::onDeleteStock()
{
    const auto &stocks = GlobalMachiningData::instance().getStocks();
    if (stocks.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的毛坯"));
        return;
    }
    QStringList names;
    for (const auto &s : stocks) names << GlobalMachiningData::instance().generateStockName(s);
    bool ok;
    QString name = QInputDialog::getItem(this, tr("选择毛坯"), tr("毛坯:"), names, 0, false, &ok);
    if (!ok || name.isEmpty()) return;

    if (QMessageBox::question(this, tr("确认"), tr("确定删除毛坯 '%1' 吗?").arg(name))
        == QMessageBox::Yes) {
        int idx = GlobalMachiningData::instance().getStockIndexByName(name);
        GlobalMachiningData::instance().deleteStock(idx);
    }
}

void VisionMainWindow::onSelectComponent()
{
    QString filter = tr("3D模型文件 (*.stl *.stp *.step);;所有文件 (*.*)");
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择部件模型"), QDir::homePath(), filter);
    if (!filePath.isEmpty()) {
        GlobalMachiningData::instance().componentModelPath = filePath;
        statusBar()->showMessage(tr("已选择部件: %1").arg(QFileInfo(filePath).fileName()), 3000);
    }
}
