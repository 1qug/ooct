#include "CreatePathDialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QScrollArea>

// ========== CoordinateDialog ==========
CoordinateDialog::CoordinateDialog(QWidget *parent, const QString& title, const GlobalMachiningData::Coordinate* initData)
    : QDialog(parent)
{
    setWindowTitle(title.isEmpty() ? tr("坐标系设置") : title);
    if (initData) m_coord = *initData;

    setStyleSheet("font-size: 12pt;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    // 名称
    m_editName = new QLineEdit(m_coord.name);
    formLayout->addRow(tr("名称:"), m_editName);

    // 原点坐标
    QHBoxLayout *originLayout = new QHBoxLayout();
    m_editOriginX = new UnitQLineEdit();
    m_editOriginX->setText(QString::number(m_coord.originX));
  //  m_editOriginX->setUnitText("mm");
    m_editOriginY = new UnitQLineEdit();
    m_editOriginY->setText(QString::number(m_coord.originY));
 //   m_editOriginY->setUnitText("mm");
    m_editOriginZ = new UnitQLineEdit();
    m_editOriginZ->setText(QString::number(m_coord.originZ));
 //   m_editOriginZ->setUnitText("mm");
    originLayout->addWidget(new QLabel(tr("X:")));
    originLayout->addWidget(m_editOriginX);
    originLayout->addWidget(new QLabel(tr("Y:")));
    originLayout->addWidget(m_editOriginY);
    originLayout->addWidget(new QLabel(tr("Z:")));
    originLayout->addWidget(m_editOriginZ);
    formLayout->addRow(tr("原点位置:"), originLayout);

    // X轴方向
    QHBoxLayout *xAxisLayout = new QHBoxLayout();
    m_editXAxisX = new UnitQLineEdit();
    m_editXAxisX->setText(QString::number(m_coord.xAxisX));
    m_editXAxisY = new UnitQLineEdit();
    m_editXAxisY->setText(QString::number(m_coord.xAxisY));
    m_editXAxisZ = new UnitQLineEdit();
    m_editXAxisZ->setText(QString::number(m_coord.xAxisZ));
    xAxisLayout->addWidget(new QLabel(tr("X:")));
    xAxisLayout->addWidget(m_editXAxisX);
    xAxisLayout->addWidget(new QLabel(tr("Y:")));
    xAxisLayout->addWidget(m_editXAxisY);
    xAxisLayout->addWidget(new QLabel(tr("Z:")));
    xAxisLayout->addWidget(m_editXAxisZ);
    formLayout->addRow(tr("X轴方向:"), xAxisLayout);

    // Y轴方向
    QHBoxLayout *yAxisLayout = new QHBoxLayout();
    m_editYAxisX = new UnitQLineEdit();
    m_editYAxisX->setText(QString::number(m_coord.yAxisX));
    m_editYAxisY = new UnitQLineEdit();
    m_editYAxisY->setText(QString::number(m_coord.yAxisY));
    m_editYAxisZ = new UnitQLineEdit();
    m_editYAxisZ->setText(QString::number(m_coord.yAxisZ));
    yAxisLayout->addWidget(new QLabel(tr("X:")));
    yAxisLayout->addWidget(m_editYAxisX);
    yAxisLayout->addWidget(new QLabel(tr("Y:")));
    yAxisLayout->addWidget(m_editYAxisY);
    yAxisLayout->addWidget(new QLabel(tr("Z:")));
    yAxisLayout->addWidget(m_editYAxisZ);
    formLayout->addRow(tr("Y轴方向:"), yAxisLayout);

    // Z轴方向
    QHBoxLayout *zAxisLayout = new QHBoxLayout();
    m_editZAxisX = new UnitQLineEdit();
    m_editZAxisX->setText(QString::number(m_coord.zAxisX));
    m_editZAxisY = new UnitQLineEdit();
    m_editZAxisY->setText(QString::number(m_coord.zAxisY));
    m_editZAxisZ = new UnitQLineEdit();
    m_editZAxisZ->setText(QString::number(m_coord.zAxisZ));
    zAxisLayout->addWidget(new QLabel(tr("X:")));
    zAxisLayout->addWidget(m_editZAxisX);
    zAxisLayout->addWidget(new QLabel(tr("Y:")));
    zAxisLayout->addWidget(m_editZAxisY);
    zAxisLayout->addWidget(new QLabel(tr("Z:")));
    zAxisLayout->addWidget(m_editZAxisZ);
    formLayout->addRow(tr("Z轴方向:"), zAxisLayout);

    mainLayout->addLayout(formLayout);

    // 底部按钮（取消左，确认右）
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(20, 10, 20, 0);
    btnLayout->setSpacing(6);

    QPushButton *btnCancel = new QPushButton(tr("取消"));
    QPushButton *btnOk = new QPushButton(tr("确认"));
    btnCancel->setMinimumSize(90, 32);
    btnOk->setMinimumSize(90, 32);

    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);

    mainLayout->addLayout(btnLayout);

    connect(btnOk, &QPushButton::clicked, this, &CoordinateDialog::onAccepted);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void CoordinateDialog::onAccepted()
{
    bool ok;
    m_coord.name = m_editName->text().trimmed();
    if (m_coord.name.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("名称不能为空"));
        return;
    }

    // 原点坐标
    m_coord.originX = m_editOriginX->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("原点X值无效")); return; }
    m_coord.originY = m_editOriginY->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("原点Y值无效")); return; }
    m_coord.originZ = m_editOriginZ->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("原点Z值无效")); return; }

    // X轴方向
    m_coord.xAxisX = m_editXAxisX->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("X轴方向X分量无效")); return; }
    m_coord.xAxisY = m_editXAxisY->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("X轴方向Y分量无效")); return; }
    m_coord.xAxisZ = m_editXAxisZ->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("X轴方向Z分量无效")); return; }

    // Y轴方向
    m_coord.yAxisX = m_editYAxisX->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("Y轴方向X分量无效")); return; }
    m_coord.yAxisY = m_editYAxisY->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("Y轴方向Y分量无效")); return; }
    m_coord.yAxisZ = m_editYAxisZ->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("Y轴方向Z分量无效")); return; }

    // Z轴方向
    m_coord.zAxisX = m_editZAxisX->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("Z轴方向X分量无效")); return; }
    m_coord.zAxisY = m_editZAxisY->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("Z轴方向Y分量无效")); return; }
    m_coord.zAxisZ = m_editZAxisZ->text().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, tr("错误"), tr("Z轴方向Z分量无效")); return; }

    accept();
}
// ========== ToolDialog==========
ToolDialog::ToolDialog(QWidget *parent, const QString& title, const GlobalMachiningData::CuttingTool* initData)
    : QDialog(parent)
{
    setWindowTitle(title.isEmpty() ? tr("刀具设置") : title);
    if (initData) m_tool = *initData;

    setStyleSheet("font-size: 12pt;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();

    m_editNumber = new QLineEdit(m_tool.toolNumber);

    m_editDiameter = new UnitQLineEdit();
    m_editDiameter->setText(QString::number(m_tool.diameter));
    m_editDiameter->setUnitText("mm");

    m_editLowerRadius = new UnitQLineEdit();
    m_editLowerRadius->setText(QString::number(m_tool.lowerRadius));
    m_editLowerRadius->setUnitText("mm");

    m_editConeAngle = new UnitQLineEdit();
    m_editConeAngle->setText(QString::number(m_tool.coneAngle));
    m_editConeAngle->setUnitText("°");

    m_editSharpCorner = new UnitQLineEdit();
    m_editSharpCorner->setText(QString::number(m_tool.sharpCorner));
    m_editSharpCorner->setUnitText("mm");

    m_editLength = new UnitQLineEdit();
    m_editLength->setText(QString::number(m_tool.length));
    m_editLength->setUnitText("mm");

    form->addRow(tr("刀具号:"), m_editNumber);
    form->addRow(tr("直径:"), m_editDiameter);
    form->addRow(tr("下半径:"), m_editLowerRadius);
    form->addRow(tr("锥角:"), m_editConeAngle);
    form->addRow(tr("尖角:"), m_editSharpCorner);
    form->addRow(tr("长度:"), m_editLength);

    // ---------- 刀柄显示 ----------
    m_checkbox_hiltDisplay = new QCheckBox(tr("显示刀柄"));
    m_checkbox_hiltDisplay->setChecked(m_tool.isShowhilt);
    form->addRow(m_checkbox_hiltDisplay);

    m_hiltParamsWidget = new QWidget();
    QFormLayout *hiltLayout = new QFormLayout(m_hiltParamsWidget);
    hiltLayout->setContentsMargins(20, 0, 0, 0);

    m_edit_hiltUpperDiameter = new UnitQLineEdit();
    m_edit_hiltUpperDiameter->setText(QString::number(m_tool.hiltUpperDiameter));
    m_edit_hiltUpperDiameter->setUnitText("mm");

    m_edit_hiltLowerDiameter = new UnitQLineEdit();
    m_edit_hiltLowerDiameter->setText(QString::number(m_tool.hiltLowerDiameter));
    m_edit_hiltLowerDiameter->setUnitText("mm");

    m_edit_hiltLength = new UnitQLineEdit();
    m_edit_hiltLength->setText(QString::number(m_tool.hiltLength));
    m_edit_hiltLength->setUnitText("mm");

    m_edit_hiltConeAngle = new UnitQLineEdit();
    m_edit_hiltConeAngle->setText(QString::number(m_tool.hiltConeAngle));
    m_edit_hiltConeAngle->setUnitText("°");

    hiltLayout->addRow(tr("上直径:"), m_edit_hiltUpperDiameter);
    hiltLayout->addRow(tr("下直径:"), m_edit_hiltLowerDiameter);
    hiltLayout->addRow(tr("刀柄长度:"), m_edit_hiltLength);
    hiltLayout->addRow(tr("刀柄锥角:"), m_edit_hiltConeAngle);

    form->addRow(m_hiltParamsWidget);
    m_hiltParamsWidget->setVisible(m_tool.isShowhilt);
    connect(m_checkbox_hiltDisplay, &QCheckBox::toggled, this, &ToolDialog::onHiltDisplayToggled);

    mainLayout->addLayout(form);

    // 底部按钮（取消左，确认右）
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 10, 0, 0);
    btnLayout->setSpacing(6);

    QPushButton *btnCancel = new QPushButton(tr("取消"));
    QPushButton *btnOk = new QPushButton(tr("确认"));
    btnCancel->setMinimumSize(90, 32);
    btnOk->setMinimumSize(90, 32);

    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);

    mainLayout->addLayout(btnLayout);

    connect(btnOk, &QPushButton::clicked, this, &ToolDialog::onAccepted);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    adjustSize();
}

