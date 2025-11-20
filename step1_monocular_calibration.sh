#!/bin/bash
set -e

# Step 1: Monocular Calibration for Left and Right Cameras
# This script performs monocular calibration for both left and right cameras
# as the first step of the three-step hand-eye calibration workflow.

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
IMAGE_EXTENSION=${IMAGE_EXTENSION:-jpg}

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Output files
LEFT_MONO="${OUTPUT_DIR}/left_ds.yml"
RIGHT_MONO="${OUTPUT_DIR}/right_ds.yml"

# Function to verify file creation with retry logic
# Handles filesystem sync delays that can occur in containerized environments
verify_file_with_retry() {
    local file_path="$1"
    local max_retries=10
    local retry_delay=0.1  # 100ms between retries
    local retry_count=0
    
    while [ $retry_count -lt $max_retries ]; do
        if [ -f "$file_path" ] && [ -r "$file_path" ]; then
            return 0
        fi
        sleep $retry_delay
        retry_count=$((retry_count + 1))
    done
    
    return 1
}

echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         STEP 1: MONOCULAR CALIBRATION (LEFT AND RIGHT CAMERAS)            ║"
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
echo "  Output directory: $OUTPUT_DIR"
echo ""

# Step 1a: Monocular calibration for left camera
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║  Step 1a: Monocular Calibration - Left Camera                             ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Command: ./calibrate_ds -w $BOARD_WIDTH -h $BOARD_HEIGHT -s $SQUARE_SIZE -d $IMG_DIR -l $LEFT_PREFIX -e $IMAGE_EXTENSION --mono -o $LEFT_MONO"
echo ""

if ! ./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --mono \
    -o "$LEFT_MONO"; then
    echo "Error: Left camera monocular calibration failed"
    exit 1
fi

# Verify that the output file was created and is readable (with retry logic)
if ! verify_file_with_retry "$LEFT_MONO"; then
    echo "Error: Left camera calibration file was not created or not readable: $LEFT_MONO"
    echo "This is a known issue with the file-saving logic in mono mode."
    echo "Please rebuild the Docker image with the latest code fix."
    echo ""
    echo "Workaround: You can manually copy a generated calibration file if available,"
    echo "or re-run with stereo calibration mode (both -l and -r)."
    echo ""
    ls -la "$OUTPUT_DIR" 2>/dev/null || echo "Directory does not exist"
    exit 1
fi

echo ""
echo "✓ Left camera monocular calibration completed: $LEFT_MONO"
echo ""

# Step 1b: Monocular calibration for right camera
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║  Step 1b: Monocular Calibration - Right Camera                            ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Command: ./calibrate_ds -w $BOARD_WIDTH -h $BOARD_HEIGHT -s $SQUARE_SIZE -d $IMG_DIR -r $RIGHT_PREFIX -e $IMAGE_EXTENSION --mono -o $RIGHT_MONO"
echo ""

if ! ./calibrate_ds \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    --mono \
    -o "$RIGHT_MONO"; then
    echo "Error: Right camera monocular calibration failed"
    exit 1
fi

# Verify that the output file was created and is readable (with retry logic)
if ! verify_file_with_retry "$RIGHT_MONO"; then
    echo "Error: Right camera calibration file was not created or not readable: $RIGHT_MONO"
    echo "Waited up to 1 second for file to become available"
    ls -la "$OUTPUT_DIR" 2>/dev/null || echo "Directory does not exist"
    exit 1
fi

echo ""
echo "✓ Right camera monocular calibration completed: $RIGHT_MONO"
echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         STEP 1 COMPLETED SUCCESSFULLY                                      ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Output files:"
echo "  - Left camera: $LEFT_MONO"
echo "  - Right camera: $RIGHT_MONO"
echo ""
echo "Next step: Run step2_handeye_calibration.sh"
echo ""
