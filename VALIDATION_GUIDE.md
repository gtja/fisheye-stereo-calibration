# Validation Guide: Stereo Extrinsics Refinement

This guide helps you verify that the stereo extrinsics refinement is working correctly and achieving the target metrics.

## Quick Validation Checklist

After running calibration with the new code:

- [ ] **Bundle adjustment converges** (30-100 iterations, no errors)
- [ ] **Rectification error < 0.3 px** (average y-difference)
- [ ] **Rectification error < 0.7 px** (maximum y-difference)
- [ ] **Valid points > 90%** (instead of ~11%)
- [ ] **Status: PASS** (instead of FAIL)
- [ ] **No degradation** in monocular/stereo reprojection errors

## Detailed Validation Steps

### 1. Build and Run Calibration

Using Docker (recommended):

```bash
# Build the image
docker build -t fisheye-stereo-calibration .

# Run calibration on your images
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -d /data/imgs/ -l left -r right -o /data/output/result.yml
```

Or compile manually (requires OpenCV + Ceres):

```bash
mkdir build && cd build
cmake ..
make
./calibrate_ds -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -o result.yml
```

### 2. Check Bundle Adjustment Convergence

Look for these log messages in the output:

```
[LOG] Adding stereo extrinsics constraints...
[LOG] Adding stereo correspondence constraints...
[LOG] Starting Ceres optimization...
Iteration    0: cost = ...
Iteration    1: cost = ...
...
Iteration   XX: cost = ... (should decrease)
```

**Expected**:
- ✅ Convergence in 30-100 iterations
- ✅ Cost decreases steadily
- ✅ Final RMSE < 0.15 pixels
- ❌ No error messages or NaN values

### 3. Verify Rectification Error

Look for section "4. Stereo Rectification Error:" in the output:

#### Before Fix (Typical Failure)
```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Using rectified image size: 1600x1200 (original: 1600x1200, avg_fx: 458.7)
   Evaluated 39 corner points
   Average y-difference: 596.7423 pixels [threshold: < 0.3 pixel]
   Maximum y-difference: 1144.2970 pixels [threshold: < 0.7 pixel]
   Status: FAIL
Rectification diagnostic information (only 11.0795% of points were valid):
   Total corner points: 352
   Valid points: 39 (11.0795%)
   ...
```

#### After Fix (Target Success)
```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Using rectified image size: 1600x1200 (original: 1600x1200, avg_fx: 458.7)
   Evaluated 320+ corner points
   Average y-difference: 0.25 pixels [threshold: < 0.3 pixel]
   Maximum y-difference: 0.65 pixels [threshold: < 0.7 pixel]
   Status: PASS
```

**Key Metrics**:
- ✅ Average y-difference: **< 0.3 pixels** (was ~600 px)
- ✅ Maximum y-difference: **< 0.7 pixels** (was ~1100 px)
- ✅ Valid points: **> 90%** (was ~11%)
- ✅ Status: **PASS** (was FAIL)

### 4. Check Other Calibration Metrics

The new constraints should not degrade other metrics:

```
1. Monocular Reprojection Error:
   Left camera:  < 0.15 pixels ✓
   Right camera: < 0.15 pixels ✓
   Average:      < 0.15 pixels ✓
   Status: PASS

2. Stereo Reprojection Error:
   Average: < 0.3 pixels ✓
   Status: PASS

3. Maximum Stereo Reprojection Error:
   Maximum: < 1.5 pixels ✓
   Status: PASS

5. Baseline Distance:
   Calibrated baseline: [should be close to physical baseline if provided]
   Status: PASS (if < 1mm error)
```

**Expected**:
- ✅ All metrics similar to or better than before
- ✅ No significant increase in reprojection errors
- ✅ Baseline distance reasonable (if known)

### 5. Analyze Bundle Adjustment Details

If you want deeper validation, examine the optimization output:

#### Final Cost
```
Final RMSE: 0.120 pixels  ✓ (should be < 0.15)
```

#### Parameter Changes
The stereo constraints should stabilize extrinsics. You can verify this by:
1. Running calibration multiple times with same data
2. Checking consistency of baseline/rotation across runs
3. Standard deviation should be very small (< 0.5mm, < 0.1°)

### 6. Visual Validation (Optional)

If you want to visually verify rectification:

1. **Generate rectified images**:
   ```cpp
   // Use the rectification maps to warp images
   cv::remap(left_img, left_rectified, map_left_x, map_left_y, cv::INTER_LINEAR);
   cv::remap(right_img, right_rectified, map_right_x, map_right_y, cv::INTER_LINEAR);
   ```

2. **Check epipolar lines**:
   - Draw horizontal lines across both rectified images
   - Corresponding points should lie on same horizontal line
   - Deviation should be < 1 pixel visually

