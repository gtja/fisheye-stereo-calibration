git pull
# 1. Build Docker image
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .

docker run -v $(pwd)/imgs2:/data/imgs -v $(pwd)/output:/data/output \
  fisheye-stereo-calibration ./step1_monocular_calibration.sh -w 11 -h 8 -s 0.02 -d /data/imgs  -e bmp -o /data/output