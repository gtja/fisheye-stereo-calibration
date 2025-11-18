FROM ubuntu:18.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies for building OpenCV 3.4 and Python utilities
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    unzip \
    libpopt-dev \
    # Python dependencies for utility scripts
    python3 \
    python3-pip \
    # OpenCV dependencies
    libgtk2.0-dev \
    pkg-config \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libtbb2 \
    libtbb-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libdc1394-22-dev \
    && rm -rf /var/lib/apt/lists/*

# Download and build OpenCV 3.4.20
RUN cd /tmp && \
    wget --no-check-certificate -O opencv.zip https://github.com/opencv/opencv/archive/3.4.20.zip && \
    unzip opencv.zip && \
    cd opencv-3.4.20 && \
    mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=RELEASE \
          -D CMAKE_INSTALL_PREFIX=/usr/local \
          -D WITH_TBB=ON \
          -D WITH_V4L=ON \
          -D WITH_QT=OFF \
          -D WITH_OPENGL=ON \
          .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/opencv*

# Install Python packages for utility scripts
RUN pip3 install --no-cache-dir opencv-python numpy

# Set working directory
WORKDIR /app

# Copy the source code
COPY . /app/

# Build the calibration executable
RUN mkdir -p build && \
    cd build && \
    cmake .. && \
    make

# Copy entrypoint script
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# Set default working directory for runtime
WORKDIR /app/build

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

# Default parameters (can be overridden)
# By default, use the sample images included in the container
CMD ["-w", "9", "-h", "6", "-s", "0.02423", "-n", "29", "-d", "/app/imgs/", "-l", "left", "-r", "right", "-o", "/data/output/cam_stereo.yml"]
