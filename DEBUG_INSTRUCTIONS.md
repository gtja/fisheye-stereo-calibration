# Debug Instructions for Mono Calibration Issue

## Quick Start

Your mono calibration is failing to create `/data/output/left_ds.yml` even though it reports success. I've added extensive debug logging to identify exactly where the problem occurs.

## What You Need to Do

### 1. Rebuild Docker Image
```bash
docker build -t fisheye-stereo-calibration:debug .
```

### 2. Run Test
```bash
# Option A: Use the minimal test script (recommended)
docker run --rm \
  -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration:debug \
  /app/build/test_mono_minimal.sh 2>&1 | tee debug_output.log

# Option B: Run full hand-eye workflow
docker run --rm \
  -e CALIBRATION_WORKFLOW=hand_eye \
  -e CALIBRATION_MODEL=double_sphere \
  -v $(pwd)/imgs:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration:debug \
  2>&1 | tee debug_output.log
```

### 3. Share the Output
Please share the complete contents of `debug_output.log` or paste the terminal output.

## What to Look For

The debug output will contain messages like:
```
[DEBUG] About to set K2_kb4 = K1_kb4
[DEBUG] Successfully set K2_kb4 and D2_kb4
[DEBUG] Exiting mono_mode block, mono_mode = 1
[DEBUG] Reached line 801, fisheye_success = 1
[DEBUG] About to print camera parameters
... etc ...
[DEBUG] Entering mono mode file save section
```

**The LAST `[DEBUG]` message you see will tell us exactly where the program stops.**

## Common Scenarios

### Scenario 1: No Debug Messages at All
```
KB4 fisheye calibration succeeded for left camera
Error: Left camera calibration file was not created
```
**Diagnosis**: Program crashes immediately after line 641  
**Likely cause**: Memory corruption, segfault in variable assignment

### Scenario 2: Some Debug Messages, Then Stops
```
KB4 fisheye calibration succeeded for left camera
[DEBUG] About to set K2_kb4 = K1_kb4
Error: Left camera calibration file was not created
```
**Diagnosis**: Program crashes at line 645 (K2_kb4 = K1_kb4)  
**Likely cause**: Matrix assignment issue, memory problem

### Scenario 3: Many Debug Messages, Stops Before File Save
```
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
Error: Left camera calibration file was not created
```
**Diagnosis**: Program crashes during KB4Params construction  
**Likely cause**: Issue in kb4_model.h constructor

### Scenario 4: Reaches File Save Section But No File
```
... (all previous debug messages) ...
[DEBUG] mono_mode = 1, is_left_only = 1, is_right_only = 0
[DEBUG] Entering mono mode file save section
[DEBUG] DS params initialized: fx=XXX, fy=XXX, cx=XXX, cy=XXX
[DEBUG] Ensuring output directory exists for: /data/output/left_ds.yml
Error: Left camera calibration file was not created
```
**Diagnosis**: File creation fails  
**Likely cause**: Directory permissions, disk space, FileStorage issue

### Scenario 5: All Debug Messages, File Created, But Shell Can't Find It
```
... (all previous debug messages) ...
[DEBUG] File verified on retry 0
Mono calibration saved successfully to /data/output/left_ds.yml
Error: Left camera calibration file was not created
```
**Diagnosis**: File exists in container but not on host  
**Likely cause**: Volume mount issue, wrong path

## Additional Information to Provide

Along with the debug output, please also share:

1. **Docker version**:
   ```bash
   docker --version
   ```

2. **Image list size**:
   ```bash
   ls -l /path/to/images/left*.bmp | wc -l
   ```

3. **Output directory permissions**:
   ```bash
   ls -lah /path/to/output/
   ```

4. **Disk space**:
   ```bash
   df -h /path/to/output/
   ```

5. **System info** (if possible):
   ```bash
   uname -a
   ```

## What Happens Next

Once I see your debug output:
1. I'll identify the exact line where execution stops
2. I'll determine the root cause
3. I'll implement a targeted fix
4. You'll rebuild and test
5. If successful, I'll clean up the debug code
6. PR will be ready for merge

## Questions?

If anything is unclear or if you encounter any issues running the test, please let me know!

## Files in This PR

- `calibrate_ds.cpp`: Added 27 debug statements
- `MONO_CALIBRATION_DEBUG.md`: Detailed troubleshooting guide
- `test_mono_minimal.sh`: Isolated test script
- `DEBUG_INSTRUCTIONS.md`: This file

---

**Expected turnaround**: Once you provide the debug output, I can usually identify the issue and implement a fix within 1-2 hours.
