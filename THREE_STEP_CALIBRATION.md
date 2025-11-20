# Three-Step Calibration Scripts

This guide explains how to use the three separate scripts for running the hand-eye calibration workflow step by step, instead of using the monolithic `docker-entrypoint.sh`.

## Overview

The hand-eye calibration workflow has been split into three independent scripts:

1. **`step1_monocular_calibration.sh`** - Calibrates left and right cameras separately
2. **`step2_handeye_calibration.sh`** - Computes stereo extrinsics using hand-eye calibration
3. **`step3_stereo_bundle_adjustment.sh`** - Performs final stereo bundle adjustment

## Prerequisites

- Compiled executables: `calibrate_ds` and `compute_handeye` 
- Calibration images in a directory with left and right image pairs
- All three scripts should be executable (`chmod +x step*.sh`)

## Quick Start

If you want to run all three steps in sequence with a single command, use the convenience script:

```bash
./run_three_step_calibration.sh
```

This script will run all three steps automatically with common parameters. You can still set environment variables to customize the parameters:

```bash
BOARD_WIDTH=9 BOARD_HEIGHT=6 SQUARE_SIZE=0.025 IMG_DIR=./imgs/ \
  LEFT_PREFIX=left RIGHT_PREFIX=right IMAGE_EXTENSION=bmp \
  OUTPUT_DIR=./output ./run_three_step_calibration.sh
```

For more control and debugging, continue reading to learn how to run each step individually.

## Usage

### Step 1: Monocular Calibration

Run the first script to calibrate both cameras separately:

```bash
./step1_monocular_calibration.sh
```

**Environment Variables:**
```bash
BOARD_WIDTH=11              # Checkerboard width (default: 11)
BOARD_HEIGHT=8              # Checkerboard height (default: 8)
SQUARE_SIZE=0.02            # Square size in meters (default: 0.02)
IMG_DIR=/data/imgs/         # Image directory (default: /data/imgs/)
LEFT_PREFIX=left            # Left image prefix (default: left)
RIGHT_PREFIX=right          # Right image prefix (default: right)
OUTPUT_DIR=/data/output     # Output directory (default: /data/output)
IMAGE_EXTENSION=jpg         # Image extension (default: jpg)
```

**Example with custom parameters:**
```bash
BOARD_WIDTH=9 BOARD_HEIGHT=6 SQUARE_SIZE=0.025 IMG_DIR=./imgs/ \
  LEFT_PREFIX=left RIGHT_PREFIX=right IMAGE_EXTENSION=bmp \
  OUTPUT_DIR=./output ./step1_monocular_calibration.sh
```

**Output files:**
- `left_ds.yml` - Left camera intrinsics
- `right_ds.yml` - Right camera intrinsics

### Step 2: Hand-Eye Calibration

Run the second script to compute stereo extrinsics:

```bash
./step2_handeye_calibration.sh
```

**Environment Variables:**
Same as Step 1, plus:
```bash
# The script automatically uses left_ds.yml and right_ds.yml from OUTPUT_DIR
```

**Example with custom parameters:**
```bash
BOARD_WIDTH=9 BOARD_HEIGHT=6 SQUARE_SIZE=0.025 IMG_DIR=./imgs/ \
  LEFT_PREFIX=left RIGHT_PREFIX=right IMAGE_EXTENSION=bmp \
  OUTPUT_DIR=./output ./step2_handeye_calibration.sh
```

**Output file:**
- `handeye.yml` - Stereo extrinsics (T_right_left)

### Step 3: Stereo Bundle Adjustment

Run the third script to perform final optimization:

```bash
./step3_stereo_bundle_adjustment.sh
```

**Environment Variables:**
Same as Step 1, plus:
```bash
OUTPUT_FILE=/data/output/cam_stereo.yml  # Final output file (default: /data/output/cam_stereo.yml)
```

**Example with custom parameters:**
```bash
BOARD_WIDTH=9 BOARD_HEIGHT=6 SQUARE_SIZE=0.025 IMG_DIR=./imgs/ \
  LEFT_PREFIX=left RIGHT_PREFIX=right IMAGE_EXTENSION=bmp \
  OUTPUT_DIR=./output OUTPUT_FILE=./output/final_stereo.yml \
  ./step3_stereo_bundle_adjustment.sh
```

**Output file:**
- `cam_stereo.yml` (or custom filename) - Final stereo calibration

## Complete Example

Here's a complete example running all three steps:

```bash
# Set common parameters
export BOARD_WIDTH=11
export BOARD_HEIGHT=8
export SQUARE_SIZE=0.02
export IMG_DIR=/data/imgs/
export LEFT_PREFIX=left
export RIGHT_PREFIX=right
export IMAGE_EXTENSION=jpg
export OUTPUT_DIR=/data/output

# Step 1: Monocular calibration
./step1_monocular_calibration.sh

# Step 2: Hand-eye calibration
./step2_handeye_calibration.sh

# Step 3: Stereo bundle adjustment
./step3_stereo_bundle_adjustment.sh
```

## Docker Usage

You can also run these scripts inside a Docker container:

```bash
# Build the Docker image
docker build -t fisheye-stereo-calibration .

# Run Step 1
docker run --rm \
  -v /path/to/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e BOARD_WIDTH=11 \
  -e BOARD_HEIGHT=8 \
  -e SQUARE_SIZE=0.02 \
  fisheye-stereo-calibration \
  ./step1_monocular_calibration.sh

# Run Step 2
docker run --rm \
  -v /path/to/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e BOARD_WIDTH=11 \
  -e BOARD_HEIGHT=8 \
  -e SQUARE_SIZE=0.02 \
  fisheye-stereo-calibration \
  ./step2_handeye_calibration.sh

# Run Step 3
docker run --rm \
  -v /path/to/imgs:/data/imgs \
  -v /path/to/output:/data/output \
  -e BOARD_WIDTH=11 \
  -e BOARD_HEIGHT=8 \
  -e SQUARE_SIZE=0.02 \
  fisheye-stereo-calibration \
  ./step3_stereo_bundle_adjustment.sh
```

## Advantages of Separate Scripts

1. **Flexibility** - Run each step independently for testing or debugging
2. **Resume capability** - If a step fails, fix the issue and continue from that step
3. **Partial workflows** - Run only the steps you need (e.g., just monocular calibration)
4. **Easy debugging** - Isolate problems to specific steps
5. **Intermediate inspection** - Examine output files between steps

## Error Handling

Each script includes:
- Automatic creation of output directories
- File verification with retry logic (handles filesystem sync delays)
- Clear error messages indicating which step failed
- Prerequisites checking (verifying input files from previous steps exist)

## See Also

- [HAND_EYE_CALIBRATION.md](HAND_EYE_CALIBRATION.md) - Detailed explanation of the hand-eye calibration workflow
- [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md) - General calibration best practices
- [README.md](README.md) - Main project documentation
