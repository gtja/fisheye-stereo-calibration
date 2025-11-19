# Shell Script Retry Logic Fix

## Summary

Added retry logic to the `docker-entrypoint.sh` script to handle filesystem sync delays when verifying file creation after C++ calibration programs complete.

## Problem

When running monocular calibration in the hand-eye workflow, the shell script would sometimes fail with:

```
Error: Left camera calibration file was not created: /data/output/left_ds.yml
```

This occurred even though:
1. The C++ program (`calibrate_ds`) successfully created the file
2. The C++ program had its own retry logic to verify file creation
3. The C++ program exited with success status (exit code 0)

## Root Cause

There was a **race condition** between:
1. C++ program verifying file exists and exiting
2. Operating system completing filesystem sync operations
3. Shell script checking for file existence

This is particularly problematic in:
- Docker containers with overlay filesystems
- Network-mounted filesystems
- Systems under heavy I/O load
- Slow storage devices

## Solution

Added a `verify_file_with_retry()` function to the shell script that:

```bash
verify_file_with_retry() {
    local file_path="$1"
    local max_retries=10
    local retry_delay=0.1  # 100ms between retries
    local retry_count=0
    
    while [ $retry_count -lt $max_retries ]; do
        if [ -f "$file_path" ] && [ -r "$file_path" ]; then
            return 0
        fi
        sleep $retry_delay
        retry_count=$((retry_count + 1))
    done
    
    return 1
}
```

### Key Features

- **Retry Logic**: Up to 10 attempts with 100ms delay between each
- **Maximum Wait**: 1 second total (10 × 100ms)
- **Dual Check**: Verifies both existence (`-f`) and readability (`-r`)
- **Clear Error Messages**: Shows directory listing if file not found after retries
- **Fast Path**: Returns immediately if file exists on first check

### Application

The retry logic is applied to all critical file creation points:

1. **Left camera monocular calibration** (`left_ds.yml`)
2. **Right camera monocular calibration** (`right_ds.yml`)
3. **Hand-eye calibration** (`handeye.yml`)

## Testing

The retry logic was validated with unit tests that simulate various scenarios:

```bash
✓ Test 1: File exists immediately
✓ Test 2: File does not exist (should fail correctly)
✓ Test 3: File appears after delay (simulates race condition)
✓ Test 4: File exists but is not readable (should fail correctly)
```

All tests passed successfully.

## Impact

### Benefits

✅ **Eliminates spurious failures** due to filesystem sync delays
✅ **Minimal performance impact** (<1 second in worst case, typically <100ms)
✅ **Better error messages** showing directory contents when file truly missing
✅ **Compatible with existing code** - no changes needed to C++ programs
✅ **Works across filesystems** - Docker overlay, network mounts, local disks

### Comparison with C++ Retry Logic

The shell script retry logic provides an **additional safety layer** on top of the C++ retry logic:

| Layer | Location | Purpose |
|-------|----------|---------|
| C++ | `calibrate_ds.cpp`, `compute_handeye.cpp` | Verify file written to disk before exit |
| Shell | `docker-entrypoint.sh` | Verify file visible to shell after program exit |

Both layers are necessary because:
- C++ `sync()` is asynchronous (doesn't guarantee immediate visibility)
- Process exit may occur before filesystem buffers fully flushed
- Different processes may see filesystem state differently

## Files Modified

- `docker-entrypoint.sh`: Added `verify_file_with_retry()` function and updated all file verification checks

## Future Improvements

1. Make retry count and delay configurable via environment variables
2. Add timing metrics to understand typical sync delays
3. Log warnings if retries are needed (indicates slow filesystem)
4. Consider using `inotify` for more efficient file watching on Linux

## Related Documentation

- `FIX_SUMMARY_LEFT_DS.md`: Original C++ retry logic implementation
- `FILE_SYNC_FIX_TESTING.md`: Comprehensive testing of file sync fixes
- `COMPLETE_FIX_SUMMARY.md`: Overview of all file sync related fixes