void ToolDialog::onHiltDisplayToggled(bool checked)
{
    m_hiltParamsWidget->setVisible(checked);
    layout()->invalidate();
    layout()->activate();
    resize(sizeHint());
}

void ToolDialog::onAccepted()
{
    bool ok;
    m_tool.toolNumber = m_editNumber->text().trimmed();
    if (m_tool.toolNumber.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("刀具号不能为空"));
        return;
    }
    m_tool.diameter = m_editDiameter->text().toDouble(&ok);
    if (!ok || m_tool.diameter <= 0) {
        QMessageBox::warning(this, tr("错误"), tr("直径必须为正数"));
        return;
    }
    m_tool.lowerRadius = m_editLowerRadius->text().toDouble(&ok);
    if (!ok || m_tool.lowerRadius < 0) {
        QMessageBox::warning(this, tr("错误"), tr("下半径必须为非负数"));
        return;
    }
    m_tool.coneAngle = m_editConeAngle->text().toDouble(&ok);
    if (!ok || m_tool.coneAngle < 0) {
        QMessageBox::warning(this, tr("错误"), tr("锥角必须为非负数"));
        return;
    }
    m_tool.sharpCorner = m_editSharpCorner->text().toDouble(&ok);
    if (!ok || m_tool.sharpCorner < 0) {
        QMessageBox::warning(this, tr("错误"), tr("尖角必须为非负数"));
        return;
    }
    m_tool.length = m_editLength->text().toDouble(&ok);
    if (!ok || m_tool.length < 0) {
        QMessageBox::warning(this, tr("错误"), tr("长度必须为非负数"));
        return;
    }

    m_tool.isShowhilt = m_checkbox_hiltDisplay->isChecked();
    if (m_tool.isShowhilt) {
        m_tool.hiltUpperDiameter = m_edit_hiltUpperDiameter->text().toDouble(&ok);
        if (!ok || m_tool.hiltUpperDiameter < 0) {
            QMessageBox::warning(this, tr("错误"), tr("刀柄上直径必须为非负数"));
            return;
        }
        m_tool.hiltLowerDiameter = m_edit_hiltLowerDiameter->text().toDouble(&ok);
        if (!ok || m_tool.hiltLowerDiameter < 0) {
            QMessageBox::warning(this, tr("错误"), tr("刀柄下直径必须为非负数"));
            return;
        }
        m_tool.hiltLength = m_edit_hiltLength->text().toDouble(&ok);
        if (!ok || m_tool.hiltLength <= 0) {
            QMessageBox::warning(this, tr("错误"), tr("刀柄长度必须为正数"));
            return;
        }
        m_tool.hiltConeAngle = m_edit_hiltConeAngle->text().toDouble(&ok);
        if (!ok || m_tool.hiltConeAngle < 0) {
            QMessageBox::warning(this, tr("错误"), tr("刀柄锥角必须为非负数"));
            return;
        }
    }

    accept();
}
// ========== StockDialog ==========
StockDialog::StockDialog(QWidget *parent, const QString& title, const GlobalMachiningData::Stock* initData)
    : QDialog(parent)
{
    setWindowTitle(title.isEmpty() ? tr("毛坯设置") : title);

    // 类似 ToolDialog，若无传入数据则使用默认构造的 Stock（成员已有默认值，但需设置合理的尺寸）
    if (initData) {
        m_stock = *initData;
    } else {
        // 新建时赋予合理的初始值
        m_stock.type = 0;               // 默认方体
        m_stock.name = QString();       // 名称留空
        m_stock.length = 100.0;
        m_stock.width = 100.0;
        m_stock.height = 100.0;
        m_stock.centerX = 0.0;
        m_stock.centerY = 0.0;
        m_stock.centerZ = 0.0;
        m_stock.angle = 0.0;
        m_stock.diameter = 100.0;
        m_stock.cylHeight = 100.0;
        m_stock.autoCenter = true;
        m_stock.autoAngle = true;
        m_stock.autoSize = true;
    }

    this->setMaximumWidth(560);
    this->setMinimumHeight(384);
    setStyleSheet("font-size: 12pt;");
    // ----- 全局布局 -----
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(10, 10, 10, 10);
    scrollLayout->setSpacing(8);
    scrollArea->setWidget(scrollWidget);

    // ----- 毛坯名称 -----
    QHBoxLayout *nameLayout = new QHBoxLayout();
    nameLayout->setContentsMargins(10, 0, 10, 0);
    QLabel *labName = new QLabel(tr("毛坯名称"));
    m_editStockName = new QLineEdit();
    m_editStockName->setText(m_stock.name);
    nameLayout->addWidget(labName);
    nameLayout->addWidget(m_editStockName);
    nameLayout->addStretch();
    scrollLayout->addLayout(nameLayout);

    // ----- 单选框组 + 打开相机 -----
    QWidget *radioGroupWidget = new QWidget();
    QHBoxLayout *radioLayout = new QHBoxLayout(radioGroupWidget);
    radioLayout->setContentsMargins(5, 5, 5, 5);
    radioLayout->setSpacing(8);

    QLabel *labBlank = new QLabel(tr("毛坯选择"));
    labBlank->setStyleSheet("font-weight: bold; font-size: 14px;");
    radioLayout->addWidget(labBlank);

    m_rbtnBlock = new QRadioButton(tr("方体"));
    m_rbtnCylinder = new QRadioButton(tr("圆柱"));
    QButtonGroup *btnGroup = new QButtonGroup(this);
    btnGroup->addButton(m_rbtnBlock, 0);
    btnGroup->addButton(m_rbtnCylinder, 1);
    radioLayout->addWidget(m_rbtnBlock);
    radioLayout->addWidget(m_rbtnCylinder);
    radioLayout->addStretch(1);

    m_btnOpenCamera = new QPushButton(tr("打开相机"));
    m_btnOpenCamera->setMinimumSize(100, 35);
    radioLayout->addWidget(m_btnOpenCamera);
    scrollLayout->addWidget(radioGroupWidget);

    // ----- 方体参数控件 -----
    m_blockWidget = new QWidget();
    QFormLayout *blockForm = new QFormLayout(m_blockWidget);
    blockForm->setContentsMargins(2, 2, 2, 2);
    blockForm->setSpacing(6);
    blockForm->setLabelAlignment(Qt::AlignRight);

    // 长宽高一行
    QHBoxLayout *lwhLayout = new QHBoxLayout();
    lwhLayout->setSpacing(12);

    QLabel *labL = new QLabel(tr("长:"));
    m_editLength = new UnitQLineEdit();
    m_editLength->setMinimumWidth(80);
    m_editLength->setUnitText("mm");
    QHBoxLayout *lLayout = new QHBoxLayout();
    lLayout->setContentsMargins(0, 0, 0, 0);
    lLayout->setSpacing(0);
    lLayout->addWidget(labL);
    lLayout->addWidget(m_editLength);
    lwhLayout->addLayout(lLayout);

    QLabel *labW = new QLabel(tr("宽:"));
    m_editWidth = new UnitQLineEdit();
    m_editWidth->setMinimumWidth(80);
    m_editWidth->setUnitText("mm");
    QHBoxLayout *wLayout = new QHBoxLayout();
    wLayout->setContentsMargins(0, 0, 0, 0);
    wLayout->setSpacing(0);
    wLayout->addWidget(labW);
    wLayout->addWidget(m_editWidth);
    lwhLayout->addLayout(wLayout);

    QLabel *labH = new QLabel(tr("高:"));
    m_editHeight = new UnitQLineEdit();
    m_editHeight->setMinimumWidth(80);
    m_editHeight->setUnitText("mm");
    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);
    hLayout->addWidget(labH);
    hLayout->addWidget(m_editHeight);
    lwhLayout->addLayout(hLayout);

    lwhLayout->setAlignment(Qt::AlignLeft);
    lwhLayout->addStretch(1);
    blockForm->addRow("", lwhLayout);

    // x,y,z,角度一行
    QHBoxLayout *xyzALayout = new QHBoxLayout();
    xyzALayout->setSpacing(12);

    QLabel *labX = new QLabel("x:");
    m_editCenterX = new UnitQLineEdit();
    m_editCenterX->setMinimumWidth(65);
    QHBoxLayout *xLayout = new QHBoxLayout();
    xLayout->setContentsMargins(0, 0, 0, 0);
    xLayout->setSpacing(0);
    xLayout->addWidget(labX);
    xLayout->addWidget(m_editCenterX);
    xyzALayout->addLayout(xLayout);

    QLabel *labY = new QLabel("y:");
    m_editCenterY = new UnitQLineEdit();
    m_editCenterY->setMinimumWidth(65);
    QHBoxLayout *yLayout = new QHBoxLayout();
    yLayout->setContentsMargins(0, 0, 0, 0);
    yLayout->setSpacing(0);
    yLayout->addWidget(labY);
    yLayout->addWidget(m_editCenterY);
    xyzALayout->addLayout(yLayout);

    QLabel *labZ = new QLabel("z:");
    m_editCenterZ = new UnitQLineEdit();
    m_editCenterZ->setMinimumWidth(65);
    QHBoxLayout *zLayout = new QHBoxLayout();
    zLayout->setContentsMargins(0, 0, 0, 0);
    zLayout->setSpacing(0);
    zLayout->addWidget(labZ);
    zLayout->addWidget(m_editCenterZ);
    xyzALayout->addLayout(zLayout);

    QLabel *labA = new QLabel(tr("角度:"));
    m_editAngle = new UnitQLineEdit();
    m_editAngle->setMinimumWidth(65);
    m_editAngle->setUnitText("∘");
    QHBoxLayout *aLayout = new QHBoxLayout();
    aLayout->setContentsMargins(0, 0, 0, 0);
    aLayout->setSpacing(0);
    aLayout->addWidget(labA);
    aLayout->addWidget(m_editAngle);
    xyzALayout->addLayout(aLayout);

    xyzALayout->setAlignment(Qt::AlignLeft);
    xyzALayout->addStretch(1);
    blockForm->addRow("", xyzALayout);

    // ----- 圆柱参数控件 -----
    m_cylWidget = new QWidget();
    QFormLayout *cylForm = new QFormLayout(m_cylWidget);
    cylForm->setContentsMargins(2, 2, 2, 2);
    cylForm->setSpacing(6);
    cylForm->setLabelAlignment(Qt::AlignRight);

    // 直径、高一行
    QHBoxLayout *dhLayout = new QHBoxLayout();
    dhLayout->setSpacing(16);

    QLabel *labD = new QLabel(tr("直径:"));
    m_editDiameter = new UnitQLineEdit();
    m_editDiameter->setMinimumWidth(80);
    m_editDiameter->setUnitText("mm");
    QHBoxLayout *dLayout = new QHBoxLayout();
    dLayout->setContentsMargins(0, 0, 0, 0);
    dLayout->setSpacing(0);
    dLayout->addWidget(labD);
    dLayout->addWidget(m_editDiameter);
    dhLayout->addLayout(dLayout);

    QLabel *labCylH = new QLabel(tr("高:"));
    m_editCylHeight = new UnitQLineEdit();
    m_editCylHeight->setMinimumWidth(80);
    m_editCylHeight->setUnitText("mm");
    QHBoxLayout *cylHLayout = new QHBoxLayout();
    cylHLayout->setContentsMargins(0, 0, 0, 0);
    cylHLayout->setSpacing(0);
    cylHLayout->addWidget(labCylH);
    cylHLayout->addWidget(m_editCylHeight);
    dhLayout->addLayout(cylHLayout);

    dhLayout->setAlignment(Qt::AlignLeft);
    dhLayout->addStretch(1);
    cylForm->addRow("", dhLayout);

    // x,y,z一行（圆柱无角度）
    QHBoxLayout *xyzLayout = new QHBoxLayout();
    xyzLayout->setSpacing(16);

    QLabel *labCylX = new QLabel("x:");
    m_editCylCenterX = new UnitQLineEdit();
    m_editCylCenterX->setMinimumWidth(70);
    QHBoxLayout *cylXLayout = new QHBoxLayout();
    cylXLayout->setContentsMargins(0, 0, 0, 0);
    cylXLayout->setSpacing(0);
    cylXLayout->addWidget(labCylX);
    cylXLayout->addWidget(m_editCylCenterX);
    xyzLayout->addLayout(cylXLayout);

    QLabel *labCylY = new QLabel("y:");
    m_editCylCenterY = new UnitQLineEdit();
    m_editCylCenterY->setMinimumWidth(70);
    QHBoxLayout *cylYLayout = new QHBoxLayout();
    cylYLayout->setContentsMargins(0, 0, 0, 0);
    cylYLayout->setSpacing(0);
    cylYLayout->addWidget(labCylY);
    cylYLayout->addWidget(m_editCylCenterY);
    xyzLayout->addLayout(cylYLayout);

    QLabel *labCylZ = new QLabel("z:");
    m_editCylCenterZ = new UnitQLineEdit();
    m_editCylCenterZ->setMinimumWidth(70);
    QHBoxLayout *cylZLayout = new QHBoxLayout();
    cylZLayout->setContentsMargins(0, 0, 0, 0);
    cylZLayout->setSpacing(0);
    cylZLayout->addWidget(labCylZ);
    cylZLayout->addWidget(m_editCylCenterZ);
    xyzLayout->addLayout(cylZLayout);

    xyzLayout->setAlignment(Qt::AlignLeft);
    xyzLayout->addStretch(1);
    cylForm->addRow("", xyzLayout);

    // ----- 复选框（数据保存）-----
    QFormLayout *checkLayout = new QFormLayout();
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(4);

    m_checkAutoCenter = new QCheckBox(tr("移动模型至毛坯中心"), this);
    m_checkAutoCenter->setChecked(m_stock.autoCenter);
    checkLayout->addRow(m_checkAutoCenter);

    m_checkAutoAngle = new QCheckBox(tr("随毛坯旋转角度"), this);
    m_checkAutoAngle->setChecked(m_stock.autoAngle);
    checkLayout->addRow(m_checkAutoAngle);

    m_checkAutoSize = new QCheckBox(tr("自动匹配毛坯大小"), this);
    m_checkAutoSize->setChecked(m_stock.autoSize);
    checkLayout->addRow(m_checkAutoSize);

    // ----- 确认/取消按钮（左右分布）-----
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(6);

    QPushButton *btnCancel = new QPushButton(tr("取消"));
    QPushButton *btnOk = new QPushButton(tr("确认"));
    btnCancel->setMinimumSize(90, 32);
    btnOk->setMinimumSize(90, 32);

    btnLayout->addWidget(btnCancel);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);

    // ----- 添加到滚动布局 -----
    scrollLayout->addWidget(m_blockWidget);
    scrollLayout->addWidget(m_cylWidget);
    scrollLayout->addLayout(checkLayout);
    scrollLayout->addStretch(1);
    scrollLayout->addLayout(btnLayout);

    // ----- 初始状态设置 -----
    m_rbtnBlock->setChecked(m_stock.type == 0);
    m_rbtnCylinder->setChecked(m_stock.type == 1);
    updateWidgetVisibility();

    // 填充已有数据（统一使用 m_stock）
    m_editStockName->setText(m_stock.name);
    // 方体参数
    m_editLength->setText(QString::number(m_stock.length));
    m_editWidth->setText(QString::number(m_stock.width));
    m_editHeight->setText(QString::number(m_stock.height));
    m_editCenterX->setText(QString::number(m_stock.centerX));
    m_editCenterY->setText(QString::number(m_stock.centerY));
    m_editCenterZ->setText(QString::number(m_stock.centerZ));
    m_editAngle->setText(QString::number(m_stock.angle));
    // 圆柱参数
    m_editDiameter->setText(QString::number(m_stock.diameter));
    m_editCylHeight->setText(QString::number(m_stock.cylHeight));
    m_editCylCenterX->setText(QString::number(m_stock.centerX));
    m_editCylCenterY->setText(QString::number(m_stock.centerY));
    m_editCylCenterZ->setText(QString::number(m_stock.centerZ));
    // 复选框
    m_checkAutoCenter->setChecked(m_stock.autoCenter);
    m_checkAutoAngle->setChecked(m_stock.autoAngle);
    m_checkAutoSize->setChecked(m_stock.autoSize);

    // 设置焦点到名称输入框
    m_editStockName->setFocus();

    // ----- 信号连接 -----
    connect(m_rbtnBlock, &QRadioButton::toggled, this, &StockDialog::updateWidgetVisibility);
    connect(m_btnOpenCamera, &QPushButton::clicked, this, &StockDialog::onOpenCameraClicked);
    connect(btnOk, &QPushButton::clicked, this, &StockDialog::onAccepted);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}
