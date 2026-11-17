#ifndef CREATEPATHDIALOG_H
#define CREATEPATHDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QCheckBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QPointer>
#include <QRadioButton>
#include <QButtonGroup>

#include "camera_wid.h"
#include "public/globalTools.h"
#include "public/GlobalMachiningData.h"
#include "common/detected_object.h"

// 坐标系设置子窗口
class CoordinateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CoordinateDialog(QWidget *parent = nullptr, const QString& title = QString(), const GlobalMachiningData::Coordinate* initData = nullptr);
    GlobalMachiningData::Coordinate getCoordinate() const { return m_coord; }
private slots:
    void onAccepted();
private:
    GlobalMachiningData::Coordinate m_coord;
    QLineEdit *m_editName;                      // 名称输入框（不带单位）

    // 原点坐标
    UnitQLineEdit *m_editOriginX;
    UnitQLineEdit *m_editOriginY;
    UnitQLineEdit *m_editOriginZ;

    // X轴方向
    UnitQLineEdit *m_editXAxisX;
    UnitQLineEdit *m_editXAxisY;
    UnitQLineEdit *m_editXAxisZ;

    // Y轴方向
    UnitQLineEdit *m_editYAxisX;
    UnitQLineEdit *m_editYAxisY;
    UnitQLineEdit *m_editYAxisZ;

    // Z轴方向
    UnitQLineEdit *m_editZAxisX;
    UnitQLineEdit *m_editZAxisY;
    UnitQLineEdit *m_editZAxisZ;
};

// 刀具设置子窗口
class ToolDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ToolDialog(QWidget *parent = nullptr, const QString& title = QString(), const GlobalMachiningData::CuttingTool* initData = nullptr);
    GlobalMachiningData::CuttingTool getTool() const { return m_tool; }
private slots:
    void onHiltDisplayToggled(bool checked);
    void onAccepted();
private:
    GlobalMachiningData::CuttingTool m_tool;
    QLineEdit *m_editNumber;                // 刀具号（不带单位）
    UnitQLineEdit *m_editDiameter;
    UnitQLineEdit *m_editLowerRadius;
    UnitQLineEdit *m_editConeAngle;
    UnitQLineEdit *m_editSharpCorner;
    UnitQLineEdit *m_editLength;
    QCheckBox *m_checkbox_hiltDisplay;
    QWidget *m_hiltParamsWidget;
    UnitQLineEdit *m_edit_hiltUpperDiameter;
    UnitQLineEdit *m_edit_hiltLowerDiameter;
    UnitQLineEdit *m_edit_hiltLength;
    UnitQLineEdit *m_edit_hiltConeAngle;
};
// 毛坯设置子窗口
class StockDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StockDialog(QWidget *parent = nullptr, const QString& title = QString(), const GlobalMachiningData::Stock* initData = nullptr);
    GlobalMachiningData::Stock getStock() const { return m_stock; }

private slots:
    void onAccepted();
    void onOpenCameraClicked();
    void updateWidgetVisibility();

private:
    GlobalMachiningData::Stock m_stock;
    QPointer<CameraWid> m_cameraWid;

    // 界面控件
    QLineEdit *m_editStockName;                     // 毛坯名称

    QRadioButton *m_rbtnBlock;                      // 方体
    QRadioButton *m_rbtnCylinder;                   // 圆柱
    QPushButton *m_btnOpenCamera;                   // 打开相机

    QWidget *m_blockWidget;                         // 方体参数容器
    UnitQLineEdit *m_editLength;                    // 长
    UnitQLineEdit *m_editWidth;                     // 宽
    UnitQLineEdit *m_editHeight;                    // 高
    UnitQLineEdit *m_editCenterX;                   // 中心X
    UnitQLineEdit *m_editCenterY;                   // 中心Y
    UnitQLineEdit *m_editCenterZ;                   // 中心Z
    UnitQLineEdit *m_editAngle;                     // 角度

    QWidget *m_cylWidget;                           // 圆柱参数容器
    UnitQLineEdit *m_editDiameter;                  // 直径
    UnitQLineEdit *m_editCylHeight;                 // 高度
    UnitQLineEdit *m_editCylCenterX;                // 中心X
    UnitQLineEdit *m_editCylCenterY;                // 中心Y
    UnitQLineEdit *m_editCylCenterZ;                // 中心Z

    QCheckBox *m_checkAutoCenter;                   // 移动模型至毛坯中心
    QCheckBox *m_checkAutoAngle;                    // 随毛坯旋转角度
    QCheckBox *m_checkAutoSize;                     // 自动匹配毛坯大小
};
// 主对话框
class CreatePathDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreatePathDialog(QWidget *parent = nullptr);
    ~CreatePathDialog();

    // 供外部调用的接口，用于直接设置下拉框数据（若不需要可从全局数据刷新）
    void setAxisList(const QMap<QString, std::shared_ptr<DetectedObject>>& axes);
    void setBlankList(const QMap<QString, std::shared_ptr<DetectedObject>>& blanks);

    void setDefaultName(const QString &name);
    // 新增：从已有参数加载界面
    void loadFromParams(const QString &paramName, const GlobalMachiningData::MachiningParams &params);
