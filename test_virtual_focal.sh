#!/bin/bash
# 简单测试脚本验证虚拟相机焦距计算

echo "Testing virtual camera focal length calculation..."
echo ""

# 测试参数
FOV1=220  # 极端广角 (> 200°)
FOV2=175  # 极端广角 (> 170°)
FOV3=150  # 广角 (> 130°)
IMAGE_WIDTH=1600
IMAGE_HEIGHT=1200

# 计算虚拟焦距
# 公式: virtual_fx = width / (2 * tan(FOV/2))

calculate_virtual_fx() {
    local fov=$1
    local width=$2
    # 转换为弧度并计算
    python3 << EOF
import math
fov_rad = $fov * math.pi / 180.0
virtual_fx = $width / (2.0 * math.tan(fov_rad / 2.0))
# 限制在合理范围内
if $fov > 200.0:
    virtual_fx = max(100.0, min(250.0, virtual_fx))
elif $fov > 170.0:
    virtual_fx = max(150.0, min(280.0, virtual_fx))
elif $fov > 130.0:
    virtual_fx = max(200.0, min(350.0, virtual_fx))
print(f"FOV={$fov}°, virtual_fx={virtual_fx:.1f}")
EOF
}

echo "虚拟相机焦距计算结果:"
echo "=========================="
calculate_virtual_fx $FOV1 $IMAGE_WIDTH
calculate_virtual_fx $FOV2 $IMAGE_WIDTH
calculate_virtual_fx $FOV3 $IMAGE_WIDTH
echo ""
echo "说明:"
echo "- FOV > 200°: 使用公式计算，限制在 [100, 250]"
echo "- FOV > 170°: 使用公式计算，限制在 [150, 280]"
echo "- FOV > 130°: 使用公式计算，限制在 [200, 350]"
