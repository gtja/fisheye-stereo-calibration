# Testing File Sync Fix for left_ds.yml Creation Issue

## Problem Summary

When running monocular calibration in Double-Sphere mode, the program would successfully write the output file (e.g., `left_ds.yml`) but the shell script would sometimes report that the file was not created. This was caused by a race condition where:

1. The C++ code writes the file using OpenCV's FileStorage
2. The C++ code calls `sync()` to flush filesystem buffers (asynchronous)
3. The C++ code returns immediately
4. The shell script checks if the file exists
5. The file may not be visible yet due to delayed filesystem sync

## Solution

Added explicit file verification with retry logic after writing files in:
- `calibrate_ds.cpp` - monocular calibration output (line ~883)
- `calibrate_ds.cpp` - stereo calibration output (line ~1815)
- `compute_handeye.cpp` - hand-eye calibration output (line ~415)

The verification:
1. Keeps the original `sync()` call for filesystem flush
2. Uses `access()` to verify the file exists and is readable
3. Retries up to 10 times with 10ms delays (100ms total max wait)
4. Returns an error if the file cannot be verified

## How to Test

### Prerequisites
- Docker installed
- Sample calibration images available

### Build the Docker Image

```bash
cd /home/runner/work/fisheye-stereo-calibration/fisheye-stereo-calibration
docker build -t fisheye-stereo-calibration .
```

### Test 1: Monocular Calibration (Left Camera)

```bash
docker run -e CALIBRATION_MODEL=double_sphere \
  -v $(pwd)/imgs2:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  -w 11 -h 8 -s 0.025 \
  -d /data/imgs/ \
  -l left \
  -e bmp \
  --mono \
  -o /data/output/left_ds.yml
```

**Expected Result:**
- The command completes successfully
- File `/data/output/left_ds.yml` is created
- Console shows: "Mono calibration saved successfully to /data/output/left_ds.yml"
- No error: "Error: Left camera calibration file was not created"

### Test 2: Monocular Calibration (Right Camera)

```bash
docker run -e CALIBRATION_MODEL=double_sphere \
  -v $(pwd)/imgs2:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  -w 11 -h 8 -s 0.025 \
  -d /data/imgs/ \
  -r right \
  -e bmp \
  --mono \
  -o /data/output/right_ds.yml
```

**Expected Result:**
- The command completes successfully
- File `/data/output/right_ds.yml` is created
- Console shows: "Mono calibration saved successfully to /data/output/right_ds.yml"
- No error: "Error: Right camera calibration file was not created"

### Test 3: Hand-Eye Workflow (Complete 3-Step Process)

This is the most comprehensive test as it exercises all three file write locations:

```bash
docker run -e CALIBRATION_MODEL=double_sphere \
  -e CALIBRATION_WORKFLOW=hand_eye \
  -v $(pwd)/imgs2:/data/imgs \
  -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration \
  -w 11 -h 8 -s 0.025 \
  -d /data/imgs/ \
  -l left -r right \
  -e bmp \
  -o /data/output/cam_stereo.yml
```

**Expected Result:**
- Step 1a: Left monocular calibration succeeds, creates `/data/output/left_ds.yml`
- Step 1b: Right monocular calibration succeeds, creates `/data/output/right_ds.yml`
- Step 2: Hand-eye calibration succeeds, creates `/data/output/handeye.yml`
- Step 3: Stereo bundle adjustment succeeds, creates `/data/output/cam_stereo.yml`
- No file creation errors at any step
- All four output files exist and are readable

### Test 4: Verify File Contents

After any of the above tests, verify the output files are valid YAML:

```bash
# Check file exists and is not empty
ls -lh output/left_ds.yml

# Display file contents (should be valid YAML)
cat output/left_ds.yml

# Verify YAML structure
python3 -c "import yaml; print(yaml.safe_load(open('output/left_ds.yml')))"
```

**Expected Result:**
- File exists with non-zero size
- Contains valid YAML structure
- Has expected keys: `model_type`, `camera` with calibration parameters

## Success Criteria

The fix is successful if:
1. ✅ All monocular calibration tests complete without "file was not created" errors
2. ✅ Hand-eye workflow completes all 3 steps successfully
3. ✅ All output files are created and readable
4. ✅ Output files contain valid YAML with expected structure
5. ✅ No timing-related failures occur even under high system load

## Additional Stress Testing

To ensure the fix handles various system conditions:

```bash
# Run the test multiple times in succession
for i in {1..10}; do
  echo "Test iteration $i"
  docker run -e CALIBRATION_MODEL=double_sphere \
    -v $(pwd)/imgs2:/data/imgs \
    -v $(pwd)/output:/data/output \
    fisheye-stereo-calibration \
    -w 11 -h 8 -s 0.025 \
    -d /data/imgs/ \
    -l left \
    -e bmp \
    --mono \
    -o /data/output/left_ds_$i.yml
  if [ $? -ne 0 ]; then
    echo "FAILED at iteration $i"
    exit 1
  fi
done
echo "All iterations successful!"
```

## Rollback Plan

If the fix causes issues:
1. Revert to original code (without file verification loop)
2. Increase delay in shell script instead (add `sleep 1` after calibration)
3. Alternative: Use `sync; sync; sync` in shell script (triple sync)

## Notes

- The 10ms retry interval and 10 retries (100ms max) should be sufficient for most filesystems
- Modern SSDs typically flush within 1-10ms
- Network filesystems may need longer delays, but the retry logic handles this
- The fix is defensive programming and adds minimal overhead (~10ms in worst case)
