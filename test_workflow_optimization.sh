#!/bin/bash

# Test script to verify the workflow optimization
# This script checks that image quality checks are only run once in step 1
# and skipped in steps 2 and 3 to avoid redundant execution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Create test output directory
mkdir -p output
LOG_FILE="/tmp/workflow_test.log"

echo "=========================================="
echo "Testing Workflow Optimization"
echo "=========================================="
echo ""
echo "This test verifies that image quality checks"
echo "are only executed once, in Step 1."
echo ""
echo "Logging to: $LOG_FILE"
echo ""

# Run the complete workflow and capture output
timeout 900 bash test_complete_workflow.sh 2>&1 | tee "$LOG_FILE"

echo ""
echo "=========================================="
echo "Analyzing Execution Flow"
echo "=========================================="
echo ""

# Count how many times each major step appears
echo "Step execution count:"
echo "  STEP 1 (Monocular Calibration):"
grep -c "Step 1: BLUR DETECTION" "$LOG_FILE" || echo "    0 executions"

echo "  STEP 2 (Hand-Eye Calibration):"
grep -c "Step 2: BLUR DETECTION" "$LOG_FILE" || echo "    0 executions"

echo "  STEP 3 (Stereo Bundle Adjustment):"
grep -c "Step 3: BLUR DETECTION" "$LOG_FILE" || echo "    0 executions"

echo ""
echo "=========================================="
echo "Checking for --skip-image-checks flag usage:"
echo "=========================================="
echo ""

# Check if step2 uses the flag
if grep -q "step2.*--skip-image-checks\|compute_handeye.*--skip-image-checks" "$LOG_FILE"; then
    echo "✓ Step 2 uses --skip-image-checks flag"
else
    echo "✗ Step 2 does NOT use --skip-image-checks flag"
fi

# Check if step3 uses the flag
if grep -q "step3.*--skip-image-checks\|calibrate_ds.*--skip-image-checks" "$LOG_FILE"; then
    echo "✓ Step 3 uses --skip-image-checks flag"
else
    echo "✗ Step 3 does NOT use --skip-image-checks flag"
fi

echo ""
echo "=========================================="
echo "Detailed Execution Timeline:"
echo "=========================================="
echo ""

# Extract timeline of major events
grep -E "\[STEP|Step [0-9]:|BLUR DETECTION|CORNER DETECTION|THREE-STEP" "$LOG_FILE" | head -30 || true

echo ""
echo "=========================================="
echo "Test Complete"
echo "=========================================="
echo ""
echo "Full log saved to: $LOG_FILE"
echo ""
