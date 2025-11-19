git pull
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .
docker run -e CALIBRATION_MODEL=double_sphere -e CALIBRATION_WORKFLOW=hand-eye -v $(pwd)/imgs2:/data/imgs fisheye-stereo-calibration -w 11 -h 8 -s 0.025 -d /data/imgs/ -l left -r right -e bmp -b 0.06