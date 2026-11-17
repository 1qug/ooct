#include "VisionWidget.h"
#include <QFileInfo>
#include <QProgressDialog>
#include <cmath>

// OpenCASCADE 相关头文件
#include <STEPControl_Reader.hxx>
#include <StlAPI_Reader.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <Quantity_Color.hxx>

VisionWidget::VisionWidget(QWidget *parent)
    : QWidget(parent)
{
    showFullScreen();
    this->setFocusPolicy(Qt::ClickFocus);
    setupUI();
    // 设置样式
    this->setStyleSheet(R"(
        VisionWidget {
            background-color: #666666;
            border: 1px solid #444444;
            border-radius: 4px;
        }

        QTreeWidget {
            background-color: #ffffff;
            color: #333333;
            border: 1px solid #e0e0e0;
            font-size: 14px;
            font-family: "Segoe UI", "Roboto", sans-serif;
            outline: none;
            border-radius: 8px;
            padding: 4px;
        }

        QTreeWidget::item {
            padding: 12px 8px;
            border-bottom: 1px solid #f5f5f5;
            border-radius: 6px;
            margin: 1px 2px;
            min-height: 24px;
        }

        QTreeWidget::item:selected {
            background-color: #e3f2fd;
            color: #1976d2;
            border: none;
            border-radius: 6px;
        }

        QTreeWidget::item:hover {
            background-color: #f5f5f5;
            border-radius: 6px;
        }

        QTreeWidget::item:hover:selected {
            background-color: #bbdefb;
        }

        /* Material Design 滚动条 */
        QScrollBar:vertical {
            background: #f5f5f5;
            width: 10px;
            margin: 0px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical {
            background: #c0c0c0;
            border-radius: 5px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background: #a0a0a0;
        }

    )");
    connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &VisionWidget::onTreeItemDoubleClicked);
}

VisionWidget::~VisionWidget()
{
}

void VisionWidget::setupUI()
{
    // 使用水平布局替代 QSplitter
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);  // 可选：消除间隙

    // 左侧树形控件（父对象改为 this）
    treeWidget = new QTreeWidget(this);
    treeWidget->setHeaderLabel("加工设置");
    treeWidget->setHeaderHidden(true);
    treeWidget->setIndentation(12);
    treeWidget->setRootIsDecorated(true);
    treeWidget->setAnimated(true);
    treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    // treeWidget->setStyleSheet(R"(
    //     QTreeWidget { padding: 4px; background-color: #ffffff; color: #333333; outline: none; font-family: "Segoe UI", "Microsoft YaHei", sans-serif; font-size: 12px; show-decoration-selected: 1; }
    //     QTreeWidget::item:focus { outline: none; }
    //     QTreeWidget::item { height: 28px; padding: 0px 4px; margin: 1px 0px; border: none; }
    //     QTreeWidget::item:hover { background-color: #f0f7ff; }
    //     QTreeWidget::item:selected { background-color: #e3f2fd; color: #1976d2; border: none; }
    //     QTreeWidget::branch { background-color: transparent; }
    //     QTreeWidget::branch:closed:has-children { border-image: none; image: url(:/resource/images/Settings/up.png); width: 16px; height: 16px; padding-right: 2px; }
    //     QTreeWidget::branch:open:has-children { border-image: none; image: url(:/resource/images/Settings/down.png); width: 16px; height: 16px; padding-right: 2px; }
    //     QScrollBar:vertical { background: #f5f5f5; width: 8px; border-radius: 4px; }
    //     QScrollBar::handle:vertical { background: #cccccc; border-radius: 4px; min-height: 20px; }
    // )");

    // 右侧 OCCT 视图控件（父对象改为 this）
    objectOcctView = new ObjectOcctView(this);

    // 直接添加到主布局，拉伸因子 3:7
    mainLayout->addWidget(treeWidget, 1);
    mainLayout->addWidget(objectOcctView, 9);  // 这里必须写 7 才能实现 3:7

    setLayout(mainLayout);
}
void VisionWidget::addPathToTree(const QString &name, const QString &workStation)
{
    // 查找是否已存在该工位的顶层节点
    QTreeWidgetItem* workItem = nullptr;
    for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = treeWidget->topLevelItem(i);
        if (item->text(0) == workStation) {
            workItem = item;
            break;
        }
    }

    // 若工位节点不存在，则创建
    if (!workItem) {
        workItem = new QTreeWidgetItem(treeWidget);
        workItem->setText(0, workStation);
        workItem->setExpanded(true);
    }

    // 在该工位下创建子项，显示加工路径名称
    QTreeWidgetItem* pathItem = new QTreeWidgetItem(workItem);
    pathItem->setText(0, name);
    // 存储参数名称，用于后续编辑时获取参数数据
    pathItem->setData(0, Qt::UserRole, name);
}
void VisionWidget::onTreeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    // 只有子节点（有父节点）才是加工路径项
    if (!item || !item->parent()) {
        return;
    }

    QString paramName = item->data(0, Qt::UserRole).toString();
    if (paramName.isEmpty()) {
        qWarning() << "树节点未存储参数名称";
        return;
    }

    // 从全局数据获取参数
    GlobalMachiningData::MachiningParams params;
    if (!GlobalMachiningData::instance().getMachiningParams(paramName, params)) {
        QMessageBox::warning(this, tr("错误"), tr("无法加载参数 '%1'").arg(paramName));
        return;
    }

    // 创建对话框并加载参数
    CreatePathDialog dlg(this);
    dlg.loadFromParams(paramName, params);  // 需在 CreatePathDialog 中实现
    dlg.setWindowTitle(tr("编辑加工路径 - %1").arg(paramName));

    // 连接信号
    // 注意：编辑模式下不再发射 pathCreated 信号添加树节点
    connect(&dlg, &CreatePathDialog::generatePath, this, &VisionWidget::onGeneratePath);

    // 如果是编辑模式，连接一个信号用于更新树节点显示名称（当参数名称改变时）
    connect(&dlg, &CreatePathDialog::paramNameChanged, this, [this, item](const QString &oldName, const QString &newName) {
        item->setText(0, newName);
        item->setData(0, Qt::UserRole, newName);
    });

    dlg.exec();
}
void VisionWidget::onCreatePathButtonClicked()
{
    CreatePathDialog dlg(this);
    dlg.setDefaultName(tr("粗加工"));
    // 连接信号
    connect(&dlg, &CreatePathDialog::pathCreated, this, &VisionWidget::addPathToTree);
    connect(&dlg, &CreatePathDialog::generatePath, this, &VisionWidget::onGeneratePath);
    dlg.exec();
}

