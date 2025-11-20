# Quick Reference Guide

## Installation & Setup

### 1. Build Docker Image
```bash
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .
```

### 2. Prepare Calibration Images
- Place images in `imgs2/` directory with naming: `left_*.bmp`, `right_*.bmp`
- Minimum 20-30 image pairs
- Clear checkerboard pattern visible in all images
- Good lighting and focus

### 3. Create Output Directory
```bash
mkdir -p output
```

## Running Calibration

### Complete Workflow (3 Steps)
```bash
./run.sh
```

### Step-by-Step Execution

**Step 1: Monocular Calibration**
```bash
./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output
```
Expected outputs:
- `output/left_ds.yml` (353 bytes)
- `output/right_ds.yml` (353 bytes)

**Step 2: Hand-Eye Calibration**
```bash
./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output
```
Expected output:
- `output/handeye.yml` (627 bytes)

**Step 3: Stereo Bundle Adjustment**
```bash
./step3_stereo_bundle_adjustment.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output
```
Expected output:
- `output/cam_stereo.yml` (1.6K)

## Parameter Reference

| Flag | Parameter | Example | Description |
|------|-----------|---------|-------------|
| `-w` | board_width | `11` | Checkerboard width (corners) |
| `-h` | board_height | `8` | Checkerboard height (corners) |
| `-s` | square_size | `0.02` | Square size in meters |
| `-d` | img_dir | `./imgs2` | Image directory |
| `-e` | extension | `bmp` | File extension |
| `-o` | output_dir | `./output` | Output directory |

## Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| "Failed to detect corners" | Improve image quality; check board dimensions |
| "Output file not created" | Verify directory exists; check disk space |
| "Parameter file not found" | Run previous step first; check output directory |
| "Permission denied" | Make scripts executable: `chmod +x *.sh` |

## Output File Contents

### left_ds.yml / right_ds.yml (Monocular)
```yaml
model_type: double_sphere
intrinsics:
  fx: 313.4        # Focal length X
  fy: 313.6        # Focal length Y
  cx: 824.4        # Principal point X
  cy: 605.7        # Principal point Y
distortion:
  xi: 1.2          # Double-sphere xi
  alpha: 1.5       # Double-sphere alpha
  k1 to k6: [...]  # 6-order radial distortion
```

### handeye.yml (Stereo Extrinsics)
```yaml
rotation_matrix:
  - [r11, r12, r13]    # Rotation from right to left
  - [r21, r22, r23]
  - [r31, r32, r33]
translation_vector:
  - tx               # Translation X
  - ty               # Translation Y
  - tz               # Translation Z
```

### cam_stereo.yml (Complete Calibration)
Contains:
- Both camera intrinsics (left + right)
- Stereo extrinsics (R, T)
- All parameters after joint bundle adjustment

## Verification Checklist

- [ ] Images placed in correct directory with proper naming
- [ ] Board dimensions correct (-w 11 -h 8)
- [ ] Square size in meters (-s 0.02)
- [ ] Output directory exists and is writable
- [ ] All three steps complete without errors
- [ ] All four YAML files generated:
  - [ ] left_ds.yml
  - [ ] right_ds.yml
  - [ ] handeye.yml
  - [ ] cam_stereo.yml

## Performance Expectations

| Aspect | Expected Value |
|--------|-----------------|
| Step 1 Duration | 1-2 minutes |
| Step 2 Duration | 30-60 seconds |
| Step 3 Duration | 1-3 minutes |
| Mono Error | < 0.3 pixels ✓ |
| Stereo Error | < 0.3 pixels ✓ |
| Max Error | < 1.5 pixels ✓ |
| Docker Memory | 4GB |

## Docker Commands

### Run Container Interactively
```bash
docker run -it -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration /bin/bash
```

### Run Single Step
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output
```

### View Docker Logs
```bash
docker logs <container_id>
```

### List Generated Images
```bash
ls -lh output/
```

## File Paths

**Inside Docker Container**:
- Input images: `/data/imgs/`
- Output files: `/data/output/`
- Scripts: `/root/`

**On Host Machine**:
- Input images: `./imgs2/`
- Output files: `./output/`
- Scripts: `./`

## Environment Variables (Optional)

Scripts support environment variable overrides:
```bash
export BOARD_WIDTH=11
export BOARD_HEIGHT=8
export SQUARE_SIZE=0.02
./step1_monocular_calibration.sh
```

Command-line arguments override environment variables:
```bash
BOARD_WIDTH=10 ./step1_monocular_calibration.sh -w 11  # Uses -w 11
```

## Advanced Usage

### Custom Checkerboard Dimensions
```bash
./step1_monocular_calibration.sh -w 10 -h 7 -s 0.025 -d ./imgs2 -e bmp -o ./output
```

### Different Image Format
```bash
./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e jpg -o ./output
```

### Different Output Location
```bash
mkdir -p /tmp/calib_results
./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o /tmp/calib_results
```

## Debugging

### Enable Verbose Output
```bash
# Run with bash -x for command tracing
bash -x step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output
```

### Check Intermediate Files
```bash
# During calibration, watch output directory
watch -n 1 'ls -lh output/'
```

### View Calibration Details
```bash
# Print YAML content
cat output/left_ds.yml
cat output/handeye.yml
cat output/cam_stereo.yml
```

## Success Indicators

✓ Step 1:
- Both left_ds.yml and right_ds.yml created
- File sizes ~350 bytes each
- No "Failed to detect corners" errors

✓ Step 2:
- handeye.yml created
- File size ~600 bytes
- Successful matrix inversion for extrinsics

✓ Step 3:
- cam_stereo.yml created
- File size ~1.6KB
- Reprojection errors reported < 0.3 pixels

## Support & Documentation

- **WORKFLOW.md**: Complete system documentation
- **README.md**: Project overview
- **USAGE_GUIDE.md**: Detailed usage instructions
- **DOCKER_USAGE.md**: Container setup guide
- **CHANGELOG.md**: Version history and fixes

## Quick Troubleshooting

```bash
# Check Docker image exists
docker images | grep fisheye-stereo-calibration

# Check output files
ls -lh output/

# Test complete workflow
./test_complete_workflow.sh

# Clean output and restart
rm output/*.yml
./run.sh
```

## Key Parameters to Remember

- **Board Width**: Usually 11 (12 corners horizontally)
- **Board Height**: Usually 8 (9 corners vertically)
- **Square Size**: 0.02 meters (20mm) for standard checkerboard
- **Extension**: bmp (or jpg, png depending on images)
- **Output**: Always use `/data/output` inside Docker, `./output` on host
