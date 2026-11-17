#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vision/VisionMainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
   // setStyle();
    VisionMainWindow* visionWind = new VisionMainWindow;

    stackWid = new QStackedWidget();
    tabWidget = new QTabWidget();
    stackWid->addWidget(tabWidget);

    tabWidget->addTab(visionWind,QIcon(":/new/prefix1/resource/icon/servo.png"), QString("视觉"));

    setStyleSheet("font-size: 20pt;");
    setCentralWidget(stackWid);

    menuBar()->hide();
    statusBar()->showMessage(tr("Ready"));
    setWindowTitle(tr("解析G代码程序"));
    this->setMinimumSize(1024, 768);
    showFullScreen();
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::setStyle()
{
    qApp->setStyleSheet(R"(
        /* 全局基础 - 只设置颜色，不涉及尺寸 */
        QWidget {
            background-color: #2d2d2d;
            color: #e0e0e0;
            font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
            font-size: 12px;
        }

        QMainWindow, QDialog, QStackedWidget, QTabWidget {
            background-color: #252525;
        }

        QMenuBar {
            background-color: #333333;
            color: #ffffff;
            border-bottom: 1px solid #444444;
        }
        QMenuBar::item:selected {
            background-color: #4a4a4a;
        }
        QStatusBar {
            background-color: #333333;
            color: #cccccc;
            border-top: 1px solid #444444;
        }

        QLabel {
            color: #f0f0f0;
            background-color: transparent;
        }

        QPushButton {
            background-color: #3c3c3c;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #505050;
            border: 1px solid #6a6a6a;
        }
        QPushButton:pressed {
            background-color: #2a2a2a;
        }
        QPushButton:disabled {
            background-color: #2a2a2a;
            color: #888888;
            border: 1px solid #3f3f3f;
        }

        QToolButton {
            background-color: #3c3c3c;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
        }
        QToolButton:hover {
            background-color: #505050;
        }
        QToolButton:pressed {
            background-color: #2a2a2a;
        }

        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
            background-color: #1e1e1e;
            color: #f0f0f0;
            border: 1px solid #555555;
            border-radius: 3px;
            selection-background-color: #264f78;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #007acc;
        }

        QComboBox {
            background-color: #1e1e1e;
            color: #f0f0f0;
            border: 1px solid #555555;
            border-radius: 3px;
        }
        QComboBox:hover {
            border: 1px solid #6a6a6a;
        }
        QComboBox::drop-down {
            border-left: 1px solid #555555;
            background-color: #3c3c3c;
        }
        QComboBox QAbstractItemView {
            background-color: #2b2b2b;
            color: #e0e0e0;
            selection-background-color: #3e3e3e;
            border: 1px solid #555555;
        }

        QCheckBox, QRadioButton {
            color: #e0e0e0;
            background-color: transparent;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            background-color: #1e1e1e;
            border: 1px solid #777777;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background-color: #007acc;
            border: 1px solid #007acc;
        }

        /* 树形控件 */
        QTreeWidget {
            background-color: #252526;
            color: #cccccc;
            border: 1px solid #3c3c3c;
            outline: none;
            alternate-background-color: #2d2d2d;
        }
        QTreeWidget::item:hover {
            background-color: #2a2d2e;
        }
        QTreeWidget::item:selected {
            background-color: #094771;
            color: #ffffff;
        }

        QScrollBar:vertical {
            background: #2b2b2b;
        }
        QScrollBar::handle:vertical {
            background: #5a5a5a;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #787878;
        }
        QScrollBar:horizontal {
            background: #2b2b2b;
        }
        QScrollBar::handle:horizontal {
            background: #5a5a5a;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #787878;
        }

        QProgressBar {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #0e639c;
            border-radius: 3px;
        }

        QTabWidget::pane {
            background-color: #252525;
            border: 1px solid #3c3c3c;
        }
        QTabBar::tab {
            background-color: #333333;
            color: #cccccc;
            border: 1px solid #3c3c3c;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background-color: #252525;
            color: #ffffff;
            border-bottom: 1px solid #252525;
        }
        QTabBar::tab:hover {
            background-color: #3e3e3e;
        }

        QGroupBox {
            color: #f0f0f0;
            border: 1px solid #555555;
            border-radius: 5px;
            font-weight: bold;
        }

        QMessageBox {
            background-color: #2b2b2b;
        }
        QMessageBox QLabel {
            color: #f0f0f0;
        }

        /* 保留所有控件原有的 padding/margin/min-width 等，不在此处覆盖 */
    )");

}
