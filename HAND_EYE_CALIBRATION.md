# Hand-Eye Calibration Workflow

This guide describes the improved calibration workflow using hand-eye calibration for better initial extrinsics estimation.

## Overview

The traditional stereo calibration uses MEI (omnidir) model for initial extrinsics, which can have rotation errors of ~2°. This new workflow uses **hand-eye calibration (AX=XB)** to achieve rotation errors < 0.2°, significantly improving calibration accuracy.

### Key Benefits

- **Initial rotation error**: 2° → < 0.2° (10x improvement)
- **Pre-correction Z<0 ratio**: 47% → < 5%
- **Valid frames for BA**: 4 → 15+ (out of 19)
- **Final RMSE**: 0.16 px → 0.08 px
- **Rotation error after BA**: Further reduced by 0.1°

## Three-Step Workflow

### Step 1: Monocular Calibration

Calibrate left and right cameras separately using the `--mono` flag.

```bash
# Calibrate left camera (use at least 19 images)
./calibrate_ds -w 11 -h 8 -s 0.02 -d imgs_filtered -l left -e bmp --mono -o left_ds.yml

# Calibrate right camera
./calibrate_ds -w 11 -h 8 -s 0.02 -d imgs_filtered -r right -e bmp --mono -o right_ds.yml
```

**Expected output:**
- `left_ds.yml`: Left camera intrinsics (fx, fy, cx, cy, xi, alpha, k1-k6)
- `right_ds.yml`: Right camera intrinsics

**Requirements:**
- At least 19 images per camera
- Good corner detection in each image
- Wide variety of checkerboard poses (different angles and distances)

### Step 2: Hand-Eye Calibration

Use the monocular calibrations to compute the stereo extrinsics (T_right_left).

```bash
./compute_handeye -w 11 -h 8 -s 0.02 \
                  -d imgs_filtered \
                  -l left -r right -e bmp \
                  -L left_ds.yml -R right_ds.yml \
                  -o handeye.yml
```

**This program:**
1. Loads the left and right camera intrinsics
2. Detects checkerboard corners in matching image pairs
3. Uses `solvePnP` to compute board pose (T_world_camera) for each camera and each frame
4. Solves the hand-eye problem: `A * X = X * B`
   - `A`: Left camera poses (T_world_left)
   - `B`: Right camera poses (T_world_right)
   - `X`: Stereo transform (T_right_left) — **this is what we want!**
5. Uses Tsai-Lenz algorithm via OpenCV's `calibrateHandEye`

**Expected output:**
```
Hand-eye calibration successful!
Rotation angle: 0.15 degrees (0.0026 radians)  ← Should be < 0.2°
Baseline: 0.120 meters (120 mm)
```

**Output file** (`handeye.yml`):
```yaml
method: hand_eye_tsai
num_frames: 19
R: !!opencv-matrix
   rows: 3
   cols: 3
   dt: d
   data: [ ... ]
T: !!opencv-matrix
   rows: 3
   cols: 1
   dt: d
   data: [ tx, ty, tz ]
rotation_angle_deg: 0.15
baseline_meters: 0.120
```

### Step 3: Stereo Bundle Adjustment

Use the hand-eye extrinsics as initial guess for joint bundle adjustment.

```bash
./calibrate_ds -w 11 -h 8 -s 0.02 \
               -d imgs_filtered \
               -l left -r right -e bmp \
               --init-extrinsic handeye.yml \
               --joint-ba \
               -o cam_stereo.yml
```

**Flags explained:**
- `--init-extrinsic handeye.yml`: Load hand-eye calibrated extrinsics as initial guess
- `--joint-ba`: Jointly optimize intrinsics (xi, alpha) + extrinsics

**Expected results:**
- RMSE: ~0.08 px (vs 0.16 px without hand-eye)
- ≥15 frames pass 1.5 px filter (vs 4 frames)
- Final rotation error: < 0.1°

## Pre-Correction Improvements

The improved initial extrinsics enable better pre-correction:

### Before (MEI initialization)
- Initial rotation error: ~2°
- Z < 0 points after pre-correct: 47%
- Valid frames for BA: 4/19