void StockDialog::updateWidgetVisibility()
{
    bool isBlock = m_rbtnBlock->isChecked();
    m_blockWidget->setEnabled(isBlock);
    m_cylWidget->setEnabled(!isBlock);
    m_stock.type = isBlock ? 0 : 1;

    // 重置另一组输入框为默认值（与构造函数中新建时的默认值一致）
    if (isBlock) {
        // 切换到方体，重置圆柱参数为默认值
        m_editDiameter->setText("100");
        m_editCylHeight->setText("100");
        m_editCylCenterX->setText("0");
        m_editCylCenterY->setText("0");
        m_editCylCenterZ->setText("0");
    } else {
        // 切换到圆柱，重置方体参数为默认值
        m_editLength->setText("100");
        m_editWidth->setText("100");
        m_editHeight->setText("100");
        m_editCenterX->setText("0");
        m_editCenterY->setText("0");
        m_editCenterZ->setText("0");
        m_editAngle->setText("0");
    }
}
void StockDialog::onAccepted()
{
    bool ok;
    m_stock.name = m_editStockName->text().trimmed();
    if (m_stock.name.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("毛坯名称不能为空"));
        return;
    }

    m_stock.type = m_rbtnBlock->isChecked() ? 0 : 1;

    if (m_stock.type == 0) {
        m_stock.length = m_editLength->text().toDouble(&ok);
        if (!ok || m_stock.length <= 0) { QMessageBox::warning(this, tr("错误"), tr("长必须为正数")); return; }
        m_stock.width = m_editWidth->text().toDouble(&ok);
        if (!ok || m_stock.width <= 0) { QMessageBox::warning(this, tr("错误"), tr("宽必须为正数")); return; }
        m_stock.height = m_editHeight->text().toDouble(&ok);
        if (!ok || m_stock.height <= 0) { QMessageBox::warning(this, tr("错误"), tr("高必须为正数")); return; }
        m_stock.centerX = m_editCenterX->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("中心X无效")); return; }
        m_stock.centerY = m_editCenterY->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("中心Y无效")); return; }
        m_stock.centerZ = m_editCenterZ->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("中心Z无效")); return; }
        m_stock.angle = m_editAngle->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("角度无效")); return; }
    } else {
        m_stock.diameter = m_editDiameter->text().toDouble(&ok);
        if (!ok || m_stock.diameter <= 0) { QMessageBox::warning(this, tr("错误"), tr("直径必须为正数")); return; }
        m_stock.cylHeight = m_editCylHeight->text().toDouble(&ok);
        if (!ok || m_stock.cylHeight <= 0) { QMessageBox::warning(this, tr("错误"), tr("高度必须为正数")); return; }
        m_stock.centerX = m_editCylCenterX->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("中心X无效")); return; }
        m_stock.centerY = m_editCylCenterY->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("中心Y无效")); return; }
        m_stock.centerZ = m_editCylCenterZ->text().toDouble(&ok);
        if (!ok) { QMessageBox::warning(this, tr("错误"), tr("中心Z无效")); return; }
        m_stock.angle = 0.0;
    }

    // 保存复选框状态
    m_stock.autoCenter = m_checkAutoCenter->isChecked();
    m_stock.autoAngle  = m_checkAutoAngle->isChecked();
    m_stock.autoSize   = m_checkAutoSize->isChecked();

    accept();
}

