# Mono Calibration File Creation Issue - Debug Guide

## Problem Summary
When running mono calibration with the `--mono` flag, the program prints "KB4 fisheye calibration succeeded for left camera" but doesn't create the output file `/data/output/left_ds.yml`.

## Expected Behavior
The program should:
1. Perform KB4 fisheye calibration for the left camera
2. Print success message
3. Create and save the calibration file to `/data/output/left_ds.yml`
4. Return exit code 0

## Current Behavior
The program:
1. Performs KB4 fisheye calibration for the left camera ✓
2. Prints success message ✓
3. **FAILS** to create the calibration file ✗
4. Returns exit code 0 (according to shell script check) ✓

## Debug Logging Added
Extensive debug logging has been added to trace the program execution:

### Expected Debug Output Sequence
```
Step 1: KB4 Coarse Calibration (initial guess)...
Initial calibration: 19 image pairs
Attempting fisheye::calibrate for left camera with 19 images...
KB4 fisheye calibration succeeded for left camera
[DEBUG] About to set K2_kb4 = K1_kb4
[DEBUG] Successfully set K2_kb4 and D2_kb4
[DEBUG] Exiting mono_mode block, mono_mode = 1
[DEBUG] Reached line 801, fisheye_success = 1
KB4 calibration complete
[DEBUG] About to print camera parameters
  Left camera: fx=XXX, fy=XXX, cx=XXX, cy=XXX
[DEBUG] Finished printing camera parameters
[DEBUG] Creating KB4 parameters
[DEBUG] Created kb4_left
[DEBUG] Created kb4_right
[DEBUG] mono_mode = 1, is_left_only = 1, is_right_only = 0
[DEBUG] Entering mono mode file save section
[DEBUG] DS params initialized: fx=XXX, fy=XXX, cx=XXX, cy=XXX
[DEBUG] Ensuring output directory exists for: /data/output/left_ds.yml
[DEBUG] Opening file for writing
[DEBUG] Releasing FileStorage
[DEBUG] Syncing filesystem
[DEBUG] Starting file verification
[DEBUG] File verified on retry X
Mono calibration saved successfully to /data/output/left_ds.yml
```

## Troubleshooting Steps

### Step 1: Rebuild Docker Image
```bash
docker build -t fisheye-stereo-calibration .
```

### Step 2: Run Calibration with Debug Output
```bash
docker run --rm \
  -v /path/to/images:/data/imgs \
  -v /path/to/output:/data/output \
  -e CALIBRATION_WORKFLOW=hand_eye \
  -e CALIBRATION_MODEL=double_sphere \
  fisheye-stereo-calibration
```

### Step 3: Analyze Debug Output
Compare the actual output with the expected sequence above. The program execution stops at the LAST debug message printed.

## Possible Root Causes

### 1. Program Crashes Before File Save Section
**Symptom**: Debug messages stop before "[DEBUG] Entering mono mode file save section"

**Possible causes**:
- Segmentation fault when accessing K1_kb4 or K2_kb4
- Exception in KB4Params constructor
- Out of memory

**Investigation**: Check where exactly debug messages stop

### 2. Mono Mode Flag Not Set Correctly
**Symptom**: Debug shows `mono_mode = 0` when it should be `1`

**Possible causes**:
- Command line parsing issue with `--mono` flag
- Variable corruption

**Investigation**: Check the value of `mono_mode` in debug output at line 842

### 3. File Creation Fails Silently
**Symptom**: Debug messages reach file creation section but file doesn't exist

**Possible causes**:
- Directory permissions
- Disk space
- FileStorage fails to write

**Investigation**: Check if "[DEBUG] File verified on retry X" appears

### 4. Container/Mount Issue
**Symptom**: File is created in container but not visible on host

**Possible causes**:
- Volume mount not working correctly
- File created in wrong location

**Investigation**: 
- Check the exact path passed to `-o` flag
- Verify volume mounts with `docker inspect`
- Try `docker exec` into running container to check filesystem

## Code Flow for Mono Left Calibration

```
main()
  ├─ Parse command line arguments (--mono flag sets mono_mode = 1)
  ├─ Set is_left_only = true (only -l flag provided)
  ├─ Load images and detect chessboard corners
  ├─ if (mono_mode) {              // Line 629
  │    ├─ if (is_left_only) {      // Line 631
  │    │    ├─ fisheye::calibrate() for left camera
  │    │    ├─ Set fisheye_success = true
  │    │    ├─ Print success message      // Line 640
  │    │    └─ Set K2_kb4 = K1_kb4        // Line 645
  │    │  }
  │    └─ Exit mono_mode block            // Line 673
  │  }
  ├─ Print calibration complete           // Line 808-809
  ├─ Print camera parameters              // Line 820-825
  ├─ Create KB4Params objects             // Line 835-838
  ├─ if (mono_mode) {                     // Line 846
  │    ├─ Initialize DS parameters
  │    ├─ Ensure output directory exists
  │    ├─ Write to FileStorage
  │    ├─ Verify file exists
  │    └─ return 0                        // Line 959
  │  }
  └─ ... (stereo calibration continues if not mono mode)
```

## Next Steps

1. Rebuild Docker image with debug code
2. Run calibration and capture FULL output
3. Identify the LAST debug message that prints
4. Based on where execution stops, implement the appropriate fix

## Files Modified
- `calibrate_ds.cpp`: Added debug logging throughout mono calibration path
