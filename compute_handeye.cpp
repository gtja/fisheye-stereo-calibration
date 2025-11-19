#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/ccalib/omnidir.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>
#include "popt_pp.h"
#include "double_sphere.h"

using namespace std;
using namespace cv;

// Helper function to create directory recursively
bool create_directory_recursive(const char* path) {
  char tmp[256];
  char *p = NULL;
  size_t len;
  
  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;
  
  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
        if (errno != EEXIST) {
          return false;
        }
      }
      *p = '/';
    }
  }
  
  if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
    if (errno != EEXIST) {
      return false;
    }
  }
  return true;
}

// Helper function to ensure output directory exists for a file path
bool ensure_output_directory(const char* filepath) {
  char tmp[256];
  char dir_path[256];
  snprintf(tmp, sizeof(tmp), "%s", filepath);
  
  // Extract directory path manually to avoid dirname() issues
  // Find the last '/' in the path
  char* last_slash = strrchr(tmp, '/');
  if (last_slash != NULL) {
    // Copy the directory part (everything before the last '/')
    size_t dir_len = last_slash - tmp;
    if (dir_len > 0 && dir_len < sizeof(dir_path) - 1) {
      strncpy(dir_path, tmp, dir_len);
      dir_path[dir_len] = '\0';
      return create_directory_recursive(dir_path);
    }
  }
  
  // If no '/' found, file is in current directory, no need to create dir
  return true;
}

// Helper function to find all matching image files in a directory
vector<int> find_image_indices(const char* img_dir, const char* prefix, const char* extension) {
    vector<int> indices;
    DIR* dir = opendir(img_dir);
    if (dir == NULL) {
        cerr << "Error: Cannot open directory " << img_dir << endl;
        return indices;
    }
    
    struct dirent* entry;
    string prefix_str(prefix);
    string ext_str(extension);
    
    while ((entry = readdir(dir)) != NULL) {
        string filename(entry->d_name);
        
        if (filename.find(prefix_str) == 0 && 
            filename.length() > ext_str.length() &&
            filename.substr(filename.length() - ext_str.length()) == ext_str) {
            
            string number_part = filename.substr(prefix_str.length(), 
                                               filename.length() - prefix_str.length() - ext_str.length() - 1);
            
            try {
                int idx = stoi(number_part);
                indices.push_back(idx);
            } catch (...) {
                continue;
            }
        }
    }
    
    closedir(dir);
    sort(indices.begin(), indices.end());
    return indices;
}

