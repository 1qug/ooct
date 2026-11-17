#include "camera_wid.h"

#include <QMessageBox>
#include <QVBoxLayout>
#include <QDir>
#include <QCoreApplication>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
CameraWid::CameraWid(QWidget *parent)
    : QWidget{parent}
{

    this->setWindowTitle("检测毛坯");


    init_ui();
    // set_style();
    set_connect();


    this->setFixedWidth(640);
    this->setMinimumHeight(640);
}

CameraWid::~CameraWid()
{
    // 1. 先停止计时器（避免计时器仍在调用 yoloDetector，导致释放后空指针崩溃）
    if (m_detectionTimer) {
        m_detectionTimer->stop(); // 停止计时器
        delete m_detectionTimer;  // 释放计时器内存
        m_detectionTimer = nullptr;
    }

    // 2. 释放 YoloWrapper（核心）
    // if (yoloDetector) {
    //     // 若 YoloWrapper 有显式关闭资源的方法（如关闭摄像头），先调用
    //     // yoloDetector->close(); // 示例：如果 YoloWrapper 实现了 close() 方法
    //     delete yoloDetector;       // 释放 YoloWrapper 实例
    //     yoloDetector = nullptr;    // 置空，避免野指针
    // }

}


void CameraWid::init_ui(){

    QVBoxLayout *Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);

    m_detectionDisplay = new QLabel("等待相机");
    m_detectionDisplay->setAlignment(Qt::AlignCenter);
    m_detectionDisplay->setStyleSheet("background-color: black; color: white; border: 1px solid #ccc;");
    m_detectionDisplay->setMinimumSize(640, 480);

    // m_textEdit = new QTextEdit();
    // m_textEdit->setMaximumHeight(120);


    QHBoxLayout* hlay_btn = new QHBoxLayout();
    hlay_btn->setContentsMargins(0, 0, 0, 0);
    hlay_btn->setSpacing(6);
    hlay_btn->setAlignment(Qt::AlignRight);

    QHBoxLayout* hlay_sBlank = new QHBoxLayout();
    m_lab_blank = new QLabel("选择毛坯:");
    m_combox_blank = new QComboBox();
    m_btn_stop = new QPushButton("暂停");
    m_combox_blank->setFixedSize(90, 32);
    m_btn_stop->setFixedSize(90, 32);


    hlay_sBlank->addWidget(m_lab_blank);
    hlay_sBlank->addWidget(m_combox_blank);
    hlay_sBlank->addWidget(m_btn_stop);



    m_btn_enter = new QPushButton("确定");
    m_btn_cancel = new QPushButton("取消");
    m_btn_enter->setFixedSize(90, 32);
    m_btn_cancel->setFixedSize(90, 32);

    hlay_btn->addLayout(hlay_sBlank);
    hlay_btn->addWidget(m_btn_enter);
    hlay_btn->addWidget(m_btn_cancel);


    Layout->addWidget(m_detectionDisplay);
    // Layout->addWidget(m_textEdit);
    Layout->addStretch(1); // 推按钮到底部
    Layout->addLayout(hlay_btn);


    m_lab_blank->hide();
    m_combox_blank->hide();
    m_btn_enter->setEnabled(false);

    // // 初始化YOLO
    // try {
    //     // 根据可执行文件位置动态计算模型路径
    //     QString appDir = QCoreApplication::applicationDirPath();
    //     QDir dir(appDir);
    //     dir.cdUp();  // 如果可执行文件在 RC/bin 下，回到 RC 根目录
    //     dir.cdUp();
    //     QString modelPath = dir.absolutePath() + "/yolo/best.pt";

    //     qDebug() << "Model path:" << modelPath;

    //     yoloDetector = new YoloWrapper(
    //         modelPath.toStdString(),
    //         640, 640, 480
    //         );
    // } catch (const std::exception& e) {
    //     QMessageBox::critical(this, "初始化错误", e.what());
    // }


    // 定时器连接
    m_detectionTimer = new QTimer(this);
    m_detectionTimer->setInterval(300);
    m_detectionTimer->start();

    qDebug() << "计时器启动状态：" << m_detectionTimer->isActive(); // 验证启动成功


}

void CameraWid::set_connect(){
    connect(m_detectionTimer, &QTimer::timeout, this, &CameraWid::updateDetectionFrame);

    //下拉框
    // connect(m_combox_blank, QOverload<int>::of(&QComboBox::currentIndexChanged),
    //         this, &CameraWid::onComboxBlankSelected);

    connect(m_btn_stop, &QPushButton::clicked, this, &CameraWid::onBtnStopClicked);
    connect(m_btn_enter, &QPushButton::clicked, this, &CameraWid::onBtnEnterClicked);
    connect(m_btn_cancel, &QPushButton::clicked, this, &CameraWid::onBtnCancelClicked);

}

void CameraWid::setBlankList(const QMap<QString, BlankInfo>& blankList) {
    m_combox_blank->clear();

    // qDebug() << "blankList.count=" << blankList.count();

    for (QMap<QString, BlankInfo>::const_iterator it = blankList.constBegin();
         it != blankList.constEnd(); ++it) {
        const QString& key = it.key();  // 获取键（名称）
        const BlankInfo& value = it.value();  // 获取值（BlankInfo）
        m_combox_blank->addItem(key, QVariant::fromValue(value)); //
    }
}


