# Verification Checklist for Rectification Fix

## Quick Verification Steps

To verify the fix works correctly, follow these steps:

### 1. Check Code Changes Applied

Verify the three key changes are in place:

**In `double_sphere.h` around line 545:**
```cpp
} else if (avg_fx < 500.0) {
    // Wide-angle lens (FOV > ~150°)
    focal_scale = 1.0;  // Keep same as original to match projection
```
✓ Should be `1.0` (not `0.6`)

**In `double_sphere.h` around line 511:**
```cpp
} else if (approx_fov_deg > 130.0) {
    // Wide-angle lens: use partial rectification (45%)
    rect_strength = 0.45;
```
✓ Should be `0.45` (not `0.7`)

**In `calibrate_ds.cpp` around line 897:**
```cpp
if (avg_fx < 250.0) {
    // Extreme wide-angle lens detected (FOV > ~220°)
    // Use modest 1.2x increase only for very extreme cases
    rectified_size.width = static_cast<int>(img1.size().width * 1.2);
```
✓ Threshold should be `250.0` (not `500.0`), scaling should be `1.2` (not `1.5`)

### 2. Build the Code

```bash
cd /path/to/fisheye-stereo-calibration
mkdir -p build && cd build
cmake ..
make -j4
```

Expected: Clean build with no errors

### 3. Run Calibration

Using the same dataset that produced the error:

```bash
./calibrate_ds -w 11 -h 8 -s 0.02 -d /path/to/imgs/ -l left -r right -e bmp
```

### 4. Check Output Metrics

Look for the "4. Stereo Rectification Error" section:

**Expected Output (for fx≈458, 1600×1200 images):**
```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Using rectified image size: 1600x1200 (original: 1600x1200, avg_fx: 458.7)

Rectification diagnostic information:
   Total corner points: 352
   Valid points: >250 (>70%)
   Points behind camera: 0 (0%)
   Points with negative Z after rectification: <50 (<15%)
   Points outside rectified image bounds: <50 (<15%)
   Rectified image size: 1600x1200
   Calibrated focal lengths: left_fx=447.446, right_fx=469.981
   Estimated FOV: 130.7° (rectification strength: 0.45)
   Virtual camera: fx=458.x, fy=458.x, cx=800, cy=600
   Evaluated >250 corner points
   Average y-difference: <0.30 pixels [threshold: < 0.3 pixel]
   Maximum y-difference: <0.70 pixels [threshold: < 0.7 pixel]
   Status: PASS
```

### 5. Key Metrics to Verify

| Metric | Old Value | Expected New Value | Pass Criteria |
|--------|-----------|-------------------|---------------|
| Virtual fx | ~275 px | ~458 px | ≥ original fx |
| Rectified size | 2400×1800 | 1600×1200 | = original size |
| Rect strength | 0.7 | 0.45 | ≤ 0.5 for FOV>130° |
| Valid points % | 25% | >70% | >50% |
| Avg y-diff | 599 px | <0.3 px | <0.3 px |
| Max y-diff | 1354 px | <0.7 px | <0.7 px |
| Status | FAIL | PASS | PASS |

### 6. What Success Looks Like

✓ **Virtual focal length matches or exceeds original focal length**
  - Old: fx=275px (0.6× scale)
  - New: fx=458px (1.0× scale)

✓ **Rectified image size equals original size (no enlargement)**
  - Old: 2400×1800 (1.5× enlarged)
  - New: 1600×1200 (same as input)

✓ **Rectification strength reduced for wide-angle**
  - Old: 0.7 (70% rotation)
  - New: 0.45 (45% rotation)

✓ **Most points remain valid (>70%)**
  - Old: 88/352 = 25% valid
  - New: >250/352 = >70% valid

✓ **Rectification error below threshold**
  - Old: 599px avg, 1354px max → FAIL
  - New: <0.3px avg, <0.7px max → PASS

### 7. Troubleshooting

**If avg_fx is still scaled down (<1.0):**
- Check that focal_scale assignment in `double_sphere.h` line 545 is `1.0`
- Verify the condition is `avg_fx < 500.0` (not `< 300.0`)

**If rectified size is still enlarged:**
- Check that the scaling code in `calibrate_ds.cpp` line 892-899 has been updated
- Verify threshold is `< 250.0` (not `< 500.0`)

**If rectification strength is still high:**
- Check that rect_strength in `double_sphere.h` line 513 is `0.45`
- Verify FOV calculation is using `approx_fov_deg > 130.0` condition

**If errors are still high (>1 pixel):**
- Double-check all three fixes are applied
- Verify the camera calibration itself is accurate (check monocular errors)
- Consider that extreme distortion may need even lower rect_strength (try 0.4)

### 8. Success Criteria Summary

The fix is successful if:
1. ✓ Virtual fx ≥ original fx (not scaled down)
2. ✓ Rectified size = original size (not enlarged)
3. ✓ Rectification strength ≤ 0.5 for FOV > 130°
4. ✓ Valid point rate > 70%
5. ✓ Average y-difference < 0.3 pixels
6. ✓ Maximum y-difference < 0.7 pixels
7. ✓ Status shows "PASS"

All seven criteria must be met for the fix to be considered successful.
