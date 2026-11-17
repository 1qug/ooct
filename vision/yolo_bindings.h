#ifndef YOLO_BINDINGS_H
#define YOLO_BINDINGS_H

#include <QImage>
#include <string>
#include <utility>
#include <vector>

struct YoloWrapperImpl;

class YoloWrapper {
public:
    YoloWrapper(const std::string& model_path, int imgsz, int width, int height);
    std::pair<QImage, std::vector<std::string>> getFrame();
    void stop();
    ~YoloWrapper();

private:
    struct Impl;
    // Impl* impl_;  // PIMPL模式隐藏Python细节
};
#endif
