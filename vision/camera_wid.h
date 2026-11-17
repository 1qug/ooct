#ifndef CAMERA_WID_H
#define CAMERA_WID_H


#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

#include <QMatrix4x4>
#include <TopoDS_Shape.hxx>
#include "yolo_bindings.h"
#include "common/stl_slicer.h"
#include "common/stl_utils.h"

enum StockType {
    STOCK_CUBE,
    STOCK_CYLINDER
};
struct BlankInfo {
    QString name;
    StockType type;  // 使用 StockType 枚举
    TopoDS_Shape shape;
    Point3D center;
    double width;     // 对于立方体
    double height;    // 对于立方体
    double depth;     // 对于立方体
    double plane_depth;
    double radius;    // 对于圆柱
    double angle;     // 旋转角度

    BlankInfo() : /*type(STOCK_CUBE), */width(0), height(0), depth(0),
        plane_depth(0), radius(0), angle(0) {}
};


// 深度相机子窗口
class CameraWid : public QWidget
{
    Q_OBJECT
public:
    explicit CameraWid(QWidget *parent = nullptr);
    ~CameraWid();
    QTimer* m_detectionTimer;
    bool s_detectionTimer= true;
    BlankInfo blankInfo;

signals:
    void blankInfoDetected(const BlankInfo& info);  // 传递识别到的毛坯信息
public slots:
    void updateDetectionFrame();            // 计时器事件 更新视频流检测到的数据

    void onBtnStopClicked();
    void onBtnEnterClicked();
    void onBtnCancelClicked();

private:
    void init_ui();
    void set_style();
    void set_connect();

    void setBlankList(const QMap<QString, BlankInfo>& blankList) ;


private:

    QLabel* m_detectionDisplay;
    QTextEdit* m_textEdit;

   // YoloWrapper* yoloDetector;

    // QMap<QString, QVariant> blankList;//负责传给参数设置对话框的指定毛坯下拉框
    QMap<QString, BlankInfo> blankList;

    QLabel*         m_lab_blank;
    QComboBox*      m_combox_blank;
    QPushButton*    m_btn_stop;

    QPushButton*    m_btn_enter;
    QPushButton*    m_btn_cancel;
};

#endif // CAMERA_WID_H
