git pull
# 1. Build Docker image
docker build --memory 4g --memory-swap 4g -t fisheye-stereo-calibration .

# 2. Test monocular left calibration
docker run --rm -e CALIBRATION_MODEL=double_sphere \
 -v $(pwd)/imgs2:/data/imgs \
 -v $(pwd)/output:/data/output \
 fisheye-stereo-calibration \
 -w 11 -h 8 -s 0.025 \
 -d /data/imgs/ -l left -e bmp \
 --mono -o /data/output/left_ds.yml

# 3. Verify file was created
ls -lh output/left_ds.yml
cat output/left_ds.yml

# 4. Test complete hand-eye workflow
docker run --rm -e CALIBRATION_MODEL=double_sphere \
 -e CALIBRATION_WORKFLOW=hand_eye \
 -v $(pwd)/imgs2:/data/imgs \
 -v $(pwd)/output:/data/output \
 fisheye-stereo-calibration \
 -w 11 -h 8 -s 0.025 \
 -d /data/imgs/ -l left -r right -e bmp \
 -o /data/output/cam_stereo.yml