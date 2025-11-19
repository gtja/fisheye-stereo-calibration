## OpenCV C++ Stereo Fisheye Calibration

_**Note**_: I don't actively maintain this repository anymore. PRs are more than welcome to help improve it.

This contains a source file to calibrate a stereo system comprising of fisheye lenses. It calibrates the extrinsics and the intrinsics of the cameras without any initial guesses. If you are looking for stereo calibration with lenses which follow the pinhole model check [here](https://github.com/sourishg/stereo_calibration).

### 🆕 Calibration Best Practices

**New!** See [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md) for comprehensive best practices to achieve **< 0.3 pixel calibration accuracy**. The guide covers:
- Input data quality checks and filtering
- Optimal model configuration for fisheye lenses
- Image capture strategies and pose requirements
- Advanced optimization techniques
- Troubleshooting common issues

### 🎯 Advanced: Hand-Eye Calibration Workflow

**New!** For **extreme fisheye lenses (FOV > 200°)**, use the hand-eye calibration workflow for significantly better accuracy. See [HAND_EYE_CALIBRATION.md](HAND_EYE_CALIBRATION.md) for the complete guide.

**Key improvements:**
- Initial rotation error: **2° → < 0.2°** (10x improvement)
- Final RMSE: **0.16 px → 0.08 px** (2x improvement)
- Valid frames for bundle adjustment: **4 → 15+** (out of 19)

**Three-step workflow:**
1. Monocular calibration (`--mono` flag)
2. Hand-eye calibration (`compute_handeye` utility)
3. Stereo BA with joint optimization (`--joint-ba --init-extrinsic`)

### 🛠️ Utility Scripts

The `utils/` directory contains Python scripts to help improve calibration quality:

- **`pre_calibration_check.py`**: Comprehensive image quality check (recommended)
  ```bash
  python3 utils/pre_calibration_check.py imgs/ --width 9 --height 6
  ```
  This script analyzes images for blur and corner detection quality, then creates a new filtered directory with only good images, renamed sequentially (e.g., left1.jpg, left2.jpg, ..., leftN.jpg). The original images remain untouched.

- **`laplacian_var.py`**: Detect and filter blurry images using Laplacian variance
  ```bash
  python3 utils/laplacian_var.py imgs/ --threshold 100
  ```

- **`corner_analysis.py`**: Analyze corner detection quality and distribution
  ```bash
  python3 utils/corner_analysis.py imgs/ --width 9 --height 6 --prefix left
  ```

These tools help identify problematic images before calibration, potentially reducing errors by 30-50%.

### Dependencies

- OpenCV (version 4.7.0 with contrib modules)
- popt

### Docker Usage (Recommended)

The easiest way to run the calibration is using Docker. See [DOCKER.md](DOCKER.md) for detailed instructions.

**🆕 Automatic Quality Checks:** The Docker container now automatically checks and filters images before calibration by default! This includes blur detection and corner quality analysis, improving calibration accuracy by 30-50%. The original images are kept intact - a new filtered directory with sequentially-named images is created and used for calibration.

**Quick start:**

```bash
# Build the Docker image
docker build -t fisheye-stereo-calibration .

# Run with automatic quality checks (NEW - enabled by default)
# The container will automatically filter blurry images and validate corner detection
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml

# Disable quality checks to use the original workflow
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  -e RUN_QUALITY_CHECKS=false \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml

# Run with BMP images and automatic quality checks
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  -e IMAGE_EXTENSION=bmp \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml

# Or test with sample images included in the container
docker run -v $(pwd)/output:/data/output fisheye-stereo-calibration
```

### Manual Compilation

Compile all the files using the following commands.

```bash
mkdir build && cd build
cmake ..
make
```

Make sure your are in the `build` folder to run the executables.

### Data

Some sample calibration images are stored in the `imgs` folder.

### Running calibration

Run the executable with the following command

```bash
./calibrate -w [board_width] -h [board_height] -s [square_size] -n [num_imgs] -d [img_dir] -l [left_img_prefix] -r [right_img_prefix] -o [calib_file]
```

For example if you use the images in the `imgs` folder run the following command

```bash
./calibrate -w 9 -h 6 -s 0.02423 -n 29 -d ../imgs/ -l left -r right -o cam_stereo.yml
```

You can also optionally specify the image file extension (default is `jpg`). This allows you to use BMP or other supported image formats:

```bash
./calibrate -w 9 -h 6 -s 0.02423 -n 29 -d ../imgs/ -l left -r right -e bmp -o cam_stereo.yml
```

You can also optionally specify the physical baseline distance (in meters) to evaluate the baseline calibration accuracy:

```bash
./calibrate -w 9 -h 6 -s 0.02423 -n 29 -d ../imgs/ -l left -r right -o cam_stereo.yml -b 0.110
```

### Camera Models

The calibration program supports three camera models:

#### Fisheye Model (Default)

The standard fisheye model works well for most fisheye lenses up to ~200° FOV. It uses 4 distortion coefficients (k1-k4).

```bash
./calibrate -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -o cam_stereo.yml
# Or explicitly:
./calibrate -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -m fisheye -o cam_stereo.yml
```

#### 🆕 Double-Sphere Model (Recommended for High Precision)

For applications requiring sub-pixel accuracy (<0.15 pixels) or extreme fisheye lenses, the Double-Sphere model with 6-order radial distortion and Ceres Bundle Adjustment provides superior results:

```bash
./calibrate_ds -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -o cam_stereo_ds.yml
```

You can also optionally specify the physical baseline distance (in meters) for baseline accuracy evaluation:

```bash
./calibrate_ds -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -o cam_stereo_ds.yml -b 0.110
```

**Features:**
- 6-order radial distortion (k1-k6) for better edge modeling
- Double-Sphere projection (Usenko et al., 2018)
- SE(3) pre-correction for improved corner detection
- Ceres-based Bundle Adjustment with Huber robust kernel
- **Automatic fallback to omnidir model for extreme FOV (>200°, e.g., 220°)**
- Typically achieves 0.1-0.15 pixels RMSE (vs 0.3 pixels with standard methods)
- **Full calibration accuracy evaluation with all 5 metrics**

**Requirements:** Ceres Solver must be installed (`sudo apt-get install libceres-dev`)

See [DOUBLE_SPHERE_CALIBRATION.md](DOUBLE_SPHERE_CALIBRATION.md) for detailed documentation.

####  MEI/Omnidirectional Model (Experimental)

For extreme wide-angle lenses (FOV > 200°, such as 220°), the MEI (Unified Camera Model) with mirror parameter ξ provides better accuracy. This model includes:
- Mirror parameter ξ (xi) for modeling the projection surface
- 4 distortion coefficients (k1-k4)

```bash
./calibrate -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -m omnidir -o cam_stereo.yml
```

**Note**: The omnidir/MEI model requires OpenCV contrib modules. When using Docker (recommended), OpenCV 4.7.0 with contrib modules is automatically built and configured. For manual compilation, ensure you have opencv_contrib installed. For alternative production-ready tools with extreme wide-angle lenses, consider [Kalibr](https://github.com/ethz-asl/kalibr) or [Basalt](https://gitlab.com/VladyslavUsenko/basalt).

### Calibration Accuracy Evaluation

Both calibration programs (`calibrate` and `calibrate_ds`) automatically evaluate the accuracy of the calibration and output the following metrics:

1. **Monocular Reprojection Error**: The error between detected 2D corner points and projected 3D points using the calibrated camera model
   - Calculated separately for left and right cameras
   - Target threshold: < 0.3 pixels (average)

2. **Stereo Reprojection Error**: The error when projecting 3D points through the left camera, transforming to right camera coordinates, and reprojecting
   - Target threshold: < 0.3 pixels (average)

3. **Maximum Stereo Reprojection Error**: The maximum reprojection error across all corner points
   - Target threshold: < 1.5 pixels

4. **Stereo Rectification Error**: The average y-coordinate difference for corresponding points after rectification
   - Target threshold: < 0.3 pixels (average)
   - Target threshold: < 0.7 pixels (maximum)
   - **Note**: For wide-angle fisheye lenses (FOV > 130°), partial rectification is automatically applied to prevent points from going behind the camera. See [WIDE_ANGLE_RECTIFICATION_FIX.md](WIDE_ANGLE_RECTIFICATION_FIX.md) for details.

5. **Baseline Distance**: Comparison between the calibrated baseline and physical baseline (if provided)
   - Target threshold: < 1 mm difference

All evaluation metrics are displayed in the console output and saved to the output YAML file.

#### Recent Improvements

The calibration code has been enhanced with several improvements for better accuracy:
- **Improved subpixel corner refinement**: Stricter epsilon (0.01 vs 0.1) reduces corner jitter
- **Better convergence criteria**: More iterations (30 vs 12) with optimized epsilon
- **Optimized rectification**: Uses alpha=0.8 (recommended 0.7-0.8) for better valid pixel retention
- **All 4 distortion coefficients**: k1-k4 enabled for fisheye lenses (critical for edge accuracy)
- **Principal point optimization**: Not fixed to image center (important for wide-angle lenses)
- **🆕 Wide-angle rectification fix**: Automatic partial rectification and adaptive scaling for wide-angle fisheye lenses (FOV > 130°) to ensure rectification error can be evaluated. See [WIDE_ANGLE_RECTIFICATION_FIX.md](WIDE_ANGLE_RECTIFICATION_FIX.md).

See [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md) for detailed explanations and additional optimization techniques.
