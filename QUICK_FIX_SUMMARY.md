# Quick Fix Summary: File Creation Race Condition

## Problem
```
Error: Left camera calibration file was not created or not readable: /data/output/left_ds.yml
```

## Solution
Increased file verification retry timing from 100ms to 1000ms to handle slow filesystem sync.

## Key Changes

### 1. Retry Timing (THE FIX)
```cpp
// Before
int max_retries = 10;
usleep(10000);  // 10ms × 10 = 100ms total

// After  
int max_retries = 20;
usleep(50000);  // 50ms × 20 = 1000ms total
```

### 2. Debug Logging
Added 8 checkpoints to trace file save flow:
- Entering mono mode
- DS params initialized
- Creating output directory
- Opening file
- Releasing FileStorage
- Syncing filesystem
- Starting verification
- File verified on retry N

### 3. Output Flushing
Added `fflush(stdout)` after all critical messages for immediate visibility.

## Files Changed
- `calibrate_ds.cpp` (2 locations)
- `compute_handeye.cpp` (1 location)

## Testing
```bash
# Build
docker build -t fisheye-stereo-calibration .

# Test
docker run -v $(pwd)/data:/data fisheye-stereo-calibration \
  -w 11 -h 8 -s 0.025 -d /data/imgs/imgs_filtered \
  -l left -e bmp --mono -o /data/output/left_ds.yml
```

## Expected Result
✅ File created successfully
✅ DEBUG messages show flow
✅ No error message

## Documentation
- `FIX_FILE_SYNC_TIMING.md` - Complete analysis
- `QUICK_FIX_SUMMARY.md` - This file

## Why This Works
- Original 100ms was insufficient for slow filesystems
- New 1000ms handles Docker overlay FS, NFS, SMB delays
- Debug logging helps diagnose if timing still insufficient
