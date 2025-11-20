# Fisheye Stereo Camera Calibration System

## Overview

This system provides a complete pipeline for calibrating fisheye stereo cameras using a double-sphere camera model with hand-eye calibration workflow. It consists of three sequential steps:

1. **Monocular Calibration** (Step 1): Calibrate left and right cameras independently
2. **Hand-Eye Calibration** (Step 2): Compute stereo extrinsics (rotation and translation between cameras)
3. **Stereo Bundle Adjustment** (Step 3): Joint optimization of all parameters

## System Architecture

### Core Components

**C++ Backend** (`calibrate_ds.cpp`):
- 2110 lines of calibration logic
- Implements multiple camera models:
  - **KB4** (Knorr-Baumgart): Initial coarse calibration with 4 distortion coefficients
  - **Double-Sphere**: Final model with 12 parameters (fx, fy, cx, cy, xi, alpha, k1-k6 radial distortion)
  - **Omnidir (MEI)**: Fallback for extreme wide-angle lenses (FOV > 200°)
- Uses **Ceres Solver** for bundle adjustment optimization
- OpenCV fisheye and omnidir modules for camera model fitting

**Shell Scripts** (Orchestration):
- `step1_monocular_calibration.sh`: Runs left and right mono calibration
- `step2_handeye_calibration.sh`: Computes stereo extrinsics
- `step3_stereo_bundle_adjustment.sh`: Final joint optimization

**Docker Container**:
- Ubuntu 18.04 with OpenCV 4.7.0 (with contrib), Ceres Solver, popt
- Reproducible environment across different machines
- Dockerfile and docker-entrypoint.sh included

### Camera Models

#### Double-Sphere Model (Final)
```
Parameters: fx, fy, cx, cy, xi, alpha, k1, k2, k3, k4, k5, k6
- fx, fy: Focal lengths
- cx, cy: Principal point
- xi, alpha: Double-sphere specific parameters
- k1-k6: 6-order radial distortion coefficients
```

#### KB4 Model (Initial)
```
Parameters: fx, fy, cx, cy, k1, k2, k3, k4
- Used for initial coarse calibration
- Serves as starting point for double-sphere refinement
```

## Usage

### Prerequisites

- Docker (for containerized execution)
- Checkerboard calibration images for left and right cameras
- Images must be in same directory with naming pattern: `left_*.bmp`, `right_*.bmp`

### Quick Start

#### Option 1: Complete Workflow (All Three Steps)

```bash
./run.sh
```

This will:
1. Build Docker image
2. Run all three calibration steps sequentially
3. Generate four output files in `output/` directory

#### Option 2: Individual Steps

```bash
# Step 1: Monocular calibration
./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output

# Step 2: Hand-eye calibration
./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output

# Step 3: Stereo bundle adjustment
./step3_stereo_bundle_adjustment.sh -w 11 -h 8 -s 0.02 -d ./imgs2 -e bmp -o ./output
```

### Command-Line Parameters

All step scripts support the following parameters:

| Parameter | Short | Type | Default | Description |
|-----------|-------|------|---------|-------------|
| `--board_width` | `-w` | INT | 11 | Checkerboard width (number of corners) |
| `--board_height` | `-h` | INT | 8 | Checkerboard height (number of corners) |
| `--square_size` | `-s` | FLOAT | 0.02 | Size of checkerboard square in meters |
| `--img_dir` | `-d` | STRING | /data/imgs | Directory containing calibration images |
| `--extension` | `-e` | STRING | jpg | Image file extension (jpg, bmp, png, etc.) |
| `-o` | `-o` | STRING | /data/output | Output directory for calibration files |

### Docker Usage

```bash
# Build image
docker build -t fisheye-stereo-calibration .

# Run Step 1
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Run Step 2
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Run Step 3
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step3_stereo_bundle_adjustment.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output
```

## Output Files

### Step 1 Outputs

**`left_ds.yml`** (353 bytes):
- Left camera double-sphere model parameters
- Intrinsics: fx, fy, cx, cy, xi, alpha, k1-k6

**`right_ds.yml`** (353 bytes):
- Right camera double-sphere model parameters
- Same structure as left_ds.yml

### Step 2 Output

**`handeye.yml`** (627 bytes):
- Stereo extrinsics: rotation matrix (R) and translation vector (T)
- Defines spatial relationship: T_right_left = [R | t]
- Computed using hand-eye calibration on monocular results

### Step 3 Output

**`cam_stereo.yml`** (1.6K):
- Complete stereo calibration after bundle adjustment
- Contains both camera intrinsics and stereo extrinsics
- All parameters jointly optimized for best reconstruction accuracy

## Output File Format (YAML)

Example `left_ds.yml` structure:
```yaml
model_type: double_sphere
intrinsics:
  fx: 313.4
  fy: 313.6
  cx: 824.4
  cy: 605.7
distortion:
  xi: 1.2
  alpha: 1.5
  k1: -0.05
  k2: 0.01
  k3: -0.001
  k4: 0.0001
  k5: 0.0
  k6: 0.0
```

