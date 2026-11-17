#undef slots
// #include <pybind11/embed.h>
// #include <pybind11/numpy.h>
// #include <pybind11/stl.h>
#include "yolo_bindings.h"
#include <iostream>
#include <QCoreApplication>   // 新增：获取可执行文件路径
#include <QDir>               // 新增：路径操作
#define slots Q_SLOTS

// namespace py = pybind11;


// void printPythonPath() {

//     py::gil_scoped_acquire acquire;
//     py::object sys = py::module::import("sys");
//     py::list path_list = sys.attr("path");
//     for (auto item : path_list) {
//         std::string path = py::cast<std::string>(item);
//     }
// }

// struct __attribute__((visibility("hidden"))) YoloWrapper::Impl {
//     py::object python_wrapper;

//     Impl(const std::string& model_path, int imgsz, int w, int h) {
//         py::gil_scoped_acquire acquire;

//         auto sys = py::module::import("sys");

//         // 1. 获取可执行文件所在目录，并计算 yolo 模块的绝对路径
//         QString appDir = QCoreApplication::applicationDirPath();
//         QDir dir(appDir);
//         dir.cdUp();  // 假设可执行文件在 RC/bin 下，回到 RC 根目录
//         dir.cdUp();
//         QString yoloModuleDir = dir.absolutePath() + "/yolo";

//         // 2. 添加模块搜索路径（使用绝对路径）
//         sys.attr("path").attr("append")(yoloModuleDir.toStdString());

//         // 3. 可选：打印路径以调试
//         std::cout << "Added to sys.path: " << yoloModuleDir.toStdString() << std::endl;

//         // 4. 导入模块
//         auto yolo_module = py::module::import("yolo_wrapper");
//         python_wrapper = yolo_module.attr("YoloWrapper")(model_path, imgsz, w, h);
//     }
// };


// YoloWrapper::YoloWrapper(const std::string& model_path, int imgsz, int width, int height)
//     : impl_(nullptr) {
//     impl_ = new Impl(model_path, imgsz, width, height);

// }

// std::pair<QImage, std::vector<std::string>> YoloWrapper::getFrame() {
//     py::gil_scoped_acquire acquire;
//     auto result = impl_->python_wrapper.attr("get_frame")();

//     auto result_tuple = py::cast<py::tuple>(result);

//     auto img_array = py::cast<py::array_t<uint8_t>>(result_tuple[0]);
//     auto buf = img_array.request();

//     QImage image(static_cast<uchar*>(buf.ptr),
//         img_array.shape(1),
//         img_array.shape(0),
//         QImage::Format_BGR888);


//     std::vector<std::string> detections;
//     auto det_list = py::cast<py::list>(result_tuple[1]);


//     for (size_t i = 0; i < det_list.size(); ++i) {
//         py::dict det_dict = py::cast<py::dict>(det_list[i]);
//         py::str det_str = det_dict;
//         detections.push_back(static_cast<std::string>(det_str));
//     }
//     return {image.rgbSwapped(), detections};
// }

// void YoloWrapper::stop() {

//     py::gil_scoped_acquire acquire;
//     impl_->python_wrapper.attr("stop")();
// }

// YoloWrapper::~YoloWrapper() {
//     delete impl_;
//     impl_ = nullptr;
// }
