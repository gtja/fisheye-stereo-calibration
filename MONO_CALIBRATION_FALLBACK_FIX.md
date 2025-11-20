# Mono Calibration Omnidir Fallback Fix

## Problem Summary

When running monocular calibration with the `--mono` flag, if fisheye calibration fails, the program continues execution with **uninitialized camera parameters**, leading to:
- Undefined behavior (crashes, segfaults)
- Incomplete or corrupted output files
- Inconsistent results

## Root Cause

In `calibrate_ds.cpp`, the mono mode calibration code (lines 629-673) had a critical flaw:

```cpp
if (mono_mode) {
    if (is_left_only) {
        try {
            fisheye::calibrate(..., K1_kb4, D1_kb4, ...);
            fisheye_success = true;
            // Copy to K2_kb4, D2_kb4
        } catch (const cv::Exception& e) {
            printf("Fisheye model failed for left camera: %s\n", e.what());
            // ⚠️ PROBLEM: No fallback! K1_kb4 and D1_kb4 remain UNINITIALIZED
        }
    }
    // Similar issue for right camera
}

// Later in the code (line 839-842):
kb4::KB4Params kb4_left(K1_kb4, D1_kb4);   // ❌ Using potentially uninitialized data!
kb4::KB4Params kb4_right(K2_kb4, D2_kb4);  // ❌ Using potentially uninitialized data!
```

### Why This Is Critical

1. **Uninitialized memory access**: K1_kb4, D1_kb4, K2_kb4, D2_kb4 are `cv::Matx33d` and `cv::Vec4d` objects that contain random garbage if not initialized
2. **Undefined behavior**: Accessing uninitialized data causes unpredictable behavior
3. **Silent failures**: The program may continue without obvious errors but produce invalid results
4. **File creation issues**: Subsequent file operations may fail due to invalid data

### Inconsistency with Stereo Mode

The stereo mode (lines 674-807) **already had** an omnidir fallback:

```cpp
} else {  // Stereo mode
    try {
        fisheye::stereoCalibrate(...);
        fisheye_success = true;
    } catch (const cv::Exception& e) {
        printf("Fisheye model failed, falling back to omnidir...\n");
        // ✅ Comprehensive omnidir fallback that initializes all parameters
        try {
            omnidir::calibrate(...);
            // Properly initialize K1_kb4, D1_kb4, K2_kb4, D2_kb4
        } catch (...) {
            cerr << "Error: Both fisheye and omnidir failed!\n";
            return 1;  // Fail safely
        }
    }
}
```

This inconsistency meant:
- Stereo mode: Robust, supports wide FOV lenses, fails safely
- Mono mode: Fragile, crashes on failure, produces undefined behavior

## Solution

Added omnidir (MEI) model fallback support for **both left and right camera** mono mode calibration, matching the existing stereo mode implementation.

### Changes Made

#### 1. Left Camera Fallback (Lines 650-698)

```cpp
} catch (const cv::Exception& e) {
    printf("Fisheye model failed for left camera: %s\n", e.what());
    printf("Falling back to omnidir (MEI) model for left camera...\n");
    
    // Convert data types for omnidir compatibility
    vector<vector<Point2f>> left_pts_init_f;
    vector<vector<Point3f>> obj_pts_init_f;
    // ... conversion code ...
    
    try {
        // Calibrate with omnidir model
        Mat K1_mat, D1_mat, xi1_mat;
        vector<Vec3d> rvecs_left, tvecs_left;
        omnidir::calibrate(obj_pts_init_f, left_pts_init_f, img1.size(),
                          K1_mat, xi1_mat, D1_mat, rvecs_left, tvecs_left, ...);
        
        // Convert results to KB4 format
        K1_kb4 = Matx33d((double*)K1_mat.data);
        D1_kb4 = Vec4d(D1_mat.at<double>(0), ...);
        K2_kb4 = K1_kb4;  // Copy to right for consistency
        D2_kb4 = D1_kb4;
        
        printf("Omnidir calibration succeeded for left camera\n");
    } catch (const cv::Exception& e2) {
        cerr << "Error: Both fisheye and omnidir calibration failed!\n";
        return 1;  // Fail safely with error message
    }
}
```

#### 2. Right Camera Fallback (Lines 720-768)

Similar implementation for right camera mono calibration.

### Code Statistics

| Metric | Value |
|--------|-------|
| Lines added | 104 |
| Lines deleted | 0 |
| Files changed | 1 (`calibrate_ds.cpp`) |
| Functions affected | 1 (`main`) |
| New functions | 0 |
| Dependencies added | 0 |

## Benefits

### 1. **Guaranteed Initialization**
All camera parameters are now **always initialized** before use:
- Either by successful fisheye calibration, OR
- By successful omnidir calibration, OR
- Program exits with clear error message

### 2. **Wide FOV Support**
Mono mode now supports extreme wide-angle lenses (FOV > 200°):
- Previously: Crash or undefined behavior
- Now: Automatic fallback to omnidir model

### 3. **Consistent Behavior**
Mono and stereo modes now have identical fallback logic:
- Same code structure
- Same error handling
- Same user experience

