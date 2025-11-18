cmake_minimum_required(VERSION 3.10)
project(FISHEYE_STEREO)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Prefer OpenCV 4.7.0 from /usr/local if available
set(OpenCV_DIR "/usr/local/lib/cmake/opencv4" CACHE PATH "OpenCV directory override")

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

add_executable(calibrate calibrate.cpp)
target_link_libraries(calibrate ${OpenCV_LIBS} "-lpopt")