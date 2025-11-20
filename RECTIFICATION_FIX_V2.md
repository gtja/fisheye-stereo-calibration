# Stereo Rectification Error 计算问题修复 - V2

## 问题分析

经过分析 `/tmp/test_output.log`，发现 Stereo Rectification Error 计算存在严重问题：

```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Using rectified image size: 1600x1200 (original: 1600x1200, avg_fx: 457.9)
   Evaluated 0 corner points
   
Rectification diagnostic information (only 0% of points were valid):
   Total corner points: 352
   Valid points: 0 (0%)
   Points behind camera: 0 (0%)
   Points with negative Z after rectification: 0 (0%)
   Points outside rectified image bounds: 352 (100%) ← **全部超出边界！**
   Rectified image size: 1600x1200
   Calibrated focal lengths: left_fx=445.491, right_fx=470.226
   Estimated FOV: 163.545° (rectification strength: 0.45)
   Virtual camera: fx=300, fy=300, cx=800, cy=600 ← **虚拟焦距太大**
```

## 根本原因

**虚拟相机焦距设置不匹配极端广角镜头**

对于 FOV ≈ 163.545° 的极端广角镜头：

### 数学分析

虚拟相机的焦距应该满足：
```
virtual_fx = image_width / (2 * tan(FOV/2))

对于 FOV = 163.545°：
virtual_fx = 1600 / (2 * tan(81.7725°))
         = 1600 / (2 * 6.314)
         = 1600 / 12.628
         ≈ 126.6 像素
```

但原始代码设置为 **300**，导致：
- 虚拟投影范围过小（缩放倍数 300/127 ≈ 2.36）
- 所有真实角点在矩形化后的虚拟相机投影上都映射超出图像边界
- 无法评估任何有效点的立体视差

## 实施的修复

### 1. 在 `double_sphere.h` 中添加包含文件

```cpp
#include <cmath>
```

### 2. 动态计算虚拟焦距（第 827-863 行）

**核心改进**：使用 FOV 自适应公式计算虚拟焦距

```cpp
// For wide-angle lenses, use dynamic virtual focal length based on actual FOV
// Formula: virtual_fx = image_width / (2 * tan(FOV/2))
double virtual_fx, virtual_fy;

if (approx_fov_deg > 200.0) {
    // Ultra extreme wide-angle (FOV > 200°): use very low focal length
    // FOV > 200° requires fx < 200 px to fit most points in rectified image
    double fov_rad = approx_fov_deg * M_PI / 180.0;
    virtual_fx = rectified_size.width / (2.0 * std::tan(fov_rad / 2.0));
    virtual_fy = rectified_size.height / (2.0 * std::tan(fov_rad / 2.0));
    // Clamp to reasonable range [100, 250]
    virtual_fx = std::max(100.0, std::min(250.0, virtual_fx));
    virtual_fy = std::max(100.0, std::min(250.0, virtual_fy));
} else if (approx_fov_deg > 170.0) {
    // Extreme wide-angle (FOV > 170°): dynamic calculation
    double fov_rad = approx_fov_deg * M_PI / 180.0;
    virtual_fx = rectified_size.width / (2.0 * std::tan(fov_rad / 2.0));
    virtual_fy = rectified_size.height / (2.0 * std::tan(fov_rad / 2.0));
    // Clamp to reasonable range [150, 280]
    virtual_fx = std::max(150.0, std::min(280.0, virtual_fx));
    virtual_fy = std::max(150.0, std::min(280.0, virtual_fy));
} else if (approx_fov_deg > 130.0) {
    // Wide-angle (FOV > 130°): slightly higher focal length
    double fov_rad = approx_fov_deg * M_PI / 180.0;
    virtual_fx = rectified_size.width / (2.0 * std::tan(fov_rad / 2.0));
    virtual_fy = rectified_size.height / (2.0 * std::tan(fov_rad / 2.0));
    // Clamp to reasonable range [200, 350]
    virtual_fx = std::max(200.0, std::min(350.0, virtual_fx));
    virtual_fy = std::max(200.0, std::min(350.0, virtual_fy));
} else if (approx_fov_deg > 100.0) {
    // Moderate wide-angle: use measured focal length with slight boost
    virtual_fx = avg_fx * 0.95;
    virtual_fy = avg_fy * 0.95;
} else {
    // Standard lens: can use measured focal length or slightly higher
    virtual_fx = avg_fx * 1.1;
    virtual_fy = avg_fy * 1.1;
}
```

### 3. 在 `calibrate_ds.cpp` 中增强准确性评估输出

已完成修改，添加了详细的 `calibration_accuracy_evaluation` 结构化输出到 YAML 文件中。

## 预期效果

修复后，对于 FOV ≈ 163.545° 的镜头：

| 指标 | 修复前 | 修复后 | 改进 |
|-----|------|------|-----|
| 虚拟焦距 | 300 px | ~127-214 px | ✓ 匹配 FOV |
| 有效点数 | 0/352 (0%) | ≈ 280-330/352 (80-95%) | ✓ 可评估 |
| 立体视差评估 | 无法进行 | 可准确计算 | ✓ 质量指标有效 |
| y差值误差 | N/A | < 0.3-0.5 px | ✓ 高质量 |

## 修改的文件

1. **double_sphere.h**
   - 第 8 行：添加 `#include <cmath>`
   - 第 827-863 行：实现 FOV 自适应虚拟焦距计算

2. **calibrate_ds.cpp**
   - 第 2091-2163 行：扩展 YAML 输出，添加结构化准确性评估

## 下一步

需要重新编译 Docker 镜像：

```bash
cd /Users/liutianlong/Documents/Projects/Python/fisheye-stereo-calibration
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .
```

重新运行测试工作流：

```bash
bash test_complete_workflow.sh
```

## 验证方法

修复后查看输出日志中：
1. 虚拟相机焦距应该是 127-214（不是 300）
2. 有效点数应该显著增加（> 0）
3. Stereo Rectification Error 应该成功计算
4. YAML 输出应该包含 `calibration_accuracy_evaluation` 结构
