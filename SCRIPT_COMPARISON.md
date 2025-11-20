# Script Comparison: docker-entrypoint.sh vs. Three-Step Scripts

This document explains the differences between using the monolithic `docker-entrypoint.sh` and the new three-step scripts.

## Overview

### Original Approach (docker-entrypoint.sh)
The `docker-entrypoint.sh` script runs the entire three-step calibration workflow in a single execution. While convenient, this approach has limitations:

- **No intermediate inspection**: Cannot examine results between steps
- **No resume capability**: If one step fails, must restart from the beginning
- **Limited flexibility**: Cannot run individual steps for testing or debugging
- **Monolithic**: All logic in one large script (419 lines)

### New Approach (Three-Step Scripts)
The workflow is split into three independent, focused scripts:

1. **step1_monocular_calibration.sh** (134 lines) - Monocular calibration
2. **step2_handeye_calibration.sh** (112 lines) - Hand-eye calibration  
3. **step3_stereo_bundle_adjustment.sh** (83 lines) - Bundle adjustment
4. **run_three_step_calibration.sh** (75 lines) - Optional convenience script

## Functional Equivalence

The three-step scripts extract the `run_hand_eye_workflow()` function from `docker-entrypoint.sh` and split it into independent scripts. The calibration commands are **identical**:

### Step 1: Monocular Calibration (Left Camera)

**docker-entrypoint.sh (lines 70-78):**
```bash
./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --mono \
    -o "$left_mono"
```

**step1_monocular_calibration.sh (lines 68-77):**
```bash
if ! ./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --mono \
    -o "$LEFT_MONO"; then
```

### Step 2: Hand-Eye Calibration

**docker-entrypoint.sh (lines 140-150):**
```bash
./compute_handeye \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    -L "$left_mono" \
    -R "$right_mono" \
    -o "$handeye_file"
```

**step2_handeye_calibration.sh (lines 79-90):**
```bash
if ! ./compute_handeye \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    -L "$LEFT_MONO" \
    -R "$RIGHT_MONO" \
    -o "$HANDEYE_FILE"; then
```

### Step 3: Stereo Bundle Adjustment

**docker-entrypoint.sh (lines 177-187):**
```bash
./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --init-extrinsic "$handeye_file" \
    --joint-ba \
    -o "$OUTPUT_FILE"
```

**step3_stereo_bundle_adjustment.sh (lines 53-64):**
```bash
if ! ./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --init-extrinsic "$HANDEYE_FILE" \
    --joint-ba \
    -o "$OUTPUT_FILE"; then
```

## Key Improvements

### 1. Better Error Handling
**Old (docker-entrypoint.sh):**
```bash
./calibrate_ds ...
if [ $? -ne 0 ]; then
    echo "Error: ..."
    exit 1
fi
```

**New (three-step scripts):**
```bash
if ! ./calibrate_ds ...; then
    echo "Error: ..."
    exit 1
fi
```

The new approach:
- More idiomatic bash (passes shellcheck)
- Direct exit code checking
- Cleaner and more readable

### 2. Modularity
- Each script is focused on a single responsibility
- Easier to understand, test, and maintain
- Can be used independently or together

### 3. Flexibility

**Run all steps (equivalent to docker-entrypoint.sh):**
```bash
./run_three_step_calibration.sh
```

**Run individual steps:**
```bash
# Just monocular calibration
./step1_monocular_calibration.sh

# Continue with hand-eye after fixing issues
./step2_handeye_calibration.sh

# Final bundle adjustment
./step3_stereo_bundle_adjustment.sh
```

## Usage Comparison

### Using docker-entrypoint.sh (Original)

```bash
# Run complete workflow via Docker
docker run -e CALIBRATION_MODEL=double_sphere \
  -e CALIBRATION_WORKFLOW=hand_eye \
  -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  -w 11 -h 8 -s 0.02 -d /data/imgs/ -l left -r right -e jpg \
  -o /data/output/cam_stereo.yml
```

**Limitations:**
- Must complete all steps in one run
- Cannot inspect intermediate results
- If Step 2 fails, must re-run Step 1

### Using Three-Step Scripts (New)

```bash
# Run complete workflow (similar to docker-entrypoint.sh)
docker run -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  ./run_three_step_calibration.sh

# OR run steps individually for better control

# Step 1
docker run -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  ./step1_monocular_calibration.sh

# Inspect left_ds.yml and right_ds.yml...

# Step 2
docker run -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  ./step2_handeye_calibration.sh

# Inspect handeye.yml...

# Step 3
docker run -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  ./step3_stereo_bundle_adjustment.sh
```

**Advantages:**
- Inspect results after each step
- Resume from any step if one fails
- Debug individual steps
- Run partial workflows

## Backward Compatibility

The `docker-entrypoint.sh` script **remains unchanged** and fully functional. Users can continue using it as before. The three-step scripts provide an **alternative** approach for users who need more flexibility.

## When to Use Which Approach

### Use docker-entrypoint.sh when:
- Running automated calibration pipelines
- Processing multiple calibration sessions in batch
- You don't need intermediate inspection
- One-command execution is preferred

### Use three-step scripts when:
- Debugging calibration issues
- Learning the calibration workflow
- Need to inspect intermediate results
- Want to re-run specific steps with different parameters
- Developing or testing calibration improvements

## Summary

| Feature | docker-entrypoint.sh | Three-Step Scripts |
|---------|---------------------|-------------------|
| **Execution** | Single run | Step-by-step or all at once |
| **Resume capability** | ❌ No | ✅ Yes |
| **Intermediate inspection** | ❌ No | ✅ Yes |
| **Debugging** | ⚠️ Harder | ✅ Easier |
| **Code quality** | Good | ✅ Better (shellcheck clean) |
| **Flexibility** | ⚠️ Limited | ✅ High |
| **Use case** | Production/batch | Development/debugging |
| **Lines of code** | 419 (monolithic) | 329 (split across 3 files) |

Both approaches are valid and produce **identical calibration results**. Choose based on your workflow needs.