void CameraWid::updateDetectionFrame() {
    // qDebug() << "updateDetectionFrame 触发";

    // if (!yoloDetector) {
    //     qDebug() << "错误：yoloDetector 未初始化！";
    //     return;
    // }

    // blankList.clear();
    // try {
    //     auto [frame, detections] = yoloDetector->getFrame();

    //     m_detectionDisplay->setPixmap(QPixmap::fromImage(frame).scaled(
    //         m_detectionDisplay->size(),
    //         Qt::KeepAspectRatio,
    //         Qt::SmoothTransformation
    //         ));

    //     if (!s_detectionTimer) {
    //         if (m_detectionTimer && m_detectionTimer->isActive()) {
    //             m_detectionTimer->stop();

    //             auto parsedData = BlankdataUtils::parseDetections(detections);

    //             for (const auto& data : parsedData) {
    //                 if (data.find("cls_name") == data.end()) {
    //                     qDebug() << "未检测到数据中含有cls_name";
    //                     return;
    //                 }
    //                 std::string cls_name = data.at("cls_name");
    //                 qDebug() << "cls_name:" << QString::fromStdString(cls_name);

    //                 BlankInfo testBlank;

    //                 if (cls_name == "metal block") {
    //                     testBlank.name = "方体" + QString::fromStdString(data.at("name"));
    //                     testBlank.type = STOCK_CUBE;
    //                     testBlank.center = Point3D(
    //                         std::stof(data.at("center_x")),
    //                         std::stof(data.at("center_y")),
    //                         std::stof(data.at("z"))
    //                         );

    //                     double length = std::stof(data.at("width"));    // X方向
    //                     double width  = std::stof(data.at("height"));   // Y方向
    //                     double height = std::stof(data.at("depth"));    // Z方向
    //                     double angle  = std::stof(data.at("angle"));    // 旋转角度

    //                     testBlank.width  = length;
    //                     testBlank.height = width;
    //                     testBlank.depth  = height;
    //                     testBlank.angle  = angle;

    //                     // 创建立方体：先以原点为中心，再平移到中心并旋转
    //                     double halfX = length / 2.0;
    //                     double halfY = width  / 2.0;
    //                     double halfZ = height / 2.0;

    //                     gp_Pnt pMin(-halfX, -halfY, -halfZ);
    //                     gp_Pnt pMax( halfX,  halfY,  halfZ);
    //                     TopoDS_Shape boxAtOrigin = BRepPrimAPI_MakeBox(pMin, pMax).Shape();

    //                     gp_Trsf transform;
    //                     // 先平移，后旋转（旋转绕Z轴）
    //                     gp_Trsf translation;
    //                     translation.SetTranslation(gp_Vec(testBlank.center.x, testBlank.center.y, testBlank.center.z));
    //                     gp_Trsf rotation;
    //                     if (std::abs(angle) > 1e-6) {
    //                         rotation.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), angle * M_PI / 180.0);
    //                     }
    //                     transform = translation * rotation; // 先旋转后平移？注意顺序：先旋转（绕原点），再平移
    //                     BRepBuilderAPI_Transform shapeTransform(boxAtOrigin, transform);
    //                     testBlank.shape = shapeTransform.Shape();

    //                 } else if (cls_name == "cylinder") {
    //                     testBlank.name = "圆柱体" + QString::fromStdString(data.at("name"));
    //                     testBlank.type = STOCK_CYLINDER;
    //                     testBlank.center = Point3D(
    //                         std::stof(data.at("center_x")),
    //                         std::stof(data.at("center_y")),
    //                         std::stof(data.at("z"))
    //                         );

    //                     double radius = std::stof(data.at("radius"));
    //                     double height = std::stof(data.at("depth"));

    //                     testBlank.radius = radius;
    //                     testBlank.depth  = height;

    //                     // 创建圆柱体：几何中心在原点（底面在 -height/2，顶面在 height/2）
    //                     gp_Ax2 axis(gp_Pnt(0, 0, -height/2), gp_Dir(0, 0, 1));
    //                     TopoDS_Shape cylinderAtOrigin = BRepPrimAPI_MakeCylinder(axis, radius, height).Shape();

    //                     // 平移到目标中心
    //                     gp_Trsf translation;
    //                     translation.SetTranslation(gp_Vec(testBlank.center.x, testBlank.center.y, testBlank.center.z));
    //                     BRepBuilderAPI_Transform shapeTransform(cylinderAtOrigin, translation);
    //                     testBlank.shape = shapeTransform.Shape();
    //                 }

    //                 blankList.insert(testBlank.name, testBlank);
    //             }
    //             setBlankList(blankList);
    //             qDebug() << "填充下拉框";
    //         }
    //     }

    // } catch (const std::exception& e) {
    //     qWarning() << "检测出错:" << e.what();
    // }
}


void CameraWid::onBtnStopClicked(){

    if (s_detectionTimer) {
        // 当前正在运行，点击后停止
        s_detectionTimer = false;
        // m_detectionTimer->stop();
        m_lab_blank->show();
        m_combox_blank->show();
        m_btn_enter->setEnabled(true);
        m_btn_stop->setText("启动");
        qDebug() << "检测定时器已停止";
    } else {
        // 当前已停止，点击后启动
        s_detectionTimer = true;
        m_detectionTimer->start();
        m_lab_blank->hide();
        m_combox_blank->hide();
        m_btn_enter->setEnabled(false);
        m_btn_stop->setText("暂停");
        qDebug() << "检测定时器已启动";
    }

}

void CameraWid::onBtnEnterClicked()
{
    // 获取选中的毛坯信息
    if (m_combox_blank->currentIndex() >= 0) {
        QVariant data = m_combox_blank->currentData();
        if (data.canConvert<BlankInfo>()) {
            BlankInfo info = data.value<BlankInfo>();

            // 直接发射信号，将识别数据传递给调用方（如 StockDialog）
            emit blankInfoDetected(info);

            close();  // 关闭窗口
        }
    } else {
        QMessageBox::warning(this, "未选择", "请先选择一个毛坯");
    }
}
void CameraWid::onBtnCancelClicked(){
    close();
}