void VisionWidget::onCreateFinishPathClicked()
{
    CreatePathDialog dlg(this);
    dlg.setDefaultName(tr("精加工"));
    // connect(&dlg, &CreatePathDialog::pathCreated, this, &VisionWidget::addPathToTree);
    // connect(&dlg, &CreatePathDialog::generatePath, this, &VisionWidget::onGeneratePath);
    // dlg.exec();  // // 连接信号

}


// VisionWidget.cpp
gp_Trsf VisionWidget::CreateCoordinateTransform(const GlobalMachiningData::Coordinate& coord)
{
    gp_Trsf trsf;

    gp_Vec xVec(coord.xAxisX, coord.xAxisY, coord.xAxisZ);
    gp_Vec yVec(coord.yAxisX, coord.yAxisY, coord.yAxisZ);
    gp_Vec zVec(coord.zAxisX, coord.zAxisY, coord.zAxisZ);

    if (xVec.Magnitude() < 1e-9 || yVec.Magnitude() < 1e-9 || zVec.Magnitude() < 1e-9) {
        qWarning() << "Invalid axis vector, using identity transform";
        return trsf; // 单位变换
    }

    xVec.Normalize();
    yVec.Normalize();
    zVec.Normalize();

    // 正交化：以 X 为主轴，修正 Z 和 Y 构成右手系
    gp_Vec newX = xVec;
    gp_Vec newY = zVec.Crossed(newX);
    if (newY.Magnitude() < 1e-9) {
        newY = yVec;   // 若 X 与 Z 平行，回退到原始 Y
    }
    newY.Normalize();
    gp_Vec newZ = newX.Crossed(newY);
    newZ.Normalize();

    // 构建旋转矩阵（列向量为工件坐标系基在世界坐标系下的分量）
    gp_Mat rotMat(newX.X(), newY.X(), newZ.X(),
                  newX.Y(), newY.Y(), newZ.Y(),
                  newX.Z(), newY.Z(), newZ.Z());

    // 使用 SetValues 直接设置 3x4 变换矩阵（行主序）
    trsf.SetValues(rotMat(1,1), rotMat(1,2), rotMat(1,3), coord.originX,
                   rotMat(2,1), rotMat(2,2), rotMat(2,3), coord.originY,
                   rotMat(3,1), rotMat(3,2), rotMat(3,3), coord.originZ);

    return trsf;
}
void VisionWidget::applyCoordinateTransform(const GlobalMachiningData::MachiningParams& params)
{
    m_coordTransform = gp_Trsf(); // 默认单位矩阵
    if (!params.currentCoordinateName.isEmpty()) {
        auto coord = GlobalMachiningData::instance().getCoordinateByName(params.currentCoordinateName);
        m_coordTransform = CreateCoordinateTransform(coord); // 使用之前定义的函数

        // 【关键调用】在视图中显示坐标系，轴长设为 50 mm
        if (objectOcctView) {
            objectOcctView->displayCoordinateSystem(m_coordTransform, 10.0);
        }
    } else {
        // 如果没有选择坐标系，则隐藏之前显示的轴
        if (objectOcctView) {
            objectOcctView->hideCoordinateSystem();
        }
    }
}
void VisionWidget::onGeneratePath(QString name,GlobalMachiningData::MachiningParams &params,std::shared_ptr<DetectedObject> blank)
{
    this->blank=blank;
    // 计算工件坐标系变换
    if (!params.currentCoordinateName.isEmpty()) {
        auto coord = GlobalMachiningData::instance().getCoordinateByName(params.currentCoordinateName);
        m_coordTransform = CreateCoordinateTransform(coord);
    } else {
        m_coordTransform = gp_Trsf(); // identity
    }
    //1
    qDebug()<<"================读取STL文件===========";
    loadSTLDataFromfile(params);
    qDebug()<<"================生成路径===========";
    generatePathAndSaveSettings(params);

}


void VisionWidget::loadSTLDataFromfile(GlobalMachiningData::MachiningParams &params)
{
    STLfilepath = params.componentModelPath;
    qDebug()<<"STLfilepath:"<<STLfilepath;
    if (!STLfilepath.isEmpty())
    {
        modelname=QFileInfo(STLfilepath).fileName();
        // 延迟初始化：第一次使用时才创建
        if (!m_stlLoader) {
            qDebug() << "Lazy initializing STLModelLoader";
            try {
                m_stlLoader = new STLModelLoader();
                qDebug() << "STLModelLoader created successfully";
            } catch (const std::exception& e) {
                qCritical() << "Failed to create STLModelLoader:" << e.what();
            }
        }

        Reset();

        objectOcctView->loadModel(STLfilepath.toStdString());

    }

    qDebug() << "模型设置已应用";
    return ;
}

