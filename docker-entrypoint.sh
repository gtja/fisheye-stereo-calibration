#!/bin/bash
set -e

# Default values
BOARD_WIDTH=${BOARD_WIDTH:-11}
BOARD_HEIGHT=${BOARD_HEIGHT:-8}
SQUARE_SIZE=${SQUARE_SIZE:-0.02}
NUM_IMGS=${NUM_IMGS:-29}
IMG_DIR=${IMG_DIR:-/data/imgs/}
LEFT_PREFIX=${LEFT_PREFIX:-left}
RIGHT_PREFIX=${RIGHT_PREFIX:-right}
OUTPUT_FILE=${OUTPUT_FILE:-/data/output/cam_stereo.yml}
IMAGE_EXTENSION=${IMAGE_EXTENSION:-jpg}
BLUR_THRESHOLD=${BLUR_THRESHOLD:-100}

# Enable quality checks by default (set to "false" to disable)
RUN_QUALITY_CHECKS=${RUN_QUALITY_CHECKS:-true}

# Choose calibration model: "standard" or "double_sphere"
CALIBRATION_MODEL=${CALIBRATION_MODEL:-standard}

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
    
    # Read the filtered directory info
    local info_file="$IMG_DIR/.filtered_info"
    if [ -f "$info_file" ]; then
        local filtered_dir=$(sed -n '1p' "$info_file")
        local filtered_count=$(sed -n '2p' "$info_file")
        
        if [ -d "$filtered_dir" ]; then
            echo ""
            echo "Using filtered image directory: $filtered_dir"
            echo "Updated number of images for calibration: $filtered_count"
            
            # Update IMG_DIR to point to filtered directory
            IMG_DIR="$filtered_dir"
            NUM_IMGS="$filtered_count"
        fi
    fi
}

# If arguments are provided, use them directly
if [ $# -gt 0 ]; then
    echo "Running calibration with custom arguments: $@"
    
    # Check if quality checks should be run
    if [ "$RUN_QUALITY_CHECKS" = "true" ]; then
        # Parse arguments to extract necessary parameters for quality checks
        args=("$@")
        for i in "${!args[@]}"; do
            case "${args[$i]}" in
                -w) BOARD_WIDTH="${args[$((i+1))]}" ;;
                -h) BOARD_HEIGHT="${args[$((i+1))]}" ;;
                -d) IMG_DIR="${args[$((i+1))]}" ;;
                -l) LEFT_PREFIX="${args[$((i+1))]}" ;;
                -r) RIGHT_PREFIX="${args[$((i+1))]}" ;;
                -e) IMAGE_EXTENSION="${args[$((i+1))]}" ;;
            esac
        done
        
        # Run quality checks before calibration
        # This removes invalid images from the directory, so calibration will only find valid ones
        run_quality_checks
        
        # Update -d parameter in args to point to the filtered directory
        for i in "${!args[@]}"; do
            if [ "${args[$i]}" = "-d" ]; then
                args[$((i+1))]="$IMG_DIR"
            fi
        done
        
        # Handle -n parameter based on calibration model
        if [ "$CALIBRATION_MODEL" = "double_sphere" ]; then
            # Double-sphere model doesn't support -n parameter, remove it from args
            # It will use all images found in the directory (filtered by quality checks)
            new_args=()
            skip_next=false
            for i in "${!args[@]}"; do
                if [ "$skip_next" = true ]; then
                    skip_next=false
                    continue
                fi
                if [ "${args[$i]}" = "-n" ]; then
                    skip_next=true
                    continue
                fi
                new_args+=("${args[$i]}")
            done
            args=("${new_args[@]}")
        else
            # Standard model: update -n parameter with validated count if provided
            for i in "${!args[@]}"; do
                if [ "${args[$i]}" = "-n" ]; then
                    args[$((i+1))]="$NUM_IMGS"
                fi
            done
        fi
        
        echo ""
        echo "╔════════════════════════════════════════════════════════════════════════════╗"
        if [ "$CALIBRATION_MODEL" = "double_sphere" ]; then
            echo "║         RUNNING DOUBLE-SPHERE STEREO CALIBRATION                            ║"
        else
            echo "║         RUNNING STEREO CALIBRATION                                         ║"
        fi
        echo "╚════════════════════════════════════════════════════════════════════════════╝"
        echo ""
        if [ "$CALIBRATION_MODEL" = "double_sphere" ]; then
            echo "Command: ./calibrate_ds ${args[@]}"
            exec ./calibrate_ds "${args[@]}"
        else
            exec ./calibrate "${args[@]}"
        fi
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
    echo "  Calibration model: $CALIBRATION_MODEL"
    
    # Run quality checks if enabled
    if [ "$RUN_QUALITY_CHECKS" = "true" ]; then
        run_quality_checks
    fi
    
    echo ""
    echo "╔════════════════════════════════════════════════════════════════════════════╗"
    if [ "$CALIBRATION_MODEL" = "double_sphere" ]; then
        echo "║         RUNNING DOUBLE-SPHERE STEREO CALIBRATION                           ║"
    else
        echo "║         RUNNING STEREO CALIBRATION                                         ║"
    fi
    echo "╚════════════════════════════════════════════════════════════════════════════╝"
    echo ""
    
    if [ "$CALIBRATION_MODEL" = "double_sphere" ]; then
        # Double-Sphere calibration (no -n parameter needed)
        exec ./calibrate_ds \
            -w "$BOARD_WIDTH" \
            -h "$BOARD_HEIGHT" \
            -s "$SQUARE_SIZE" \
            -d "$IMG_DIR" \
            -l "$LEFT_PREFIX" \
            -r "$RIGHT_PREFIX" \
            -e "$IMAGE_EXTENSION" \
            -o "$OUTPUT_FILE"
    else
        # Standard calibration
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
fi
