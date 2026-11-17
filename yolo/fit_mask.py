import cv2
import numpy as np


class FitMask():

    def __init__(self, model_names,results, original,edge_color):
        self.model_names=model_names
        self.results=results
        self.original=original
        self.edge_color=edge_color



    def get_segmentation_edges(self):
        """从分割结果中提取指定类别的物体边缘（如matel block）"""
        cls_data = []
        for r in self.results:
            i=0
            if r.masks is not None and r.boxes is not None:  # 确保存在掩码和类别信息

                # 获取掩码数据和对应的类别
                masks = r.masks.data.cpu().numpy()
                classes = r.boxes.cls.cpu().numpy()  # 获取每个目标的类别索引
                class_names = self.model_names  # 获取类别名称映射表

                # 遍历每个掩码及其对应的类别
                for mask, cls_idx in zip(masks, classes):
                    cls_name = class_names[int(cls_idx)]  # 转换为类别名称


                    # 将掩码调整为原始图像尺寸
                    mask_resized = cv2.resize(mask, (self.original.shape[1], self.original.shape[0]))

                    # 二值化掩码（0或255）
                    _, binary_mask = cv2.threshold(mask_resized, 0.5, 255, cv2.THRESH_BINARY)
                    binary_mask = binary_mask.astype(np.uint8)


                    edges = np.zeros(self.original.shape[:2], dtype=np.uint8)  # 初始化边缘图像
                    # # 使用Canny边缘检测提取边缘
                    mask_edges = cv2.Canny(binary_mask, 50, 150)
                    # # 合并所有目标类别的边缘
                    edges = cv2.bitwise_or(edges, mask_edges)
                    # cv2.imwrite(f"{cls_name}.jpg",edges)
                    # self.edge_color[edges > 0] = [0, 0, 255]  # 在全0图像上绘制红色边缘线


                    square_data,mask,border_mask=self.fit_standard(cls_name,edges,i)
                    i+=1
                    if square_data is not None:
                        cls_data.append([square_data,mask,border_mask])

        return  cls_data

    def fit_standard(self,cls_name,edges,i):

        if cls_name == "metal block":  # 只处理目标类别
            square_data,mask=self.fit_square_from_edges(cls_name,edges,i)
        elif cls_name == "cylinder"  or cls_name == "annulus":   # annulus_outer
            square_data,mask=self.fit_circle_from_edges(cls_name,edges,i)
        else:
            return None,None,None

        border_mask = self.get_mask_border(mask, border_width=25)

        return   square_data,mask,border_mask

    def get_mask_border(self, mask, border_width=5):
        """
        获取掩码周围一圈的边缘掩码（边界区域）

        参数:
            mask: 原始二值掩码（0或255）
            border_width: 边缘宽度（像素），控制周围一圈的粗细

        返回:
            border_mask: 边缘区域的掩码（0或255）
        """
        """
        优化：通过腐蚀-膨胀创建更纯净的背景环形区域
        确保背景区域与目标有一定间距，避免边缘噪声影响
        """
        _, binary_mask = cv2.threshold(mask, 127, 255, cv2.THRESH_BINARY)

        # 定义结构元素（根据border_width动态调整）
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))


        # # 先腐蚀目标掩码：缩小目标区域，与背景区域拉开距离
        # eroded_mask = cv2.erode(binary_mask, kernel, iterations=2)  # 腐蚀2次，增加间距

        # 先膨胀目标掩码：放大目标区域，与背景区域拉开距离
        dilate_mask1 = cv2.dilate(binary_mask, kernel, iterations=2)  # 腐蚀2次，增加间距

        # 二次膨胀掩码：生成包含背景的区域
        dilated_mask2 = cv2.dilate(dilate_mask1, kernel, iterations=border_width)  # 膨胀至目标外border_width像素



        # 背景区域 = 膨胀区域 - 腐蚀后的目标区域（纯净的环形背景）
        border_mask = cv2.subtract(dilated_mask2, dilate_mask1)

        # 确保背景区域在图像范围内（避免越界无效值）
        h, w = binary_mask.shape[:2]
        border_mask = cv2.rectangle(border_mask, (0, 0), (w, h), 0, 1)  # 去除图像边缘1像素，避免边界无效值


        # zero_img = np.zeros_like(self.edge_color)  # 创建全0图像
        # zero_img[binary_mask == 255] = 100
        # zero_img[border_mask == 255] = 255  # 赋值白色
        # cv2.imwrite("border_mask.jpg", zero_img)

        return border_mask

    def fit_square_from_edges(self,cls_name, edges,i):
        """从边缘图像拟合成标准图形，返回更多信息"""
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            return None,None

        largest_contour = max(contours, key=cv2.contourArea)

        # rect = cv2.minAreaRect(largest_contour)

        epsilon = 0.01 * cv2.arcLength(largest_contour, True)  # 轮廓周长的1%作为阈值
        approx_contour = cv2.approxPolyDP(largest_contour, epsilon, True)
        rect = cv2.minAreaRect(approx_contour)  # 用简化后的轮廓拟合矩形
        # print("111",rect)
        box = cv2.boxPoints(rect)
        box = np.intp(box)

        center = rect[0]
        width, height = rect[1]
        angle = rect[2]
        # side_length = min(width,height)


        # 标准化角度
        if width < height:
            angle = 90 - angle
        if angle < -45:
            angle += 90
        if angle > 45:
            angle = 90 - angle    #换可编程管线不能用

        # aspect_ratio = min(width, height) / max(width, height) if max(width, height) > 0 else 0

        """将square_data['box']转换为掩码（ROI）"""
        mask = np.zeros(self.original.shape[:2], dtype=np.uint8)
        # 用多边形填充生成掩码（box的4个顶点构成多边形）
        cv2.fillPoly(mask, [np.int32(box)], 255)  # 255表示ROI区域

        # 计算掩码周围一圈的边缘掩码（宽度为2像素）
        # border_mask = self.get_mask_border(mask, border_width=20)
        # 动态计算边框宽度：目标最小边长的10%（但不小于3像素，不大于15像素）
        min_side = min(width, height)
        border_width = max(3, min(15, int(min_side * 0.1)))  # 自适应调整


        cv2.drawContours(self.edge_color, [box], 0, (0, 255, 0), 1)
        # border_masks.append(border_mask)  # 保存边缘掩码


        # if aspect_ratio > 0.85:
        return {
            'name':f"{i}",
            'cls_name':cls_name,
            # 'box': box,
            'center_x': center[0],
            'center_y': center[1],
            'angle': angle,
            # 'side_length_px': side_length,  # 边长（像素）
                'width_px': width,
                'height_px': height
            },mask
        # return None,None


    def fit_circle_from_edges(self, cls_name,edges,i):
        """从边缘图像拟合圆形，返回圆形信息"""
        # 先找到轮廓
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            return None, None

        # 选择面积最大的轮廓
        largest_contour = max(contours, key=cv2.contourArea)

        # 确保轮廓有足够的点来拟合圆
        if len(largest_contour) < 5:
            return None, None

        # 用最小二乘法拟合圆
        (x, y), radius = cv2.minEnclosingCircle(largest_contour)

        # # 计算圆形度（判断是否接近圆形）
        # area = cv2.contourArea(largest_contour)
        # perimeter = cv2.arcLength(largest_contour, True)
        # if perimeter == 0:
        #     circularity = 0
        # else:
        #     circularity = 4 * np.pi * area / (perimeter ** 2)

        # 创建圆形掩码
        mask = np.zeros(self.original.shape[:2], dtype=np.uint8)
        cv2.circle(mask, (int(x),int(y)),int(radius),255, -1)  # -1表示填充圆


        # 只有圆形度足够高才认为是有效的圆
        # if circularity > 0.7:  # 可根据实际情况调整阈值
        cv2.circle(self.edge_color,(int(x),int(y)),int(radius),(0,255,0),1)
        return {
            'name':f"{i}",
            'cls_name':cls_name,
            'center_x': x,
            'center_y': y,
            'radius_px': radius  #半径
        }, mask
        # else:print("圆形度不够")
        # return None, None