void StockDialog::onOpenCameraClicked()
{
    // 如果相机窗口已存在，激活并前置
    if (m_cameraWid && !m_cameraWid.isNull()) {
        m_cameraWid->activateWindow();
        m_cameraWid->raise();
        return;
    }

    // 创建相机窗口
    m_cameraWid = new CameraWid(this);
    m_cameraWid->setWindowModality(Qt::ApplicationModal);
    m_cameraWid->setAttribute(Qt::WA_DeleteOnClose);
    m_cameraWid->setWindowFlags(Qt::Dialog);
    m_cameraWid->show();

    // 连接识别完成信号
    connect(m_cameraWid.data(), &CameraWid::blankInfoDetected,
            this, [this](const BlankInfo& info) {
                // 根据识别类型填充界面
                if (info.type == STOCK_CUBE) {
                    m_rbtnBlock->setChecked(true);
                    m_editLength->setText(QString::number(info.width,  'f', 3));
                    m_editWidth ->setText(QString::number(info.height, 'f', 3));
                    m_editHeight->setText(QString::number(info.depth,  'f', 3));
                    m_editCenterX->setText(QString::number(info.center.x, 'f', 3));
                    m_editCenterY->setText(QString::number(info.center.y, 'f', 3));
                    m_editCenterZ->setText(QString::number(info.center.z, 'f', 3));
                    m_editAngle->setText(QString::number(info.angle, 'f', 3));
                } else if (info.type == STOCK_CYLINDER) {
                    m_rbtnCylinder->setChecked(true);
                    m_editDiameter ->setText(QString::number(info.radius * 2.0, 'f', 3));
                    m_editCylHeight->setText(QString::number(info.depth, 'f', 3));
                    m_editCylCenterX->setText(QString::number(info.center.x, 'f', 3));
                    m_editCylCenterY->setText(QString::number(info.center.y, 'f', 3));
                    m_editCylCenterZ->setText(QString::number(info.center.z, 'f', 3));
                }
                // 自动生成毛坯名称
                m_editStockName->setText(tr("相机识别%1-%2")
                                             .arg(info.type == STOCK_CUBE ? tr("方体") : tr("圆柱"))
                                             .arg(QDateTime::currentDateTime().toString("hhmmss")));
                m_cameraWid->close();
            });
}
// ========== CreatePathDialog ==========
CreatePathDialog::CreatePathDialog(QWidget *parent)
    : QDialog(parent)
{
    initUI();
    setStyleSheet("font-size: 12pt;");
    setWindowTitle(tr("创建加工路径"));
    resize(1000, 750);

    refreshCoordinateCombo();
    refreshToolCombo();
    refreshStockCombo();
    connect(m_combox_Blank, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreatePathDialog::onComboxBlankClicked);
    refreshComponentCombo();
}

CreatePathDialog::~CreatePathDialog() {}

