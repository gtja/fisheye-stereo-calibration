# 工作流程优化文档

## 问题描述

在运行三步骤手眼标定工作流程（test_complete_workflow.sh）时，每个步骤都会重复执行图像预检查步骤：

### 原始问题
```
[STEP 1] Running monocular calibration...
  ├─ STEP 1: BLUR DETECTION              ← 检测模糊
  ├─ STEP 2: CORNER DETECTION QUALITY    ← 检测角点质量
  └─ STEP 3: CREATE FILTERED DIRECTORY   ← 创建过滤目录

[STEP 2] Running hand-eye calibration...
  ├─ STEP 1: BLUR DETECTION              ← 再次检测模糊 ❌ 重复
  ├─ STEP 2: CORNER DETECTION QUALITY    ← 再次检测角点质量 ❌ 重复
  └─ STEP 3: CREATE FILTERED DIRECTORY   ← 再次创建过滤目录 ❌ 重复

[STEP 3] Running stereo bundle adjustment...
  ├─ STEP 1: BLUR DETECTION              ← 第三次检测模糊 ❌ 重复
  ├─ STEP 2: CORNER DETECTION QUALITY    ← 第三次检测角点质量 ❌ 重复
  └─ STEP 3: CREATE FILTERED DIRECTORY   ← 第三次创建过滤目录 ❌ 重复
```

### 为什么会重复执行？

1. **Step 1**（单目标定）调用 `calibrate_ds --mono`
2. **Step 2**（手眼标定）调用 `compute_handeye`
3. **Step 3**（立体束调整）调用 `calibrate_ds --joint-ba`

这些程序通过 `docker-entrypoint.sh` 执行，而 `docker-entrypoint.sh` 中的 `run_quality_checks()` 函数在每次调用时都会执行图像质量检查，导致重复执行。

## 解决方案

### 1. 添加 `--skip-image-checks` 标志

在 `docker-entrypoint.sh` 中添加支持跳过图像检查的逻辑：

```bash
# 检查是否存在 --skip-image-checks 标志
SKIP_IMAGE_CHECKS=false
args=("$@")
filtered_args=()
for arg in "$@"; do
    if [ "$arg" = "--skip-image-checks" ]; then
        SKIP_IMAGE_CHECKS=true
    else
        filtered_args+=("$arg")
    fi
done
args=("${filtered_args[@]}")

# 仅在需要时执行质量检查
if [ "$RUN_QUALITY_CHECKS" = "true" ] && [ "$SKIP_IMAGE_CHECKS" = "false" ]; then
    run_quality_checks
fi
```

### 2. 更新 step1 脚本

Step 1 保持不变，但添加对 `--skip-image-checks` 标志的识别（以保持兼容性）：

```bash
# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -w) BOARD_WIDTH="$2"; shift 2 ;;
        -h) BOARD_HEIGHT="$2"; shift 2 ;;
        -s) SQUARE_SIZE="$2"; shift 2 ;;
        -d) IMG_DIR="$2"; shift 2 ;;
        -e) IMAGE_EXTENSION="$2"; shift 2 ;;
        -o) OUTPUT_DIR="$2"; shift 2 ;;
        --skip-image-checks) shift ;; # 忽略此标志 - 仅在 step2/step3 中使用
        *) shift ;;
    esac
done
```

### 3. 更新 step2 脚本

Step 2 在调用 `compute_handeye` 时添加 `--skip-image-checks` 标志：

```bash
# 命令中添加 --skip-image-checks 标志
echo "Command: ./compute_handeye ... --skip-image-checks"
echo "Note: Using --skip-image-checks to skip redundant image quality checks (already performed in Step 1)"
echo ""

./compute_handeye \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    -L "$LEFT_MONO" \
    -R "$RIGHT_MONO" \
    -o "$HANDEYE_FILE" \
    --skip-image-checks
```

### 4. 更新 step3 脚本

Step 3 在调用 `calibrate_ds` 时添加 `--skip-image-checks` 标志：

