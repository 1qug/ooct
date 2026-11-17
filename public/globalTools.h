#ifndef GLOBALTOOLS_H
#define GLOBALTOOLS_H

#include <QApplication>
#include <QStyleFactory>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QStyle>
#include <QSpinBox>
#include <QScrollArea>
#include <QRegularExpression>
#include <QKeyEvent>


/// 自定义带单位的QLineEdit
class UnitQLineEdit : public QLineEdit {
    Q_OBJECT

public:
    UnitQLineEdit(QWidget *parent = nullptr): QLineEdit{parent} {
        m_unitButton = new QPushButton;
        // 单位按钮扁平无边框
        m_unitButton->setStyleSheet("border-style: solid; "); //color: black;
        m_unitButton->setFlat(true);

        QWidget *container = new QWidget(this);

        // 单位加入布局
        QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight, container);
        layout->addStretch();
        layout->setSpacing(0);
        // 单位按钮的Margins
        layout->setContentsMargins(0, 0, 13, 0);
        layout->addWidget(m_unitButton);
        container->setLayout(layout);

        QHBoxLayout *mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        mainLayout->addWidget(container, 0, Qt::AlignRight);

        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    ~UnitQLineEdit() {}

    // 获取编辑框单位
    QString unitText() const {
        return m_unitButton->text();
    }
    // 设置编辑框单位
    void setUnitText(const QString &unitText) {
        if (unitText.isEmpty() || m_unitButton->text() == unitText) {
            return;
        }
        m_unitButton->setFont(this->font());
        m_unitButton->setText(unitText);
        // 获得文字的宽度
        QFontMetrics fm(this->font());  // 获取字体度量对象
        int textWidth = fm.horizontalAdvance(m_unitButton->text());  // 计算文本宽度
        m_unitButton->setFixedWidth(textWidth);

        // 调整文本边距，防止输入内容被单位按钮覆盖
        // QBoxLayout *layout = static_cast<QBoxLayout*>(this->layout());
        // QMargins margins = layout->contentsMargins();
        // setTextMargins(margins.left(), margins.top(), textWidth + margins.right(), margins.bottom());

        m_unitButton->parentWidget()->adjustSize();
        setTextMargins(0, 0, textWidth + 13, 0);
    }

private:
    QPushButton *m_unitButton = nullptr; // 单位文本按钮
};


///@note: 类模板 -> 通过布局拿到里面的所有符合类型的控件指针
template<typename T>
QList<T*> findWidgetsInLayout(QLayout* layout) {
    QList<T*> result;
    if (!layout) return result;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);

        if (item->widget()) {
            if (T* widget = qobject_cast<T*>(item->widget())) {
                result.append(widget);
            }
        } else if (item->layout()) {
            // 递归查找嵌套布局
            result.append(findWidgetsInLayout<T>(item->layout()));
        }
    }
    return result;
}

// lineEdit文本编辑工具 -> 响应自定义的按键
class TextEditHelper {
public:
    // 追加文本
    static void appendText(QLineEdit* lineEdit, const QString& text) {
        if (!lineEdit) return;

        int cursorPos = lineEdit->cursorPosition();
        QString currentText = lineEdit->text();

        // 在光标位置插入文本
        currentText.insert(cursorPos, text);
        lineEdit->setText(currentText);

        // 移动光标到插入位置后
        lineEdit->setCursorPosition(cursorPos + text.length());
    }

    // 退格删除
    static void backspace(QLineEdit* lineEdit) {
        if (!lineEdit || lineEdit->text().isEmpty()) return;

        int cursorPos = lineEdit->cursorPosition();
        if (cursorPos > 0) {
            QString currentText = lineEdit->text();
            currentText.remove(cursorPos - 1, 1);
            lineEdit->setText(currentText);
            lineEdit->setCursorPosition(cursorPos - 1);
        }
    }

    // 删除键（删除光标后的字符）
    static void deleteChar(QLineEdit* lineEdit) {
        if (!lineEdit || lineEdit->text().isEmpty()) return;

        int cursorPos = lineEdit->cursorPosition();
        QString currentText = lineEdit->text();

        if (cursorPos < currentText.length()) {
            currentText.remove(cursorPos, 1);
            lineEdit->setText(currentText);
            lineEdit->setCursorPosition(cursorPos);
        }
    }

    // 移动到文本开头
    static void moveToHome(QLineEdit* lineEdit) {
        if (lineEdit) {
            lineEdit->setCursorPosition(0);
        }
    }

    // 移动到文本末尾
    static void moveToEnd(QLineEdit* lineEdit) {
        if (lineEdit) {
            lineEdit->setCursorPosition(lineEdit->text().length());
        }
    }
};


// QSpinBox类型适配
class SpinBoxEditHelper {
public:
    // 追加数字字符
    static void appendText(QSpinBox* spinBox, const QString& text) {
        if (!spinBox) return;

        // 获取当前值并转换为字符串
        int currentValue = spinBox->value();
        QString currentText = QString::number(currentValue);

        // 在字符串末尾追加字符
        QString newText = currentText + text;

        // 转换为数字并设置，如果转换失败则忽略
        bool ok;
        int newValue = newText.toInt(&ok);
        if (ok) {
            // 检查是否在有效范围内
            if (newValue >= spinBox->minimum() && newValue <= spinBox->maximum()) {
                spinBox->setValue(newValue);
            }
        }
    }

