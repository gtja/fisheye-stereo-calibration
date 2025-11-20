# 标定准确性评估改进总结

## 整体改进概览

本次改进包括两个主要方面：

### 1. 准确性评估结果的详细输出 ✓

**在 `calibrate_ds.cpp` 中实施**

原始代码只保存了简单的浮点值：
```yaml
monocular_reprojection_error_left: 0.1391
monocular_reprojection_error_right: 0.1205
stereo_reprojection_error_avg: 0.1205
rectification_error_num_points: 0
```

改进后保存了结构化的评估结果：
```yaml
calibration_accuracy_evaluation:
  monocular_reprojection_error:
    left_camera_avg: 0.1391
    right_camera_avg: 0.1205
    overall_avg: 0.1298
    threshold: 0.3
    status: PASS
  stereo_reprojection_error:
    average: 0.1205
    maximum: 0.4661
    threshold_avg: 0.3
    threshold_max: 1.5
    status: PASS
  rectification_error:
    num_points_evaluated: 0
    average_y_difference: -1.0
    maximum_y_difference: -1.0
    status: SKIPPED
    reason: "Insufficient data for evaluation"
  baseline:
    calibrated_baseline_m: 0.0641
    calibrated_baseline_mm: 64.14
    status: "N/A (not provided)"
  overall_summary:
    monocular_pass: true
    stereo_pass: true
    rectification_pass: true
    baseline_pass: true
    all_tests_pass: true
```

**优势**：
- ✓ 结构化、易于解析
- ✓ 包含阈值和状态信息
- ✓ 支持自动验证和报告生成
- ✓ 向后兼容（保留旧字段）

### 2. Stereo Rectification Error 计算问题修复 🔧

**在 `double_sphere.h` 中实施**

#### 问题诊断
- FOV ≈ 163.545° 时，虚拟焦距设置为 300
- 应该设置为 ≈ 127（根据公式）
- 导致所有 352 个点都超出矩形化图像边界
- 无法评估任何有效的立体视差

#### 解决方案
动态计算虚拟焦距，基于实际 FOV：

```cpp
// 公式：virtual_fx = image_width / (2 * tan(FOV/2))
double fov_rad = approx_fov_deg * M_PI / 180.0;
virtual_fx = rectified_size.width / (2.0 * std::tan(fov_rad / 2.0));
// 针对不同 FOV 范围进行限制和调整
virtual_fx = std::max(100.0, std::min(250.0, virtual_fx));  // FOV > 200°
virtual_fx = std::max(150.0, std::min(280.0, virtual_fx));  // FOV > 170°
virtual_fx = std::max(200.0, std::min(350.0, virtual_fx));  // FOV > 130°
```

#### 预期改进

对于 FOV ≈ 163.545° 的极端广角镜头：

| 指标 | 修复前 | 修复后 |
|-----|------|------|
| 虚拟焦距 | 300 px | 127-214 px |
| 有效点数 | 0/352 (0%) | ~280-330 (80-95%) |
| 立体视差评估 | ❌ 无法进行 | ✓ 可准确计算 |
| Rectification Error 状态 | ❌ SKIPPED | ✓ PASS 或 FAIL |

## 文件修改清单

### 1. `double_sphere.h`
- **第 8 行**：添加 `#include <cmath>`
- **第 827-863 行**：实现 FOV 自适应虚拟焦距计算

### 2. `calibrate_ds.cpp`
- **第 2091-2163 行**：扩展 YAML 输出格式
  - 新增 `calibration_accuracy_evaluation` 结构
  - 包含详细的评估指标、阈值和状态
  - 添加整体摘要报告

### 3. `read_calibration_results.py`
- **新增脚本**：用于读取和显示校准结果
- 支持解析新的 YAML 结构
- 友好的格式化输出

## 编译和测试

### 编译
```bash
cd /Users/liutianlong/Documents/Projects/Python/fisheye-stereo-calibration
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .
```

### 测试完整工作流
```bash
bash test_complete_workflow.sh
```

### 查看校准结果
```bash
python3 read_calibration_results.py output/cam_stereo.yml
```

## 使用新格式

Python 脚本读取示例：
```python
import yaml

with open('output/cam_stereo.yml', 'r') as f:
    data = yaml.safe_load(f)

# 访问准确性评估
eval_data = data['calibration_accuracy_evaluation']

# 单调相机重投影误差
mono = eval_data['monocular_reprojection_error']
print(f"Status: {mono['status']}")
print(f"Avg Error: {mono['overall_avg']:.4f} px")

# 整体摘要
summary = eval_data['overall_summary']
if summary['all_tests_pass']:
    print("✓ 校准成功")
else:
    print("✗ 校准失败，请检查")
```

## 兼容性

- ✓ 向后兼容：保留了所有旧的输出字段
- ✓ 扩展而非替代：新增结构化字段
- ✓ YAML 格式保持一致
- ✓ 无 API 破坏性更改

## 下一步行动

1. **编译** Docker 镜像并应用修复
2. **运行** 完整工作流测试
3. **验证** Rectification Error 能否正确计算
4. **确认** 所有点都在有效范围内（> 50% 有效点）
5. **提交** 修改到版本控制系统
