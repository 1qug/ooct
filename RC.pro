QT       += core gui openglwidgets sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TEMPLATE = app

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
unix {
    INCLUDEPATH +=  /usr/local/include \
                    /usr/local/include/opencascade \   # 改为新版安装路径
                    /usr/include/libxml2 \
                    $$PWD/src/thirdparty/clipper/ \
                    $$PWD/src/thirdparty/muparser/include \
                    $$PWD/src/manager/ \
                    $$PWD/src/public/ \


    LIBS += -L/usr/local/lib -lethercat -lxml2 -lsqlite3 -lpthread  -lsocketcan # 添加依赖库
    LIBS += -lTKernel \
                -lTKMath \
                -lTKOffset \
                -lTKBRep \
                -lTKTopAlgo \
                -lTKShHealing \
                -lTKMesh \
                -lTKG2d \
                -lTKG3d \
                -lTKGeomBase \
                -lTKGeomAlgo \
                # -lTKSTEP \
                # -lTKSTEPBase \
                # -lTKSTEPAttr \
                # -lTKSTEP209 \
                -lTKDESTEP \
                -lTKDESTL  \
                # -lTKXDE \
                -lTKXSBase \
                -lTKV3d \
                -lTKOpenGl \
                -lTKService \
                -lTKCDF \
                -lTKCAF \
                -lTKLCAF \
                -lTKPrim \
                -lTKBool \
                -lTKBO \
                -lTKDEIGES \
                # -lTKSTL \
                -lTKXMesh   \

    # LIBS += /usr/lib/llvm-18/lib/libomp.so

CONFIG += link_pkgconfig
# PKGCONFIG += opencv4


# conda环境配置

#CONDA_ENV = /home/fish/miniconda3/envs/yolov8
#CONDA_ENV = /home/firsh/miniconda3/envs/yolov8
# CONDA_ENV = $$(HOME)/miniconda3/envs/yolov8


# PYTHON_INCLUDE = $$CONDA_ENV/include/python3.10


# # Python 库文件路径（包含 libpython3.10.so 的目录）
# PYTHON_LIB = $$CONDA_ENV/lib/libpython3.10.so

# # 引入头文件和库
# INCLUDEPATH += $$PYTHON_INCLUDE
# LIBS += $$PYTHON_LIB

# # 关键：添加运行时库路径（rpath），程序会自动到这个目录找库
# LIBS += -Wl,-rpath=$$CONDA_ENV/lib

# DEFINES += PYTHON_INCLUDE=\\\"$$PYTHON_INCLUDE\\\"
# DEFINES += PYTHON_LIB=\\\"$$PYTHON_LIB\\\"

# # 链接Python库
# INCLUDEPATH += $$PYTHON_INCLUDE

# # Pybind11
# PYBIND11_INCLUDE = $$CONDA_ENV/lib/python3.10/site-packages/pybind11/include
# exists($$PYBIND11_INCLUDE) {
#     INCLUDEPATH += $$PYBIND11_INCLUDE
# }

# CONDA_ENV = $$(HOME)/miniconda3/envs/yolov8
#     PYTHON_INCLUDE = $$CONDA_ENV/include/python3.10
#     PYTHON_LIB = $$CONDA_ENV/lib/libpython3.10.so

#     exists($$PYTHON_INCLUDE) {
#         INCLUDEPATH += $$PYTHON_INCLUDE
#         LIBS += $$PYTHON_LIB
#         LIBS += -Wl,-rpath=$$CONDA_ENV/lib
#     } else {
#         error("Python include path not found: $$PYTHON_INCLUDE")
#     }

#     # Pybind11 路径
#     PYBIND11_INCLUDE = $$CONDA_ENV/lib/python3.10/site-packages/pybind11/include
#     exists($$PYBIND11_INCLUDE) {
#         INCLUDEPATH += $$PYBIND11_INCLUDE
#     } else {
#         message("pybind11 not found in conda env, please install it.")
#     }


}

SOURCES += \
    common/detected_object.cpp \
    common/object_occt_view.cpp \
    common/occwindow.cpp \
    common/stl_slicer.cpp \
    common/stl_utils.cpp \
    common/stlmodelloader.cpp \
    main.cpp \
    mainwindow.cpp \
    public/GlobalMachiningData.cpp \
    thirdparty/clipper/clipper2/clipper.engine.cpp \
    thirdparty/clipper/clipper2/clipper.offset.cpp \
    thirdparty/clipper/clipper2/clipper.rectclip.cpp \
    vision/CreatePathDialog.cpp \
    vision/VisionMainWindow.cpp \
    vision/VisionWidget.cpp \
    vision/camera_wid.cpp \
    vision/yolo_bindings.cpp

HEADERS += \
    common/detected_object.h \
    common/object_occt_view.h \
    common/occwindow.h \
    common/stl_slicer.h \
    common/stl_utils.h \
    common/stlmodelloader.h \
    mainwindow.h \
    public/GlobalMachiningData.h \
    public/globalTools.h \
    thirdparty/clipper/clipper2/clipper.core.h \
    thirdparty/clipper/clipper2/clipper.engine.h \
    thirdparty/clipper/clipper2/clipper.export.h \
    thirdparty/clipper/clipper2/clipper.h \
    thirdparty/clipper/clipper2/clipper.minkowski.h \
    thirdparty/clipper/clipper2/clipper.offset.h \
    thirdparty/clipper/clipper2/clipper.rectclip.h \
    thirdparty/clipper/clipper2/clipper.version.h \
    vision/CreatePathDialog.h \
    vision/VisionMainWindow.h \
    vision/VisionWidget.h \
    vision/camera_wid.h \
    vision/yolo_bindings.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    image.qrc
