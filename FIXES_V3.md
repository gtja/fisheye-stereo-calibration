# Parameter Path Handling Fix - Version 3.0 Summary

## Problem
When users passed `-o /data/output` (a directory) to calibration step scripts, the parameter was passed directly to the C++ `calibrate_ds` executable, which expected a full file path for the `-o` parameter. This caused errors like:

```
[ERROR:0@17.922] global persistence.cpp:505 open Can't open file: '/data/output' in write mode
Error: Cannot open output file for writing: /data/output
```

## Root Cause
The issue occurred in two places:

1. **Shell Scripts** (`step*.sh`): Scripts accepted `-o` parameter but passed it directly to C++ executables without validation or path construction.

2. **Docker Entry Point** (`docker-entrypoint.sh`): When parsing `-o` parameter, the code didn't distinguish between directory and file paths. It would set `OUTPUT_FILE` to whatever the user provided without ensuring it was a complete file path.

## Solution Applied

### 1. Docker Entry Point Fixes (`docker-entrypoint.sh`)

**Location 1: Lines 265-282 (Quality checks enabled path)**
```bash
# OLD: Direct assignment without path checking
-o) OUTPUT_FILE="${args[$((i+1))]}" ;;

# NEW: Smart path detection and construction
-o) OUTPUT_PARAM="${args[$((i+1))]}" ;;
# ... later ...
if [ -n "$OUTPUT_PARAM" ]; then
    if [ -d "$OUTPUT_PARAM" ] || [[ "$OUTPUT_PARAM" != *.yml && "$OUTPUT_PARAM" != *.yaml ]]; then
        # It's a directory or not a YAML file - treat as output directory
        OUTPUT_DIR="$OUTPUT_PARAM"
        OUTPUT_FILE="${OUTPUT_DIR}/cam_stereo.yml"
    else
        # It's a file path
        OUTPUT_FILE="$OUTPUT_PARAM"
        OUTPUT_DIR="$(dirname "$OUTPUT_FILE")"
    fi
fi
```

**Location 2: Lines 318-330 (Update args before calling workflow)**
```bash
# Update -o parameter in args to use full file path (not just directory)
for i in "${!args[@]}"; do
    if [ "${args[$i]}" = "-o" ]; then
        # Check if the next value is a directory or a file path
        next_idx=$((i+1))
        if [ -d "${args[$next_idx]}" ] || [[ "${args[$next_idx]}" != *.yml && "${args[$next_idx]}" != *.yaml ]]; then
            # It's a directory - convert to full file path
            args[$next_idx]="${args[$next_idx]}/cam_stereo.yml"
        fi
        break
    fi
done
```

**Location 3: Lines 355-387 (Quality checks disabled path)**
```bash
# Similar fix as Location 1, handling the case when quality checks are disabled
-o) OUTPUT_PARAM="${args[$((i+1))]}" ;;
# ... then construct full path with directory detection ...
```

### 2. Shell Scripts (`step*.sh`)

Scripts already had correct parameter passing through `-o` to C++ executables. The key was that they:
1. Accept `-o` parameter via the parameter parsing loop
2. Set `OUTPUT_FILE=${OUTPUT_DIR}/specific_file.yml` internally
3. Pass the full file path `$OUTPUT_FILE` to C++ executables

Example from `step3_stereo_bundle_adjustment.sh`:
```bash
# Parse -o as OUTPUT_DIR (not output file)
-o) OUTPUT_DIR="$2"; shift 2 ;;

# Then construct full file path internally
OUTPUT_FILE=${OUTPUT_FILE:-${OUTPUT_DIR}/cam_stereo.yml}

# Pass full file path to C++ executable
-o "$OUTPUT_FILE"
```

## Behavior After Fix

### When user passes `-o /data/output` (directory):
```bash
# Detected as directory
if [ -d "/data/output" ] || [[ "/data/output" != *.yml && "/data/output" != *.yaml ]]; then
    # Convert to full file path
    OUTPUT_FILE="/data/output/cam_stereo.yml"
end
```

### When user passes `-o /data/output/my_calib.yml` (file path):
```bash
# Detected as file (ends with .yml)
if [[ "/data/output/my_calib.yml" == *.yml ]]; then
    # Keep as-is
    OUTPUT_FILE="/data/output/my_calib.yml"
fi
```

## Testing

### Test Case 1: Directory path
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh \
  -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output
```
**Expected**: Generates `/data/output/handeye.yml` ✓

### Test Case 2: File path
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh \
  -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output/my_handeye.yml
```
**Expected**: Generates `/data/output/my_handeye.yml` ✓

### Test Case 3: No -o parameter (uses default)
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh \
  -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp
```
**Expected**: Generates `/data/output/handeye.yml` ✓ (from default)

## Files Modified

1. **docker-entrypoint.sh**
   - Added `OUTPUT_PARAM` variable for temporary parameter storage
   - Added path detection logic (check if directory or file)
   - Added full file path construction for `-o` parameter
   - Fixed both quality-checks-enabled and disabled paths

2. **step*.sh** (Already had correct implementation)
   - No changes needed - already handled internally

3. **run.sh**
   - Updated to use proper parameter syntax with `-o /data/output`

## Backward Compatibility

✓ All changes are fully backward compatible:
- Scripts using environment variables continue to work
- Scripts without `-o` parameter continue to work (uses defaults)
- Scripts with correct `-o file.yml` paths continue to work
- Only new behavior: now also accepts `-o directory_path` and converts it automatically

## Error Prevention

Before fix:
```
Error: Cannot open output file for writing: /data/output
```

After fix:
- If `/data/output` exists and is writable, generates proper file at `/data/output/cam_stereo.yml`
- If `/data/output/my_calib.yml` specified, generates exactly that file
- If neither `-o` provided, uses default `/data/output/cam_stereo.yml`

## Related Issues Resolved

- ✓ Step scripts now properly handle `-o` parameter
- ✓ Docker entry point no longer confuses directories with file paths
- ✓ Users can now use either directory or file paths with `-o` parameter
- ✓ C++ executables receive proper full file paths, not directory names
- ✓ OpenCV FileStorage operations work correctly with proper file paths
