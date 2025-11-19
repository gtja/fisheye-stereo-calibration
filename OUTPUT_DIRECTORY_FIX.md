# Fix: Automatic Output Directory Creation

## Issue
After step1a and step1b of the hand-eye calibration workflow complete, the left and right camera calibration files were not saved to the specified path (e.g., `/data/output/left_ds.yml` and `/data/output/right_ds.yml`).

Users would encounter the error:
```
[ERROR:0@0.000] global persistence.cpp:505 open Can't open file: '/data/output/left_ds.yml' in read mode
Error: Cannot open /data/output/left_ds.yml
```

## Root Cause
The calibration programs did not create output directories before attempting to write files. When OpenCV's `FileStorage::WRITE` was called on a path where the parent directory didn't exist, the file creation would fail silently.

## Solution
All calibration programs now automatically create the necessary output directories before saving files.

### Implementation Details
Added directory creation helper functions to:
- `calibrate.cpp` - Fisheye/Omnidir stereo calibration
- `calibrate_ds.cpp` - Double-sphere stereo calibration with monocular mode
- `compute_handeye.cpp` - Hand-eye calibration utility

The implementation uses POSIX `mkdir()` to create directories recursively (similar to `mkdir -p`), with proper error handling for already-existing directories.

### Impact
✅ No user action required - directories are created automatically
✅ Works for nested directory structures (e.g., `/data/output/subdir/file.yml`)
✅ Handles existing directories gracefully
✅ Clear error messages if directory creation fails

## Technical Changes

### Modified Files
1. **calibrate.cpp**
   - Added directory creation before writing omnidir calibration results
   - Added directory creation before writing fisheye calibration results

2. **calibrate_ds.cpp**
   - Added directory creation before writing monocular calibration results
   - Added directory creation before writing stereo calibration results

3. **compute_handeye.cpp**
   - Added directory creation before writing hand-eye calibration results

### New Functions
```cpp
// Creates directories recursively (like mkdir -p)
bool create_directory_recursive(const char* path);

// Ensures the directory exists for a given file path
bool ensure_output_directory(const char* filepath);
```

## Testing
✅ Unit test verified directory creation logic
✅ Workflow simulation confirmed calibration files are created successfully
✅ Verified handling of existing directories (returns success)

## Backwards Compatibility
This change is fully backwards compatible. If the output directory already exists (e.g., created by docker-entrypoint.sh), the programs work exactly as before. If the directory doesn't exist, it will be created automatically.

## Related Issue
Fixes: step1a和step1b 执行完之后，左右相机的标定文件没有保存到指定路径下

(Translation: After step1a and step1b execute, the left and right camera calibration files are not saved to the specified path)
