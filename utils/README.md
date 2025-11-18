# Calibration Utility Scripts

This directory contains Python utility scripts to help improve fisheye stereo calibration quality by detecting and filtering problematic input data.

## Prerequisites

```bash
pip3 install opencv-python numpy
```

## Scripts

### 1. laplacian_var.py - Blur Detection

Detects blurry images using Laplacian variance analysis. Blurry images lead to poor corner detection and should be removed from calibration datasets.

**Usage:**
```bash
# Check all images for blur
python3 laplacian_var.py <image_directory> [--threshold 100]

# Check with custom threshold
python3 laplacian_var.py ../imgs/ --threshold 150

# Check BMP images
python3 laplacian_var.py ../imgs/ --threshold 100 --extension bmp

# Delete blurry images (USE WITH CAUTION - makes backup first!)
python3 laplacian_var.py ../imgs/ --threshold 100 --delete
```

**Threshold Guidelines:**
- **σ² > 100**: Minimum acceptable for 8-bit grayscale
- **σ² > 150**: High quality
- **σ² < 100**: Image should be deleted

**Expected Impact:** Can reduce calibration error by 30-50% by removing blurry images.

**Example Output:**
```
Analyzing 29 images in imgs
Blur threshold: Laplacian variance < 100.0
----------------------------------------------------------------------
✓ left1.jpg                      Laplacian var:   286.30 [SHARP]
✓ left2.jpg                      Laplacian var:   193.47 [SHARP]
✗ left3.jpg                      Laplacian var:    85.23 [BLURRY]
...

Summary:
  Sharp images:  28 / 29
  Blurry images: 1 / 29
```

---

### 2. corner_analysis.py - Corner Quality Analysis

Analyzes checkerboard corner detection quality and distribution. Good calibration requires:
- All expected corners detected (100% detection rate)
- Corners reaching edge regions (within 20% border from each edge)
- At least 3 out of 4 edges should have corner coverage

**Usage:**
```bash
# Analyze corner detection quality
python3 corner_analysis.py <image_directory> --width <W> --height <H> [options]

# Analyze left camera images
python3 corner_analysis.py ../imgs/ --width 9 --height 6 --prefix left

# Analyze all images with visualization
python3 corner_analysis.py ../imgs/ --width 9 --height 6 --save

# Display images during analysis (requires X11/display)
python3 corner_analysis.py ../imgs/ --width 9 --height 6 --show

# Analyze BMP images
python3 corner_analysis.py ../imgs/ --width 9 --height 6 --extension bmp
```

**Parameters:**
- `--width`: Checkerboard width (number of inner corners)
- `--height`: Checkerboard height (number of inner corners)
- `--prefix`: Filter images by filename prefix (e.g., "left", "right")
- `--extension`: Image file extension (default: jpg)
- `--show`: Display images with corners marked
- `--save`: Save corner visualization images to `corner_analysis/` subdirectory

**Expected Impact:** Identifies images with poor coverage that should be replaced.

**Example Output:**
```
Analyzing 29 images
Chessboard size: 9x6 (inner corners)
Expected corners per image: 54
--------------------------------------------------------------------------------
✓ left1.jpg                       54/ 54 corners (4/4 edges) [GOOD]
⚠ left2.jpg                       54/ 54 corners (2/4 edges) [POOR DIST]
✗ left3.jpg                       NO CORNERS DETECTED
...

Summary:
  Good images:    20 / 29
  Warning images: 7 / 29
  Failed images:  2 / 29

⚠ Images with warnings (poor distribution or incomplete corners):
  - left2.jpg
  - left5.jpg
  ...

Recommendations:
  - Recapture images with better coverage of image edges
  - Ensuring the checkerboard is visible at all 4 corners when possible
```

---

## Workflow

**Recommended workflow for best calibration results:**

1. **Capture images** following guidelines in [CALIBRATION_GUIDE.md](../CALIBRATION_GUIDE.md)
   - 30-40 images minimum
   - Cover all 4 edges
   - Various tilts and rotations

2. **Check for blur:**
   ```bash
   python3 utils/laplacian_var.py imgs/ --threshold 100
   ```
   - Remove any blurry images identified

3. **Analyze corner quality:**
   ```bash
   python3 utils/corner_analysis.py imgs/ --width 9 --height 6 --prefix left --save
   python3 utils/corner_analysis.py imgs/ --width 9 --height 6 --prefix right --save
   ```
   - Review images with poor distribution
   - Consider recapturing problematic images

4. **Run calibration:**
   ```bash
   ./build/calibrate -w 9 -h 6 -s 0.02423 -n <count> -d imgs/ -l left -r right -o output.yml
   ```

5. **Review results:**
   - Target: < 0.3 pixel reprojection error
   - If errors are high, review data quality and capture guidelines

---

## Technical Details

### Laplacian Variance Method

The Laplacian operator computes the second derivative of the image, which highlights rapid intensity changes (edges). Blurry images have lower variance in their Laplacian:

```python
laplacian = cv2.Laplacian(gray, cv2.CV_64F)
variance = laplacian.var()
```

Sharp edges → high variance; blurry edges → low variance.

### Corner Distribution Check

Good calibration requires corners in edge regions to properly constrain the distortion model. The script checks if corners exist in border regions defined as:
- Left border: x < 20% of image width
- Right border: x > 80% of image width
- Top border: y < 20% of image height
- Bottom border: y > 80% of image height

At least 3 out of 4 edges should have corners for good distribution.

---

## Troubleshooting

### "No module named 'cv2'"
Install OpenCV:
```bash
pip3 install opencv-python numpy
```

### "No images found"
- Check that image directory path is correct
- Verify image file extension matches (use `--extension` parameter)
- Ensure images follow naming convention: `<prefix><number>.<extension>`

### "No corners detected"
Possible causes:
- Checkerboard is out of focus
- Lighting is poor
- Checkerboard pattern is occluded
- Wrong board dimensions specified

Review images visually and recapture if necessary.

---

## Additional Resources

- [CALIBRATION_GUIDE.md](../CALIBRATION_GUIDE.md) - Comprehensive calibration best practices
- [OpenCV Camera Calibration](https://docs.opencv.org/4.x/dc/dbb/tutorial_py_calibration.html)
- [Fisheye Camera Model](https://docs.opencv.org/4.x/db/d58/group__calib3d__fisheye.html)

For questions or issues, please open a GitHub issue.
