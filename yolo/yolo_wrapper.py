# yolo_wrapper.py
import cv2
import numpy as np

# import numpy as np
# from numpy.distutils.misc_util import get_frame

from yolo_rs_object_size import MetalBlockDetector
# from fit_mask import FitMask
import sys
import os

# 获取当前模块所在目录的绝对路径
current_dir = os.path.dirname(os.path.abspath(__file__))
# 添加父目录或其他依赖路径（根据实际情况调整）
sys.path.append(os.path.join(current_dir, ".."))


class YoloWrapper:
    def __init__(self, model_path, imgsz=640, camera_width=640, camera_height=480):
        """初始化封装类，适配Qt显示需求"""
        self.detector = MetalBlockDetector(
            model_path=model_path,
            imgsz=imgsz,
            camera_width=camera_width,
            camera_height=camera_height
        )

    def get_frame(self):
        """
        获取适合Qt显示的帧和检测结果
        返回: (RGB图像numpy数组, 检测结果列表)
        """
        color_image, depth_image = self.detector.get_frames()
        if color_image is None:
            return None, []

        results, img = self.detector.detect(color_image)


        display_img,detection_data=self.detector.visualize_results(color_image, depth_image, results, img)
        # combined = np.hstack((display_img,depth_colormap))  #color_image,annotated_img,depth_colormap
        # cv2.imshow("Original | Edges | Segmentation | Depth", combined)

        # 转换为RGB格式(Qt需要)
        rgb_img = cv2.cvtColor(display_img, cv2.COLOR_BGR2RGB)

        # print("图像shape:", rgb_img.shape, "数据类型:", rgb_img.dtype)  # 应输出 (height, width, 3) 和 uint8
        # print(detection_data)  # 应包含 'name' 和 'depth'

        result = [rgb_img, detection_data]
        print(detection_data)
        return result

    def stop(self):
        """停止检测"""
        self.detector.stop_pipeline()

#aa=YoloWrapper('/home/fish/QSZ/yolov8/my_seg_train/dataset38/weights/best.pt',640,640,480)
#while True:
#    aa.get_frame()