### 4. **Better Error Messages**
Clear, actionable error messages when calibration fails:
```
Fisheye model failed for left camera: <error details>
Falling back to omnidir (MEI) model for left camera...
  Calibrating left camera with omnidir...
  Left camera RMS: 0.2543
  Left mirror parameter: xi1=0.123456
Omnidir calibration succeeded for left camera
```

### 5. **Fail-Safe Design**
If both models fail, program exits cleanly with status 1:
```cpp
cerr << "Error: Both fisheye and omnidir calibration failed for left camera!" << endl;
cerr << "Omnidir error: " << e2.what() << endl;
return 1;
```

## Testing Recommendations

### Test Case 1: Normal Fisheye Images (FOV < 185°)
**Expected**: Fisheye calibration succeeds, omnidir fallback not triggered
```bash
docker run -v $(pwd)/imgs:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh \
  -w 11 -h 8 -s 0.02 -d /data/imgs -l left -e jpg -o /data/output
```
**Look for**:
- "KB4 fisheye calibration succeeded for left camera"
- File created: `/data/output/left_ds.yml`

### Test Case 2: Extreme Wide-Angle (FOV > 200°)
**Expected**: Fisheye calibration fails, omnidir fallback succeeds
```bash
docker run -v $(pwd)/extreme_fov_imgs:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh \
  -w 11 -h 8 -s 0.02 -d /data/imgs -l left -e jpg -o /data/output
```
**Look for**:
- "Fisheye model failed for left camera"
- "Falling back to omnidir (MEI) model"
- "Omnidir calibration succeeded for left camera"
- File created: `/data/output/left_ds.yml`

### Test Case 3: Invalid/Corrupted Images
**Expected**: Both models fail, program exits with error
```bash
docker run -v $(pwd)/bad_imgs:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh \
  -w 11 -h 8 -s 0.02 -d /data/imgs -l left -e jpg -o /data/output
```
**Look for**:
- "Error: Both fisheye and omnidir calibration failed"
- Exit code: 1
- No file created

## Technical Details

### Omnidir (MEI) Model

The omnidirectional camera model (also called MEI model) is designed for cameras with field of view greater than 180°. It uses:

- **Mirror parameter ξ (xi)**: Models the virtual mirror surface
- **Standard distortion coefficients**: Similar to fisheye model
- **Extended FOV range**: Supports up to 360° (full panoramic)

Formula: The model projects 3D points to a unit sphere, then to a virtual mirror, then to the image plane.

### Data Type Conversions

OpenCV's omnidir module requires `Point2f` and `Point3f` (32-bit floats), while the fisheye module uses `Point2d` and `Point3d` (64-bit doubles). The fallback code includes proper type conversions:

```cpp
for (size_t i = 0; i < obj_pts_init.size(); i++) {
    vector<Point3f> obj_f;
    for (size_t j = 0; j < obj_pts_init[i].size(); j++) {
        obj_f.push_back(Point3f(
            (float)obj_pts_init[i][j].x,
            (float)obj_pts_init[i][j].y,
            (float)obj_pts_init[i][j].z
        ));
    }
    obj_pts_init_f.push_back(obj_f);
}
```

### Calibration Flags

Both fallback implementations use consistent calibration flags:
```cpp
int omni_flags = 0;
omni_flags |= omnidir::CALIB_FIX_SKEW;  // Assume zero skew (perpendicular axes)
```

This matches the stereo mode omnidir fallback for consistency.

## Security Considerations

### No New Security Issues

The changes do not introduce security vulnerabilities because:

1. **Uses existing OpenCV APIs**: No custom memory management
2. **Proper exception handling**: Two-level try-catch prevents crashes
3. **Input validation**: Image data validated before reaching omnidir::calibrate
4. **Bounded operations**: No unbounded loops or recursion
5. **No external data**: No network calls or file system operations beyond existing code

### Improved Stability

Actually **reduces** security risks by:
- Eliminating undefined behavior from uninitialized memory
- Preventing potential segmentation faults
- Providing controlled failure paths

## Compatibility

### Backward Compatibility

✅ **Fully backward compatible**:
- Existing calibration results unchanged (fisheye path identical)
- Same output file format
- Same command-line interface
- Only activates when fisheye calibration fails

### Dependencies

✅ **No new dependencies**:
- Uses existing `opencv_ccalib` module
- Same OpenCV version (4.7.0)
- No additional libraries

### Build System

✅ **No build system changes**:
- Same CMakeLists.txt
- Same Dockerfile
- Same compilation flags

## Related Issues

This fix addresses a **different root cause** than previous file sync fixes:

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| [Previous] File not visible to shell script | Filesystem sync timing | Added retry logic with `access()` calls |
| [This PR] Undefined behavior in mono mode | Uninitialized camera parameters | Added omnidir fallback |

Both fixes are complementary and necessary for robust mono calibration.

## Commit Information

- **Commit**: d19cd2a
- **Branch**: copilot/fix-left-camera-calibration-issue  
- **Date**: 2025-11-20
- **Files**: calibrate_ds.cpp (+104 lines)

## References

- OpenCV omnidir module: https://docs.opencv.org/4.7.0/dd/d12/tutorial_omnidir_calib_main.html
- MEI camera model: Mei & Rives, "Single View Point Omnidirectional Camera Calibration from Planar Grids", ICRA 2007
- Previous file sync fix: PR_FIX_LEFT_DS.md, FIX_SUMMARY_LEFT_DS.md
