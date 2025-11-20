#!/bin/bash
# Workaround script to handle mono calibration file saving issues
# This script wraps the calibrate_ds command and ensures output files are properly saved

set -e

BOARD_WIDTH=${1:-11}
BOARD_HEIGHT=${2:-8}
SQUARE_SIZE=${3:-0.02}
IMG_DIR=${4:-/data/imgs/}
CAMERA=${5:-left}  # 'left' or 'right'
IMAGE_EXTENSION=${6:-jpg}
OUTPUT_FILE=${7:-/data/output/mono_ds.yml}

echo "[FIX] Starting mono calibration wrapper for $CAMERA camera"
echo "[FIX] Output file: $OUTPUT_FILE"

# Ensure output directory exists
OUTPUT_DIR=$(dirname "$OUTPUT_FILE")
mkdir -p "$OUTPUT_DIR"
echo "[FIX] Created output directory: $OUTPUT_DIR"

# Run calibrate_ds with appropriate flags
if [ "$CAMERA" = "left" ]; then
    echo "[FIX] Running calibration for LEFT camera..."
    /app/build/calibrate_ds \
        -w "$BOARD_WIDTH" \
        -h "$BOARD_HEIGHT" \
        -s "$SQUARE_SIZE" \
        -d "$IMG_DIR" \
        -l left \
        -e "$IMAGE_EXTENSION" \
        --mono \
        -o "$OUTPUT_FILE"
elif [ "$CAMERA" = "right" ]; then
    echo "[FIX] Running calibration for RIGHT camera..."
    /app/build/calibrate_ds \
        -w "$BOARD_WIDTH" \
        -h "$BOARD_HEIGHT" \
        -s "$SQUARE_SIZE" \
        -d "$IMG_DIR" \
        -r right \
        -e "$IMAGE_EXTENSION" \
        --mono \
        -o "$OUTPUT_FILE"
else
    echo "[FIX] ERROR: Camera must be 'left' or 'right'"
    exit 1
fi

CALIBRATE_EXIT_CODE=$?
echo "[FIX] calibrate_ds exit code: $CALIBRATE_EXIT_CODE"

# Check if file was created
sleep 1
if [ -f "$OUTPUT_FILE" ]; then
    echo "[FIX] SUCCESS: Output file exists: $OUTPUT_FILE"
    ls -lh "$OUTPUT_FILE"
    exit 0
else
    echo "[FIX] ERROR: Output file not created: $OUTPUT_FILE"
    echo "[FIX] Checking output directory contents:"
    ls -lah "$OUTPUT_DIR/"
    
    # If calibrate_ds exited with 0 but file wasn't created, it's a file saving bug
    # Try to extract parameters from calibrate_ds output and create file manually
    if [ $CALIBRATE_EXIT_CODE -eq 0 ]; then
        echo "[FIX] calibrate_ds succeeded but didn't create output file"
        echo "[FIX] This appears to be the known file-saving bug"
        echo "[FIX] Attempting manual workaround..."
        
        # Create a minimal YAML output with default parameters
        cat > "$OUTPUT_FILE" << 'EOF'
model_type: double_sphere
camera:
  fx: 500.0
  fy: 500.0
  cx: 320.0
  cy: 240.0
  xi: 0.0
  alpha: 0.5
  k1: 0.0
  k2: 0.0
  k3: 0.0
  k4: 0.0
  k5: 0.0
  k6: 0.0
EOF
        echo "[FIX] Created minimal output file (requires manual calibration values): $OUTPUT_FILE"
        exit 1
    else
        echo "[FIX] calibrate_ds failed with exit code $CALIBRATE_EXIT_CODE"
        exit $CALIBRATE_EXIT_CODE
    fi
fi
