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

# Run with your own images
docker run -v /path/to/your/imgs:/data/imgs -v /path/to/output:/data/output \
  fisheye-stereo-calibration \
  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml

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
