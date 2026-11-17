#include "mainwindow.h"
#include <QApplication>

// 解决 Qt 的 slots 宏与 Python.h 的冲突
// #undef slots
// #include <pybind11/embed.h>
// #define slots Q_SLOTS

// namespace py = pybind11;

int main(int argc, char *argv[])
{
    // py::scoped_interpreter guard{};

    // {
    //     py::gil_scoped_release release;
    // }

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
