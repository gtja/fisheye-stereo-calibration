# Docker Usage for Fisheye Stereo Calibration

This document explains how to use the Docker container to run the fisheye stereo calibration with the complete workflow including quality checks.

## Building the Docker Image

```bash
docker build -t fisheye-stereo-calibration .
```

## Recommended Workflow

For best calibration results, follow this complete workflow:

### Step 1: Capture Images
Follow guidelines in [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md):
- 30-40 images minimum
- Cover all 4 edges
- Various tilts and rotations

### Step 2: Check for Blur
```bash
docker run --rm -v /path/to/your/imgs:/data/imgs \
  fisheye-stereo-calibration \
  python3 /app/utils/laplacian_var.py /data/imgs/ --threshold 100
```
Remove any blurry images identified (Laplacian variance < 100).

### Step 3: Analyze Corner Quality
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

### Step 4: Run Calibration
See sections below for running the calibration.

### Step 5: Review Results
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
  -e OUTPUT_FILE=/data/output/cam_stereo.yml \
  fisheye-stereo-calibration
```

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
