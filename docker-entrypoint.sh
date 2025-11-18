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
IMAGE_EXTENSION=${IMAGE_EXTENSION:-jpg}
BLUR_THRESHOLD=${BLUR_THRESHOLD:-100}

# Enable quality checks by default (set to "false" to disable)
RUN_QUALITY_CHECKS=${RUN_QUALITY_CHECKS:-true}

# Create output directory if it doesn't exist
mkdir -p "$(dirname "$OUTPUT_FILE")"

# Function to run quality checks
run_quality_checks() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════════════════╗"
    echo "║         PRE-CALIBRATION IMAGE QUALITY CHECKS                               ║"
    echo "╚════════════════════════════════════════════════════════════════════════════╝"
    echo ""
    
    python3 /app/utils/pre_calibration_check.py "$IMG_DIR" \
        --width "$BOARD_WIDTH" \
        --height "$BOARD_HEIGHT" \
        --blur-threshold "$BLUR_THRESHOLD" \
        --extension "$IMAGE_EXTENSION" \
        --left-prefix "$LEFT_PREFIX" \
        --right-prefix "$RIGHT_PREFIX"
    
    local exit_code=$?
    
    if [ $exit_code -ne 0 ]; then
        echo "Error: Quality checks failed. Please review the images."
        exit $exit_code
    fi
    
    # Read the validated image count
    local count_file="$IMG_DIR/.valid_image_count"
    if [ -f "$count_file" ]; then
        NUM_IMGS=$(cat "$count_file")
        echo ""
        echo "Updated number of images for calibration: $NUM_IMGS"
    fi
}

# If arguments are provided, use them directly
if [ $# -gt 0 ]; then
    echo "Running calibration with custom arguments: $@"
    
    # Check if quality checks should be run
    if [ "$RUN_QUALITY_CHECKS" = "true" ]; then
        # Parse arguments to extract necessary parameters for quality checks
        # Run quality checks before calibration
        run_quality_checks
        
        # If -n parameter was provided, override with validated count
        args=("$@")
        for i in "${!args[@]}"; do
            if [ "${args[$i]}" = "-n" ]; then
                args[$((i+1))]="$NUM_IMGS"
            fi
        done
        
        echo ""
        echo "╔════════════════════════════════════════════════════════════════════════════╗"
        echo "║         RUNNING STEREO CALIBRATION                                         ║"
        echo "╚════════════════════════════════════════════════════════════════════════════╝"
        echo ""
        exec ./calibrate "${args[@]}"
    else
        exec ./calibrate "$@"
    fi
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
    echo "  Image extension: $IMAGE_EXTENSION"
    echo "  Output file: $OUTPUT_FILE"
    echo "  Quality checks: $RUN_QUALITY_CHECKS"
    echo "  Blur threshold: $BLUR_THRESHOLD"
    
    # Run quality checks if enabled
    if [ "$RUN_QUALITY_CHECKS" = "true" ]; then
        run_quality_checks
    fi
    
    echo ""
    echo "╔════════════════════════════════════════════════════════════════════════════╗"
    echo "║         RUNNING STEREO CALIBRATION                                         ║"
    echo "╚════════════════════════════════════════════════════════════════════════════╝"
    echo ""
    
    exec ./calibrate \
        -w "$BOARD_WIDTH" \
        -h "$BOARD_HEIGHT" \
        -s "$SQUARE_SIZE" \
        -n "$NUM_IMGS" \
        -d "$IMG_DIR" \
        -l "$LEFT_PREFIX" \
        -r "$RIGHT_PREFIX" \
        -e "$IMAGE_EXTENSION" \
        -o "$OUTPUT_FILE"
fi