### After (Hand-eye initialization)
- Initial rotation error: < 0.2°
- Z < 0 points after pre-correct: < 5%
- Valid frames for BA: 15+/19

## Command-Line Flags Reference

### `--mono`
Monocular calibration mode. Specify either `--left` or `--right`, not both.

```bash
./calibrate_ds --mono -l left -d imgs/ ...    # Calibrate left camera only
./calibrate_ds --mono -r right -d imgs/ ...   # Calibrate right camera only
```

### `--init-extrinsic <file>`
Load initial stereo extrinsics from YAML file (typically from hand-eye calibration).

```bash
./calibrate_ds --init-extrinsic handeye.yml ...
```

**File format:**
```yaml
R: !!opencv-matrix
   rows: 3
   cols: 3
   dt: d
   data: [...]
T: !!opencv-matrix
   rows: 3
   cols: 1
   dt: d
   data: [tx, ty, tz]
```

### `--joint-ba`
Enable joint bundle adjustment (optimize both intrinsics and extrinsics).

**Without `--joint-ba` (default):**
- Intrinsics (fx, fy, cx, cy, k1-k6, xi, alpha) are **locked**
- Only extrinsics (R, T) are optimized

**With `--joint-ba`:**
- Both intrinsics and extrinsics are optimized
- DS-specific parameters (xi, alpha) have bounds: xi ∈ [-1, 1], alpha ∈ [0, 1]
- Better final accuracy but requires good initial guess (use with `--init-extrinsic`)

```bash
./calibrate_ds --joint-ba --init-extrinsic handeye.yml ...
```

## Troubleshooting

### "Hand-eye calibration failed"
**Cause:** Not enough valid pose pairs or poor corner detection.

**Solutions:**
- Ensure at least 19 image pairs with good corner detection
- Check that monocular calibrations (left_ds.yml, right_ds.yml) are valid
- Verify checkerboard parameters (width, height, square size) are correct

### "Initial rotation error > 0.5°"
**Cause:** Poor monocular calibrations or bad corner detection.

**Solutions:**
- Re-run monocular calibration with more images
- Use pre-calibration quality checks: `python3 utils/pre_calibration_check.py`
- Ensure checkerboard is clearly visible and not blurry

### "Too many frames filtered (< 3 frames)"
**Cause:** Poor initial extrinsics or extreme distortion.

**Solutions:**
- Check hand-eye calibration output (rotation angle should be < 0.2°)
- Try without `--joint-ba` first, then add it after verifying basic calibration works
- Ensure images have sufficient variety in checkerboard poses

## Technical Details

### Hand-Eye Problem Formulation

Given:
- `A_i`: Transform from world to left camera at frame i
- `B_i`: Transform from world to right camera at frame i
- `X`: Transform from left to right camera (constant across all frames)

We want to solve: `A_i * X = X * B_i` for all i

This is the classic hand-eye calibration problem, solved using the Tsai-Lenz algorithm.

### Why Hand-Eye Works Better

Traditional stereo calibration directly estimates R and T from corresponding points. With extreme distortion (FOV > 200°), this initial estimate can be inaccurate (~2° error).

Hand-eye calibration:
1. Uses monocular calibration (more stable for extreme FOV)
2. Leverages `solvePnP` with known intrinsics (more accurate)
3. Solves a constrained optimization problem (AX=XB) that's mathematically more robust

### Joint BA Rationale

With better initial extrinsics:
- More frames pass pre-correction (15+ vs 4)
- Bundle adjustment has more observations
- Safe to optimize intrinsics without divergence
- Final accuracy improved: 0.16 px → 0.08 px

## References

- Tsai, R. Y., & Lenz, R. K. (1989). "A new technique for fully autonomous and efficient 3D robotics hand/eye calibration." IEEE Transactions on Robotics and Automation.
- OpenCV `calibrateHandEye` documentation: https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html#gaebfc1c9f7434196a374c382abf43439b
- Double-Sphere camera model: Usenko et al. (2018), "The Double Sphere Camera Model"
