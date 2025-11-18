# Docker Usage for Fisheye Stereo Calibration

This document explains how to use the Docker container to run the fisheye stereo calibration with the complete workflow including automatic image quality checks.

## 🆕 Automatic Image Quality Checks

The Docker container now includes **automatic pre-calibration quality checks** that run by default! This feature:

✅ **Detects and removes blurry images** using Laplacian variance analysis  
✅ **Validates corner detection quality** for all images  
✅ **Automatically filters out problematic images** that would degrade calibration accuracy  
✅ **Backs up removed images** for review in `.removed_images_backup/` directory  
✅ **Updates the image count** automatically for calibration  

This can improve calibration accuracy by **30-50%** by ensuring only high-quality images are used.

**Key benefits:**
- No manual pre-processing required
- Consistent quality standards applied automatically
- Better calibration results with less effort
- Problematic images are backed up (not permanently deleted)

To disable automatic checks and use the original workflow, set `RUN_QUALITY_CHECKS=false`.

## Building the Docker Image

```bash
docker build -t fisheye-stereo-calibration .
```

## Recommended Workflow

For best calibration results, follow this complete workflow:

### Automated Workflow (Recommended)

**NEW:** The Docker container now includes **automatic image quality checks** that run before calibration by default! This workflow:
1. Checks all images for blur using Laplacian variance
2. Analyzes corner detection quality and distribution
3. Removes images that don't meet quality requirements
4. Backs up removed images for review
5. Runs calibration with the validated images

Simply run:
```bash
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -d /data/imgs/ -l left -r right -e jpg -o /data/output/cam_stereo.yml
```

The container will automatically:
- Check for blurry images (Laplacian variance < 100)
- Validate corner detection for all images
- Remove problematic images (backed up to `.removed_images_backup/`)
- Calibrate using only the validated images

**Customize quality check thresholds:**
```bash
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e BLUR_THRESHOLD=150 \
  -e RUN_QUALITY_CHECKS=true \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml
```

**Disable automatic quality checks (use original workflow):**
```bash
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e RUN_QUALITY_CHECKS=false \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml
```

### 🆕 Double-Sphere Calibration (High Precision)

For applications requiring sub-pixel accuracy (<0.15 pixels RMSE), use the Double-Sphere model with 6-order radial distortion and Ceres Bundle Adjustment:

```bash
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e CALIBRATION_MODEL=double_sphere \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo_ds.yml
```

**Key features:**
- 6-order radial distortion (k1-k6) for better edge modeling
- SE(3) pre-correction for improved corner detection
- Ceres-based Bundle Adjustment with Huber robust kernel
- Typically achieves 0.1-0.15 pixels RMSE (vs 0.3 pixels with standard methods)

**Note:** The `-n` (number of images) parameter is not needed for Double-Sphere calibration as it automatically detects all valid image pairs.

For more details, see [DOUBLE_SPHERE_CALIBRATION.md](DOUBLE_SPHERE_CALIBRATION.md).

### Manual Workflow (Advanced Users)

If you prefer to run quality checks manually or need more control:

#### Step 1: Capture Images
Follow guidelines in [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md):
- 30-40 images minimum
- Cover all 4 edges
- Various tilts and rotations

#### Step 2: Check for Blur
```bash
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/laplacian_var.py /data/imgs/ --threshold 100
```
Remove any blurry images identified (Laplacian variance < 100).

#### Step 3: Analyze Corner Quality
```bash
# For left camera images
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix left

# For right camera images
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix right
```

To save corner visualizations:
```bash
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix left --save
```

#### Step 4: Run Calibration (with quality checks disabled)
```bash
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e RUN_QUALITY_CHECKS=false \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml
```

#### Step 5: Review Results
- Target: < 0.3 pixel reprojection error
- If errors are high, review data quality and capture guidelines

## Running the Calibration

### Basic Usage with Default Parameters

The easiest way to run the calibration is to mount your image directory and output directory:

```bash
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output fisheye-stereo-calibration
```

This will use the default parameters:
- Board width: 9
- Board height: 6
- Square size: 0.02423
- Number of images: 29
- Image directory: /data/imgs/
- Left image prefix: left
- Right image prefix: right
- Output file: /data/output/cam_stereo.yml

### Custom Parameters via Environment Variables

You can customize the calibration parameters using environment variables:

```bash
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e BOARD_WIDTH=9 \
  -e BOARD_HEIGHT=6 \
  -e SQUARE_SIZE=0.02423 \
  -e NUM_IMGS=29 \
  -e IMG_DIR=/data/imgs/ \
  -e LEFT_PREFIX=left \
  -e RIGHT_PREFIX=right \
  -e IMAGE_EXTENSION=jpg \
  -e OUTPUT_FILE=/data/output/cam_stereo.yml \
  -e RUN_QUALITY_CHECKS=true \
  -e BLUR_THRESHOLD=100 \
  fisheye-stereo-calibration
```

