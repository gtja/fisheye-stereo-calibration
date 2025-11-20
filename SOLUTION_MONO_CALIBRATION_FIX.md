# 单目标定文件保存问题 - 完整解决方案

## 问题描述

在 Docker 中运行 `step1_monocular_calibration.sh` 时，mono 模式只为左相机生成了标定文件 `left_ds.yml`，但右相机的 `right_ds.yml` 文件没有生成。

### 错误信息
```
Error: Right camera calibration file was not created or not readable: /data/output/right_ds.yml
```

## 根本原因分析

`calibrate_ds.cpp` 代码中存在两个分离的 mono 模式处理块：

1. **第 629-776 行**（初始标定块）
   - 执行 KB4 fisheye 或 omnidir 模型标定
   - 成功标定后设置相机参数
   - **问题**：标定成功后直接继续执行，未保存文件

2. **第 954+ 行**（文件保存块）
   - 本应保存标定结果到文件
   - **问题**：由于代码流程，没有被正确执行

## 解决方案

### 修复内容

在第一个 mono 模式块中的成功分支，标定成功后**立即保存文件并返回**，而不是继续执行后续代码。

#### 左相机修复（第 665-707 行）
```cpp
printf("[DEBUG] Successfully set K2_kb4 and D2_kb4\n");
fflush(stdout);

// EARLY EXIT: 成功标定后立即保存文件
if (!ensure_output_directory(out_file)) {
    cerr << "Error: Cannot create output directory" << endl;
    return 1;
}

FileStorage fs(out_file, FileStorage::WRITE);
if (!fs.isOpened()) {
    cerr << "Error: Cannot open output file" << endl;
    return 1;
}

// 写入Double-Sphere参数...
fs << "model_type" << "double_sphere";
fs << "camera" << "{";
fs << "fx" << K1_kb4(0,0);
// ... 其他参数 ...
fs << "}";
fs.release();
sync();

return 0;  // 立即返回，避免继续执行
```

#### 右相机修复（第 765-809 行）
相同的逻辑应用于右相机成功分支。

### 文件生成结果

成功运行后生成的文件：

```
output/
├── left_ds.yml       ✓ 左相机单目标定
├── right_ds.yml      ✓ 右相机单目标定
├── handeye.yml       ✓ 手眼标定结果
└── cam_stereo.yml    ✓ 立体标定最终结果
```

## 验证

### 生成的文件格式

**left_ds.yml:**
```yaml
%YAML:1.0
---
model_type: double_sphere
camera:
   fx: 3.1336135503161950e+02
   fy: 3.1361410096740565e+02
   cx: 8.2248142011674236e+02
   cy: 6.0522535600345782e+02
   xi: 0.
   alpha: 5.0000000000000000e-01
   k1: 5.2989331611378462e-02
   k2: -3.9641105243864119e-02
   k3: 1.3736758520847298e-02
   k4: -1.8668028003095199e-03
   k5: 0.
   k6: 0.
```

**right_ds.yml:**
```yaml
%YAML:1.0
---
model_type: double_sphere
camera:
   fx: 4.2031196437779937e+02
   fy: 4.1878554119899144e+02
   cx: 8.7552754412841728e+02
   cy: 5.9038896449692345e+02
   xi: 0.
   alpha: 5.0000000000000000e-01
   k1: 6.7197841943205425e-02
   k2: -1.9072143038920950e-01
   k3: 1.5853551153868178e-01
   k4: -4.8678650404749911e-02
   k5: 0.
   k6: 0.
```

## 手眼标定工作流程结果

### Step 1：单目标定 ✓
- 左相机: 313.4 - 313.6 mm (焦距)
- 右相机: 420.3 - 418.8 mm (焦距)

### Step 2：手眼标定 ✓
- 输出文件: `handeye.yml`
- 包含相对位姿（R, T）

### Step 3：立体束调整 ✓
- 输出文件: `cam_stereo.yml`
- 最终标定精度：
  - 单目重投影误差: 0.1298 pixels (目标: < 0.3)
  - 立体重投影误差: 0.1205 pixels (目标: < 0.3)
  - 最大重投影误差: 0.4661 pixels (目标: < 1.5)
  - 基线距离: 64.14 mm

## 标定精度评估

```
========== Calibration Accuracy Evaluation ==========
1. Monocular Reprojection Error:
   Left camera:  0.1391 pixels (avg) ✓ PASS
   Right camera: 0.1205 pixels (avg) ✓ PASS
   Overall:      0.1298 pixels (avg) [threshold: < 0.3 pixel] ✓ PASS

2. Stereo Reprojection Error:
   Average: 0.1205 pixels [threshold: < 0.3 pixel] ✓ PASS

3. Maximum Stereo Reprojection Error:
   Maximum: 0.4661 pixels [threshold: < 1.5 pixel] ✓ PASS

5. Baseline Distance:
   Calibrated baseline: 0.064140 meters (64.14 mm)
```

## 使用方法

### 方法 1：使用脚本（推荐）
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp
```

### 方法 2：手动运行单个命令
```bash
# 左相机
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  /app/build/calibrate_ds -w 11 -h 8 -s 0.02 -d /data/imgs \
  -l left -e bmp --mono -o /data/output/left_ds.yml

# 右相机
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  /app/build/calibrate_ds -w 11 -h 8 -s 0.02 -d /data/imgs \
  -r right -e bmp --mono -o /data/output/right_ds.yml
```

## 代码变更总结

| 文件 | 行号 | 变更 |
|------|------|------|
| calibrate_ds.cpp | 665-707 | 左相机成功分支添加早期返回和文件保存 |
| calibrate_ds.cpp | 765-809 | 右相机成功分支添加早期返回和文件保存 |

## 注意事项

1. **修复原理**：不依赖第二个 mono 处理块，而是在第一个块中成功后立即保存并返回
2. **线程安全**：使用 `sync()` 确保文件完全写入磁盘
3. **兼容性**：修复后的代码对立体标定（非 mono 模式）无影响
4. **性能**：早期返回避免不必要的后续处理

---
**解决方案验证日期**: 2025年11月20日
**状态**: ✅ 已完全解决并验证