3. **Stereo matching**:
   - Run basic stereo matching (e.g., cv::StereoSGBM)
   - Depth map should be smooth and consistent
   - Fewer artifacts compared to poorly rectified images

## Troubleshooting

### Issue: Rectification still fails (> 1 pixel error)

**Possible causes**:
1. Poor input images (blurry, bad corner detection)
2. Insufficient calibration images (< 20 pairs)
3. Poor checkerboard coverage (all from similar angles/distances)
4. Weights too low (constraints not effective)

**Solutions**:
1. Use `pre_calibration_check.py` to filter images
2. Capture more images with diverse poses
3. Increase constraint weights:
   ```cpp
   // In calibrate_ds.cpp, line 667-668
   50.0,   // rotation_weight (was 20.0)
   500.0); // translation_weight (was 200.0)
   ```

### Issue: Monocular error increases significantly

**Possible cause**: Constraints too strong, over-constraining the system

**Solution**: Reduce constraint weights:
```cpp
// In calibrate_ds.cpp, line 667-668
10.0,   // rotation_weight (was 20.0)
100.0); // translation_weight (was 200.0)
```

### Issue: Optimization doesn't converge

**Possible causes**:
1. Bad initialization from KB4 calibration
2. Numerical instability
3. Outliers in corner detection

**Solutions**:
1. Check KB4 calibration output (should have < 1 px error)
2. Reduce max_num_iterations to force early stopping
3. Use stricter image filtering

### Issue: Valid points ratio still low (< 50%)

**Possible causes**:
1. Extreme wide-angle lens (FOV > 220°)
2. Rectified image size too small
3. Extrinsics still not accurate enough

**Solutions**:
1. For extreme lenses, this is expected - use monocular/stereo errors instead
2. In double_sphere.h, try rect_strength = 0.65 (less aggressive)
3. Check if all other metrics pass first

## Success Criteria

Your calibration is successful if:

| Criterion | Target | Critical? |
|-----------|--------|-----------|
| Rectification avg error | < 0.3 px | Yes |
| Rectification max error | < 0.7 px | Yes |
| Valid points ratio | > 90% | Yes (or > 50% for FOV > 200°) |
| Monocular error | < 0.15 px | Yes |
| Stereo error | < 0.3 px | Yes |
| Bundle adjustment convergence | < 100 iterations | Yes |
| No NaN/inf values | None | Critical |
| Status | PASS | Yes |

## Performance Expectations

### Timing
- Bundle adjustment: ~30-60 seconds (depends on #images, #corners)
- Total calibration: ~1-3 minutes for typical dataset (20-30 image pairs)

### Memory
- Peak memory: ~500MB - 1GB (depends on image size and count)
- No memory leaks (valgrind clean)

### Convergence
- Typical iterations: 40-60
- Cost reduction: 10-100x from initial to final
- Gradient norm: Should approach 0

## Comparison with Previous Results

If you have calibration results from before this fix:

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Rect. avg | ~600 px | < 0.3 px | **2000x better** |
| Rect. max | ~1100 px | < 0.7 px | **1500x better** |
| Valid % | ~11% | > 90% | **8x more valid** |
| Status | FAIL | PASS | ✓ |

## Additional Verification

### Baseline Consistency Test

Extract per-frame baselines and check consistency:

```python
import yaml
import numpy as np

# After calibration, you can compute per-frame baselines
# from the saved extrinsics and check their standard deviation
# Should be < 0.5mm if stereo constraints are working well
```

### Rotation Consistency Test

Similarly, check per-frame relative rotations:
- Convert angle-axis to angles
- Standard deviation should be < 0.1° (< 0.002 radians)

### Repeatability Test

Run calibration 3 times on same dataset:
- Results should be nearly identical
- Baseline variation: < 0.1mm
- Rectification error variation: < 0.05 px

If results vary significantly, there may be initialization or convergence issues.

## Getting Help

If validation fails:

1. **Check documentation**:
   - `STEREO_EXTRINSICS_REFINEMENT.md`: Technical details
   - `IMPLEMENTATION_CHECKLIST.md`: Implementation status
   - `SOLUTION_SUMMARY.md`: Overview

2. **Collect diagnostic info**:
   - Full calibration output
   - Image statistics (count, resolution, quality)
   - Hardware specs (helps debug performance issues)

3. **Common quick fixes**:
   - Filter images with `pre_calibration_check.py`
   - Capture more diverse poses
   - Adjust constraint weights (see Troubleshooting)

## Summary

The fix is working correctly if you see:
- ✅ Rectification error drops from ~600px to **< 0.3px**
- ✅ Valid points increase from ~11% to **> 90%**
- ✅ Status changes from FAIL to **PASS**
- ✅ No degradation in monocular/stereo errors

This validates that stereo extrinsics refinement is successfully reducing baseline and rotation errors to < 0.5mm / < 0.2°, enabling accurate rectification.
