#!/bin/bash
set -e

# Step 2: Hand-Eye Calibration
# This script performs hand-eye calibration using the monocular calibration results
# from step 1 to compute the stereo extrinsics (T_right_left).

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

# Input files (from step 1)
LEFT_MONO="${OUTPUT_DIR}/left_ds.yml"
RIGHT_MONO="${OUTPUT_DIR}/right_ds.yml"

# Output file
HANDEYE_FILE="${OUTPUT_DIR}/handeye.yml"

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
echo "║         STEP 2: HAND-EYE CALIBRATION                                       ║"
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
echo "  Left mono calibration: $LEFT_MONO"
echo "  Right mono calibration: $RIGHT_MONO"
echo "  Output file: $HANDEYE_FILE"
echo ""

# Check if input files exist
if [ ! -f "$LEFT_MONO" ]; then
    echo "Error: Left camera monocular calibration file not found: $LEFT_MONO"
    echo "Please run step1_monocular_calibration.sh first"
    exit 1
fi

if [ ! -f "$RIGHT_MONO" ]; then
    echo "Error: Right camera monocular calibration file not found: $RIGHT_MONO"
    echo "Please run step1_monocular_calibration.sh first"
    exit 1
fi

echo "Running hand-eye calibration..."
echo "Command: ./compute_handeye -w $BOARD_WIDTH -h $BOARD_HEIGHT -s $SQUARE_SIZE -d $IMG_DIR -l $LEFT_PREFIX -r $RIGHT_PREFIX -e $IMAGE_EXTENSION -L $LEFT_MONO -R $RIGHT_MONO -o $HANDEYE_FILE"
echo ""

if ! ./compute_handeye \
    -w "$BOARD_WIDTH" \
    -h "$BOARD_HEIGHT" \
    -s "$SQUARE_SIZE" \
    -d "$IMG_DIR" \
    -l "$LEFT_PREFIX" \
    -r "$RIGHT_PREFIX" \
    -e "$IMAGE_EXTENSION" \
    -L "$LEFT_MONO" \
    -R "$RIGHT_MONO" \
    -o "$HANDEYE_FILE"; then
    echo "Error: Hand-eye calibration failed"
    exit 1
fi

# Verify that the output file was created and is readable (with retry logic)
if ! verify_file_with_retry "$HANDEYE_FILE"; then
    echo "Error: Hand-eye calibration file was not created or not readable: $HANDEYE_FILE"
    echo "Waited up to 1 second for file to become available"
    ls -la "$OUTPUT_DIR" 2>/dev/null || echo "Directory does not exist"
    exit 1
fi

echo ""
echo "✓ Hand-eye calibration completed: $HANDEYE_FILE"
echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         STEP 2 COMPLETED SUCCESSFULLY                                      ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Output file: $HANDEYE_FILE"
echo ""
echo "Next step: Run step3_stereo_bundle_adjustment.sh"
echo ""
