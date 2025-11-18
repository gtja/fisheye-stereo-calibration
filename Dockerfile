FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libopencv-dev \
    libpopt-dev \
    && rm -rf /var/lib/apt/lists/*

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
