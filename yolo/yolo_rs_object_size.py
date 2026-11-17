import cv2
import numpy as np
import pyrealsense2 as rs
from ultralytics import YOLO
import time

from fit_mask import FitMask


class MetalBlockDetector:
    def __init__(self, model_path, imgsz=640, camera_width=640, camera_height=480):
        """初始化金属块检测器，整合YOLO检测和尺寸计算功能"""
        # 初始化YOLO模型
        self.model = YOLO(model_path)
        self.imgsz = imgsz

        # 初始化深度相机
        self.pipeline = rs.pipeline()
        self.config = rs.config()

        # 配置相机流
        self.config.enable_stream(rs.stream.color, camera_width, camera_height, rs.format.bgr8, 15)
        self.config.enable_stream(rs.stream.depth, camera_width, camera_height, rs.format.z16, 15)

        # 存储相机参数
        self.camera_width = camera_width
        self.camera_height = camera_height

        # 相机内参
        self.color_intrinsics=None
        self.fx = None
        self.fy = None
        self.cx = None
        self.cy = None

        # 启动相机并获取设备信息
        profile = self.pipeline.start(self.config)
        depth_sensor = profile.get_device().first_depth_sensor()
        self.depth_scale = depth_sensor.get_depth_scale()  # 深度缩放系数（1毫米=0.001米）

        self.device = profile.get_device()  # 修正：通过profile获取设备




        # 创建对齐对象（将深度帧与彩色帧对齐）
        align_to = rs.stream.color
        self.align = rs.align(align_to)

        # 用于计算FPS
        self.prev_time = 0

        # 获取相机内参
        self._get_camera_intrinsics(profile)  # 修正：传入profile参数



    def _get_camera_intrinsics(self, profile):
        """获取相机内参（修正版本）"""
        # 通过流配置文件获取内参
        # depth_profile = rs.video_stream_profile(profile.get_stream(rs.stream.depth))
        # depth_intrinsics = depth_profile.get_intrinsics()
        #
        # self.fx = depth_intrinsics.fx
        # self.fy = depth_intrinsics.fy
        # self.cx = depth_intrinsics.ppx
        # self.cy = depth_intrinsics.ppy

        # 改用彩色流的内参
        color_profile = rs.video_stream_profile(profile.get_stream(rs.stream.color))
        self.color_intrinsics = color_profile.get_intrinsics()
        self.fx = self.color_intrinsics.fx  # 替换为彩色流的fx
        self.fy = self.color_intrinsics.fy
        self.cx = self.color_intrinsics.ppx
        self.cy = self.color_intrinsics.ppy

    def get_frames(self):
        """获取对齐后的彩色帧和深度帧"""
        frames = self.pipeline.wait_for_frames(timeout_ms=10000)
        aligned_frames = self.align.process(frames)

        color_frame = aligned_frames.get_color_frame()
        depth_frame = aligned_frames.get_depth_frame()

        if not color_frame or not depth_frame:
            return None, None

        # 转换为numpy数组
        color_image = np.asanyarray(color_frame.get_data())
        depth_image = np.asanyarray(depth_frame.get_data())

        # 将深度图转换为米
        depth_image = depth_image * self.depth_scale  # RealSense深度数据单位是毫米 # 单位转换（毫米→米，float64）

        depth_image = depth_image.astype(np.float32)  # 转换为float32
        # 中值滤波去除孤立噪声点
        kernel_size = 3 if np.mean(depth_image[depth_image > 0]) < 2.0 else 5
        depth_image = cv2.medianBlur(depth_image, ksize=kernel_size)
        # 双边滤波保留边缘同时平滑区域
        depth_image = cv2.bilateralFilter(depth_image, d=9, sigmaColor=100, sigmaSpace=100)


        return color_image, depth_image

    def detect(self, color_image):
        """对图像进行目标分割检测"""
        img = cv2.resize(color_image, (self.imgsz, self.imgsz))
        results = self.model(img, conf=0.6, iou=0.5)
        return list(results), img

    def get_average_depth(self, depth_map, mask,use_median=True):
        """根据掩码计算平均深度"""
        """
           优化：增加离群点过滤，支持中位数（更抗异常值）
           use_median: True 适用于采集毛坯深度，False 适用于采集台面深度
           """
        roi_depth = depth_map[mask > 0]
        # 1. 基础过滤：去除0和极端大值（根据实际场景设定阈值，如最大合理深度2米）
        valid_depth = roi_depth[(roi_depth > 0) & (roi_depth < 0.5)]
        if len(valid_depth) == 0:
            raise ValueError("ROI区域内没有有效的深度数据")

        # 2. 离群点过滤：保留均值±2倍标准差范围内的数据
        mean = np.mean(valid_depth)
        std = np.std(valid_depth)
        valid_depth = valid_depth[(valid_depth >= mean - 2 * std) & (valid_depth <= mean + 2 * std)]

        if len(valid_depth) == 0:
            raise ValueError("过滤后无有效深度数据，可能存在大量异常值")


        if(use_median):
            median=np.median(valid_depth)
        else:
            # 获取前30%的最低值（值越低离相机越近 即深度值越小 实际高度越高 成反比）
            # 对有效深度升序排序（从小到大）
            sorted_depth = np.sort(valid_depth)  # 升序  [::-1] 实现降序
            # 计算30%的数量（向上取整，避免数量为0）
            top10_percent_num = max(1, int(len(sorted_depth) * 0.3))
            # 截取前30%的最低值
            top10_percent_depth = sorted_depth[:top10_percent_num]
            # 计算前30%最低值的中位数（代表前30%的水平）
            median = np.median(top10_percent_depth)


        print("深度值",median)
        return median


    def calculate_real_size(self, pixel_length, depth):
        """计算实际尺寸"""
        return (pixel_length * depth) / self.fx

    def visualize_results(self, color_image, depth_image, results, img):
        """可视化检测结果"""
        curr_time = time.time()
        fps = 1 / (curr_time - self.prev_time) if (curr_time - self.prev_time) > 0 else 0
        self.prev_time = curr_time
        print(f"帧率：{int(fps)}")
        text_params = {
            'fontFace': cv2.FONT_HERSHEY_SIMPLEX,
            'fontScale': 0.4,
            'color': (0, 255, 255),
            'thickness': 1,
            'lineType': cv2.LINE_AA
        }
        detection_data=[]
        edge_color = np.zeros_like(color_image)  # 创建全0图像
        Class_fit=FitMask(self.model.names,results,color_image,edge_color)
        cls_datas=Class_fit.get_segmentation_edges()

        # edge_color[edges > 0] = [0, 0, 255]     #在全0图像上绘制红色边缘线

        if cls_datas is not None and cls_datas !=[]:
            i=60
            for cls_data in cls_datas:
                cv2.circle(edge_color,(int(cls_data[0]['center_x']),int(cls_data[0]['center_y'])),1,(0,0,255),-1)
                cv2.putText(edge_color, cls_data[0]['name'],
                            (int(cls_data[0]['center_x']), int(cls_data[0]['center_y'])), **text_params)
                try:
                    # 用掩码提取深度数据
                    obj_avg_depth = self.get_average_depth(depth_image, cls_data[1],True) /self.depth_scale
                    bg_avg_depth = self.get_average_depth(depth_image, cls_data[2]) /self.depth_scale
                    # print(bg_avg_depth)
                    error_compensation=0
                    cls_data[0]['depth']=float(error_compensation+bg_avg_depth-obj_avg_depth)
                    cls_data[0]['plane_depth']=float(bg_avg_depth)

                    side_px = {k: v for k, v in cls_data[0].items() if k.endswith("_px")}
                    for key in side_px:
                        cls_data[0][key[:-3]]=float(self.calculate_real_size(side_px[key], obj_avg_depth))

                    cls_data[0]['center_x']=(cls_data[0]['center_x']-self.cx)*cls_data[0]['plane_depth']/ self.fx
                    cls_data[0]['center_y']=-(cls_data[0]['center_y']-self.cy)*cls_data[0]['plane_depth']/ self.fy
                    cls_data[0]['z'] =0
                    non_px = {k: v for k, v in cls_data[0].items() if not k.endswith("_px")}


                    i += 20
                    detection_data.append(non_px)

                except Exception as e:
                    print(f"深度计算错误: {str(e)}")
                    bg_avg_depth=0

            # cv2.putText(edge_color, f"FPS: {int(fps)}", (10, 20), **text_params)
            # cv2.putText(edge_color, f"O: ({int(self.cx)}, {int(self.cy)},{int(bg_avg_depth)})", (10, 40), **text_params)

        original_with_edges = cv2.addWeighted(color_image, 0.8, edge_color, 1.0, 0)

        annotated_img = None
        for r in results:
            annotated_img = r.plot()  #boxes=False
            annotated_img = cv2.resize(annotated_img, (self.camera_width, self.camera_height))

        if annotated_img is None:
            annotated_img = cv2.resize(img, (self.camera_width, self.camera_height))

        depth_colormap = cv2.applyColorMap(cv2.convertScaleAbs(depth_image /self.depth_scale, alpha=0.1), cv2.COLORMAP_JET)
        # combined = np.hstack((original_with_edges,annotated_img))  #color_image,annotated_img,depth_colormap
        # cv2.imshow("Original | Edges | Segmentation | Depth", combined)

        return original_with_edges,detection_data


    def run(self):
        """运行实时检测主循环"""

        # 初始化视频编写器
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')  # 使用MP4编码
        out = cv2.VideoWriter("../111.mp4", fourcc, 30.0,
                              (self.color_intrinsics.width, self.color_intrinsics.height))
        try:
            while True:
                color_image, depth_image = self.get_frames()
                if color_image is None:
                    continue

                results, img = self.detect(color_image)

                self.visualize_results(color_image, depth_image, results, img)

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    out.release()
                    cv2.destroyAllWindows()

                    break


        finally:
            self.pipeline.stop()
            cv2.destroyAllWindows()


if __name__ == "__main__":
    MODEL_PATH = "/home/fish/PyCharmProjects/yolov8/my_seg_train/exp_13/weights/best.pt"  # 替换为你的模型路径
    IMAGE_SIZE = 640
    CAMERA_WIDTH = 640
    CAMERA_HEIGHT = 480

    detector = MetalBlockDetector(
        model_path=MODEL_PATH,
        imgsz=IMAGE_SIZE,
        camera_width=CAMERA_WIDTH,
        camera_height=CAMERA_HEIGHT
    )
    # detector.run()