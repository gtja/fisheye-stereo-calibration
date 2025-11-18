## OpenCV C++ Stereo Fisheye Calibration

_**Note**_: I don't actively maintain this repository anymore. PRs are more than welcome to help improve it.

This contains a source file to calibrate a stereo system comprising of fisheye lenses. It calibrates the extrinsics and the intrinsics of the cameras without any initial guesses. If you are looking for stereo calibration with lenses which follow the pinhole model check [here](https://github.com/sourishg/stereo_calibration).

### Dependencies

- OpenCV (version 3.4)
- popt

### Docker Usage (Recommended)

The easiest way to run the calibration is using Docker. See [DOCKER.md](DOCKER.md) for detailed instructions.

**Quick start:**

```bash
# Build the Docker image
docker build -t fisheye-stereo-calibration .

# Run with your own images (JPG by default)
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml

# Run with BMP images
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -e bmp -o /data/output/cam_stereo.yml

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

### Calibration Accuracy Evaluation

The calibration program automatically evaluates the accuracy of the calibration and outputs the following metrics:

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

5. **Baseline Distance**: Comparison between the calibrated baseline and physical baseline (if provided)
   - Target threshold: < 1 mm difference

All evaluation metrics are displayed in the console output and saved to the output YAML file.