signals:
    void pathCreated(const QString &name, const QString &workStation);
    void generatePath(QString name, GlobalMachiningData::MachiningParams &params, std::shared_ptr<DetectedObject> blank);
    // 新增：当参数名称被修改时发射（用于更新树节点）
    void paramNameChanged(const QString &oldName, const QString &newName);
private slots:
    void onNewCoordinate();
    void onEditCoordinate();
    void onDeleteCoordinate();
    void onCoordinateSelected(int index);

    void onNewTool();
    void onEditTool();
    void onDeleteTool();
    void onToolSelected(int index);

    void onNewStock();
    void onEditStock();
    void onDeleteStock();
    void onStockSelected(int index);

    void onSelectComponent();
    void onCuttingEnterSettingsChanged(int index);
    void onGenerateGCode();
    void onSave();
    void onCancel();

    void onComboxBlankClicked();

private:
    void initUI();
    void refreshCoordinateCombo();
    void refreshToolCombo();
    void refreshStockCombo();
    void refreshComponentCombo();
    void collectCurrentParams(GlobalMachiningData::MachiningParams& params);

    QVBoxLayout *m_vlay_this;
    QScrollArea *m_scrollArea;
    QVBoxLayout *m_vlay_scrArea;

    QLabel *lab_name;
    QLineEdit *edit_name;
    QHBoxLayout *hlay_name;

    QLabel *lab_work;
    QComboBox *combox_work;
    QHBoxLayout *hlay_work;

    QLabel *lab_roughFinish;
    QVBoxLayout *m_roughFinish;

    // 坐标系
    QLabel *m_lab_Coordinate;
    QComboBox *m_combox_Coordinate;
    QToolButton *m_btn_Coordinate;
    QAction *m_actionNewCoordinate;
    QAction *m_actionEditCoordinate;
    QAction *m_actionDeleteCoordinate;
    QHBoxLayout *m_hlay_Coordinate;

    // 刀具
    QLabel *m_lab_cuttingTool;
    QComboBox *m_combox_cuttingTool;
    QToolButton *m_btn_cuttingTool;
    QAction *m_actionNewTool;
    QAction *m_actionEditTool;
    QAction *m_actionDeleteTool;
    QHBoxLayout *m_hlay_cuttingTool;

    // 部件
    QLabel *m_lab_Component;
    QComboBox *m_combox_Component;
    QPushButton *m_btn_Component;
    QHBoxLayout *m_hlay_Component;

    // 毛坯
    QLabel *m_lab_Blank;
    QComboBox *m_combox_Blank;
    QToolButton *m_btn_Blank;
    QAction *m_actionNewBlank;
    QAction *m_actionEditBlank;
    QAction *m_actionDeleteBlank;
    QHBoxLayout *m_hlay_Blank;

    // 检查（预留）
    QLabel *m_lab_Check;
    QComboBox *m_combox_Check;
    QHBoxLayout *m_hlay_Check;

    // 边界（预留）
    QLabel *m_lab_Boundary;
    QComboBox *m_combox_Boundary;
    QHBoxLayout *m_hlay_Boundary;

    // 顶部/底部/层高
    QLabel *m_lab_top;
    UnitQLineEdit *m_ledit_top;
    QHBoxLayout *m_hlay_top;

    QLabel *m_lab_bottom;
    UnitQLineEdit *m_ledit_bottom;
    QHBoxLayout *m_hlay_bottom;

    QLabel *m_lab_height;
    UnitQLineEdit *m_ledit_height;
    QHBoxLayout *m_hlay_height;

    // 切削参数
    QLabel *m_lab_cuttingStyle;
    QComboBox *m_combox_cuttingStyle;
    QHBoxLayout *m_hlay_cuttingStyle;

    QLabel *m_lab_pace;
    UnitQLineEdit *m_ledit_pace;
    QHBoxLayout *m_hlay_pace;

    QLabel *m_lab_cuttingDirection;
    QComboBox *m_combox_cuttingDirection;
    QHBoxLayout *m_hlay_cuttingDirection;

    // 下刀设置
    QLabel *m_lab_cuttingEnterSettings;
    QComboBox *m_combox_cuttingEnterSettings;
    QHBoxLayout *m_hlay_cuttingEnterSettings;

    QLabel *m_lab_cuttingHeight;
    UnitQLineEdit *m_ledit_cuttingHeight;
    QHBoxLayout *m_hlay_cuttingHeight;

    QLabel *m_lab_spiralRadius;
    UnitQLineEdit *m_ledit_spiralRadius;
    QHBoxLayout *m_hlay_spiralRadius;
    QWidget *m_spiralRadiusContainer;

    QLabel *m_lab_spiralSlope;
    UnitQLineEdit *m_ledit_spiralSlope;
    QHBoxLayout *m_hlay_spiralSlope;
    QWidget *m_spiralSlopeContainer;

    // 进刀设置
    QLabel *m_lab_cuttingFeedSetting;
    QComboBox *m_combox_cuttingFeedSetting;
    QHBoxLayout *m_hlay_cuttingFeedSetting;

    QLabel *m_lab_arcRadius;
    UnitQLineEdit *m_ledit_arcRadius;
    QHBoxLayout *m_hlay_arcRadius;

    QLabel *m_lab_lineLength;
    UnitQLineEdit *m_ledit_lineLength;
    QHBoxLayout *m_hlay_lineLength;

    // 退刀设置
    QLabel *m_lab_returnCutting;
    QComboBox *m_combox_returnCutting;
    QCheckBox *m_checkbox_returnCutting;
    QHBoxLayout *m_hlay_returnCutting;

    // 安全平面
    QLabel *m_lab_interactivePlane;
    QComboBox *m_combox_interactivePlane;
    QHBoxLayout *m_hlay_interactivePlane;

    QLabel *m_lab_interactiveHeight;
    UnitQLineEdit *m_ledit_interactiveHeight;
    QHBoxLayout *m_hlay_interactiveHeight;

    // 余量公差
    QLabel *m_lab_Margin2Tolerance;
    QLabel *m_lab_sideMargin;
    UnitQLineEdit *m_ledit_sideMargin;
    QHBoxLayout *m_hlay_sideMargin;
    QLabel *m_lab_bottomMargin;
    UnitQLineEdit *m_ledit_bottomMargin;
    QHBoxLayout *m_hlay_bottomMargin;
    QLabel *m_lab_Tolerance;
    UnitQLineEdit *m_ledit_Tolerance;
    QHBoxLayout *m_hlay_Tolerance;
    QVBoxLayout *m_vlay_Margin2Tolerance;

    // 速度
    QLabel *m_lab_speed;
    QLabel *m_lab_makeFeed;
    UnitQLineEdit *m_ledit_makeFeed;
    QHBoxLayout *m_hlay_makeFeed;
    QLabel *m_lab_cuttingFeed;
    UnitQLineEdit *m_ledit_cuttingFeed;
    QHBoxLayout *m_hlay_cuttingFeed;
    QLabel *m_lab_processing;
    UnitQLineEdit *m_ledit_processing;
    QHBoxLayout *m_hlay_processing;
    QVBoxLayout *m_vlay_speed;

    // 按钮
    QPushButton *m_btn_gcode;
    QPushButton *m_btn_save;
    QPushButton *m_btn_cancel;
    QHBoxLayout *m_btn;

    // 当前选中的毛坯对象（用于生成刀路）
    std::shared_ptr<DetectedObject> m_currentBlank;

    bool m_editMode=false;                // 是否为编辑模式
    QString m_editParamName;        // 编辑时的原始参数名称
};

#endif // CREATEPATHDIALOG_H