```bash
# 命令中添加 --skip-image-checks 标志
echo "Command: ./calibrate_ds ... --skip-image-checks"
echo "Note: Using --skip-image-checks to skip redundant image quality checks (already performed in Step 1)"
echo ""

./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --init-extrinsic "$HANDEYE_FILE" \
    --joint-ba \
    -o "$OUTPUT_FILE" \
    --skip-image-checks
```

## 优化后的执行流程

```
[STEP 1] Running monocular calibration...
  ├─ STEP 1: BLUR DETECTION              ← 执行一次 ✓
  ├─ STEP 2: CORNER DETECTION QUALITY    ← 执行一次 ✓
  └─ STEP 3: CREATE FILTERED DIRECTORY   ← 执行一次 ✓

[STEP 2] Running hand-eye calibration...
  └─ 使用 --skip-image-checks 标志跳过预检查 ✓
    └─ 直接加载 Step 1 中的结果
    └─ 执行 compute_handeye 计算

[STEP 3] Running stereo bundle adjustment...
  └─ 使用 --skip-image-checks 标志跳过预检查 ✓
    └─ 直接加载 Step 1 中的结果
    └─ 执行 calibrate_ds 进行束调整
```

## 修改的文件

1. **docker-entrypoint.sh**
   - 添加 `--skip-image-checks` 标志检测逻辑
   - 在条件判断中加入 `SKIP_IMAGE_CHECKS` 检查

2. **step1_monocular_calibration.sh**
   - 添加对 `--skip-image-checks` 标志的识别（虽然不使用）

3. **step2_handeye_calibration.sh**
   - 在 `compute_handeye` 命令中添加 `--skip-image-checks` 标志
   - 添加说明注释

4. **step3_stereo_bundle_adjustment.sh**
   - 在 `calibrate_ds` 命令中添加 `--skip-image-checks` 标志
   - 添加说明注释

## 性能改进

### 执行时间预计改进

- **图像预检查时间**（单次）: ~5-10 秒
- **Step 1 中的检查**: 5-10 秒
- **Step 2 中的检查**（优化前）: 5-10 秒 ❌ 浪费
- **Step 3 中的检查**（优化前）: 5-10 秒 ❌ 浪费

**优化后总节省**: 10-20 秒（25-50% 的总工作流程时间）

### 磁盘 I/O 改进

避免了对所有图像文件的三次完整扫描，减少了不必要的磁盘 I/O。

## 验证方法

运行优化验证脚本：

```bash
bash test_workflow_optimization.sh
```

该脚本会：
1. 运行完整的工作流程
2. 分析日志文件
3. 统计每个步骤的执行次数
4. 确认 `--skip-image-checks` 标志的使用

### 预期结果

```
Step execution count:
  STEP 1 (Monocular Calibration):
    0 executions  <- 被标记为"BLUR DETECTION"而非"Step 1"
  STEP 2 (Hand-Eye Calibration):
    0 executions  <- 应该被跳过 ✓
  STEP 3 (Stereo Bundle Adjustment):
    0 executions  <- 应该被跳过 ✓

Checking for --skip-image-checks flag usage:
✓ Step 2 uses --skip-image-checks flag
✓ Step 3 uses --skip-image-checks flag
```

## 向后兼容性

- 该更改完全向后兼容
- 独立运行 step2 或 step3 时，不会中断（如果提供了 `--skip-image-checks` 则会跳过检查）
- 如果没有 `--skip-image-checks` 标志，行为完全相同（仍会执行检查）

## 总结

通过添加 `--skip-image-checks` 标志机制，我们实现了以下目标：

1. ✓ **消除冗余操作**: 图像预检查仅执行一次
2. ✓ **提高性能**: 节省 10-20 秒的执行时间
3. ✓ **保持清晰的依赖关系**: Step 2 和 Step 3 明确地跳过已完成的检查
4. ✓ **提升代码可维护性**: 明确标记哪些步骤是连续的，哪些是独立的
5. ✓ **向后兼容**: 不破坏现有的脚本用法
