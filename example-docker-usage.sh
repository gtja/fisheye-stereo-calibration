#!/bin/bash
# Example script to demonstrate Docker usage for fisheye stereo calibration

echo "=== Fisheye Stereo Calibration Docker Examples ==="
echo ""

# Example 1: Using default parameters with sample images
echo "Example 1: Running calibration with included sample images"
echo "Command: docker run -v \$(pwd)/output:/data/output fisheye-stereo-calibration"
echo ""

# Example 2: Using custom image directory
echo "Example 2: Running calibration with your own images"
echo "Command: docker run -v /path/to/your/imgs:/data/imgs -v \$(pwd)/output:/data/output \\"
echo "  fisheye-stereo-calibration \\"
echo "  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml"
echo ""

# Example 3: Using environment variables
echo "Example 3: Using environment variables for configuration"
echo "Command: docker run \\"
echo "  -v /path/to/your/imgs:/data/imgs \\"
echo "  -v \$(pwd)/output:/data/output \\"
echo "  -e BOARD_WIDTH=9 \\"
echo "  -e BOARD_HEIGHT=6 \\"
echo "  -e SQUARE_SIZE=0.02423 \\"
echo "  -e NUM_IMGS=29 \\"
echo "  -e IMG_DIR=/data/imgs/ \\"
echo "  -e LEFT_PREFIX=left \\"
echo "  -e RIGHT_PREFIX=right \\"
echo "  -e OUTPUT_FILE=/data/output/cam_stereo.yml \\"
echo "  fisheye-stereo-calibration"
echo ""

# Example 4: Building the image
echo "Example 4: Building the Docker image"
echo "Command: docker build -t fisheye-stereo-calibration ."
echo ""

echo "For more details, see DOCKER.md"