**Available environment variables:**
- `BOARD_WIDTH`: Checkerboard width (default: 9)
- `BOARD_HEIGHT`: Checkerboard height (default: 6)
- `SQUARE_SIZE`: Checkerboard square size in meters (default: 0.02423)
- `NUM_IMGS`: Number of image pairs (default: 29, auto-updated if quality checks are enabled)
- `IMG_DIR`: Image directory path (default: /data/imgs/)
- `LEFT_PREFIX`: Left camera image prefix (default: left)
- `RIGHT_PREFIX`: Right camera image prefix (default: right)
- `IMAGE_EXTENSION`: Image file extension (default: jpg)
- `OUTPUT_FILE`: Output YAML file path (default: /data/output/cam_stereo.yml)
- `RUN_QUALITY_CHECKS`: Enable automatic quality checks (default: true)
- `BLUR_THRESHOLD`: Laplacian variance threshold for blur detection (default: 100)

### Custom Parameters via Command Line

Alternatively, you can pass command-line arguments directly:

```bash
# With JPG images (default)
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml

# With BMP images
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -e bmp -o /data/output/cam_stereo.yml
```

### Example with Sample Images

To test with the included sample images:

```bash
# Build the image
docker build -t fisheye-stereo-calibration .

# Run with sample images (already included in the container)
docker run \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /app/imgs/ -l left -r right -o /data/output/cam_stereo.yml
```

## Image Naming Convention

Your images should follow this naming pattern:
- Left camera images: `{LEFT_PREFIX}1.{EXT}`, `{LEFT_PREFIX}2.{EXT}`, ..., `{LEFT_PREFIX}N.{EXT}`
- Right camera images: `{RIGHT_PREFIX}1.{EXT}`, `{RIGHT_PREFIX}2.{EXT}`, ..., `{RIGHT_PREFIX}N.{EXT}`

Where `{EXT}` is the image file extension (default: `jpg`).

For example, with default prefixes and jpg format:
- `left1.jpg`, `left2.jpg`, ..., `left29.jpg`
- `right1.jpg`, `right2.jpg`, ..., `right29.jpg`

For BMP images:
- `left1.bmp`, `left2.bmp`, ..., `left29.bmp`
- `right1.bmp`, `right2.bmp`, ..., `right29.bmp`

## Output

The calibration will generate a YAML file with the following calibration parameters:
- `K1`, `K2`: Camera intrinsic matrices
- `D1`, `D2`: Distortion coefficients
- `R`: Rotation matrix between cameras
- `T`: Translation vector between cameras
- `R1`, `R2`: Rectification rotation matrices
- `P1`, `P2`: Projection matrices
- `Q`: Disparity-to-depth mapping matrix

## Command Line Options

- `-w, --board_width NUM`: Checkerboard width (number of inner corners)
- `-h, --board_height NUM`: Checkerboard height (number of inner corners)
- `-s, --square_size NUM`: Checkerboard square size in meters
- `-n, --num_imgs NUM`: Number of image pairs to process
- `-d, --img_dir STR`: Directory containing the images (must end with /)
- `-l, --leftimg_filename STR`: Prefix for left camera images
- `-r, --rightimg_filename STR`: Prefix for right camera images
- `-e, --extension STR`: Image file extension (default: jpg). Supports jpg, bmp, png, and other OpenCV-compatible formats
- `-o, --out_file STR`: Output calibration file path (YAML format)
- `-b, --physical_baseline NUM`: Physical baseline distance in meters (optional, for accuracy evaluation)

## Utility Scripts

The Docker image includes Python utility scripts to help improve calibration quality. These scripts are located in `/app/utils/` inside the container.

### Blur Detection (laplacian_var.py)

Detect blurry images using Laplacian variance:

```bash
# Check all images
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/laplacian_var.py /data/imgs/ --threshold 100

# Check with custom threshold
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/laplacian_var.py /data/imgs/ --threshold 150

# Check BMP images
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/laplacian_var.py /data/imgs/ --extension bmp --threshold 100
```

### Corner Quality Analysis (corner_analysis.py)

Analyze corner detection quality and distribution:

```bash
# Analyze left camera images
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix left

# Save visualizations (output to mounted directory)
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix left --save

# Analyze with different board dimensions
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/corner_analysis.py /data/imgs/ --width 8 --height 5 --prefix right
```

**Note**: These scripts help identify problematic images before calibration, potentially reducing errors by 30-50%. See [utils/README.md](utils/README.md) for detailed documentation.

## Troubleshooting

### Permission Issues

If you encounter permission issues with the output file, you can run the container with your user ID:

```bash
docker run --user $(id -u):$(id -g) \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  fisheye-stereo-calibration
```

### Images Not Found

Make sure:
1. Your image directory is correctly mounted
2. Image files follow the naming convention (prefix + number + extension)
3. The extension parameter matches your image file format (use `-e bmp` for BMP files, `-e png` for PNG files, etc.)
4. The number of images specified matches the actual number of image pairs
5. The directory path ends with a `/`

### Utility Scripts Not Working

If utility scripts fail:
1. Ensure the image directory is correctly mounted
2. Check Python packages are installed (they should be included in the image)
3. Verify image files exist and are readable
4. For BMP/PNG files, use the `--extension` parameter
