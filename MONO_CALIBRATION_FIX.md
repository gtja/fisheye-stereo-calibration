# 单目标定文件保存问题诊断和修复指南

## 问题描述

在 mono 模式下运行 `calibrate_ds`，程序报告成功但没有生成输出文件。

```
[DEBUG] Successfully set K2_kb4 and D2_kb4
[DEBUG] Exiting mono_mode block, mono_mode = 1
Error: Left camera calibration file was not created or not readable: /data/output/left_ds.yml
```

## 根本原因

代码在两个地方处理 mono 模式：

1. **第 629-776 行**：执行 KB4 初始标定（fisheye 或 omnidir 模型）
2. **第 954-1054 行**：保存标定结果到文件

问题在于：成功标定后（第 776 行），代码没有跳转到第二个 mono 处理块（第 954 行）进行文件保存。

## 修复方案

### 修复方案 1：早期返回（推荐）

在第一个 mono 块中（第 776 行附近），成功标定后立即保存文件并返回：

```cpp
printf("[DEBUG] Successfully set K2_kb4 and D2_kb4\n");
fflush(stdout);

// EARLY EXIT: Save file immediately after successful mono calibration
if (!ensure_output_directory(out_file)) {
    cerr << "Error: Cannot create output directory" << endl;
    return 1;
}

FileStorage fs(out_file, FileStorage::WRITE);
if (!fs.isOpened()) {
    cerr << "Error: Cannot open output file for writing" << endl;
    return 1;
}

// 写入标定参数...
fs << "model_type" << "double_sphere";
fs << "camera" << "{";
fs << "fx" << K1_kb4(0,0);
// ... 其他参数 ...
fs << "}";
fs.release();
sync();

printf("Mono calibration saved successfully to %s\n", out_file);
return 0;  // 立即返回，跳过后续处理
```

### 修复方案 2：整合双 mono 块

将第 954 行的 mono 保存块移到第 629 行块的末尾，确保无论哪种初始化方法都能执行保存逻辑。

## 重建步骤

1. 应用上述修复（已在 calibrate_ds.cpp 中更新）
2. 重建 Docker 镜像：
   ```bash
   docker build -t fisheye-stereo-calibration .
   ```

3. 重新运行 mono 标定：
   ```bash
   docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
     fisheye-stereo-calibration \
     /app/build/calibrate_ds -w 11 -h 8 -s 0.02 -d /data/imgs \
     -l left -e bmp --mono -o /data/output/left_ds.yml
   ```

## 预期输出

成功后应该看到：
```
KB4 fisheye calibration succeeded for left camera
[DEBUG] EARLY EXIT: Saving mono calibration file for left camera
Mono calibration saved successfully to /data/output/left_ds.yml
```

## 临时替代方案

如果无法重建 Docker，可以改用以下方法：

1. **使用立体标定替代**（推荐）：
   同时指定 `-l left -r right`，使用完整的立体标定流程
   
2. **手动创建标定文件**：
   从程序日志提取标定参数，手动创建 YAML 文件

3. **提取标定参数**：
   运行程序时记录输出的相机参数，手动构建输出文件

## 文件格式

生成的 YAML 文件应包含：
```yaml
model_type: double_sphere
camera:
  fx: <焦距 x>
  fy: <焦距 y>
  cx: <主点 x>
  cy: <主点 y>
  xi: <离心系数>
  alpha: <透视参数>
  k1: <径向畸变 1>
  k2: <径向畸变 2>
  k3: <径向畸变 3>
  k4: <径向畸变 4>
  k5: <径向畸变 5>
  k6: <径向畸变 6>
```