## Calibration Accuracy Metrics

Typical results with good calibration images:
- **Monocular reprojection error**: < 0.3 pixels ✓
- **Stereo reprojection error**: < 0.3 pixels ✓
- **Max reprojection error**: < 1.5 pixels ✓

## Image Requirements

### Checkerboard Pattern
- High contrast between black and white squares
- Clearly visible corners for corner detection
- Recommended: 11×8 pattern with 20mm squares

### Image Quality
- Clear focus throughout the image
- Minimal motion blur
- No bright reflections or shadows on checkerboard
- Good lighting conditions
- Adequate contrast (avoid very dark or very bright images)

### Image Naming
- Left camera: `left_001.bmp`, `left_002.bmp`, ...
- Right camera: `right_001.bmp`, `right_002.bmp`, ...
- Matching index numbers ensure synchronized pairs

### Recommended Coverage
- Minimum 20-30 image pairs
- Varied checkerboard positions:
  - Center, corners, diagonal
  - Different distances from camera
  - Various angles (frontal, tilted)

## Troubleshooting

### Issue: "Failed to detect corners in images"

**Solution**:
1. Check image quality (contrast, focus, lighting)
2. Verify checkerboard pattern is visible and clear
3. Ensure correct board dimensions in parameters (`-w 11 -h 8`)
4. Try different image samples
5. Check image file extension matches (`-e bmp`)

### Issue: "Output file not created"

**Solution**:
1. Verify output directory exists and is writable
2. Check disk space availability
3. Ensure full file path is provided (not just directory)
4. Check Docker volume mounts if using containers

### Issue: "Parameter file not found"

**Solution**:
1. Verify previous step completed successfully
2. Check output directory for expected files:
   - Step 1: `left_ds.yml`, `right_ds.yml`
   - Step 2: `handeye.yml`
3. Run with `-o` parameter to specify output location explicitly

## Key Algorithms

### 1. Initial KB4 Calibration
- Uses OpenCV fisheye module
- Fits 4-parameter distortion model
- Provides starting estimate for DS model

### 2. Double-Sphere Model Fitting
- Implements 12-parameter double-sphere model
- Uses Ceres Solver for non-linear optimization
- Handles radial distortion up to 6th order

### 3. Hand-Eye Calibration
- Computes stereo extrinsics from monocular calibrations
- Uses pose estimation from monocular results
- Outputs rotation matrix R and translation vector t

### 4. Bundle Adjustment
- Joint optimization of all intrinsic and extrinsic parameters
- Uses Ceres Solver with multiple residual blocks:
  - Reprojection error (intrinsics)
  - Stereo constraints (extrinsics)
  - Correspondence constraints
- Adaptive parameter bounds to prevent solution drift

## Performance Notes

- Docker memory limit: 4GB (sufficient for typical calibrations)
- Processing time: 2-5 minutes per step (depends on image count)
- GPU acceleration: Not currently implemented (CPU only)

## File Structure

```
fisheye-stereo-calibration/
├── CMakeLists.txt              # Build configuration
├── Dockerfile                  # Docker container definition
├── docker-entrypoint.sh        # Docker entry point
├── calibrate_ds.cpp            # Main calibration executable source
├── compute_handeye.cpp         # Hand-eye calibration source
├── step1_monocular_calibration.sh
├── step2_handeye_calibration.sh
├── step3_stereo_bundle_adjustment.sh
├── run.sh                      # Main workflow runner
├── test_complete_workflow.sh   # Complete workflow test
├── imgs2/                      # Input calibration images (left_*.bmp, right_*.bmp)
├── output/                     # Generated calibration files
│   ├── left_ds.yml
│   ├── right_ds.yml
│   ├── handeye.yml
│   └── cam_stereo.yml
└── README.md
```

## Recent Fixes and Improvements

### Critical Bug Fixes
1. **Monocular File Saving Bug** (v2): Fixed right camera calibration file not being generated
   - Added early-return with immediate FileStorage write after successful calibration
   - Prevents unintended code paths that skip file saving

2. **Parameter Path Handling** (v3): Fixed parameter parsing in shell scripts
   - Added `-o` parameter support to all three step scripts
   - Scripts now properly extract output directory and construct file paths internally

### Reliability Improvements
- Added `sync()` calls after file writes to ensure data is flushed to disk
- Implemented directory existence checks before file operations
- Added file verification after writing to confirm successful creation
- Comprehensive debug logging at critical points

## Future Enhancements

- GPU acceleration using CUDA
- Support for more camera models (polynomial distortion)
- Real-time preview during calibration
- Web-based calibration interface
- Batch processing for multiple camera pairs

## Citation

If you use this calibration system in your research, please cite:

```bibtex
@software{fisheye_stereo_calibration,
  title={Fisheye Stereo Camera Calibration System},
  author={},
  year={2024},
  url={https://github.com/your-repo}
}
```

## License

[License information to be added]

## Support

For issues or questions:
1. Check the Troubleshooting section
2. Review debug output in generated log files
3. Ensure input images meet quality requirements
4. Verify parameter values match your setup
