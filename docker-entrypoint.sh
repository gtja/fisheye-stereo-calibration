#!/bin/bash
set -e

# Default values
BOARD_WIDTH=${BOARD_WIDTH:-9}
BOARD_HEIGHT=${BOARD_HEIGHT:-6}
SQUARE_SIZE=${SQUARE_SIZE:-0.02423}
NUM_IMGS=${NUM_IMGS:-29}
IMG_DIR=${IMG_DIR:-/data/imgs/}
LEFT_PREFIX=${LEFT_PREFIX:-left}
RIGHT_PREFIX=${RIGHT_PREFIX:-right}
OUTPUT_FILE=${OUTPUT_FILE:-/data/output/cam_stereo.yml}

# Create output directory if it doesn't exist
mkdir -p "$(dirname "$OUTPUT_FILE")"

# If arguments are provided, use them directly
if [ $# -gt 0 ]; then
    echo "Running calibration with custom arguments: $@"
    exec ./calibrate "$@"
else
    # Use environment variables
    echo "Running calibration with environment variables:"
    echo "  Board width: $BOARD_WIDTH"
    echo "  Board height: $BOARD_HEIGHT"
    echo "  Square size: $SQUARE_SIZE"
    echo "  Number of images: $NUM_IMGS"
    echo "  Image directory: $IMG_DIR"
    echo "  Left prefix: $LEFT_PREFIX"
    echo "  Right prefix: $RIGHT_PREFIX"
    echo "  Output file: $OUTPUT_FILE"
    
    exec ./calibrate \
        -w "$BOARD_WIDTH" \
        -h "$BOARD_HEIGHT" \
        -s "$SQUARE_SIZE" \
        -n "$NUM_IMGS" \
        -d "$IMG_DIR" \
        -l "$LEFT_PREFIX" \
        -r "$RIGHT_PREFIX" \
        -o "$OUTPUT_FILE"
fi
