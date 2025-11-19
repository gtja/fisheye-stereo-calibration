#!/bin/bash

# Test script to demonstrate the shell retry logic fix
# This script shows how the verify_file_with_retry function handles various scenarios

set -e

echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║  Shell Script Retry Logic Test                                            ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""

# Source the verify_file_with_retry function from docker-entrypoint.sh
# Extract just the function definition
eval "$(sed -n '/^verify_file_with_retry()/,/^}/p' docker-entrypoint.sh)"

echo "Test 1: Immediate file availability (normal case)"
echo "------------------------------------------------"
TEST_FILE="/tmp/test_immediate.yml"
echo "test data" > "$TEST_FILE"
echo "Creating file: $TEST_FILE"
if verify_file_with_retry "$TEST_FILE"; then
    echo "✓ SUCCESS: File found immediately (typical case)"
    echo "  This is what happens when filesystem is fast and responsive"
else
    echo "✗ FAILED: Should have found the file"
    exit 1
fi
rm -f "$TEST_FILE"
echo ""

echo "Test 2: Delayed file availability (simulates race condition)"
echo "-------------------------------------------------------------"
TEST_FILE="/tmp/test_delayed.yml"
echo "Simulating filesystem delay..."
(sleep 0.4 && echo "test data" > "$TEST_FILE" && echo "  [Background] File created after 400ms delay") &
if verify_file_with_retry "$TEST_FILE"; then
    echo "✓ SUCCESS: File found after delay"
    echo "  This demonstrates the fix handling the race condition"
    echo "  Without retry logic, this would have failed"
else
    echo "✗ FAILED: Should have found the file after delay"
    exit 1
fi
rm -f "$TEST_FILE"
echo ""

echo "Test 3: File never created (error case)"
echo "----------------------------------------"
TEST_FILE="/tmp/nonexistent.yml"
echo "Attempting to verify nonexistent file: $TEST_FILE"
if verify_file_with_retry "$TEST_FILE"; then
    echo "✗ FAILED: Should have detected missing file"
    exit 1
else
    echo "✓ SUCCESS: Correctly detected that file was not created"
    echo "  Error message would be shown to user in real scenario"
fi
echo ""

echo "Test 4: File created but unreadable (permission issue)"
echo "-------------------------------------------------------"
TEST_FILE="/tmp/test_unreadable.yml"
echo "test data" > "$TEST_FILE"
chmod 000 "$TEST_FILE"
echo "Created file with no read permissions: $TEST_FILE"
if verify_file_with_retry "$TEST_FILE"; then
    echo "✗ FAILED: Should have detected unreadable file"
    chmod 644 "$TEST_FILE"
    rm -f "$TEST_FILE"
    exit 1
else
    echo "✓ SUCCESS: Correctly detected that file is not readable"
    echo "  This ensures file is fully accessible, not just present"
    chmod 644 "$TEST_FILE"
    rm -f "$TEST_FILE"
fi
echo ""

echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║  All Tests Passed!                                                         ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Summary:"
echo "--------"
echo "✓ Handles immediate file availability (fast path)"
echo "✓ Handles delayed file visibility (race condition fix)"
echo "✓ Correctly reports when file is not created"
echo "✓ Verifies file is readable, not just present"
echo ""
echo "This fix ensures reliable file verification in the monocular calibration"
echo "workflow, eliminating spurious 'file was not created' errors."
