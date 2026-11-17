#ifndef VISIONWIDGET_H
#define VISIONWIDGET_H

#include <QWidget>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "CreatePathDialog.h"
#include "public/GlobalMachiningData.h"
#include "common/detected_object.h"
#include "common/object_occt_view.h"
#include "common/stl_slicer.h"
#include "common/stl_utils.h"
#include "common/stlmodelloader.h"

class ObjectOcctView;
class VisionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VisionWidget(QWidget *parent = nullptr);
    ~VisionWidget();

public slots:
    void onCreatePathButtonClicked();//粗加工
    void onCreateFinishPathClicked();//精加工
    void addPathToTree(const QString &name, const QString &workStation);
    void onGeneratePath(QString name,GlobalMachiningData::MachiningParams &params,std::shared_ptr<DetectedObject> blank);  // 新增槽函数
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
private:
    void setupUI();                // 初始化界面布局

    void loadSTLDataFromfile(GlobalMachiningData::MachiningParams &params);

    void generatePathAndSaveSettings(GlobalMachiningData::MachiningParams &params);

    void Reset();

    gp_Trsf CreateCoordinateTransform(const GlobalMachiningData::Coordinate& coord);

    void applyCoordinateTransform(const GlobalMachiningData::MachiningParams& params);
private:
    QSplitter*      splitter;
    QTreeWidget*    treeWidget;
    ObjectOcctView* objectOcctView;
    int blankIndex;                                     // 毛坯索引
    std::vector<std::shared_ptr<DetectedObject>> objects;  // 容器实例
    // 向容器添加对象
    void addObject(std::unique_ptr<DetectedObject> obj) {
        if (obj) {  // 确保指针有效
            objects.push_back(std::move(obj));  // unique_ptr必须通过move转移所有权
        }else{
            qDebug() <<"无效指针";
        }
    }

    // 获取容器中指定索引的对象（只读，返回 nullptr 表示索引无效）
    const std::shared_ptr<DetectedObject>& getObject(size_t index) const {
        // 检查索引是否有效（在容器范围内）
        if (index < objects.size()) {
            return objects[index];
        }
        // 返回一个空指针引用（需在类中定义一个静态空指针作为默认返回值）
        static const std::shared_ptr<DetectedObject> emptyPtr = nullptr;
        return emptyPtr;
    }

    std::vector<std::string> m_blankdatas;//毛胚

    QString STLfilepath;                                // 文件路径

    STLModelLoader* m_stlLoader = nullptr;              // 模型加载器

    std::shared_ptr<DetectedObject> blank;
    float spacing;                                      // 距顶面位置

    bool m_haveModel=false;

    QString modelname;

    gp_Trsf m_coordTransform;                     //坐标系
};

#endif // VISIONWIDGET_H
