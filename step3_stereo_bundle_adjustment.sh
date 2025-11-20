#!/bin/bash
set -e

# Step 3: Stereo Bundle Adjustment
# This script performs stereo bundle adjustment with joint optimization
# using the hand-eye extrinsics from step 2 as initial guess.

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -w) BOARD_WIDTH="$2"; shift 2 ;;
        -h) BOARD_HEIGHT="$2"; shift 2 ;;
        -s) SQUARE_SIZE="$2"; shift 2 ;;
        -d) IMG_DIR="$2"; shift 2 ;;
        -e) IMAGE_EXTENSION="$2"; shift 2 ;;
        -o) OUTPUT_DIR="$2"; shift 2 ;;
        *) shift ;;
    esac
done

# Default values
BOARD_WIDTH=${BOARD_WIDTH:-11}
BOARD_HEIGHT=${BOARD_HEIGHT:-8}
SQUARE_SIZE=${SQUARE_SIZE:-0.02}
IMG_DIR=${IMG_DIR:-/data/imgs/}
LEFT_PREFIX=${LEFT_PREFIX:-left}
RIGHT_PREFIX=${RIGHT_PREFIX:-right}
OUTPUT_DIR=${OUTPUT_DIR:-/data/output}
OUTPUT_FILE=${OUTPUT_FILE:-${OUTPUT_DIR}/cam_stereo.yml}
IMAGE_EXTENSION=${IMAGE_EXTENSION:-jpg}

# Input file (from step 2)
HANDEYE_FILE="${OUTPUT_DIR}/handeye.yml"

# Create output directory if it doesn't exist
mkdir -p "$(dirname "$OUTPUT_FILE")"

echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         STEP 3: STEREO BUNDLE ADJUSTMENT                                   ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Configuration:"
echo "  Board width: $BOARD_WIDTH"
echo "  Board height: $BOARD_HEIGHT"
echo "  Square size: $SQUARE_SIZE"
echo "  Image directory: $IMG_DIR"
echo "  Left prefix: $LEFT_PREFIX"
echo "  Right prefix: $RIGHT_PREFIX"
echo "  Image extension: $IMAGE_EXTENSION"
echo "  Hand-eye calibration: $HANDEYE_FILE"
echo "  Output file: $OUTPUT_FILE"
echo ""

# Check if input file exists
if [ ! -f "$HANDEYE_FILE" ]; then
    echo "Error: Hand-eye calibration file not found: $HANDEYE_FILE"
    echo "Please run step2_handeye_calibration.sh first"
    exit 1
fi

echo "Running stereo bundle adjustment..."
echo "Command: ./calibrate_ds -w $BOARD_WIDTH -h $BOARD_HEIGHT -s $SQUARE_SIZE -d $IMG_DIR -l $LEFT_PREFIX -r $RIGHT_PREFIX -e $IMAGE_EXTENSION --init-extrinsic $HANDEYE_FILE --joint-ba -o $OUTPUT_FILE --skip-image-checks"
echo ""
echo "Note: Using --skip-image-checks to skip redundant image quality checks (already performed in Step 1)"
echo ""

if ! ./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --init-extrinsic "$HANDEYE_FILE" \
    --joint-ba \
    -o "$OUTPUT_FILE" \
    --skip-image-checks; then
    echo "Error: Stereo bundle adjustment failed"
    exit 1
fi

echo ""
echo "✓ Stereo bundle adjustment completed: $OUTPUT_FILE"
echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         STEP 3 COMPLETED SUCCESSFULLY                                      ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         THREE-STEP HAND-EYE CALIBRATION WORKFLOW COMPLETED                 ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Output files:"
echo "  - Monocular calibrations: ${OUTPUT_DIR}/left_ds.yml, ${OUTPUT_DIR}/right_ds.yml"
echo "  - Hand-eye calibration: $HANDEYE_FILE"
echo "  - Final stereo calibration: $OUTPUT_FILE"
echo ""
