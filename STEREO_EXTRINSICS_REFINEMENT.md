# Stereo Extrinsics Refinement for Rectification Error Reduction

## Problem Statement

Despite optimizing virtual focal length (fx=290) and FOV (180°), rectification error remained at ~600px y-difference. The root cause is **incorrect stereo extrinsics** (relative rotation R and translation T between left and right cameras).

### Initial State
- Average y-difference: 596.7423 pixels (threshold: < 0.3 pixel)
- Maximum y-difference: 1144.2970 pixels (threshold: < 0.7 pixel)
- Valid points: Only 11.08% (39 out of 352)
- Focal length: Already optimized to fx=290
- FOV: Already at 180°

## Solution: Bundle Adjustment with Stereo Constraints

The fix implements stereo extrinsics refinement through bundle adjustment with two new constraint types:

### 1. Stereo Extrinsics Constraint

**Purpose**: Enforce consistent relative pose between left and right cameras across all frames.

**Implementation** (`StereoExtrinsicsConstraint` in `double_sphere.h`):
- Constrains the baseline vector (translation) to match KB4 calibration
- Constrains the relative rotation (angle-axis difference) to match KB4 calibration
- Uses high weights: rotation_weight=20.0, translation_weight=200.0
- Target accuracy: baseline error < 0.5mm, rotation error < 0.2°

**How it works**:
```
For each frame i:
  - Compute baseline: T_i = t_right_i - R_stereo * t_left_i
  - Compare with target: residual = weight * (T_i - T_target)
  - Compute relative rotation: ΔR_i ≈ rvec_right_i - rvec_left_i  
  - Compare with target: residual = weight * (ΔR_i - ΔR_target)
```

### 2. Stereo Correspondence Constraint

**Purpose**: Enforce epipolar geometry by ensuring corresponding points in left/right images project from the same 3D point.

**Implementation** (`StereoCorrespondenceConstraint` in `double_sphere.h`):
- For each checkerboard corner pair (left, right)
- Projects the 3D world point through both camera models
- Computes 4 residuals: 2 for left image (x,y), 2 for right image (x,y)
- Uses Huber loss (threshold=0.5) to handle outliers

**How it works**:
```
For each corner point (left_2d, right_2d, world_3d):
  - Transform world_3d → left camera coords → project → predicted_left_2d
  - Transform world_3d → right camera coords → project → predicted_right_2d
  - Residuals = [predicted_left_2d - left_2d, predicted_right_2d - right_2d]
```

### 3. Rectification Strategy Adjustment

After refining extrinsics, we can use stronger rectification (less partial):

**Previous**: rect_strength = 0.60 → ~180° rectified FOV
**New**: rect_strength = 0.75 → ~150° rectified FOV

**Benefits**:
- Fewer edge points (which have higher distortion)
- Better valid point ratio
- Reduced rectification error

## Integration into Bundle Adjustment

Changes in `calibrate_ds.cpp`:

```cpp
// After adding monocular reprojection residuals...

// Add stereo extrinsics constraints for each frame
for (size_t i = 0; i < object_points.size(); i++) {
    ceres::CostFunction* stereo_constraint = 
        double_sphere::StereoExtrinsicsConstraint::Create(
            Mat(R_kb4), Mat(T_kb4), 20.0, 200.0);
    problem.AddResidualBlock(stereo_constraint, new ceres::HuberLoss(1.0),
                            camera_extrinsics_left[i], camera_extrinsics_right[i]);
}

// Add stereo correspondence constraints for all corner pairs
for (size_t i = 0; i < object_points.size(); i++) {
    for (size_t j = 0; j < object_points[i].size(); j++) {
        ceres::CostFunction* correspondence_constraint =
            double_sphere::StereoCorrespondenceConstraint::Create(
                left_img_points[i][j], right_img_points[i][j], object_points[i][j]);
        problem.AddResidualBlock(correspondence_constraint, new ceres::HuberLoss(0.5),
                                camera_intrinsics_left, camera_intrinsics_right,
                                camera_extrinsics_left[i], camera_extrinsics_right[i]);
    }
}
```

## Expected Impact

### Extrinsics Refinement
- Baseline error: < 0.5mm (from potentially several mm)
- Rotation error: < 0.2° (from potentially > 1°)
- Per-frame pose variance: Significantly reduced

### Rectification Error
- Average y-difference: ~600 px → **< 0.5 px** (target: < 0.3 px)
- Maximum y-difference: ~1100 px → **< 1.0 px** (target: < 0.7 px)
- Valid point ratio: 11% → **> 90%**

### Why This Works

1. **Tighter pose constraints**: The stereo constraints force all frame extrinsics to respect a consistent relative transformation, reducing pose estimation variance.

2. **Epipolar geometry enforcement**: The correspondence constraints ensure that stereo geometry is respected during optimization, preventing drift between left and right calibrations.

3. **Balanced optimization**: Monocular reprojection errors optimize intrinsics and per-frame poses, while stereo constraints optimize relative poses. Together they converge to a globally consistent solution.

4. **Reduced rectified FOV**: With accurate extrinsics, we can safely use stronger rectification (0.75 vs 0.60), reducing FOV from 180° to 150° and eliminating problematic edge regions.

## Technical Details

### Weight Selection
- **Rotation weight (20.0)**: Balances ~0.01 radians (~0.5°) rotation error with ~1 pixel reprojection error
- **Translation weight (200.0)**: Balances ~0.0005m (0.5mm) translation error with ~1 pixel reprojection error
- These weights ensure stereo constraints have similar magnitude to reprojection errors

### Loss Functions
- **Huber loss (0.5)**: For reprojection and correspondence - robust to outliers
- **Huber loss (1.0)**: For stereo extrinsics - allows slightly more flexibility

### Convergence
- Bundle adjustment typically converges in 30-50 iterations
- Final RMSE should be < 0.15 pixels for both monocular and stereo reprojection
- Stereo constraint residuals should be < 0.01 for rotation, < 0.001 for translation

## Verification

After applying this fix, verify:

1. **Baseline consistency**: Standard deviation of per-frame baselines should be < 0.5mm
2. **Rotation consistency**: Standard deviation of per-frame relative rotations should be < 0.1°
3. **Rectification error**: Average < 0.3 px, max < 0.7 px
4. **Valid points**: > 90% of corner points should be valid for rectification

## References

- Problem statement: Issue describing 600px rectification error despite fx=290, FOV=180°
- Solution approach: Bundle adjustment + checkerboard BA re-optimization
- Target metrics: baseline < 0.5mm, rotation < 0.2°, rectification < 0.3px
