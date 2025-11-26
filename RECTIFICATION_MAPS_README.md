# Rectification Maps

## Overview

After stereo calibration completes, the calibration tool automatically generates and saves rectification maps to a separate folder. These maps can be used to rectify stereo images for disparity computation and 3D reconstruction.

## Location

Rectification maps are saved in a `rectification_maps` subfolder relative to the output calibration file location.

For example:
- If calibration results are saved to: `calib_results/stereo_ds.yml`
- Rectification maps will be saved to: `calib_results/rectification_maps/`

## Files Generated

The following files are created in the `rectification_maps` folder:

1. **`map_left_x.yml`** - X-coordinates mapping for left camera rectification
2. **`map_left_y.yml`** - Y-coordinates mapping for left camera rectification
3. **`map_right_x.yml`** - X-coordinates mapping for right camera rectification
4. **`map_right_y.yml`** - Y-coordinates mapping for right camera rectification
5. **`rectification_params.yml`** - Rectification parameters including:
   - Original image size
   - Rectified image size
   - Stereo rotation matrix (R_stereo)
   - Stereo translation vector (T_stereo)

## Usage

### Loading Rectification Maps

Use OpenCV to load and apply the rectification maps:

```cpp
#include <opencv2/opencv.hpp>

// Load rectification maps
cv::Mat map_left_x, map_left_y, map_right_x, map_right_y;
cv::FileStorage fs_lx("rectification_maps/map_left_x.yml", cv::FileStorage::READ);
cv::FileStorage fs_ly("rectification_maps/map_left_y.yml", cv::FileStorage::READ);
cv::FileStorage fs_rx("rectification_maps/map_right_x.yml", cv::FileStorage::READ);
cv::FileStorage fs_ry("rectification_maps/map_right_y.yml", cv::FileStorage::READ);

fs_lx["map_left_x"] >> map_left_x;
fs_ly["map_left_y"] >> map_left_y;
fs_rx["map_right_x"] >> map_right_x;
fs_ry["map_right_y"] >> map_right_y;

fs_lx.release();
fs_ly.release();
fs_rx.release();
fs_ry.release();
```

### Rectifying Images

Apply the maps to rectify stereo images:

```cpp
cv::Mat left_img = cv::imread("left_image.jpg");
cv::Mat right_img = cv::imread("right_image.jpg");

cv::Mat left_rectified, right_rectified;

// Apply rectification
cv::remap(left_img, left_rectified, map_left_x, map_left_y, cv::INTER_LINEAR);
cv::remap(right_img, right_rectified, map_right_x, map_right_y, cv::INTER_LINEAR);

// Now left_rectified and right_rectified are rectified and ready for stereo matching
```

### Python Example

```python
import cv2
import numpy as np

# Load rectification maps
fs_lx = cv2.FileStorage("rectification_maps/map_left_x.yml", cv2.FILE_STORAGE_READ)
fs_ly = cv2.FileStorage("rectification_maps/map_left_y.yml", cv2.FILE_STORAGE_READ)
fs_rx = cv2.FileStorage("rectification_maps/map_right_x.yml", cv2.FILE_STORAGE_READ)
fs_ry = cv2.FileStorage("rectification_maps/map_right_y.yml", cv2.FILE_STORAGE_READ)

map_left_x = fs_lx.getNode("map_left_x").mat()
map_left_y = fs_ly.getNode("map_left_y").mat()
map_right_x = fs_rx.getNode("map_right_x").mat()
map_right_y = fs_ry.getNode("map_right_y").mat()

fs_lx.release()
fs_ly.release()
fs_rx.release()
fs_ry.release()

# Load and rectify images
left_img = cv2.imread("left_image.jpg")
right_img = cv2.imread("right_image.jpg")

left_rectified = cv2.remap(left_img, map_left_x, map_left_y, cv2.INTER_LINEAR)
right_rectified = cv2.remap(right_img, map_right_x, map_right_y, cv2.INTER_LINEAR)

# Save or process rectified images
cv2.imwrite("left_rectified.jpg", left_rectified)
cv2.imwrite("right_rectified.jpg", right_rectified)
```

## Example Script

A complete example script is provided: `example_rectify.py`

This script demonstrates how to:
- Load rectification maps from the generated files
- Apply rectification to stereo image pairs
- Save rectified images with horizontal alignment lines

**Usage:**
```bash
python3 example_rectify.py calib_results/rectification_maps imgs/left1.jpg imgs/right1.jpg rectified_output
```

The script will:
1. Load the rectification maps
2. Display the map dimensions and parameters
3. Rectify both images
4. Save individual rectified images
5. Create a side-by-side comparison
6. Generate a version with horizontal lines to visualize alignment

## Notes

- Rectification maps are generated using the Double-Sphere camera model
- The maps transform images from the original fisheye projection to a rectified pinhole projection
- Rectification map generation is only performed for stereo calibration (not mono mode)
- The rectified image size is automatically determined based on the camera parameters and field of view
- For extreme wide-angle lenses (FOV > 200°), the rectified image size may be larger than the original to capture all valid points

## Technical Details

The rectification maps are computed using the `createStereoRectificationMaps` function from the Double-Sphere camera model implementation. This function:

1. Computes rectification rotation matrices to align both cameras with a common coordinate system
2. Sets the X-axis along the baseline (left-to-right direction)
3. Sets the Z-axis pointing forward
4. Creates remap lookup tables for efficient image rectification

The maps are stored as `CV_32FC1` (32-bit floating point) matrices containing pixel coordinates.