// void VisionWidget::generatePathAndSaveSettings(GlobalMachiningData::MachiningParams &params)
// {
//     // ==================== 创建进度对话框 ====================
//     QProgressDialog progressDialog("正在准备参数...", "取消", 0, 100, this);
//     progressDialog.setWindowTitle("刀路生成");
//     progressDialog.setWindowModality(Qt::WindowModal);
//     progressDialog.setMinimumDuration(0);
//     progressDialog.setAutoClose(false);
//     progressDialog.setAutoReset(false);
//     progressDialog.setStyleSheet(R"(
//         QProgressDialog {
//             font-family: "Microsoft YaHei", "Segoe UI";
//             font-size: 12px;
//             min-width: 450px;
//             background-color: white;
//             border: 1px solid #cccccc;
//             border-radius: 8px;
//             padding: 15px;
//         }
//         QProgressDialog QLabel {
//             font-size: 13px;
//             color: #333333;
//             margin-bottom: 10px;
//         }
//         QProgressDialog QProgressBar {
//             border: 1px solid #bbbbbb;
//             border-radius: 4px;
//             text-align: center;
//             background-color: #f5f5f5;
//             height: 22px;
//         }
//         QProgressDialog QProgressBar::chunk {
//             background-color: #4CAF50;
//             border-radius: 3px;
//         }
//         QProgressDialog QPushButton {
//             background-color: #f0f0f0;
//             border: 1px solid #cccccc;
//             border-radius: 4px;
//             padding: 6px 15px;
//             font-size: 12px;
//             min-width: 70px;
//         }
//         QProgressDialog QPushButton:hover {
//             background-color: #e0e0e0;
//         }
//         QProgressDialog QPushButton:pressed {
//             background-color: #d0d0d0;
//         }
//     )");

//     std::atomic<bool> cancelled{false};
//     QObject::connect(&progressDialog, &QProgressDialog::canceled, [&]() {
//         cancelled = true;
//         progressDialog.setLabelText("正在取消刀路生成...");
//         progressDialog.setCancelButtonText("取消中...");
//         progressDialog.setCancelButton(nullptr);
//         qDebug() << "用户取消了刀路生成";
//     });

//     bool modelLoaded = false;

//     // ==================== 步骤1：解析并验证参数 ====================
//     progressDialog.setLabelText("正在准备参数...");
//     progressDialog.setValue(10);
//     QApplication::processEvents();

//     // 获取刀具信息
//     double toolDiameter = 6.0;
//     if (!params.currentToolNumber.isEmpty()) {
//         auto tool = GlobalMachiningData::instance().getToolByNumber(params.currentToolNumber);
//         toolDiameter = tool.diameter;
//         qDebug() << "刀具直径:" << toolDiameter;
//     }

//     // 解析切片范围
//     double topZ = std::numeric_limits<double>::infinity();
//     double bottomZ = -std::numeric_limits<double>::infinity();
//     if (params.top != "+∞" && !params.top.isEmpty()) {
//         topZ = params.top.toDouble();
//     }
//     if (params.bottom != "-∞" && !params.bottom.isEmpty()) {
//         bottomZ = params.bottom.toDouble();
//     }

//     std::shared_ptr<DetectedObject> blank = this->blank;
//     if (!blank) {
//         QMessageBox::critical(this, "错误", "未指定毛坯对象");
//         return;
//     }

//     if (cancelled) goto cleanup;

//     // ==================== 步骤2：加载模型 ====================
//     progressDialog.setLabelText("正在加载模型...");
//     progressDialog.setValue(20);
//     QApplication::processEvents();

//     if (!STLfilepath.isEmpty()) {
//         if (m_haveModel) {
//             objects.pop_back();
//         }
//         Reset();
//         try {
//             auto rawModel = m_stlLoader->loadSTLModel(
//                 STLfilepath.toStdString(), blank, spacing);
//             if (!rawModel) {
//                 throw std::runtime_error("模型加载失败，返回空指针");
//             }
//             objects.push_back(std::move(rawModel));
//         } catch (const std::exception& e) {
//             QMessageBox::critical(this, "加载失败", QString("加载模型时发生异常: %1").arg(e.what()));
//             goto cleanup;
//         }
//         objects.back()->setName(modelname.toStdString());
//         objects.back()->setCenter(blank->getCenter().x, blank->getCenter().y, blank->getCenter().z);
//         objects.back()->setDepth(blank->getDepth());
//         objects.back()->setAngle(blank->getAngle());

//         applyCoordinateTransform(params);
//         objectOcctView->m_stlShape = nullptr;
//         objectOcctView->updateDetectedObjects(objects);
//         modelLoaded = true;
//     }

//     // 更新毛坯包围盒
//     if (modelLoaded) {
//         auto bbox = blank->getModelBbox();
//         bbox.min.z = objects.back()->getModelBbox().min.z;
//         bbox.min.x = objects.back()->getModelBbox().min.x;
//         bbox.min.y = objects.back()->getModelBbox().min.y;
//         bbox.max.x = objects.back()->getModelBbox().max.x;
//         bbox.max.y = objects.back()->getModelBbox().max.y;
//         blank->setModelBbox(bbox);
//     }

//     if (cancelled) goto cleanup;

//     // ==================== 步骤3：生成加工路径 ====================
//     progressDialog.setLabelText("正在生成加工路径...");
//     progressDialog.setValue(50);
//     QApplication::processEvents();

//     if (modelLoaded) {
//         STLObject* stlObj = dynamic_cast<STLObject*>(objects.back().get());
//         if (stlObj) {
//             auto generator = SlicePathGenerator::fromSTLObject(stlObj);
//             if (generator) {
//                 // 构建切片参数
//                 SliceParams sp;
//                 sp.layerHeight = params.layerHeight;
//                 sp.topZ = topZ;
//                 sp.bottomZ = bottomZ;
//                 sp.sideMargin = params.sideMargin;
//                 sp.toolDiameter = toolDiameter;
//                 sp.climbMilling = (params.cuttingDirection == 0);
//                 sp.safeZ = params.safeHeight;
//                 sp.cuttingHeight = params.cuttingHeight;
//                 sp.stepover = params.stepover;
//                 sp.pointSpacing = 0.2;

//                 // 螺旋和进刀参数
//                 // sp.spiralRadius = (params.spiralRadius / 100.0) * (toolDiameter / 2.0);  // 界面为%，转换为实际半径
//                 // sp.spiralSlope = params.spiralSlope;
//                 // sp.entryArcRadius = params.arcRadius;
//                 // sp.entryArcAngle = 45.0;
//                 // sp.useSpiralEntry = (params.enterMode == 0);   // 1=螺旋
//                 // sp.useArcEntry = (params.feedMode == 0);       // 0=圆弧

