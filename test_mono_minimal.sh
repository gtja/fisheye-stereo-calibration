#!/bin/bash
# Minimal test script to diagnose mono calibration issue
# This script isolates the mono calibration call and captures all output

set -x  # Print commands as they execute
set -e  # Exit on error

echo "=========================================="
echo "Minimal Mono Calibration Test"
echo "=========================================="

# Test parameters
BOARD_WIDTH=11
BOARD_HEIGHT=8
SQUARE_SIZE=0.025
IMG_DIR=/data/imgs
LEFT_PREFIX=left
IMAGE_EXTENSION=bmp
OUTPUT_FILE=/data/output/test_left_ds.yml

echo ""
echo "Test Configuration:"
echo "  Board: ${BOARD_WIDTH}x${BOARD_HEIGHT}"
echo "  Square size: ${SQUARE_SIZE}"
echo "  Images: ${IMG_DIR}/${LEFT_PREFIX}*.${IMAGE_EXTENSION}"
echo "  Output: ${OUTPUT_FILE}"
echo ""

# Ensure output directory exists
mkdir -p "$(dirname "$OUTPUT_FILE")"

echo "Running calibrate_ds in mono mode..."
echo "Command: ./calibrate_ds -w $BOARD_WIDTH -h $BOARD_HEIGHT -s $SQUARE_SIZE -d $IMG_DIR -l $LEFT_PREFIX -e $IMAGE_EXTENSION --mono -o $OUTPUT_FILE"
echo ""

# Run calibration and capture exit code
./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --mono \
    -o "$OUTPUT_FILE" 2>&1 | tee /tmp/calibrate_output.log

EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo "=========================================="
echo "Calibration completed with exit code: $EXIT_CODE"
echo "=========================================="
echo ""

# Check if file was created
echo "Checking for output file..."
if [ -f "$OUTPUT_FILE" ]; then
    echo "✓ SUCCESS: File was created"
    ls -lh "$OUTPUT_FILE"
    echo ""
    echo "File contents (first 20 lines):"
    head -20 "$OUTPUT_FILE"
else
    echo "✗ FAIL: File was NOT created"
    echo ""
    echo "Directory contents:"
    ls -lah "$(dirname "$OUTPUT_FILE")"
    echo ""
    echo "Last 50 lines of output:"
    tail -50 /tmp/calibrate_output.log
    exit 1
fi

echo ""
echo "Test completed successfully!"
