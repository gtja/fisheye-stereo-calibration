#!/bin/bash
set -e

# Example script to run the complete three-step calibration workflow
# This script runs all three steps in sequence with common parameters

echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         THREE-STEP HAND-EYE CALIBRATION WORKFLOW                           ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "This script will run the complete three-step calibration workflow:"
echo "  1. Monocular calibration (left and right cameras)"
echo "  2. Hand-eye calibration"
echo "  3. Stereo bundle adjustment"
echo ""

# Set common parameters (can be overridden by environment variables)
export BOARD_WIDTH=${BOARD_WIDTH:-11}
export BOARD_HEIGHT=${BOARD_HEIGHT:-8}
export SQUARE_SIZE=${SQUARE_SIZE:-0.02}
export IMG_DIR=${IMG_DIR:-/data/imgs/}
export LEFT_PREFIX=${LEFT_PREFIX:-left}
export RIGHT_PREFIX=${RIGHT_PREFIX:-right}
export IMAGE_EXTENSION=${IMAGE_EXTENSION:-jpg}
export OUTPUT_DIR=${OUTPUT_DIR:-/data/output}
export OUTPUT_FILE=${OUTPUT_FILE:-${OUTPUT_DIR}/cam_stereo.yml}

echo "Parameters:"
echo "  Board width: $BOARD_WIDTH"
echo "  Board height: $BOARD_HEIGHT"
echo "  Square size: $SQUARE_SIZE"
echo "  Image directory: $IMG_DIR"
echo "  Left prefix: $LEFT_PREFIX"
echo "  Right prefix: $RIGHT_PREFIX"
echo "  Image extension: $IMAGE_EXTENSION"
echo "  Output directory: $OUTPUT_DIR"
echo "  Final output file: $OUTPUT_FILE"
echo ""
echo "Press Enter to start, or Ctrl+C to cancel..."
read -r

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Step 1: Monocular calibration
echo ""
echo "════════════════════════════════════════════════════════════════════════════"
echo "Running Step 1: Monocular Calibration"
echo "════════════════════════════════════════════════════════════════════════════"
"$SCRIPT_DIR/step1_monocular_calibration.sh"

# Step 2: Hand-eye calibration
echo ""
echo "════════════════════════════════════════════════════════════════════════════"
echo "Running Step 2: Hand-Eye Calibration"
echo "════════════════════════════════════════════════════════════════════════════"
"$SCRIPT_DIR/step2_handeye_calibration.sh"

# Step 3: Stereo bundle adjustment
echo ""
echo "════════════════════════════════════════════════════════════════════════════"
echo "Running Step 3: Stereo Bundle Adjustment"
echo "════════════════════════════════════════════════════════════════════════════"
"$SCRIPT_DIR/step3_stereo_bundle_adjustment.sh"

echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║         ALL THREE STEPS COMPLETED SUCCESSFULLY!                            ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Calibration complete! Output files:"
echo "  - Monocular calibrations: ${OUTPUT_DIR}/left_ds.yml, ${OUTPUT_DIR}/right_ds.yml"
echo "  - Hand-eye calibration: ${OUTPUT_DIR}/handeye.yml"
echo "  - Final stereo calibration: $OUTPUT_FILE"
echo ""