//                 // 毛坯尺寸限制（用于防止偏置超出）
//                 GlobalMachiningData::Stock currentStock;
//                 if (!params.currentStockName.isEmpty()) {
//                     currentStock = GlobalMachiningData::instance().getStockByName(params.currentStockName);
//                 } else {
//                     auto stocks = GlobalMachiningData::instance().getStocks();
//                     if (!stocks.isEmpty()) currentStock = stocks.first();
//                 }

//                 double toolDia = sp.toolDiameter;
//                 if (currentStock.type == 0) {
//                     // 矩形毛坯：长度 = length (X方向)，宽度 = width (Y方向)
//                     sp.maxAllowedLength = currentStock.length + toolDia * 0.55;   // X方向允许范围（长度）
//                     sp.maxAllowedWidth  = currentStock.width  + toolDia * 0.55;   // Y方向允许范围（宽度）
//                     sp.maxAllowedRadius = 0.0;
//                 } else {
//                     double radius = currentStock.diameter / 2.0;
//                     sp.maxAllowedRadius = radius + toolDia * 0.55;
//                     sp.maxAllowedLength = 0.0;   // 非矩形时清零长度限制
//                     sp.maxAllowedWidth  = 0.0;   // 非矩形时清零宽度限制
//                 }
//                 sp.stockCenterX = currentStock.centerX;
//                 sp.stockCenterY = currentStock.centerY;
//                 generator->setParams(sp);

//                 progressDialog.setValue(60);
//                 QApplication::processEvents();
//                 // 根据 currentStock 显示毛坯
//                 // 在生成轮廓之前，计算毛坯位置使顶面对齐
//                 if (modelLoaded && !objects.empty()) {
//                     // 模型顶面 Z（局部坐标，变换前）
//                     double modelTopZ = objects.back()->getModelBbox().max.z;
//                     // 毛坯半高
//                     double halfHeight = (currentStock.type == 0)
//                                             ? currentStock.height / 2.0
//                                             : currentStock.cylHeight / 2.0;
//                     // 毛坯中心 Z = 模型顶面 - 半高
//                     double stockCenterZ = modelTopZ - halfHeight;

//                     qDebug() << "模型顶面 Z:" << modelTopZ
//                              << " 毛坯半高:" << halfHeight
//                              << " 调整后毛坯中心 Z:" << stockCenterZ;

//                     if (objectOcctView) {
//                         objectOcctView->displayStockShape(currentStock.type,
//                                                           currentStock.length, currentStock.width, currentStock.height,
//                                                           currentStock.diameter, currentStock.cylHeight,
//                                                           currentStock.centerX, currentStock.centerY,
//                                                           stockCenterZ,          // 关键：使用计算出的 Z
//                                                           currentStock.angle,
//                                                           m_coordTransform);
//                     }
//                 }
//                 // 1. 生成偏置轮廓
//                 if (generator->generateContoursOnly()) {
//                     ContourGroups groups = generator->groupContours();
//                     // ★ 提前计算内轮廓最大内切圆（含区域编号），供 drawContourGroups 使用
//                     std::vector<MaxIncircleInfo> innerCircles = generator->computeIncircles(false, 0.2);
//                     // 传入内腔最大内切圆列表（包含区域编号）
//                     objectOcctView->drawContourGroups(groups, this->blank, m_coordTransform, &innerCircles);

//                     // 构建毛坯局部变换（与 drawContourGroups 一致）
//                     gp_Trsf blankTrsf;
//                     {
//                         double angleRad = blank->getAngle() * M_PI / 180.0;
//                         if (std::abs(angleRad) > 1e-6) {
//                             gp_Trsf rot;
//                             rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angleRad);
//                             blankTrsf = rot;
//                         }
//                         gp_Trsf trans;
//                         trans.SetTranslation(gp_Vec(blank->getCenter().x,
//                                                     blank->getCenter().y,
//                                                     blank->getCenter().z));
//                         blankTrsf = trans * blankTrsf;
//                     }
//                     gp_Trsf fullTransform = m_coordTransform * blankTrsf;

//                     // 计算世界安全高度和直接连接阈值
//                     gp_Pnt refPt(0, 0, params.safeHeight);
//                     refPt.Transform(fullTransform);
//                     double worldSafeZ = refPt.Z();
//                     double maxDirectDist = toolDiameter * 1.5;
//                     qDebug()<<"maxDirectDist:"<<maxDirectDist;
//                     // 先清除上一次生成的所有白点（外、内）
//                     objectOcctView->clearMaxIncircles();
//                     // ========== 外轮廓最大内切圆与路径 ==========
//                     // 外轮廓内切圆及路径
//                     std::vector<MaxIncircleInfo> outerCircles = generator->computeIncircles(true, 0.2);

//                     // 毛坯长宽对角线的一半作为“距离过大”判定阈值
//                     double halfStockDiag = std::sqrt(sp.maxAllowedLength * sp.maxAllowedLength
//                                                      + sp.maxAllowedWidth * sp.maxAllowedWidth) * 0.25;

//                     if (!outerCircles.empty()) {
//                         objectOcctView->displayMaxIncircles(outerCircles, fullTransform);
//                         objectOcctView->drawOuterBluesPath(outerCircles, fullTransform, worldSafeZ, halfStockDiag);
//                     }

//                     // ========== 内轮廓最大内切圆与路径 ==========
//                     std::vector<MaxIncircleInfo> innerCircless = generator->computeIncircles(false, 0.2);

