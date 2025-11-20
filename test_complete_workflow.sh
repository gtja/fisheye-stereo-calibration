#!/bin/bash

# Complete workflow test script
# This script tests the full three-step calibration workflow

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Create test output directory
mkdir -p output

echo "=========================================="
echo "Testing Complete Calibration Workflow"
echo "=========================================="
echo ""

# # Check if Docker image exists
# if ! docker image inspect fisheye-stereo-calibration &>/dev/null; then
#     echo "[INFO] Building Docker image..."
#     docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .
# fi
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .

# Test Step 1: Monocular Calibration
echo "[STEP 1] Running monocular calibration..."
echo "Command: ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output"
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Verify Step 1 outputs
echo ""
echo "[CHECK] Verifying Step 1 outputs..."
if [ -f output/left_ds.yml ]; then
    echo "✓ left_ds.yml generated"
    ls -lh output/left_ds.yml
else
    echo "✗ ERROR: left_ds.yml not found"
    exit 1
fi

if [ -f output/right_ds.yml ]; then
    echo "✓ right_ds.yml generated"
    ls -lh output/right_ds.yml
else
    echo "✗ ERROR: right_ds.yml not found"
    exit 1
fi

# Test Step 2: Hand-Eye Calibration
echo ""
echo "[STEP 2] Running hand-eye calibration..."
echo "Command: ./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output"
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Verify Step 2 outputs
echo ""
echo "[CHECK] Verifying Step 2 outputs..."
if [ -f output/handeye.yml ]; then
    echo "✓ handeye.yml generated"
    ls -lh output/handeye.yml
else
    echo "✗ ERROR: handeye.yml not found"
    exit 1
fi

# Test Step 3: Stereo Bundle Adjustment
echo ""
echo "[STEP 3] Running stereo bundle adjustment..."
echo "Command: ./step3_stereo_bundle_adjustment.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output"
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step3_stereo_bundle_adjustment.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Verify Step 3 outputs
echo ""
echo "[CHECK] Verifying Step 3 outputs..."
if [ -f output/cam_stereo.yml ]; then
    echo "✓ cam_stereo.yml generated"
    ls -lh output/cam_stereo.yml
else
    echo "✗ ERROR: cam_stereo.yml not found"
    exit 1
fi

# Summary
echo ""
echo "=========================================="
echo "✓ Workflow Test PASSED!"
echo "=========================================="
echo ""
echo "Generated calibration files:"
ls -lh output/*.yml
echo ""
echo "All three-step calibration workflow completed successfully!"