void CreatePathDialog::initUI()
{
    m_vlay_this = new QVBoxLayout(this);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *scrollContent = new QWidget();
    m_vlay_scrArea = new QVBoxLayout(scrollContent);
    m_vlay_scrArea->setAlignment(Qt::AlignTop);

    // 参数名称和工位
    lab_name = new QLabel(tr("参数名称:"));
    edit_name = new QLineEdit();
    hlay_name = new QHBoxLayout();
    hlay_name->addWidget(lab_name);
    hlay_name->addWidget(edit_name);

    lab_work = new QLabel(tr("工位:"));
    combox_work = new QComboBox();
    combox_work->addItems({tr("默认工位1"), tr("默认工位2"), tr("默认工位3")});
    hlay_work = new QHBoxLayout();
    hlay_work->addWidget(lab_work, 1);
    hlay_work->addWidget(combox_work, 7);
    hlay_work->addStretch(1);

    lab_roughFinish = new QLabel(tr("加工路径设置"));
    m_roughFinish = new QVBoxLayout();
    m_roughFinish->setContentsMargins(10, 0, 0, 0);
    m_roughFinish->addWidget(lab_roughFinish);

    // 坐标系
    m_lab_Coordinate = new QLabel(tr("坐标系"));
    m_combox_Coordinate = new QComboBox();
    m_btn_Coordinate = new QToolButton();
    m_btn_Coordinate->setFixedSize(32, 32);
    m_btn_Coordinate->setPopupMode(QToolButton::InstantPopup);
    m_btn_Coordinate->setStyleSheet("QToolButton{ border-image: url(:/resource/images/Settings/axis.png); }");
    QMenu *coordinateMenu = new QMenu(this);
    m_actionNewCoordinate = coordinateMenu->addAction(tr("新建坐标系"));
    m_actionEditCoordinate = coordinateMenu->addAction(tr("编辑当前坐标系"));
    m_actionDeleteCoordinate = coordinateMenu->addAction(tr("删除当前坐标系"));
    m_btn_Coordinate->setMenu(coordinateMenu);
    connect(m_actionNewCoordinate, &QAction::triggered, this, &CreatePathDialog::onNewCoordinate);
    connect(m_actionEditCoordinate, &QAction::triggered, this, &CreatePathDialog::onEditCoordinate);
    connect(m_actionDeleteCoordinate, &QAction::triggered, this, &CreatePathDialog::onDeleteCoordinate);
    connect(m_combox_Coordinate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CreatePathDialog::onCoordinateSelected);
    m_hlay_Coordinate = new QHBoxLayout();
    m_hlay_Coordinate->addWidget(m_lab_Coordinate, 2);
    m_hlay_Coordinate->addWidget(m_combox_Coordinate, 5);
    m_hlay_Coordinate->addWidget(m_btn_Coordinate, 1);

    // 刀具
    m_lab_cuttingTool = new QLabel(tr("刀具"));
    m_combox_cuttingTool = new QComboBox();
    m_btn_cuttingTool = new QToolButton();
    m_btn_cuttingTool->setFixedSize(32, 32);
    m_btn_cuttingTool->setPopupMode(QToolButton::InstantPopup);
    m_btn_cuttingTool->setStyleSheet("QToolButton{ border-image: url(:/resource/images/Settings/cutting.png); }");
    QMenu *toolMenu = new QMenu(this);
    m_actionNewTool = toolMenu->addAction(tr("新建刀具"));
    m_actionEditTool = toolMenu->addAction(tr("编辑当前刀具"));
    m_actionDeleteTool = toolMenu->addAction(tr("删除当前刀具"));
    m_btn_cuttingTool->setMenu(toolMenu);
    connect(m_actionNewTool, &QAction::triggered, this, &CreatePathDialog::onNewTool);
    connect(m_actionEditTool, &QAction::triggered, this, &CreatePathDialog::onEditTool);
    connect(m_actionDeleteTool, &QAction::triggered, this, &CreatePathDialog::onDeleteTool);
    connect(m_combox_cuttingTool, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CreatePathDialog::onToolSelected);
    m_hlay_cuttingTool = new QHBoxLayout();
    m_hlay_cuttingTool->addWidget(m_lab_cuttingTool, 2);
    m_hlay_cuttingTool->addWidget(m_combox_cuttingTool, 5);
    m_hlay_cuttingTool->addWidget(m_btn_cuttingTool, 1);

    // 部件
    m_lab_Component = new QLabel(tr("指定部件"));
    m_combox_Component = new QComboBox();
    m_btn_Component = new QPushButton();
    m_btn_Component->setFixedSize(32, 32);
    m_btn_Component->setStyleSheet("QPushButton{ border-image: url(:/resource/images/Settings/scrisshair.png); }");
    connect(m_btn_Component, &QPushButton::clicked, this, &CreatePathDialog::onSelectComponent);
    m_hlay_Component = new QHBoxLayout();
    m_hlay_Component->addWidget(m_lab_Component, 2);
    m_hlay_Component->addWidget(m_combox_Component, 5);
    m_hlay_Component->addWidget(m_btn_Component, 1);

    // 毛坯
    m_lab_Blank = new QLabel(tr("指定毛坯"));
    m_combox_Blank = new QComboBox();
    m_btn_Blank = new QToolButton();
    m_btn_Blank->setFixedSize(32, 32);
    m_btn_Blank->setPopupMode(QToolButton::InstantPopup);
    m_btn_Blank->setStyleSheet("QToolButton{ border-image: url(:/resource/images/Settings/scrisshair.png); }");
    QMenu *blankMenu = new QMenu(this);
    m_actionNewBlank = blankMenu->addAction(tr("新建毛坯"));
    m_actionEditBlank = blankMenu->addAction(tr("编辑当前毛坯"));
    m_actionDeleteBlank = blankMenu->addAction(tr("删除当前毛坯"));
    m_btn_Blank->setMenu(blankMenu);
    connect(m_actionNewBlank, &QAction::triggered, this, &CreatePathDialog::onNewStock);
    connect(m_actionEditBlank, &QAction::triggered, this, &CreatePathDialog::onEditStock);
    connect(m_actionDeleteBlank, &QAction::triggered, this, &CreatePathDialog::onDeleteStock);
    connect(m_combox_Blank, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CreatePathDialog::onStockSelected);
    m_hlay_Blank = new QHBoxLayout();
    m_hlay_Blank->addWidget(m_lab_Blank, 2);
    m_hlay_Blank->addWidget(m_combox_Blank, 5);
    m_hlay_Blank->addWidget(m_btn_Blank, 1);

    // 检查/边界（预留）
    m_lab_Check = new QLabel(tr("指定检查"));
    m_combox_Check = new QComboBox();
    m_hlay_Check = new QHBoxLayout();
    m_hlay_Check->addWidget(m_lab_Check, 2);
    m_hlay_Check->addWidget(m_combox_Check, 5);

    m_lab_Boundary = new QLabel(tr("指定边界"));
    m_combox_Boundary = new QComboBox();
    m_hlay_Boundary = new QHBoxLayout();
    m_hlay_Boundary->addWidget(m_lab_Boundary, 2);
    m_hlay_Boundary->addWidget(m_combox_Boundary, 5);
    // 第一行组合
    QHBoxLayout *tmp1 = new QHBoxLayout();
    tmp1->setContentsMargins(10, 0, 10, 0);
    tmp1->addLayout(m_hlay_Coordinate, 4);
    tmp1->addStretch(1);
    tmp1->addLayout(m_hlay_cuttingTool, 4);
    tmp1->addStretch(1);

    QHBoxLayout *tmp2 = new QHBoxLayout();
    tmp2->setContentsMargins(10, 0, 10, 0);
    tmp2->addLayout(m_hlay_Component, 4);
    tmp2->addStretch(1);
    tmp2->addLayout(m_hlay_Blank, 4);
    tmp2->addStretch(1);

    QHBoxLayout *tmp3 = new QHBoxLayout();
    tmp3->setContentsMargins(10, 0, 10, 0);
    tmp3->addLayout(m_hlay_Check, 4);
    tmp3->addStretch(1);
    tmp3->addLayout(m_hlay_Boundary, 4);
    tmp3->addStretch(1);

    // 顶部/底部/层高
    m_lab_top = new QLabel(tr("指定顶部"));
    m_ledit_top = new UnitQLineEdit();
    m_ledit_top->setUnitText("mm");
    m_ledit_top->setText("+∞");
    m_hlay_top = new QHBoxLayout();
    m_hlay_top->addWidget(m_lab_top, 3);
    m_hlay_top->addWidget(m_ledit_top, 2);
    m_hlay_top->addStretch(3);

    m_lab_bottom = new QLabel(tr("指定底部"));
    m_ledit_bottom = new UnitQLineEdit();
    m_ledit_bottom->setUnitText("mm");
    m_ledit_bottom->setText("-∞");
    m_hlay_bottom = new QHBoxLayout();
    m_hlay_bottom->addWidget(m_lab_bottom, 3);
    m_hlay_bottom->addWidget(m_ledit_bottom, 2);
    m_hlay_bottom->addStretch(3);

    m_lab_height = new QLabel(tr("每层高度"));
    m_ledit_height = new UnitQLineEdit();
    m_ledit_height->setUnitText("mm");
    m_ledit_height->setText("4");
    m_hlay_height = new QHBoxLayout();
    m_hlay_height->addWidget(m_lab_height, 3);
    m_hlay_height->addWidget(m_ledit_height, 2);
    m_hlay_height->addStretch(3);

    QHBoxLayout *tmp4 = new QHBoxLayout();
    tmp4->setContentsMargins(10, 0, 10, 0);
    tmp4->addLayout(m_hlay_top);
    tmp4->addLayout(m_hlay_bottom);
    tmp4->addLayout(m_hlay_height);
    tmp4->addStretch();

    // 切削样式/步距/方向
    m_lab_cuttingStyle = new QLabel(tr("切削样式"));
    m_combox_cuttingStyle = new QComboBox();
    m_combox_cuttingStyle->addItems({tr("跟随周边"), tr("跟随部件")});
    m_hlay_cuttingStyle = new QHBoxLayout();
    m_hlay_cuttingStyle->addWidget(m_lab_cuttingStyle, 3);
    m_hlay_cuttingStyle->addWidget(m_combox_cuttingStyle, 2);
    m_hlay_cuttingStyle->addStretch(3);

    m_lab_pace = new QLabel(tr("切削步距"));
    m_ledit_pace = new UnitQLineEdit();
    m_ledit_pace->setUnitText("mm");
    m_ledit_pace->setText("2");
    m_hlay_pace = new QHBoxLayout();
    m_hlay_pace->addWidget(m_lab_pace, 3);
    m_hlay_pace->addWidget(m_ledit_pace, 2);
    m_hlay_pace->addStretch(3);

    m_lab_cuttingDirection = new QLabel(tr("切削方向"));
    m_combox_cuttingDirection = new QComboBox();
    m_combox_cuttingDirection->addItems({tr("顺铣"), tr("逆铣")});
    m_hlay_cuttingDirection = new QHBoxLayout();
    m_hlay_cuttingDirection->addWidget(m_lab_cuttingDirection, 3);
    m_hlay_cuttingDirection->addWidget(m_combox_cuttingDirection, 2);
    m_hlay_cuttingDirection->addStretch(3);

    QHBoxLayout *tmpcutting = new QHBoxLayout();
    tmpcutting->setContentsMargins(10, 0, 0, 0);
    tmpcutting->addLayout(m_hlay_cuttingStyle);
    tmpcutting->addLayout(m_hlay_pace);
    tmpcutting->addLayout(m_hlay_cuttingDirection);
    tmpcutting->addStretch();

    // 下刀设置（下刀高度后跟螺旋角度和螺旋半径）
    m_lab_cuttingEnterSettings = new QLabel(tr("下刀设置"));
    m_combox_cuttingEnterSettings = new QComboBox();
    m_combox_cuttingEnterSettings->addItems({ tr("螺旋"),tr("直接")});
    connect(m_combox_cuttingEnterSettings, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CreatePathDialog::onCuttingEnterSettingsChanged);
    m_hlay_cuttingEnterSettings = new QHBoxLayout();
    m_hlay_cuttingEnterSettings->addWidget(m_lab_cuttingEnterSettings, 3);
    m_hlay_cuttingEnterSettings->addWidget(m_combox_cuttingEnterSettings, 3);
    m_hlay_cuttingEnterSettings->addStretch(3);

    m_lab_cuttingHeight = new QLabel(tr("下刀高度"));
    m_ledit_cuttingHeight = new UnitQLineEdit();
    m_ledit_cuttingHeight->setUnitText("mm");
    m_ledit_cuttingHeight->setText("5");
    m_hlay_cuttingHeight = new QHBoxLayout();
    m_hlay_cuttingHeight->addWidget(m_lab_cuttingHeight, 3);
    m_hlay_cuttingHeight->addWidget(m_ledit_cuttingHeight, 2);
    m_hlay_cuttingHeight->addStretch(3);

    // 螺旋角度容器
    m_spiralSlopeContainer = new QWidget();
    m_lab_spiralSlope = new QLabel(tr("螺旋角度"));
    m_ledit_spiralSlope = new UnitQLineEdit();
    m_ledit_spiralSlope->setUnitText(tr("度"));
    m_ledit_spiralSlope->setText("5");
    m_hlay_spiralSlope = new QHBoxLayout(m_spiralSlopeContainer);
    m_hlay_spiralSlope->addWidget(m_lab_spiralSlope, 3);
    m_hlay_spiralSlope->addWidget(m_ledit_spiralSlope, 2);
    m_hlay_spiralSlope->addStretch(3);

    // 螺旋半径容器
    m_spiralRadiusContainer = new QWidget();
    m_lab_spiralRadius = new QLabel(tr("下刀半径"));
    m_ledit_spiralRadius = new UnitQLineEdit();
    m_ledit_spiralRadius->setUnitText("%");
    m_ledit_spiralRadius->setText("80");
    m_hlay_spiralRadius = new QHBoxLayout(m_spiralRadiusContainer);
    m_hlay_spiralRadius->addWidget(m_lab_spiralRadius, 3);
    m_hlay_spiralRadius->addWidget(m_ledit_spiralRadius, 2);
    m_hlay_spiralRadius->addStretch(3);

    m_spiralSlopeContainer->setVisible(true);
    m_spiralRadiusContainer->setVisible(true);

    QHBoxLayout *tmpcuttingsetting1 = new QHBoxLayout();
    tmpcuttingsetting1->setContentsMargins(10, 0, 10, 0);
    tmpcuttingsetting1->addLayout(m_hlay_cuttingEnterSettings);
    tmpcuttingsetting1->addLayout(m_hlay_cuttingHeight);
    tmpcuttingsetting1->addWidget(m_spiralSlopeContainer);
    tmpcuttingsetting1->addWidget(m_spiralRadiusContainer);
    tmpcuttingsetting1->addStretch();

    // 进刀设置
    m_lab_cuttingFeedSetting = new QLabel(tr("进刀设置"));
    m_combox_cuttingFeedSetting = new QComboBox();
    m_combox_cuttingFeedSetting->addItems({tr("圆弧"), tr("直线")});
    m_hlay_cuttingFeedSetting = new QHBoxLayout();
    m_hlay_cuttingFeedSetting->addWidget(m_lab_cuttingFeedSetting, 3);
    m_hlay_cuttingFeedSetting->addWidget(m_combox_cuttingFeedSetting, 3);
    m_hlay_cuttingFeedSetting->addStretch(3);

    m_lab_arcRadius = new QLabel(tr("圆弧半径"));
    m_ledit_arcRadius = new UnitQLineEdit();
    m_ledit_arcRadius->setText("10");
    m_ledit_arcRadius->setUnitText("mm");
    m_hlay_arcRadius = new QHBoxLayout();
    m_hlay_arcRadius->addWidget(m_lab_arcRadius, 3);
    m_hlay_arcRadius->addWidget(m_ledit_arcRadius, 2);
    m_hlay_arcRadius->addStretch(3);

    m_lab_lineLength = new QLabel(tr("直线长度"));
    m_ledit_lineLength = new UnitQLineEdit();
    m_ledit_lineLength->setUnitText("mm");
    m_hlay_lineLength = new QHBoxLayout();
    m_hlay_lineLength->addWidget(m_lab_lineLength, 3);
    m_hlay_lineLength->addWidget(m_ledit_lineLength, 2);
    m_hlay_lineLength->addStretch(3);

    QHBoxLayout *tmpcuttingsetting = new QHBoxLayout();
    tmpcuttingsetting->setContentsMargins(10, 0, 10, 0);
    tmpcuttingsetting->addLayout(m_hlay_cuttingFeedSetting);
    tmpcuttingsetting->addLayout(m_hlay_arcRadius);
    tmpcuttingsetting->addLayout(m_hlay_lineLength);
    tmpcuttingsetting->addStretch();

    // 退刀设置
    m_lab_returnCutting = new QLabel(tr("退刀设置"));
    m_combox_returnCutting = new QComboBox();
    m_combox_returnCutting->addItems({tr("与进刀相同")});
    m_combox_returnCutting->setEnabled(false);
    m_checkbox_returnCutting = new QCheckBox();
    connect(m_checkbox_returnCutting, &QCheckBox::toggled, this, [=](bool checked){
        m_combox_returnCutting->setEnabled(checked);
    });
    m_hlay_returnCutting = new QHBoxLayout();
    m_hlay_returnCutting->addWidget(m_lab_returnCutting);
    m_hlay_returnCutting->addWidget(m_combox_returnCutting);
    m_hlay_returnCutting->addWidget(m_checkbox_returnCutting);
    m_hlay_returnCutting->addStretch();

    // 安全平面
    m_lab_interactivePlane = new QLabel(tr("安全平面"));
    m_combox_interactivePlane = new QComboBox();
    m_combox_interactivePlane->addItems({tr("平面"), tr("柱"), tr("方块"), tr("球")});
    m_hlay_interactivePlane = new QHBoxLayout();
    m_hlay_interactivePlane->addWidget(m_lab_interactivePlane, 2);
    m_hlay_interactivePlane->addWidget(m_combox_interactivePlane, 5);
    m_hlay_interactivePlane->addStretch(2);

    m_lab_interactiveHeight = new QLabel(tr("安全高度"));
    m_ledit_interactiveHeight = new UnitQLineEdit();
    m_ledit_interactiveHeight->setUnitText("mm");
    m_ledit_interactiveHeight->setText("50");
    m_hlay_interactiveHeight = new QHBoxLayout();
    m_hlay_interactiveHeight->addWidget(m_lab_interactiveHeight, 3);
    m_hlay_interactiveHeight->addWidget(m_ledit_interactiveHeight, 5);
    m_hlay_interactiveHeight->addStretch(2);

    QHBoxLayout *tmpinteractive = new QHBoxLayout();
    tmpinteractive->setContentsMargins(10, 0, 10, 0);
    tmpinteractive->addLayout(m_hlay_interactivePlane);
    tmpinteractive->addLayout(m_hlay_interactiveHeight);
    tmpinteractive->addStretch();

    // 余量公差
    m_lab_Margin2Tolerance = new QLabel(tr("余量、公差"));
    m_lab_sideMargin = new QLabel(tr("侧部余量"));
    m_ledit_sideMargin = new UnitQLineEdit();
    m_ledit_sideMargin->setUnitText("mm");
    m_ledit_sideMargin->setText("0.1");
    m_hlay_sideMargin = new QHBoxLayout();
    m_hlay_sideMargin->addWidget(m_lab_sideMargin, 3);
    m_hlay_sideMargin->addWidget(m_ledit_sideMargin, 3);
    m_hlay_sideMargin->addStretch(3);

    m_lab_bottomMargin = new QLabel(tr("底部余量"));
    m_ledit_bottomMargin = new UnitQLineEdit();
    m_ledit_bottomMargin->setUnitText("mm");
    m_ledit_bottomMargin->setText("0.1");
    m_hlay_bottomMargin = new QHBoxLayout();
    m_hlay_bottomMargin->addWidget(m_lab_bottomMargin, 3);
    m_hlay_bottomMargin->addWidget(m_ledit_bottomMargin, 3);
    m_hlay_bottomMargin->addStretch(3);

    m_lab_Tolerance = new QLabel(tr("公差"));
    m_ledit_Tolerance = new UnitQLineEdit();
    m_ledit_Tolerance->setUnitText("mm");
    m_ledit_Tolerance->setText("0.01");
    m_hlay_Tolerance = new QHBoxLayout();
    m_hlay_Tolerance->addStretch();
    m_hlay_Tolerance->addWidget(m_lab_Tolerance, 3);
    m_hlay_Tolerance->addWidget(m_ledit_Tolerance, 3);
    m_hlay_Tolerance->addStretch(3);

    QHBoxLayout *tmpMargin = new QHBoxLayout();
    tmpMargin->setContentsMargins(20, 0, 20, 0);
    tmpMargin->addLayout(m_hlay_sideMargin, 3);
    tmpMargin->addLayout(m_hlay_bottomMargin, 3);
    tmpMargin->addLayout(m_hlay_Tolerance, 3);
    tmpMargin->addStretch(3);

    m_vlay_Margin2Tolerance = new QVBoxLayout();
    m_vlay_Margin2Tolerance->addWidget(m_lab_Margin2Tolerance);
    m_vlay_Margin2Tolerance->addLayout(tmpMargin);

    // 速度
    m_lab_speed = new QLabel(tr("速度"));
    m_lab_makeFeed = new QLabel(tr("下刀"));
    m_ledit_makeFeed = new UnitQLineEdit();
    m_ledit_makeFeed->setUnitText("mm/分");
    m_ledit_makeFeed->setText("1000");
    m_hlay_makeFeed = new QHBoxLayout();
    m_hlay_makeFeed->addWidget(m_lab_makeFeed);
    m_hlay_makeFeed->addWidget(m_ledit_makeFeed);

    m_lab_cuttingFeed = new QLabel(tr("进刀"));
    m_ledit_cuttingFeed = new UnitQLineEdit();
    m_ledit_cuttingFeed->setUnitText("mm/分");
    m_ledit_cuttingFeed->setText("1000");
    m_hlay_cuttingFeed = new QHBoxLayout();
    m_hlay_cuttingFeed->addWidget(m_lab_cuttingFeed);
    m_hlay_cuttingFeed->addWidget(m_ledit_cuttingFeed);

    m_lab_processing = new QLabel(tr("加工"));
    m_ledit_processing = new UnitQLineEdit();
    m_ledit_processing->setUnitText("mm/分");
    m_ledit_processing->setText("2000");
    m_hlay_processing = new QHBoxLayout();
    m_hlay_processing->addWidget(m_lab_processing);
    m_hlay_processing->addWidget(m_ledit_processing);

    QHBoxLayout *tmpSpeed = new QHBoxLayout();
    tmpSpeed->setContentsMargins(20, 0, 0, 0);
    tmpSpeed->addLayout(m_hlay_makeFeed, 3);
    tmpSpeed->addLayout(m_hlay_cuttingFeed, 3);
    tmpSpeed->addLayout(m_hlay_processing, 3);
    tmpSpeed->addStretch(3);

    m_vlay_speed = new QVBoxLayout();
    m_vlay_speed->addWidget(m_lab_speed);
    m_vlay_speed->addLayout(tmpSpeed);

    // 底部按钮
    m_btn_gcode = new QPushButton(tr("生成刀路"));
    m_btn_save = new QPushButton(tr("保存"));
    m_btn_cancel = new QPushButton(tr("取消"));
    m_btn = new QHBoxLayout();
    m_btn->setAlignment(Qt::AlignRight);
    m_btn->addWidget(m_btn_gcode);
    m_btn->addSpacing(10);
    m_btn->addWidget(m_btn_save);
    m_btn->addSpacing(10);
    m_btn->addWidget(m_btn_cancel);
    connect(m_btn_gcode, &QPushButton::clicked, this, &CreatePathDialog::onGenerateGCode);
    connect(m_btn_save, &QPushButton::clicked, this, &CreatePathDialog::onSave);
    connect(m_btn_cancel, &QPushButton::clicked, this, &CreatePathDialog::onCancel);

    // 分割线
    QLabel *lab_solid1 = new QLabel(); lab_solid1->setFixedHeight(1); lab_solid1->setStyleSheet("background:#d1d1d1;");
    QLabel *lab_solid2 = new QLabel(); lab_solid2->setFixedHeight(1); lab_solid2->setStyleSheet("background:#d1d1d1;");
    QLabel *lab_solid3 = new QLabel(); lab_solid3->setFixedHeight(1); lab_solid3->setStyleSheet("background:#d1d1d1;");
    QLabel *lab_solid4 = new QLabel(); lab_solid4->setFixedHeight(1); lab_solid4->setStyleSheet("background:#d1d1d1;");
    QLabel *lab_solid5 = new QLabel(); lab_solid5->setFixedHeight(1); lab_solid5->setStyleSheet("background:#d1d1d1;");

    m_vlay_scrArea->setSpacing(10);
    m_vlay_scrArea->addLayout(hlay_name);
    m_vlay_scrArea->addLayout(hlay_work);
    m_vlay_scrArea->addWidget(lab_solid5);
    m_vlay_scrArea->addLayout(m_roughFinish);
    m_vlay_scrArea->addLayout(tmp1);
    m_vlay_scrArea->addLayout(tmp2);
    m_vlay_scrArea->addLayout(tmp3);
    m_vlay_scrArea->addLayout(tmp4);
    m_vlay_scrArea->addLayout(tmpcutting);
    m_vlay_scrArea->addWidget(lab_solid1);
    m_vlay_scrArea->addLayout(tmpcuttingsetting1);
    m_vlay_scrArea->addLayout(tmpcuttingsetting);
    m_vlay_scrArea->addLayout(m_hlay_returnCutting);
    m_vlay_scrArea->addWidget(lab_solid2);
    m_vlay_scrArea->addLayout(tmpinteractive);
    m_vlay_scrArea->addWidget(lab_solid3);
    m_vlay_scrArea->addLayout(m_vlay_Margin2Tolerance);
    m_vlay_scrArea->addWidget(lab_solid4);
    m_vlay_scrArea->addLayout(m_vlay_speed);
    m_vlay_scrArea->addStretch();

    m_scrollArea->setWidget(scrollContent);
    m_vlay_this->addWidget(m_scrollArea, 9);
    m_vlay_this->addLayout(m_btn, 1);
}