int main(int argc, char const *argv[])
{
    int board_width, board_height;
    float square_size;
    char* img_dir;
    char* leftimg_filename = NULL;
    char* rightimg_filename = NULL;
    char* left_calib_file;
    char* right_calib_file;
    char* out_file = (char*)"handeye.yml";
    char* extension = (char*)"jpg";
    
    static struct poptOption options[] = {
        { "board_width",'w',POPT_ARG_INT,&board_width,0,"Checkerboard width","NUM" },
        { "board_height",'h',POPT_ARG_INT,&board_height,0,"Checkerboard height","NUM" },
        { "square_size",'s',POPT_ARG_FLOAT,&square_size,0,"Checkerboard square size","NUM" },
        { "img_dir",'d',POPT_ARG_STRING,&img_dir,0,"Directory containing images","STR" },
        { "left",'l',POPT_ARG_STRING,&leftimg_filename,0,"Left image prefix","STR" },
        { "right",'r',POPT_ARG_STRING,&rightimg_filename,0,"Right image prefix","STR" },
        { "left_calib",'L',POPT_ARG_STRING,&left_calib_file,0,"Left camera calibration file (YML)","STR" },
        { "right_calib",'R',POPT_ARG_STRING,&right_calib_file,0,"Right camera calibration file (YML)","STR" },
        { "out_file",'o',POPT_ARG_STRING,&out_file,0,"Output hand-eye calibration filename (default: handeye.yml)","STR" },
        { "extension",'e',POPT_ARG_STRING,&extension,0,"Image file extension (default: jpg)","STR" },
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };
    
    POpt popt(NULL, argc, argv, options, 0);
    int c;
    while((c = popt.getNextOpt()) >= 0) {}
    
    printf("========== Hand-Eye Calibration (AX=XB Solver) ==========\n");
    printf("This program computes the stereo extrinsics (T_right_left)\n");
    printf("from separate monocular calibrations of left and right cameras.\n\n");
    fflush(stdout);
    
    // Load left camera calibration
    printf("Loading left camera calibration from %s...\n", left_calib_file);
    FileStorage fs_left(left_calib_file, FileStorage::READ);
    if (!fs_left.isOpened()) {
        cerr << "\n========================================" << endl;
        cerr << "ERROR: Cannot open left calibration file" << endl;
        cerr << "========================================" << endl;
        cerr << "File: " << left_calib_file << endl;
        cerr << "\nThis file should be an INPUT file created by monocular calibration." << endl;
        cerr << "It appears the file does not exist or cannot be read.\n" << endl;
        cerr << "To fix this issue:" << endl;
        cerr << "1. First run monocular calibration for the left camera:" << endl;
        cerr << "   ./calibrate_ds -w <width> -h <height> -s <square_size> \\" << endl;
        cerr << "                  -d <img_dir> -l left -e <ext> --mono \\" << endl;
        cerr << "                  -o <output_dir>/left_ds.yml" << endl;
        cerr << "\n2. Then run this hand-eye calibration tool" << endl;
        cerr << "\nFor more details, see: HAND_EYE_CALIBRATION.md" << endl;
        cerr << "========================================\n" << endl;
        return 1;
    }
    
    Mat K_left, D_left;
    FileNode camera_node_left = fs_left["camera"];
    if (camera_node_left.empty()) {
        camera_node_left = fs_left["left_camera"];
    }
    if (camera_node_left.empty()) {
        cerr << "Error: Cannot find camera parameters in " << left_calib_file << endl;
        return 1;
    }
    
    double fx_left = (double)camera_node_left["fx"];
    double fy_left = (double)camera_node_left["fy"];
    double cx_left = (double)camera_node_left["cx"];
    double cy_left = (double)camera_node_left["cy"];
    
    K_left = (Mat_<double>(3,3) << fx_left, 0, cx_left,
                                    0, fy_left, cy_left,
                                    0, 0, 1);
    
    // For simplicity, use pinhole model for solvePnP (distortion will be minimal after calibration)
    D_left = Mat::zeros(4, 1, CV_64F);
    if (!camera_node_left["k1"].empty()) D_left.at<double>(0) = (double)camera_node_left["k1"];
    if (!camera_node_left["k2"].empty()) D_left.at<double>(1) = (double)camera_node_left["k2"];
    if (!camera_node_left["k3"].empty()) D_left.at<double>(2) = (double)camera_node_left["k3"];
    if (!camera_node_left["k4"].empty()) D_left.at<double>(3) = (double)camera_node_left["k4"];
    
    fs_left.release();
    printf("  Left: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f\n", fx_left, fy_left, cx_left, cy_left);
    
    // Load right camera calibration
    printf("Loading right camera calibration from %s...\n", right_calib_file);
    FileStorage fs_right(right_calib_file, FileStorage::READ);
    if (!fs_right.isOpened()) {
        cerr << "\n========================================" << endl;
        cerr << "ERROR: Cannot open right calibration file" << endl;
        cerr << "========================================" << endl;
        cerr << "File: " << right_calib_file << endl;
        cerr << "\nThis file should be an INPUT file created by monocular calibration." << endl;
        cerr << "It appears the file does not exist or cannot be read.\n" << endl;
        cerr << "To fix this issue:" << endl;
        cerr << "1. First run monocular calibration for the right camera:" << endl;
        cerr << "   ./calibrate_ds -w <width> -h <height> -s <square_size> \\" << endl;
        cerr << "                  -d <img_dir> -r right -e <ext> --mono \\" << endl;
        cerr << "                  -o <output_dir>/right_ds.yml" << endl;
        cerr << "\n2. Then run this hand-eye calibration tool" << endl;
        cerr << "\nFor more details, see: HAND_EYE_CALIBRATION.md" << endl;
        cerr << "========================================\n" << endl;
        return 1;
    }
    
    Mat K_right, D_right;
    FileNode camera_node_right = fs_right["camera"];
    if (camera_node_right.empty()) {
        camera_node_right = fs_right["right_camera"];
    }
    if (camera_node_right.empty()) {
        cerr << "Error: Cannot find camera parameters in " << right_calib_file << endl;
        return 1;
    }
    
    double fx_right = (double)camera_node_right["fx"];
    double fy_right = (double)camera_node_right["fy"];
    double cx_right = (double)camera_node_right["cx"];
    double cy_right = (double)camera_node_right["cy"];
    
    K_right = (Mat_<double>(3,3) << fx_right, 0, cx_right,
                                     0, fy_right, cy_right,
                                     0, 0, 1);
    
    D_right = Mat::zeros(4, 1, CV_64F);
    if (!camera_node_right["k1"].empty()) D_right.at<double>(0) = (double)camera_node_right["k1"];
    if (!camera_node_right["k2"].empty()) D_right.at<double>(1) = (double)camera_node_right["k2"];
    if (!camera_node_right["k3"].empty()) D_right.at<double>(2) = (double)camera_node_right["k3"];
    if (!camera_node_right["k4"].empty()) D_right.at<double>(3) = (double)camera_node_right["k4"];
    
    fs_right.release();
    printf("  Right: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f\n", fx_right, fy_right, cx_right, cy_right);
    fflush(stdout);
    
    // Load images and detect corners
    printf("\nDetecting checkerboard corners...\n");
    
    vector<int> left_indices = find_image_indices(img_dir, leftimg_filename, extension);
    vector<int> right_indices = find_image_indices(img_dir, rightimg_filename, extension);
    
    vector<int> common_indices;
    for (int idx : left_indices) {
        if (find(right_indices.begin(), right_indices.end(), idx) != right_indices.end()) {
            common_indices.push_back(idx);
        }
    }
    
    if (common_indices.empty()) {
        cerr << "Error: No matching image pairs found" << endl;
        return 1;
    }
    
    printf("Found %zu image pairs\n", common_indices.size());
    
    Size board_size(board_width, board_height);
    vector<vector<Point3f>> object_points;
    vector<vector<Point2f>> left_image_points, right_image_points;
    
    // Create object points (3D coordinates of checkerboard corners)
    vector<Point3f> obj;
    for (int r = 0; r < board_height; ++r) {
        for (int c = 0; c < board_width; ++c) {
            obj.push_back(Point3f(c * square_size, r * square_size, 0.0f));
        }
    }
    
    for (int i : common_indices) {
        char left_img_path[256], right_img_path[256];
        sprintf(left_img_path, "%s/%s%d.%s", img_dir, leftimg_filename, i, extension);
        sprintf(right_img_path, "%s/%s%d.%s", img_dir, rightimg_filename, i, extension);
        
        Mat img_left = imread(left_img_path, IMREAD_GRAYSCALE);
        Mat img_right = imread(right_img_path, IMREAD_GRAYSCALE);
        
        if (img_left.empty() || img_right.empty()) {
            printf("Warning: Failed to load image pair %d\n", i);
            continue;
        }
        
        vector<Point2f> corners_left, corners_right;
        bool found_left = findChessboardCorners(img_left, board_size, corners_left,
                                                CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FILTER_QUADS);
        bool found_right = findChessboardCorners(img_right, board_size, corners_right,
                                                 CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FILTER_QUADS);
        
        if (found_left && found_right) {
            cornerSubPix(img_left, corners_left, Size(5, 5), Size(-1, -1),
                        TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 0.01));
            cornerSubPix(img_right, corners_right, Size(5, 5), Size(-1, -1),
                        TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 0.01));
            
            object_points.push_back(obj);
            left_image_points.push_back(corners_left);
            right_image_points.push_back(corners_right);
            
            printf("  Image pair %d: OK\n", i);
        } else {
            printf("  Image pair %d: Failed (left=%d, right=%d)\n", i, found_left, found_right);
        }
    }
    
    if (object_points.size() < 3) {
        cerr << "Error: Need at least 3 image pairs with valid corners, got " << object_points.size() << endl;
        return 1;
    }
    
    printf("Successfully detected corners in %zu image pairs\n\n", object_points.size());
    fflush(stdout);
    
    // Solve PnP for each camera to get board poses
    printf("Computing board poses using solvePnP...\n");
    
    vector<Vec3d> rvecs_left, tvecs_left;
    vector<Vec3d> rvecs_right, tvecs_right;
    
    for (size_t i = 0; i < object_points.size(); i++) {
        Mat rvec_left, tvec_left, rvec_right, tvec_right;
        
        // Solve PnP for left camera
        bool success_left = solvePnP(object_points[i], left_image_points[i], K_left, D_left,
                                      rvec_left, tvec_left, false, SOLVEPNP_ITERATIVE);
        
        // Solve PnP for right camera
        bool success_right = solvePnP(object_points[i], right_image_points[i], K_right, D_right,
                                       rvec_right, tvec_right, false, SOLVEPNP_ITERATIVE);
        
        if (success_left && success_right) {
            rvecs_left.push_back(Vec3d(rvec_left.at<double>(0), rvec_left.at<double>(1), rvec_left.at<double>(2)));
            tvecs_left.push_back(Vec3d(tvec_left.at<double>(0), tvec_left.at<double>(1), tvec_left.at<double>(2)));
            rvecs_right.push_back(Vec3d(rvec_right.at<double>(0), rvec_right.at<double>(1), rvec_right.at<double>(2)));
            tvecs_right.push_back(Vec3d(tvec_right.at<double>(0), tvec_right.at<double>(1), tvec_right.at<double>(2)));
            
            printf("  Frame %zu: OK\n", i);
        } else {
            printf("  Frame %zu: solvePnP failed\n", i);
        }
    }
    
    if (rvecs_left.size() < 2) {
        cerr << "Error: Need at least 2 valid poses for hand-eye calibration, got " << rvecs_left.size() << endl;
        return 1;
    }
    
    printf("Successfully computed %zu board poses\n\n", rvecs_left.size());
    fflush(stdout);
    
    // Solve hand-eye calibration (AX=XB)
    printf("Solving hand-eye calibration (AX=XB)...\n");
    
    Mat R_handeye, t_handeye;
    bool success = double_sphere::solveHandEyeCalibration(rvecs_left, tvecs_left,
                                                          rvecs_right, tvecs_right,
                                                          R_handeye, t_handeye);
    
    if (!success) {
        cerr << "Error: Hand-eye calibration failed" << endl;
        return 1;
    }
    
    // Compute rotation angle
    Mat rvec_handeye;
    Rodrigues(R_handeye, rvec_handeye);
    double angle_norm = norm(rvec_handeye);
    double angle_deg = angle_norm * 180.0 / CV_PI;
    
    printf("\nHand-eye calibration successful!\n");
    printf("Rotation (R_right_left):\n");
    printf("  [%.6f, %.6f, %.6f]\n", R_handeye.at<double>(0,0), R_handeye.at<double>(0,1), R_handeye.at<double>(0,2));
    printf("  [%.6f, %.6f, %.6f]\n", R_handeye.at<double>(1,0), R_handeye.at<double>(1,1), R_handeye.at<double>(1,2));
    printf("  [%.6f, %.6f, %.6f]\n", R_handeye.at<double>(2,0), R_handeye.at<double>(2,1), R_handeye.at<double>(2,2));
    printf("\nTranslation (T_right_left):\n");
    printf("  [%.6f, %.6f, %.6f]\n", t_handeye.at<double>(0), t_handeye.at<double>(1), t_handeye.at<double>(2));
    printf("\nRotation angle: %.4f degrees (%.4f radians)\n", angle_deg, angle_norm);
    printf("Baseline: %.6f meters (%.2f mm)\n", norm(t_handeye), norm(t_handeye) * 1000.0);
    fflush(stdout);
    
    // Save to file
    printf("\nSaving hand-eye calibration to %s...\n", out_file);
    
    // Ensure output directory exists
    if (!ensure_output_directory(out_file)) {
        cerr << "Error: Cannot create output directory for: " << out_file << endl;
        return 1;
    }
    
    FileStorage fs(out_file, FileStorage::WRITE);
    if (!fs.isOpened()) {
        cerr << "Error: Cannot open output file for writing: " << out_file << endl;
        cerr << "Please check that you have write permissions." << endl;
        return 1;
    }
    
    fs << "method" << "hand_eye_tsai";
    fs << "num_frames" << (int)rvecs_left.size();
    fs << "R" << R_handeye;
    fs << "T" << t_handeye;
    fs << "rotation_angle_deg" << angle_deg;
    fs << "rotation_angle_rad" << angle_norm;
    fs << "baseline_meters" << norm(t_handeye);
    fs.release();
    
    // Ensure file is fully written to disk
    sync();
    
    // Verify that the file was created and is readable
    // This is a workaround for potential race conditions with file system sync
    int max_retries = 20;  // Increased from 10 to 20 for better reliability
    bool file_exists = false;
    for (int retry = 0; retry < max_retries; retry++) {
        if (access(out_file, F_OK) == 0 && access(out_file, R_OK) == 0) {
            file_exists = true;
            printf("[DEBUG] File verified on retry %d\n", retry);
            fflush(stdout);
            break;
        }
        // Wait a short time before retrying (50ms - increased from 10ms)
        usleep(50000);
    }
    
    if (!file_exists) {
        cerr << "Error: Failed to verify file creation: " << out_file << endl;
        cerr << "The file was written but could not be verified to exist." << endl;
        return 1;
    }
    
    printf("Hand-eye calibration saved successfully!\n");
    printf("\nNext steps:\n");
    printf("  1. Use this file with calibrate_ds: --init-extrinsic %s\n", out_file);
    printf("  2. Run full stereo calibration with joint BA: --joint-ba\n");
    printf("  3. Expected improvement: Initial rotation error < 0.2° (vs ~2° with MEI)\n");
    fflush(stdout);
    
    return 0;
}