//                     if (!innerCircless.empty()) {
//                         objectOcctView->displayMaxIncircles(innerCircless, fullTransform);// 构建 contourId -> groupId 映射
//                         std::map<int, int> contourToGroup;
//                         for (const auto& group : groups.groups) {
//                             for (const auto& gc : group) {
//                                 if (gc.contour) {
//                                     contourToGroup[gc.contour->id] = gc.groupId;
//                                 }
//                             }
//                         }
//                         // 然后调用
//                         objectOcctView->displayIncirclePath(innerCircless, fullTransform, worldSafeZ, maxDirectDist, contourToGroup);
//                         // ========== 外轮廓绿点 → 内轮廓 groupId=1 独占区域B紫红点 ==========
//                         if (!outerCircles.empty()) {
//                             // 1. 找外轮廓绿点：最低层 + passIndex 最小（即 globalIdx 最大）
//                             const MaxIncircleInfo* outerGreen = nullptr;
//                             for (const auto& info : outerCircles) {
//                                 if (info.isOuter && info.isConcave && !info.isInnermost && !info.contactPoints.empty()) {
//                                     if (!outerGreen ||
//                                         info.z < outerGreen->z - 1e-6 ||
//                                         (std::abs(info.z - outerGreen->z) < 1e-6 && info.passIndex < outerGreen->passIndex)) {
//                                         outerGreen = &info;
//                                     }
//                                 }
//                             }

//                             // 2. 找内轮廓目标紫红点：groupId==1, regionId==1(B), isInnermost, 最低层passIndex最大
//                             const MaxIncircleInfo* targetPurple = nullptr;
//                             if (outerGreen) {
//                                 for (const auto& info : innerCircless) {
//                                     if (!info.isOuter && !info.contactPoints.empty() && info.isInnermost && info.regionId == 1) {
//                                         auto it = contourToGroup.find(info.contourId);
//                                         if (it != contourToGroup.end() && it->second == 1) {
//                                             if (!targetPurple ||
//                                                 info.z < targetPurple->z - 1e-6 ||
//                                                 (std::abs(info.z - targetPurple->z) < 1e-6 && info.passIndex > targetPurple->passIndex)) {
//                                                 targetPurple = &info;
//                                             }
//                                         }
//                                     }
//                                 }
//                             }

//                             // 3. 绘制抬刀-平移-下刀路径
//                             if (outerGreen && targetPurple) {
//                                 gp_Pnt fromWorld(outerGreen->contactPoints[0].x, outerGreen->contactPoints[0].y, outerGreen->z);
//                                 gp_Pnt toWorld(targetPurple->contactPoints[0].x, targetPurple->contactPoints[0].y, targetPurple->z);
//                                 fromWorld.Transform(fullTransform);
//                                 toWorld.Transform(fullTransform);
//                                 objectOcctView->drawLiftTravelPlunge(fromWorld, toWorld, worldSafeZ);
//                                 qDebug() << "已添加外轮廓绿点→内腔B紫红点抬刀路径(groupId=1)";
//                             } else {
//                                 if (!outerGreen) qDebug() << "未找到外轮廓绿点 (isConcave, !innermost)";
//                                 if (!targetPurple) qDebug() << "未找到内轮廓目标紫红点 (groupId=1, regionB, innermost)";
//                             }


//                             // qDebug()<<"2222222222";
//                             // objectOcctView->displayAllContourPoints(groups, fullTransform);
//                             // qDebug()<<"33333333333333";
//                         }
//                     }


//                     std::vector<MaxIncircleInfo> outerCircless = generator->computeIncircles(true, 0.2);
//                     for (const auto& info : outerCircless) {
//                         if (info.contactPoints.empty()) continue;
//                         QString type;
//                         if (info.isConcave)
//                             type = info.isInnermost ? "内凹环最内层(紫红)" : "内凹环(绿)";
//                         else
//                             type = "主体外环/边界残片(蓝)";

//                         // qDebug().noquote()
//                         //     << QString("外轮廓点: contourId=%1 pass=%2 z=%3 area=%4 %5 切点(%6,%7) isConcave=%8 touchesBoundary=?")
//                         //            .arg(info.contourId).arg(info.passIndex)
//                         //            .arg(info.z, 0, 'f', 2).arg(info.area, 0, 'f', 2)
//                         //            .arg(type)
//                         //            .arg(info.contactPoints[0].x, 0, 'f', 3)
//                         //            .arg(info.contactPoints[0].y, 0, 'f', 3)
//                         //            .arg(info.isConcave);
//                     }
//                 }

//             } else {
//                 QMessageBox::warning(this, "生成失败", "无法从模型创建路径生成器。");
//                 goto cleanup;
//             }
//         }
//     }
//     // 正常结束，移除临时模型
//     if (modelLoaded && !objects.empty()) {
//         objects.pop_back();
//     }
//     return;

// cleanup:
//     if (modelLoaded && !objects.empty()) {
//         objects.pop_back();
//     }
//     progressDialog.setValue(100);
//     progressDialog.setLabelText("操作已取消");
//     QTimer::singleShot(500, &progressDialog, &QProgressDialog::close);
// }

