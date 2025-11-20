# Changelog

All notable changes to this project will be documented in this file.

## [3.0] - 2024-11-20

### Fixed
- **Parameter Parsing in Shell Scripts**: All three step scripts now properly accept and parse command-line arguments (`-w`, `-h`, `-s`, `-d`, `-e`, `-o`)
  - Step 1: `step1_monocular_calibration.sh`
  - Step 2: `step2_handeye_calibration.sh`
  - Step 3: `step3_stereo_bundle_adjustment.sh`
- **Output Directory Handling**: Scripts now correctly accept `-o` parameter and use it to construct proper file paths for OpenCV FileStorage

### Added
- `test_complete_workflow.sh`: Complete three-step workflow test script
- `WORKFLOW.md`: Comprehensive documentation covering system architecture, usage, output formats, and troubleshooting
- `CHANGELOG.md`: This file

### Changed
- `run.sh`: Updated to use proper parameter syntax with `-o` flag for all three steps
- Shell script parameter parsing: Added while loop pattern for robust argument handling

### Documentation
- Added detailed command-line parameter reference table
- Documented all output file formats (YAML structure)
- Added Docker usage examples
- Added troubleshooting section

## [2.0] - 2024-11-20

### Fixed
- **Critical: Monocular Calibration File Not Generated** (Right Camera)
  - **Root Cause**: Right camera successful calibration was not immediately saved to file
  - **Solution**: Added early-return file saving immediately after successful calibration in `calibrate_ds.cpp` lines 765-809
  - **Verification**: Both `left_ds.yml` and `right_ds.yml` now successfully generated
- **File I/O Reliability**: Added `sync()` calls after FileStorage operations to ensure data is flushed to disk
- **Directory Creation**: Added `ensure_output_directory()` function to create parent directories before writing files
- **File Verification**: Added post-write verification to confirm file creation before proceeding

### Changed
- `calibrate_ds.cpp`: Modified mono calibration flow to save files immediately after successful KB4 or omnidir calibration
  - Left camera (lines 665-707): Early exit with file save
  - Right camera (lines 765-809): Early exit with file save
  - Prevents secondary mono handling block from interfering with file writing

### Code Quality
- Comprehensive debug logging throughout calibration pipeline
- Print statements at critical points for error diagnosis
- Better error messages for file I/O operations

## [1.0] - 2024-11-19

### Initial Features
- **Three-Step Calibration Pipeline**:
  1. Monocular calibration (left and right cameras independently)
  2. Hand-eye calibration (compute stereo extrinsics)
  3. Stereo bundle adjustment (joint optimization)

- **Camera Models Supported**:
  - KB4 (Knorr-Baumgart): Initial coarse calibration
  - Double-Sphere: Final model with 6-order radial distortion
  - Omnidir (MEI): Fallback for extreme wide-angle

- **Core Components**:
  - `calibrate_ds.cpp`: Main calibration executable (2110 lines)
  - `compute_handeye.cpp`: Hand-eye calibration executable
  - Docker containerization with Ubuntu 18.04 + OpenCV 4.7.0 + Ceres Solver

- **Shell Scripts**:
  - `step1_monocular_calibration.sh`: Left/right mono calibration
  - `step2_handeye_calibration.sh`: Stereo extrinsics computation
  - `step3_stereo_bundle_adjustment.sh`: Final bundle adjustment

- **Documentation**:
  - README.md with system overview
  - USAGE_GUIDE.md for end users
  - DOCKER_USAGE.md for container setup

### Initial Testing
- Validated monocular reprojection error: 0.1298 pixels (target < 0.3) ✓
- Validated stereo reprojection error: 0.1205 pixels (target < 0.3) ✓
- Validated max reprojection error: 0.4661 pixels (target < 1.5) ✓
- Baseline distance: 64.14 mm

## Version History Notes

### Version 1.0 to 2.0 Transition
**Problem**: Right camera mono calibration was completing successfully (no error messages) but the calibration file was not being created. This caused step 2 (hand-eye calibration) to fail with "right_ds.yml not found" error.

**Investigation**: The `calibrate_ds.cpp` code had two potential paths for mono calibration:
1. Initial section (lines 629-776): KB4/omnidir calibration logic
2. Secondary section (later lines): Another mono handling block that could interfere

**Solution**: Added immediate file save and return after successful calibration in the initial section, preventing unintended execution of secondary blocks.

### Version 2.0 to 3.0 Transition
**Problem**: Shell scripts didn't accept `-o` parameter for output directory specification. When users passed `-o /data/output`, the scripts ignored it and used hardcoded defaults, causing OpenCV FileStorage errors when trying to open a directory instead of a file.

**Solution**: Added parameter parsing loop to all three step scripts using bash `case` statement pattern to extract and apply command-line arguments. Scripts now properly:
1. Accept `-o OUTPUT_DIR` parameter
2. Extract the directory path
3. Internally construct proper file paths (e.g., `${OUTPUT_DIR}/handeye.yml`)
4. Pass full file paths to C++ executables

## Known Limitations

- GPU acceleration not currently implemented (CPU-only)
- No real-time preview during calibration
- Requires pre-calculated checkerboard dimensions (no automatic detection)
- Memory limit set to 4GB (may need adjustment for very large image sets)

## Breaking Changes

### None between versions
All changes are backward compatible. Existing workflows using environment variables or hardcoded paths continue to work.

## Dependencies

- OpenCV 4.7.0 (with contrib modules: fisheye, ccalib, omnidir)
- Ceres Solver 2.0+
- popt (command-line argument parsing)
- CMake 3.0+
- C++11 or later

## Migration Guide

### From v1.0/v2.0 to v3.0

**Old command**:
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh
```

**New command** (recommended):
```bash
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output
```

**Note**: Old command still works because scripts use defaults if no arguments provided.

## Testing

### Unit Tests
- `test_calibration.py`: Python validation of calibration outputs
- `test_complete_workflow.sh`: Shell script for full pipeline testing

### Integration Tests
- Docker image builds successfully
- All three steps complete without errors
- Output files generated in correct locations
- Calibration accuracy metrics validated

## Contributors

- [Your name/team]

## Future Roadmap

### v4.0 (Planned)
- GPU acceleration with CUDA
- Additional camera models (polynomial distortion, unified projection)
- Real-time calibration preview
- Batch processing for multiple camera pairs

### v5.0 (Planned)
- Web-based calibration interface
- Machine learning-based image quality assessment
- Automatic detection of checkerboard dimensions
- Multi-calibration session management
