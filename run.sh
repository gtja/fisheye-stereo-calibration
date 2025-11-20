git pull
# 1. Build Docker image
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .

# Step 1: Monocular Calibration (outputs: left_ds.yml, right_ds.yml)
echo "Step 1: Running monocular calibration..."
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Step 2: Hand-Eye Calibration (output: handeye.yml)
echo "Step 2: Running hand-eye calibration..."
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step2_handeye_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

# Step 3: Stereo Bundle Adjustment (output: cam_stereo.yml)
echo "Step 3: Running stereo bundle adjustment..."
docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step3_stereo_bundle_adjustment.sh -w 11 -h 8 -s 0.02 -d /data/imgs -e bmp -o /data/output

echo "All steps completed! Check output directory for results."
ls -lh $(pwd)/output/