void CreatePathDialog::setDefaultName(const QString &name)
{
    if (edit_name) edit_name->setText(name);
}
void CreatePathDialog::loadFromParams(const QString &paramName, const GlobalMachiningData::MachiningParams &params)
{
    m_editMode = true;
    m_editParamName = paramName;

    // 填充参数名称和工位
    edit_name->setText(params.paramName);
    combox_work->setCurrentText(params.workStation);

    // 坐标系
    int coordIdx = m_combox_Coordinate->findData(params.currentCoordinateName);
    if (coordIdx >= 0) m_combox_Coordinate->setCurrentIndex(coordIdx);

    // 刀具
    int toolIdx = m_combox_cuttingTool->findData(params.currentToolNumber);
    if (toolIdx >= 0) m_combox_cuttingTool->setCurrentIndex(toolIdx);

    // 部件
    if (!params.componentModelPath.isEmpty()) {
        QString fileName = QFileInfo(params.componentModelPath).fileName();
        if (m_combox_Component->findText(fileName) == -1)
            m_combox_Component->addItem(fileName, params.componentModelPath);
        m_combox_Component->setCurrentText(fileName);
        GlobalMachiningData::instance().componentModelPath = params.componentModelPath;
    }

    // 毛坯
    int stockIdx = m_combox_Blank->findData(params.currentStockName);
    if (stockIdx >= 0) m_combox_Blank->setCurrentIndex(stockIdx);

    // 顶部/底部/层高
    m_ledit_top->setText(params.top);
    m_ledit_bottom->setText(params.bottom);
    m_ledit_height->setText(QString::number(params.layerHeight));

    // 切削样式/步距/方向
    m_combox_cuttingStyle->setCurrentIndex(params.cuttingStyle);
    m_ledit_pace->setText(QString::number(params.stepover));
    m_combox_cuttingDirection->setCurrentIndex(params.cuttingDirection);

    // 下刀设置
    m_combox_cuttingEnterSettings->setCurrentIndex(params.enterMode);
    m_ledit_cuttingHeight->setText(QString::number(params.cuttingHeight));
    m_ledit_spiralSlope->setText(QString::number(params.spiralSlope));
    m_ledit_spiralRadius->setText(QString::number(params.spiralRadius));

    // 进刀设置
    m_combox_cuttingFeedSetting->setCurrentIndex(params.feedMode);
    m_ledit_arcRadius->setText(QString::number(params.arcRadius));
    m_ledit_lineLength->setText(QString::number(params.lineLength));

    // 退刀设置
    m_checkbox_returnCutting->setChecked(params.returnCustom);
    m_combox_returnCutting->setCurrentIndex(params.returnMode);
    m_combox_returnCutting->setEnabled(params.returnCustom);

    // 安全平面
    m_combox_interactivePlane->setCurrentIndex(params.safePlaneType);
    m_ledit_interactiveHeight->setText(QString::number(params.safeHeight));

    // 余量/公差
    m_ledit_sideMargin->setText(QString::number(params.sideMargin));
    m_ledit_bottomMargin->setText(QString::number(params.bottomMargin));
    m_ledit_Tolerance->setText(QString::number(params.tolerance));

    // 速度
    m_ledit_makeFeed->setText(QString::number(params.makeFeed));
    m_ledit_cuttingFeed->setText(QString::number(params.cuttingFeed));
    m_ledit_processing->setText(QString::number(params.processingSpeed));
}
// ========== 坐标系操作（已移除 DetectedObject 逻辑） ==========
void CreatePathDialog::refreshCoordinateCombo()
{
    QString currentName = m_combox_Coordinate->currentData().toString();
    m_combox_Coordinate->clear();
    m_combox_Coordinate->addItem(tr("请选择坐标系"), QString());  // data 存储空字符串

    const auto& coords = GlobalMachiningData::instance().getCoordinates();
    for (const auto& coord : coords) {
        m_combox_Coordinate->addItem(coord.name, coord.name);  // 直接存储名称字符串
    }

    if (!currentName.isEmpty()) {
        int idx = m_combox_Coordinate->findData(currentName);
        if (idx >= 0) m_combox_Coordinate->setCurrentIndex(idx);
    }
}

