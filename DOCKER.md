# Docker Usage for Fisheye Stereo Calibration

This document explains how to use the Docker container to run the fisheye stereo calibration.

## Building the Docker Image

```bash
docker build -t fisheye-stereo-calibration .
```

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
docker run \
  -v /path/to/your/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml
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
- Left camera images: `{LEFT_PREFIX}1.jpg`, `{LEFT_PREFIX}2.jpg`, ..., `{LEFT_PREFIX}N.jpg`
- Right camera images: `{RIGHT_PREFIX}1.jpg`, `{RIGHT_PREFIX}2.jpg`, ..., `{RIGHT_PREFIX}N.jpg`

For example, with default prefixes:
- `left1.jpg`, `left2.jpg`, ..., `left29.jpg`
- `right1.jpg`, `right2.jpg`, ..., `right29.jpg`

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
- `-o, --out_file STR`: Output calibration file path (YAML format)

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
2. Image files follow the naming convention (prefix + number + .jpg)
3. The number of images specified matches the actual number of image pairs
4. The directory path ends with a `/`