void VisionWidget::generatePathAndSaveSettings(GlobalMachiningData::MachiningParams &params)
{
    // ==================== 创建进度对话框 ====================
    QProgressDialog progressDialog("正在准备参数...", "取消", 0, 100, this);
    progressDialog.setWindowTitle("刀路生成");
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);
    progressDialog.setStyleSheet(R"(
        QProgressDialog {
            font-family: "Microsoft YaHei", "Segoe UI";
            font-size: 12px;
            min-width: 450px;
            background-color: white;
            border: 1px solid #cccccc;
            border-radius: 8px;
            padding: 15px;
        }
        QProgressDialog QLabel {
            font-size: 13px;
            color: #333333;
            margin-bottom: 10px;
        }
        QProgressDialog QProgressBar {
            border: 1px solid #bbbbbb;
            border-radius: 4px;
            text-align: center;
            background-color: #f5f5f5;
            height: 22px;
        }
        QProgressDialog QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 3px;
        }
        QProgressDialog QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 6px 15px;
            font-size: 12px;
            min-width: 70px;
        }
        QProgressDialog QPushButton:hover {
            background-color: #e0e0e0;
        }
        QProgressDialog QPushButton:pressed {
            background-color: #d0d0d0;
        }
    )");

    std::atomic<bool> cancelled{false};
    QObject::connect(&progressDialog, &QProgressDialog::canceled, [&]() {
        cancelled = true;
        progressDialog.setLabelText("正在取消刀路生成...");
        progressDialog.setCancelButtonText("取消中...");
        progressDialog.setCancelButton(nullptr);
        qDebug() << "用户取消了刀路生成";
    });

    bool modelLoaded = false;

    // ==================== 步骤1：解析并验证参数 ====================
    progressDialog.setLabelText("正在准备参数...");
    progressDialog.setValue(10);
    QApplication::processEvents();

    // 获取刀具信息
    double toolDiameter = 6.0;
    if (!params.currentToolNumber.isEmpty()) {
        auto tool = GlobalMachiningData::instance().getToolByNumber(params.currentToolNumber);
        toolDiameter = tool.diameter;
        qDebug() << "刀具直径:" << toolDiameter;
    }

    // 解析切片范围
    double topZ = std::numeric_limits<double>::infinity();
    double bottomZ = -std::numeric_limits<double>::infinity();
    if (params.top != "+∞" && !params.top.isEmpty()) {
        topZ = params.top.toDouble();
    }
    if (params.bottom != "-∞" && !params.bottom.isEmpty()) {
        bottomZ = params.bottom.toDouble();
    }

    std::shared_ptr<DetectedObject> blank = this->blank;
    if (!blank) {
        QMessageBox::critical(this, "错误", "未指定毛坯对象");
        return;
    }

    if (cancelled) goto cleanup;

    // ==================== 步骤2：加载模型 ====================
    progressDialog.setLabelText("正在加载模型...");
    progressDialog.setValue(20);
    QApplication::processEvents();

    if (!STLfilepath.isEmpty()) {
        if (m_haveModel) {
            objects.pop_back();
        }
        Reset();
        try {
            auto rawModel = m_stlLoader->loadSTLModel(
                STLfilepath.toStdString(), blank, spacing);
            if (!rawModel) {
                throw std::runtime_error("模型加载失败，返回空指针");
            }
            objects.push_back(std::move(rawModel));
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "加载失败", QString("加载模型时发生异常: %1").arg(e.what()));
            goto cleanup;
        }
        objects.back()->setName(modelname.toStdString());
        objects.back()->setCenter(blank->getCenter().x, blank->getCenter().y, blank->getCenter().z);
        objects.back()->setDepth(blank->getDepth());
        objects.back()->setAngle(blank->getAngle());

        applyCoordinateTransform(params);
        objectOcctView->m_stlShape = nullptr;
        objectOcctView->updateDetectedObjects(objects);
        modelLoaded = true;
    }

    // 更新毛坯包围盒
    if (modelLoaded) {
        auto bbox = blank->getModelBbox();
        bbox.min.z = objects.back()->getModelBbox().min.z;
        bbox.min.x = objects.back()->getModelBbox().min.x;
        bbox.min.y = objects.back()->getModelBbox().min.y;
        bbox.max.x = objects.back()->getModelBbox().max.x;
        bbox.max.y = objects.back()->getModelBbox().max.y;
        blank->setModelBbox(bbox);
    }

    if (cancelled) goto cleanup;

    // ==================== 步骤3：生成加工路径 ====================
    progressDialog.setLabelText("正在生成加工路径...");
    progressDialog.setValue(50);
    QApplication::processEvents();

    if (modelLoaded) {
        STLObject* stlObj = dynamic_cast<STLObject*>(objects.back().get());
        if (stlObj) {
            auto generator = SlicePathGenerator::fromSTLObject(stlObj);
            if (generator) {
                // 构建切片参数
                SliceParams sp;
                sp.layerHeight = params.layerHeight;
                sp.topZ = topZ;
                sp.bottomZ = bottomZ;
                sp.sideMargin = params.sideMargin;
                sp.toolDiameter = toolDiameter;
                sp.climbMilling = (params.cuttingDirection == 0);
                sp.safeZ = params.safeHeight;
                sp.cuttingHeight = params.cuttingHeight;
                sp.stepover = params.stepover;
                sp.pointSpacing = 0.2;

                // 毛坯尺寸限制（用于防止偏置超出）
                GlobalMachiningData::Stock currentStock;
                if (!params.currentStockName.isEmpty()) {
                    currentStock = GlobalMachiningData::instance().getStockByName(params.currentStockName);
                } else {
                    auto stocks = GlobalMachiningData::instance().getStocks();
                    if (!stocks.isEmpty()) currentStock = stocks.first();
                }

                double toolDia = sp.toolDiameter;
                if (currentStock.type == 0) {
                    // 矩形毛坯
                    sp.maxAllowedLength = currentStock.length + toolDia * 0.55;
                    sp.maxAllowedWidth  = currentStock.width  + toolDia * 0.55;
                    sp.maxAllowedRadius = 0.0;
                } else {
                    double radius = currentStock.diameter / 2.0;
                    sp.maxAllowedRadius = radius + toolDia * 0.55;
                    sp.maxAllowedLength = 0.0;
                    sp.maxAllowedWidth  = 0.0;
                }
                sp.stockCenterX = currentStock.centerX;
                sp.stockCenterY = currentStock.centerY;
                generator->setParams(sp);

                progressDialog.setValue(60);
                QApplication::processEvents();

                // 显示毛坯形状
                if (modelLoaded && !objects.empty()) {
                    double modelTopZ = objects.back()->getModelBbox().max.z;
                    double halfHeight = (currentStock.type == 0)
                                            ? currentStock.height / 2.0
                                            : currentStock.cylHeight / 2.0;
                    double stockCenterZ = modelTopZ - halfHeight;

                    if (objectOcctView) {
                        objectOcctView->displayStockShape(currentStock.type,
                                                          currentStock.length, currentStock.width, currentStock.height,
                                                          currentStock.diameter, currentStock.cylHeight,
                                                          currentStock.centerX, currentStock.centerY,
                                                          stockCenterZ,
                                                          currentStock.angle,
                                                          m_coordTransform);
                    }
                }

                // 1. 生成偏置轮廓
                if (generator->generateContoursOnly()) {
                    ContourGroups groups = generator->groupContours();

                    // ★ 提前计算内轮廓最大内切圆（含区域编号），供 drawContourGroups 使用
                    std::vector<MaxIncircleInfo> innerCircles = generator->computeIncircles(false, 0.2);
                    objectOcctView->drawContourGroups(groups, this->blank, m_coordTransform, &innerCircles);

                    // 构建毛坯局部变换（与 drawContourGroups 一致）
                    gp_Trsf blankTrsf;
                    {
                        double angleRad = blank->getAngle() * M_PI / 180.0;
                        if (std::abs(angleRad) > 1e-6) {
                            gp_Trsf rot;
                            rot.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angleRad);
                            blankTrsf = rot;
                        }
                        gp_Trsf trans;
                        trans.SetTranslation(gp_Vec(blank->getCenter().x,
                                                    blank->getCenter().y,
                                                    blank->getCenter().z));
                        blankTrsf = trans * blankTrsf;
                    }
                    gp_Trsf fullTransform = m_coordTransform * blankTrsf;

                    // 世界安全高度和直接连接阈值
                    gp_Pnt refPt(0, 0, params.safeHeight);
                    refPt.Transform(fullTransform);
                    double worldSafeZ = refPt.Z();
                    double halfStockDiag = std::sqrt(sp.maxAllowedLength * sp.maxAllowedLength
                                                     + sp.maxAllowedWidth * sp.maxAllowedWidth) * 0.25;
                    double maxDirectDist = toolDiameter * 1.5;  // 可根据需要替换为 halfStockDiag

                    // 清除旧的内切圆显示
                    objectOcctView->clearMaxIncircles();

                    // 一次性计算外轮廓内切圆（不再重复计算）
                    std::vector<MaxIncircleInfo> outerCircles = generator->computeIncircles(true, 0.2);

                    // 显示内/外轮廓内切圆小球
                    objectOcctView->displayMaxIncircles(outerCircles, fullTransform);
                    objectOcctView->displayMaxIncircles(innerCircles, fullTransform);

                    // 构建 contourId -> groupId 映射（内轮廓路径需要）
                    std::map<int, int> contourToGroup;
                    for (const auto& group : groups.groups) {
                        for (const auto& gc : group) {
                            if (gc.contour) {
                                contourToGroup[gc.contour->id] = gc.groupId;
                            }
                        }
                    }

                    // ★ 调用新的分层路径绘制（从高到低，每层先外后内）
                    objectOcctView->drawLayeredPath(outerCircles, innerCircles, fullTransform,
                                                    worldSafeZ, maxDirectDist, contourToGroup);

                    // 显示所有轮廓点（白点）
                    //objectOcctView->displayAllContourPoints(groups, fullTransform);


                    // ---------- 螺旋线连接紫红点 ----------
                    double helixAngleDeg = params.spiralSlope;   // 螺旋升角（度）
                    double deltaZForSingle = params.layerHeight; // 单点螺旋下降深度，优先使用层高
                    if (deltaZForSingle < 1e-6) deltaZForSingle = params.cuttingHeight;
                    if (deltaZForSingle < 1e-6) deltaZForSingle = 1.0; // 最小 1mm

                    // ========== 1. 外轮廓凹环紫红点：空间聚类 ==========
                    std::vector<const MaxIncircleInfo*> outerPurples;
                    for (auto& info : outerCircles) {
                        if (info.isConcave && info.isInnermost && !info.contactPoints.empty())
                            outerPurples.push_back(&info);
                    }

                    double clusterThreshold = toolDiameter * 2.0;
                    auto clusterByDistance = [&](const std::vector<const MaxIncircleInfo*>& points,
                                                 double threshold) {
                        int n = static_cast<int>(points.size());
                        std::vector<int> parent(n);
                        for (int i = 0; i < n; ++i) parent[i] = i;
                        auto find = [&](int x) {
                            while (x != parent[x]) {
                                parent[x] = parent[parent[x]];
                                x = parent[x];
                            }
                            return x;
                        };
                        auto unite = [&](int a, int b) {
                            int ra = find(a), rb = find(b);
                            if (ra != rb) parent[ra] = rb;
                        };
                        for (int i = 0; i < n; ++i) {
                            for (int j = i + 1; j < n; ++j) {
                                double dx = points[i]->contactPoints[0].x - points[j]->contactPoints[0].x;
                                double dy = points[i]->contactPoints[0].y - points[j]->contactPoints[0].y;
                                if ((dx*dx + dy*dy) < threshold * threshold)
                                    unite(i, j);
                            }
                        }
                        std::map<int, std::vector<const MaxIncircleInfo*>> clusters;
                        for (int i = 0; i < n; ++i)
                            clusters[find(i)].push_back(points[i]);
                        return clusters;
                    };

                    auto outerClusters = clusterByDistance(outerPurples, clusterThreshold);
                    for (auto& [root, vec] : outerClusters) {
                        std::sort(vec.begin(), vec.end(),
                                  [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) { return a->z > b->z; });

                        if (vec.size() >= 2) {
                            // 原有逻辑：相邻层点之间螺旋
                            for (size_t i = 0; i + 1 < vec.size(); ++i) {
                                gp_Pnt from(vec[i]->contactPoints[0].x, vec[i]->contactPoints[0].y, vec[i]->z);
                                gp_Pnt to(vec[i+1]->contactPoints[0].x, vec[i+1]->contactPoints[0].y, vec[i+1]->z);
                                gp_Pnt fromCenter(vec[i]->center.x, vec[i]->center.y, vec[i]->z);
                                gp_Pnt toCenter(vec[i+1]->center.x, vec[i+1]->center.y, vec[i+1]->z);

                                from.Transform(fullTransform);
                                to.Transform(fullTransform);
                                fromCenter.Transform(fullTransform);
                                toCenter.Transform(fullTransform);

                                double actualRadius = vec[i]->radius;
                                if (actualRadius < 1e-6) actualRadius = 1.0;

                                objectOcctView->drawSpiralPath(from, to, fromCenter, toCenter,
                                                               helixAngleDeg, actualRadius, worldSafeZ);
                            }
                        } else if (vec.size() == 1) {
                            // 单点螺旋下刀
                            const MaxIncircleInfo* info = vec[0];
                            gp_Pnt from(info->contactPoints[0].x, info->contactPoints[0].y, info->z);
                            gp_Pnt fromCenter(info->center.x, info->center.y, info->z);
                            double actualRadius = info->radius;
                            if (actualRadius < 1e-6) actualRadius = 1.0;

                            double startAngle = std::atan2(from.Y() - fromCenter.Y(), from.X() - fromCenter.X());
                            double tanA = std::tan(helixAngleDeg * M_PI / 180.0);
                            double lead = (tanA > 1e-9) ? (2.0 * M_PI * actualRadius * tanA) : 1e6;
                            double turns = deltaZForSingle / lead;
                            turns = std::max(1.0, turns);
                            double totalAngleRad = -2.0 * M_PI * turns; // 右旋
                            double endAngle = startAngle + totalAngleRad;

                            gp_Pnt toCenter(info->center.x, info->center.y, info->z - deltaZForSingle);
                            gp_Pnt to(toCenter.X() + actualRadius * std::cos(endAngle),
                                      toCenter.Y() + actualRadius * std::sin(endAngle),
                                      info->z - deltaZForSingle);

                            from.Transform(fullTransform);
                            to.Transform(fullTransform);
                            fromCenter.Transform(fullTransform);
                            toCenter.Transform(fullTransform);

                            objectOcctView->drawSpiralPath(from, to, fromCenter, toCenter,
                                                           helixAngleDeg, actualRadius, worldSafeZ);
                        }
                    }

                    // ========== 2. 内轮廓独占区域紫红点：按 (groupId, regionId) 分组 ==========
                    std::map<std::pair<int, int>, std::vector<const MaxIncircleInfo*>> innerGroupRegionMap;
                    for (auto& info : innerCircles) {
                        if (!info.isOuter && info.isInnermost && info.regionId > 0 && !info.contactPoints.empty()) {
                            auto it = contourToGroup.find(info.contourId);
                            if (it != contourToGroup.end()) {
                                int gid = it->second;
                                innerGroupRegionMap[{gid, info.regionId}].push_back(&info);
                            }
                        }
                    }

                    for (auto& [key, vec] : innerGroupRegionMap) {
                        std::sort(vec.begin(), vec.end(),
                                  [](const MaxIncircleInfo* a, const MaxIncircleInfo* b) { return a->z > b->z; });

                        if (vec.size() >= 2) {
                            for (size_t i = 0; i + 1 < vec.size(); ++i) {
                                gp_Pnt from(vec[i]->contactPoints[0].x, vec[i]->contactPoints[0].y, vec[i]->z);
                                gp_Pnt to(vec[i+1]->contactPoints[0].x, vec[i+1]->contactPoints[0].y, vec[i+1]->z);
                                gp_Pnt fromCenter(vec[i]->center.x, vec[i]->center.y, vec[i]->z);
                                gp_Pnt toCenter(vec[i+1]->center.x, vec[i+1]->center.y, vec[i+1]->z);

                                from.Transform(fullTransform);
                                to.Transform(fullTransform);
                                fromCenter.Transform(fullTransform);
                                toCenter.Transform(fullTransform);

                                double actualRadius = vec[i]->radius;
                                if (actualRadius < 1e-6) actualRadius = 1.0;

                                objectOcctView->drawSpiralPath(from, to, fromCenter, toCenter,
                                                               helixAngleDeg, actualRadius, worldSafeZ);
                            }
                        } else if (vec.size() == 1) {
                            const MaxIncircleInfo* info = vec[0];
                            gp_Pnt from(info->contactPoints[0].x, info->contactPoints[0].y, info->z);
                            gp_Pnt fromCenter(info->center.x, info->center.y, info->z);
                            double actualRadius = info->radius;
                            if (actualRadius < 1e-6) actualRadius = 1.0;

                            double startAngle = std::atan2(from.Y() - fromCenter.Y(), from.X() - fromCenter.X());
                            double tanA = std::tan(helixAngleDeg * M_PI / 180.0);
                            double lead = (tanA > 1e-9) ? (2.0 * M_PI * actualRadius * tanA) : 1e6;
                            double turns = deltaZForSingle / lead;
                            turns = std::max(1.0, turns);
                            double totalAngleRad = -2.0 * M_PI * turns;
                            double endAngle = startAngle + totalAngleRad;

                            gp_Pnt toCenter(info->center.x, info->center.y, info->z - deltaZForSingle);
                            gp_Pnt to(toCenter.X() + actualRadius * std::cos(endAngle),
                                      toCenter.Y() + actualRadius * std::sin(endAngle),
                                      info->z - deltaZForSingle);

                            from.Transform(fullTransform);
                            to.Transform(fullTransform);
                            fromCenter.Transform(fullTransform);
                            toCenter.Transform(fullTransform);

                            objectOcctView->drawSpiralPath(from, to, fromCenter, toCenter,
                                                           helixAngleDeg, actualRadius, worldSafeZ);
                        }
                    }
                }
            } else {
                QMessageBox::warning(this, "生成失败", "无法从模型创建路径生成器。");
                goto cleanup;
            }
        }
    }

    // 正常结束，移除临时模型
    if (modelLoaded && !objects.empty()) {
        objects.pop_back();
    }
    progressDialog.setValue(100);
    progressDialog.close();
    return;

cleanup:
    if (modelLoaded && !objects.empty()) {
        objects.pop_back();
    }
    progressDialog.setValue(100);
    progressDialog.setLabelText("操作已取消");
    QTimer::singleShot(500, &progressDialog, &QProgressDialog::close);
}


void VisionWidget::Reset()
{
    objects.clear();
    objectOcctView->clearAll();
}