void CreatePathDialog::onNewCoordinate()
{
    CoordinateDialog dlg(this, tr("新建坐标系"));
    if (dlg.exec() == QDialog::Accepted) {
        GlobalMachiningData::instance().addCoordinate(dlg.getCoordinate());
        refreshCoordinateCombo();
        m_combox_Coordinate->setCurrentText(dlg.getCoordinate().name);
    }
}

void CreatePathDialog::onEditCoordinate()
{
    QString name = m_combox_Coordinate->currentData().toString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个坐标系"));
        return;
    }
    auto coord = GlobalMachiningData::instance().getCoordinateByName(name);
    CoordinateDialog dlg(this, tr("编辑坐标系"), &coord);
    if (dlg.exec() == QDialog::Accepted) {
        int idx = GlobalMachiningData::instance().getCoordinateIndexByName(name);
        GlobalMachiningData::instance().updateCoordinate(idx, dlg.getCoordinate());
        refreshCoordinateCombo();
        m_combox_Coordinate->setCurrentText(dlg.getCoordinate().name);
    }
}

void CreatePathDialog::onDeleteCoordinate()
{
    QString name = m_combox_Coordinate->currentData().toString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个坐标系"));
        return;
    }
    if (QMessageBox::question(this, tr("确认"), tr("确定删除坐标系 '%1' 吗?").arg(name))
        == QMessageBox::Yes) {
        int idx = GlobalMachiningData::instance().getCoordinateIndexByName(name);
        GlobalMachiningData::instance().deleteCoordinate(idx);
        refreshCoordinateCombo();
    }
}

void CreatePathDialog::onCoordinateSelected(int index)
{
    if (index > 0) {
        QString name = m_combox_Coordinate->currentData().toString();
        GlobalMachiningData::instance().currentCoordinateName = name;
        qDebug() << "坐标系选中:" << name;
    } else {
        GlobalMachiningData::instance().currentCoordinateName.clear();
    }
}

// ========== 刀具操作（保持不变） ==========
void CreatePathDialog::refreshToolCombo()
{
    QString current = m_combox_cuttingTool->currentData().toString();
    m_combox_cuttingTool->clear();
    m_combox_cuttingTool->addItem(tr("请选择刀具"), "");
    for (const auto& tool : GlobalMachiningData::instance().getTools()) {
        m_combox_cuttingTool->addItem(tool.toolNumber, tool.toolNumber);
    }
    int idx = m_combox_cuttingTool->findData(current);
    if (idx >= 0) m_combox_cuttingTool->setCurrentIndex(idx);
}

void CreatePathDialog::onNewTool()
{
    ToolDialog dlg(this, tr("新建刀具"));
    if (dlg.exec() == QDialog::Accepted) {
        GlobalMachiningData::instance().addTool(dlg.getTool());
        refreshToolCombo();
        m_combox_cuttingTool->setCurrentText(dlg.getTool().toolNumber);
    }
}

void CreatePathDialog::onEditTool()
{
    QString num = m_combox_cuttingTool->currentData().toString();
    if (num.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一把刀具"));
        return;
    }
    auto tool = GlobalMachiningData::instance().getToolByNumber(num);
    ToolDialog dlg(this, tr("编辑刀具"), &tool);
    if (dlg.exec() == QDialog::Accepted) {
        int idx = GlobalMachiningData::instance().getToolIndexByNumber(num);
        GlobalMachiningData::instance().updateTool(idx, dlg.getTool());
        refreshToolCombo();
        m_combox_cuttingTool->setCurrentText(dlg.getTool().toolNumber);
    }
}

void CreatePathDialog::onDeleteTool()
{
    QString num = m_combox_cuttingTool->currentData().toString();
    if (num.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一把刀具"));
        return;
    }
    if (QMessageBox::question(this, tr("确认"), tr("确定删除刀具 '%1' 吗?").arg(num))
        == QMessageBox::Yes) {
        int idx = GlobalMachiningData::instance().getToolIndexByNumber(num);
        GlobalMachiningData::instance().deleteTool(idx);
        refreshToolCombo();
    }
}

void CreatePathDialog::onToolSelected(int index)
{
    if (index > 0) {
        GlobalMachiningData::instance().currentToolNumber = m_combox_cuttingTool->currentData().toString();
    }
}

// ========== 毛坯操作（已移除 DetectedObject 逻辑） ==========
void CreatePathDialog::refreshStockCombo()
{
    QString currentName = m_combox_Blank->currentData().toString();
    m_combox_Blank->clear();
    m_combox_Blank->addItem(tr("请选择毛坯"), QString());

    const auto& stocks = GlobalMachiningData::instance().getStocks();
    for (const auto& stock : stocks) {
        QString stockName = GlobalMachiningData::instance().generateStockName(stock);
        m_combox_Blank->addItem(stockName, stockName);
    }

    if (!currentName.isEmpty()) {
        int idx = m_combox_Blank->findData(currentName);
        if (idx >= 0) m_combox_Blank->setCurrentIndex(idx);
    }
}
void CreatePathDialog::refreshComponentCombo()
{
    QString currentPath = GlobalMachiningData::instance().componentModelPath;
    if (!currentPath.isEmpty()) {
        QString fileName = QFileInfo(currentPath).fileName();
        // 避免重复添加相同项
        if (m_combox_Component->findText(fileName) == -1) {
            m_combox_Component->addItem(fileName, currentPath);
        }
        m_combox_Component->setCurrentText(fileName);
    }
    // 若 path 为空，combo 保持为空或留待手动选择
}

void CreatePathDialog::onNewStock()
{
    StockDialog dlg(this, tr("新建毛坯"));
    if (dlg.exec() == QDialog::Accepted) {
        GlobalMachiningData::instance().addStock(dlg.getStock());
        refreshStockCombo();
        m_combox_Blank->setCurrentText(GlobalMachiningData::instance().generateStockName(dlg.getStock()));
    }
}

void CreatePathDialog::onEditStock()
{
    QString name = m_combox_Blank->currentData().toString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个毛坯"));
        return;
    }
    auto stock = GlobalMachiningData::instance().getStockByName(name);
    StockDialog dlg(this, tr("编辑毛坯"), &stock);
    if (dlg.exec() == QDialog::Accepted) {
        int idx = GlobalMachiningData::instance().getStockIndexByName(name);
        GlobalMachiningData::instance().updateStock(idx, dlg.getStock());
        refreshStockCombo();
        m_combox_Blank->setCurrentText(GlobalMachiningData::instance().generateStockName(dlg.getStock()));
    }
}

void CreatePathDialog::onDeleteStock()
{
    QString name = m_combox_Blank->currentData().toString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个毛坯"));
        return;
    }
    if (QMessageBox::question(this, tr("确认"), tr("确定删除毛坯 '%1' 吗?").arg(name))
        == QMessageBox::Yes) {
        int idx = GlobalMachiningData::instance().getStockIndexByName(name);
        GlobalMachiningData::instance().deleteStock(idx);
        refreshStockCombo();
    }
}

void CreatePathDialog::onStockSelected(int index)
{
    if (index > 0) {
        QString name = m_combox_Blank->currentData().toString();
        GlobalMachiningData::instance().currentStockName = name;
        GlobalMachiningData::instance().currentStockIndex = index;
        qDebug() << "毛坯选中:" << name;
    } else {
        GlobalMachiningData::instance().currentStockName.clear();
        GlobalMachiningData::instance().currentStockIndex = -1;
    }
}