    // 退格删除（删除最后一位数字）
    static void backspace(QSpinBox* spinBox) {
        if (!spinBox) return;

        int currentValue = spinBox->value();
        QString currentText = QString::number(currentValue);

        if (currentText.length() > 1) {
            // 删除最后一位字符
            currentText.chop(1);
            bool ok;
            int newValue = currentText.toInt(&ok);
            if (ok) {
                spinBox->setValue(newValue);
            }
        } else {
            // 如果只有一位数字，设置为0或最小值
            spinBox->setValue(0);
        }
    }

    // 清空（设置为0）
    static void clear(QSpinBox* spinBox) {
        if (spinBox) {
            spinBox->setValue(0);
        }
    }

    // 增加数值
    static void stepUp(QSpinBox* spinBox) {
        if (spinBox) {
            spinBox->stepUp();
        }
    }

    // 减少数值
    static void stepDown(QSpinBox* spinBox) {
        if (spinBox) {
            spinBox->stepDown();
        }
    }

    // 设置特定值
    static void setValue(QSpinBox* spinBox, int value) {
        if (spinBox) {
            spinBox->setValue(value);
        }
    }

    // 数字键直接设置（0-9）
    static void pressNumber(QSpinBox* spinBox, int number) {
        if (!spinBox || number < 0 || number > 9) return;

        int currentValue = spinBox->value();
        QString currentText = QString::number(currentValue);

        // 如果是0，直接替换；否则追加
        if (currentValue == 0) {
            spinBox->setValue(number);
        } else {
            QString newText = currentText + QString::number(number);
            bool ok;
            int newValue = newText.toInt(&ok);
            if (ok && newValue <= spinBox->maximum()) {
                spinBox->setValue(newValue);
            }
        }
    }

    static void moveToHome(QSpinBox* spinBox) {
        if(spinBox) {
            QLineEdit *lineEdit = spinBox->findChild<QLineEdit*>();
            if (lineEdit) {
                lineEdit->setCursorPosition(0);
            }
        }
    }

    static void moveToEnd(QSpinBox* spinBox) {
        if(spinBox) {
            QLineEdit *lineEdit = spinBox->findChild<QLineEdit*>();
            if (lineEdit) {
                lineEdit->setCursorPosition(lineEdit->text().length());
            }
        }
    }
};

// 确保滚动条范围能显示到这个widget
static void ensureWidgetVisible(QWidget *widget)
{
    if (!widget) return;

    // 查找父容器中的 QScrollArea 或 QAbstractScrollArea
    QWidget *parent = widget->parentWidget();
    while (parent) {
        // 如果是 QScrollArea
        if (QScrollArea *scrollArea = qobject_cast<QScrollArea*>(parent)) {
            scrollArea->ensureWidgetVisible(widget);
            return;
        }
        // // 如果是其他滚动区域
        // if (QAbstractScrollArea *abstractScrollArea = qobject_cast<QAbstractScrollArea*>(parent)) {
        //     abstractScrollArea->ensureWidgetVisible(widget);
        //     return;
        // }
        parent = parent->parentWidget();
    }
}

static void appendTextSimple(QLineEdit* lineEdit, const QString& text) {
    if (!lineEdit) return;

    // 发送按键事件让 QLineEdit 自己处理
    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];
        QKeyEvent keyPress(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(lineEdit, &keyPress);

        QKeyEvent keyRelease(QEvent::KeyRelease, 0, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(lineEdit, &keyRelease);
    }
}

// 专用函数 - 设置特定值列表（如只能输入1,2,4,6,8）
static void setLineEditSpecificValues(QLineEdit* lineEdit, const QVector<int>& values)
{
    lineEdit->setProperty("inputType", "specific_values");
    if (lineEdit->validator()) {
        delete lineEdit->validator();
        lineEdit->setValidator(nullptr);
    }

    QStringList valueStrs;
    for (int value : values) {
        valueStrs << QString::number(value);
    }

    QRegularExpression regex("^(" + valueStrs.join("|") + ")$");
    lineEdit->setValidator(new QRegularExpressionValidator(regex, lineEdit));
}

// 专用函数 - 设置自定义正则表达式
static void setLineEditRegExp(QLineEdit* lineEdit, const QString& pattern)
{
    lineEdit->setProperty("inputType", "custom_regexp");
    if (lineEdit->validator()) {
        delete lineEdit->validator();
        lineEdit->setValidator(nullptr);
    }
    lineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(pattern), lineEdit));
}

static QString getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&time);

    char buffer[9];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", localTime);
    return QString::fromUtf8(buffer);
}

// class BlankdataUtils{
// public:
//     // 解析字典 集合的数据
//     static std::vector<std::map<std::string, std::string>> parseDetections(const std::vector<std::string>& detections) {
//         std::vector<std::map<std::string, std::string>> result;
//         for (const auto& det : detections) {
//             std::map<std::string, std::string> data;
//             QString qdet = QString::fromStdString(det);

//             // 关键修复：移除所有干扰符号（单引号、双引号、大括号）
//             qdet = qdet.remove("'").remove("\"").remove("{").remove("}").trimmed();

//             // 按逗号+空格分割键值对（处理如 "name: metal block, center_x: -1" 的格式）
//             QStringList parts = qdet.split(QRegularExpression(",\\s*"));

//             for (const QString& part : parts) {
//                 // 按第一个冒号分割键和值（避免值中含冒号的情况）
//                 int colonPos = part.indexOf(':');
//                 if (colonPos == -1) continue; // 跳过格式错误的部分

//                 QString key = part.left(colonPos).trimmed();
//                 QString val = part.mid(colonPos + 1).trimmed();

//                 // qDebug() << "解析到键:" << key << "，值:" << val;

//                 data[key.toStdString()] = val.toStdString();
//             }
//             result.push_back(data);
//         }
//         return result;
//     }
// };

#endif // GLOBALTOOLS_H
