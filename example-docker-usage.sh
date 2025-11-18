#!/bin/bash
# Example script to demonstrate Docker usage for fisheye stereo calibration

echo "=== Fisheye Stereo Calibration Docker Examples ==="
echo ""

echo "=== Recommended Workflow ==="
echo ""

# Workflow Step 1
echo "Step 1: Capture images (done outside Docker)"
echo "  - 30-40 images minimum"
echo "  - Cover all 4 edges"
echo "  - Various tilts and rotations"
echo ""

# Workflow Step 2
echo "Step 2: Check for blur"
echo "Command: docker run --rm -v /path/to/your/imgs:/data/imgs \\"
echo "  fisheye-stereo-calibration \\"
echo "  python3 /app/utils/laplacian_var.py /data/imgs/ --threshold 100"
echo ""

# Workflow Step 3
echo "Step 3: Analyze corner quality"
echo "Command: docker run --rm -v /path/to/your/imgs:/data/imgs \\"
echo "  fisheye-stereo-calibration \\"
echo "  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix left"
echo ""

# Workflow Step 4
echo "Step 4: Run calibration"
echo "Command: docker run -v /path/to/your/imgs:/data/imgs -v \$(pwd)/output:/data/output \\"
echo "  fisheye-stereo-calibration \\"
echo "  -w 9 -h 6 -s 0.02423 -n 29 -d /data/imgs/ -l left -r right -o /data/output/cam_stereo.yml"
echo ""

# Workflow Step 5
echo "Step 5: Review results"
echo "  - Target: < 0.3 pixel reprojection error"
echo "  - If errors are high, review data quality"
echo ""

echo "=== Basic Usage Examples ==="
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

echo "=== Utility Script Examples ==="
echo ""

# Example 5: Corner analysis with visualization
echo "Example 5: Analyze corners and save visualizations"
echo "Command: docker run --rm -v /path/to/your/imgs:/data/imgs \\"
echo "  fisheye-stereo-calibration \\"
echo "  python3 /app/utils/corner_analysis.py /data/imgs/ --width 9 --height 6 --prefix left --save"
echo ""

# Example 6: Blur detection with custom threshold
echo "Example 6: Check for blur with custom threshold"
echo "Command: docker run --rm -v /path/to/your/imgs:/data/imgs \\"
echo "  fisheye-stereo-calibration \\"
echo "  python3 /app/utils/laplacian_var.py /data/imgs/ --threshold 150"
echo ""

echo "For more details, see DOCKER.md"
