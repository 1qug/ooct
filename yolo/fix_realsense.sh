#!/bin/bash
echo "=== RealSense D435 权限修复 ==="

# 1. 找到设备路径
echo "1. 查找设备路径..."
VIDEO_DEVICES=$(ls /dev/video* 2>/dev/null)
USB_DEVICE=$(find /dev/bus/usb -type c 2>/dev/null | xargs -I {} sh -c 'lsusb -D {} 2>/dev/null | grep -q "8086:0b07" && echo {}')

echo "视频设备:"
for dev in $VIDEO_DEVICES; do
    ls -la $dev
done

echo -e "\nUSB设备:"
for dev in $USB_DEVICE; do
    ls -la $dev
done

# 2. 授予权限
echo -e "\n2. 授予权限..."
sudo chmod 666 /dev/video* 2>/dev/null
for dev in $USB_DEVICE; do
    sudo chmod 666 $dev
done

# 3. 创建永久的udev规则
echo -e "\n3. 创建udev规则..."
cat << 'UDEV' | sudo tee /etc/udev/rules.d/99-realsense-d435.rules
# RealSense D435 (Intel 8086:0b07)
SUBSYSTEM=="usb", ATTR{idVendor}=="8086", ATTR{idProduct}=="0b07", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="video4linux", ATTR{idVendor}=="8086", ATTR{idProduct}=="0b07", MODE="0666", GROUP="video"
UDEV

# 4. 重新加载udev
sudo udevadm control --reload-rules
sudo udevadm trigger

# 5. 添加用户到组
echo -e "\n4. 添加用户到权限组..."
sudo usermod -a -G video,dialout,plugdev $USER

echo -e "\n✅ 修复完成！"
echo "请重新插拔摄像头，或重启电脑使更改生效"
echo "然后运行: python test_realsense.py"
