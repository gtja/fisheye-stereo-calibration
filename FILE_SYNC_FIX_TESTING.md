# File Sync Fix - Testing Guide

## Problem Fixed
After monocular calibration completes successfully, the subsequent `compute_handeye` command was failing with error:
```
[ERROR:0@0.000] global persistence.cpp:505 open Can't open file: '/data/output/left_ds.yml' in read mode
```

This was caused by a race condition where files weren't fully flushed to disk before being read by the next step in the pipeline.

## Changes Made

### 1. C++ Code Changes
Added explicit `sync()` system call after `FileStorage::release()` in:
- `calibrate_ds.cpp` (line 880 and line 1796)
- `compute_handeye.cpp` (line 415)

These ensure that output files are fully written to disk before the program exits.

### 2. Shell Script Changes
In `docker-entrypoint.sh`, after each calibration step (monocular left, monocular right, hand-eye), added:
- File existence check (`[ ! -f "$file" ]`)
- Filesystem sync command (`sync`)
- File readability check (`[ ! -r "$file" ]`)

These ensure that files exist and are readable before proceeding to the next step.

## How to Test

### Prerequisites
- Docker installed
- Sample calibration images (in `/data/imgs/` or mounted volume)

### Test Steps

1. **Build the Docker image**:
   ```bash
   docker build -t fisheye-calibration .
   ```

2. **Run hand-eye calibration workflow** (this was the failing scenario):
   ```bash
   docker run -v /path/to/images:/data/imgs \
              -v /path/to/output:/data/output \
              -e CALIBRATION_WORKFLOW=hand_eye \
              -e CALIBRATION_MODEL=double_sphere \
              fisheye-calibration
   ```

3. **Verify the workflow completes without errors**:
   - Step 1a (Left monocular calibration) should complete and create `/data/output/left_ds.yml`
   - Step 1b (Right monocular calibration) should complete and create `/data/output/right_ds.yml`
   - Step 2 (Hand-eye calibration) should successfully read both files and create `/data/output/handeye.yml`
   - Step 3 (Bundle adjustment) should successfully read the handeye file and create the final calibration

4. **Check for error messages**:
   The fix should prevent the error:
   ```
   [ERROR:0@0.000] global persistence.cpp:505 open Can't open file: '/data/output/left_ds.yml' in read mode
   ```

5. **Verify output files**:
   ```bash
   ls -la /data/output/
   # Should show:
   # - left_ds.yml
   # - right_ds.yml
   # - handeye.yml
   # - cam_stereo.yml (final output)
   ```

### Expected Behavior
- All files should be created successfully
- No file access errors should occur
- The workflow should complete all three steps without interruption

### Test on Different Filesystems
Test on various filesystem types to ensure sync works properly:
- Local filesystem
- Docker volumes
- Network-mounted volumes (NFS, CIFS)
- Cloud storage mounts

## Technical Details

### Why sync() is needed
The `FileStorage::release()` call closes the file handle, but the kernel may buffer the write operations. The `sync()` system call ensures all filesystem buffers are flushed to disk before the program continues.

### Why shell-level checks are needed
Even after sync() in C++, there's a small window where the shell script could continue before the file is visible. The explicit checks ensure:
1. The file exists (`-f` test)
2. The file is readable (`-r` test)
3. The filesystem is synced (shell `sync` command)

This defense-in-depth approach ensures robustness across different filesystem types and system load conditions.