// ========== 其他交互函数 ==========
void CreatePathDialog::onSelectComponent()
{
    QString filter = tr("3D模型文件 (*.stl *.stp *.step);;所有文件 (*.*)");
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择部件模型"), QDir::homePath(), filter);
    if (!filePath.isEmpty()) {
        QString fileName = QFileInfo(filePath).fileName();
        if (m_combox_Component->findText(fileName) == -1)
            m_combox_Component->addItem(fileName, filePath);
        m_combox_Component->setCurrentText(fileName);
        GlobalMachiningData::instance().componentModelPath = filePath;
    }
}

void CreatePathDialog::onCuttingEnterSettingsChanged(int index)
{
    // bool spiral = (index == 1);
    // m_spiralSlopeContainer->setVisible(spiral);
    // m_spiralRadiusContainer->setVisible(spiral);
}

void CreatePathDialog::collectCurrentParams(GlobalMachiningData::MachiningParams& params)
{
    params.paramName = edit_name->text().trimmed();
    params.workStation = combox_work->currentText();

    // 直接使用存储的 QString 名称
    params.currentCoordinateName = m_combox_Coordinate->currentData().toString();
    params.currentToolNumber = m_combox_cuttingTool->currentData().toString();
    params.currentStockName = m_combox_Blank->currentData().toString();

    params.componentModelPath = GlobalMachiningData::instance().componentModelPath;

    params.top = m_ledit_top->text();
    params.bottom = m_ledit_bottom->text();
    params.layerHeight = m_ledit_height->text().toDouble();
    params.stepover = m_ledit_pace->text().toDouble();
    params.cuttingStyle = m_combox_cuttingStyle->currentIndex();
    params.cuttingDirection = m_combox_cuttingDirection->currentIndex();

    params.enterMode = m_combox_cuttingEnterSettings->currentIndex();
    params.cuttingHeight = m_ledit_cuttingHeight->text().toDouble();
    params.spiralRadius = m_ledit_spiralRadius->text().toDouble();
    params.spiralSlope = m_ledit_spiralSlope->text().toDouble();

    params.feedMode = m_combox_cuttingFeedSetting->currentIndex();
    params.arcRadius = m_ledit_arcRadius->text().toDouble();
    params.lineLength = m_ledit_lineLength->text().toDouble();

    params.returnCustom = m_checkbox_returnCutting->isChecked();
    params.returnMode = m_combox_returnCutting->currentIndex();

    params.safePlaneType = m_combox_interactivePlane->currentIndex();
    params.safeHeight = m_ledit_interactiveHeight->text().toDouble();

    params.sideMargin = m_ledit_sideMargin->text().toDouble();
    params.bottomMargin = m_ledit_bottomMargin->text().toDouble();
    params.tolerance = m_ledit_Tolerance->text().toDouble();

    params.makeFeed = m_ledit_makeFeed->text().toDouble();
    params.cuttingFeed = m_ledit_cuttingFeed->text().toDouble();
    params.processingSpeed = m_ledit_processing->text().toDouble();
}

static void logMachiningParams(const GlobalMachiningData::MachiningParams& params)
{
    qDebug() << "========== 加工参数完整输出 ==========";
    qDebug() << "参数名称:" << params.paramName;
    qDebug() << "工位:" << params.workStation;

    qDebug() << "坐标系名称:" << params.currentCoordinateName;
    if (!params.currentCoordinateName.isEmpty()) {
        auto coord = GlobalMachiningData::instance().getCoordinateByName(params.currentCoordinateName);
        qDebug() << "  原点: (" << coord.originX << "," << coord.originY << "," << coord.originZ << ")";
        qDebug() << "  X轴方向: (" << coord.xAxisX << "," << coord.xAxisY << "," << coord.xAxisZ << ")";
        qDebug() << "  Y轴方向: (" << coord.yAxisX << "," << coord.yAxisY << "," << coord.yAxisZ << ")";
        qDebug() << "  Z轴方向: (" << coord.zAxisX << "," << coord.zAxisY << "," << coord.zAxisZ << ")";
    }

    qDebug() << "刀具号:" << params.currentToolNumber;
    if (!params.currentToolNumber.isEmpty()) {
        auto tool = GlobalMachiningData::instance().getToolByNumber(params.currentToolNumber);
        qDebug() << "  直径(mm):" << tool.diameter
                 << "下半径(mm):" << tool.lowerRadius
                 << "锥角(°):" << tool.coneAngle
                 << "尖角(mm):" << tool.sharpCorner
                 << "长度(mm):" << tool.length;
    }

    qDebug() << "毛坯名称:" << params.currentStockName;
    if (!params.currentStockName.isEmpty()) {
        auto stock = GlobalMachiningData::instance().getStockByName(params.currentStockName);
        if (stock.type == 0) {
            qDebug() << "  类型: 方体"
                     << "长:" << stock.length << "宽:" << stock.width << "高:" << stock.height
                     << "中心:(" << stock.centerX << "," << stock.centerY << "," << stock.centerZ << ")"
                     << "角度:" << stock.angle;
        } else {
            qDebug() << "  类型: 圆柱"
                     << "直径:" << stock.diameter << "高度:" << stock.cylHeight
                     << "中心:(" << stock.centerX << "," << stock.centerY << "," << stock.centerZ << ")"
                     << "角度:" << stock.angle;
        }
    }

    qDebug() << "部件模型路径:" << params.componentModelPath;
    qDebug() << "指定顶部:" << params.top;
    qDebug() << "指定底部:" << params.bottom;
    qDebug() << "每层高度(mm):" << params.layerHeight;
    qDebug() << "切削步距(mm):" << params.stepover;
    qDebug() << "切削样式:" << (params.cuttingStyle == 0 ? "跟随周边" : "跟随部件");
    qDebug() << "切削方向:" << (params.cuttingDirection == 0 ? "顺铣" : "逆铣");

    qDebug() << "下刀方式:" << (params.enterMode == 0 ?  "螺旋": "直接");
    qDebug() << "下刀高度(mm):" << params.cuttingHeight;
    if (params.enterMode == 0) {
        qDebug() << "螺旋半径(%):" << params.spiralRadius;
        qDebug() << "螺旋角度(°):" << params.spiralSlope;
    }

    qDebug() << "进刀方式:" << (params.feedMode == 0 ? "圆弧" : "直线");
    if (params.feedMode == 0) {
        qDebug() << "圆弧半径(mm):" << params.arcRadius;
    } else {
        qDebug() << "直线长度(mm):" << params.lineLength;
    }

    qDebug() << "退刀自定义:" << (params.returnCustom ? "启用" : "禁用");
    if (params.returnCustom) {
        qDebug() << "退刀方式:" << (params.returnMode == 0 ? "与进刀相同" : "其他");
    }

    QString planeType;
    switch (params.safePlaneType) {
    case 0: planeType = "平面"; break;
    case 1: planeType = "柱"; break;
    case 2: planeType = "方块"; break;
    case 3: planeType = "球"; break;
    default: planeType = "未知";
    }
    qDebug() << "安全平面类型:" << planeType;
    qDebug() << "安全高度(mm):" << params.safeHeight;

    qDebug() << "侧部余量(mm):" << params.sideMargin;
    qDebug() << "底部余量(mm):" << params.bottomMargin;
    qDebug() << "公差(mm):" << params.tolerance;

    qDebug() << "下刀速度(mm/分):" << params.makeFeed;
    qDebug() << "进刀速度(mm/分):" << params.cuttingFeed;
    qDebug() << "加工速度(mm/分):" << params.processingSpeed;

    qDebug() << "=====================================";
}
void CreatePathDialog::onGenerateGCode()
{
    QString name = edit_name->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请填写参数名称"));
        return;
    }

    GlobalMachiningData::MachiningParams params;
    collectCurrentParams(params);

    // 校验
    if (params.currentCoordinateName.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择一个有效的坐标系"));
        return;
    }
    if (params.currentStockName.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择一个有效的毛坯"));
        return;
    }
    if (params.componentModelPath.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先指定部件"));
        return;
    }
    if (m_editMode) {
        // 编辑模式：更新现有参数
        GlobalMachiningData::instance().updateMachiningParams(m_editParamName, params);
        // 如果参数名称发生了变化，发射信号通知外部更新树节点
        if (m_editParamName != name) {
            emit paramNameChanged(m_editParamName, name);
            m_editParamName = name;  // 更新内部记录
        }
    } else {
        // 新建模式：保存并发射 pathCreated
        GlobalMachiningData::instance().saveCurrentParams(name, params);
        emit pathCreated(name, params.workStation);
    }

    logMachiningParams(params);

    // 构建毛坯对象并发射 generatePath 信号（与原有逻辑相同）
    auto stockData = GlobalMachiningData::instance().getStockByName(params.currentStockName);
    std::shared_ptr<DetectedObject> blankObj;
    if (stockData.type == 0) {
        auto block = std::make_shared<MetalBlock>();
        block->setDimensions(stockData.width, stockData.height);
        block->setDepth(stockData.length);
        blankObj = block;
    } else {
        auto cylinder = std::make_shared<AnnulusOuter>();
        cylinder->setRadius(stockData.diameter / 2.0f);
        cylinder->setDepth(stockData.cylHeight);
        blankObj = cylinder;
    }
    blankObj->setName(params.currentStockName.toStdString());
    blankObj->setCenter(stockData.centerX, stockData.centerY, stockData.centerZ);
    blankObj->setAngle(stockData.angle);

    accept();
    emit generatePath(name, params, blankObj);

}

void CreatePathDialog::onSave()
{
    QString name = edit_name->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请填写参数名称"));
        return;
    }
    GlobalMachiningData::MachiningParams params;
    collectCurrentParams(params);
    if (params.componentModelPath.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先指定部件"));
        return;
    }

    if (m_editMode) {
        GlobalMachiningData::instance().updateMachiningParams(m_editParamName, params);
        if (m_editParamName != name) {
            emit paramNameChanged(m_editParamName, name);
        }
    } else {
        GlobalMachiningData::instance().saveCurrentParams(name, params);
        // 注意：仅保存时不发射 pathCreated，因为不生成路径
    }
    QMessageBox::information(this, tr("成功"), tr("参数已保存"));
    accept();
}

void CreatePathDialog::onCancel()
{
    reject();
}

void CreatePathDialog::onComboxBlankClicked()
{
    QString name = m_combox_Blank->currentData().toString();
    if (name.isEmpty()) {
        qDebug() << "警告：选中的毛坯名称为空";
        return;
    }
    qDebug() << "毛坯已变更:" << name;
}
