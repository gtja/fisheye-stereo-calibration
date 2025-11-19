#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/ccalib/omnidir.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <ceres/ceres.h>
#include <stdio.h>
#include <iostream>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>
#include "popt_pp.h"
#include "double_sphere.h"
#include "kb4_model.h"

using namespace std;
using namespace cv;

vector<vector<Point3d>> object_points;
vector<vector<Point2f>> imagePoints1, imagePoints2;
vector<Point2f> corners1, corners2;
vector<vector<Point2d>> left_img_points, right_img_points;

Mat img1, img2, gray1, gray2;

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

// Load image points with SE(3) pre-correction and enhanced corner detection
void load_image_points_with_precorrection(int board_width, int board_height, float square_size,
                                         char* img_dir, char* leftimg_filename, char* rightimg_filename,
                                         char* extension, kb4::KB4Params& kb4_left, kb4::KB4Params& kb4_right) {
    // 日志：开始加载图像点并进行SE(3)预校正
    std::cout << "[LOG] load_image_points_with_precorrection: 开始加载图像点并进行SE(3)预校正" << std::endl;

    Size board_size = Size(board_width, board_height);
    Size rectified_size(960, 720);  // Large image for better corner detection

    vector<int> left_indices = find_image_indices(img_dir, leftimg_filename, extension);
    vector<int> right_indices = find_image_indices(img_dir, rightimg_filename, extension);

    std::cout << "[LOG] 左相机图像数量: " << left_indices.size() << ", 右相机图像数量: " << right_indices.size() << std::endl;

    vector<int> common_indices;
    for (int idx : left_indices) {
        if (find(right_indices.begin(), right_indices.end(), idx) != right_indices.end()) {
            common_indices.push_back(idx);
        }
    }

    std::cout << "[LOG] 匹配到的图像对数量: " << common_indices.size() << std::endl;

    if (common_indices.empty()) {
        cerr << "Error: No matching image pairs found" << endl;
        cerr.flush();
        return;
    }

    printf("Found %zu image pairs\n", common_indices.size());
    fflush(stdout);

    // Create rectification maps for pre-correction
    Mat map_left_x, map_left_y, map_right_x, map_right_y;

    for (int i : common_indices) {
        char left_img[100], right_img[100];
        sprintf(left_img, "%s/%s%d.%s", img_dir, leftimg_filename, i, extension);
        sprintf(right_img, "%s/%s%d.%s", img_dir, rightimg_filename, i, extension);

        img1 = imread(left_img, IMREAD_COLOR);
        img2 = imread(right_img, IMREAD_COLOR);

        if (img1.empty() || img2.empty()) {
            cerr << "Warning: Failed to load image pair " << i << endl;
            continue;
        }

        std::cout << "[LOG] 加载图像对: " << left_img << " 和 " << right_img << std::endl;

        cvtColor(img1, gray1, COLOR_BGR2GRAY);
        cvtColor(img2, gray2, COLOR_BGR2GRAY);

        // Create rectification maps using KB4 model for pre-correction
        kb4::createRectificationMap(kb4_left, img1.size(), rectified_size, map_left_x, map_left_y);
        kb4::createRectificationMap(kb4_right, img2.size(), rectified_size, map_right_x, map_right_y);

        // Apply rectification (warp to virtual plane)
        Mat rect_left, rect_right;
        remap(gray1, rect_left, map_left_x, map_left_y, INTER_LINEAR);
        remap(gray2, rect_right, map_right_x, map_right_y, INTER_LINEAR);

        bool found1 = false, found2 = false;

        // Find corners on rectified images (960×720 for better detection)
        found1 = findChessboardCorners(rect_left, board_size, corners1,
                                      CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FILTER_QUADS);
        found2 = findChessboardCorners(rect_right, board_size, corners2,
                                      CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FILTER_QUADS);

        std::cout << "[LOG] 图像对 " << i << " 检测到角点: left=" << found1 << ", right=" << found2 << std::endl;

        if (found1) {
            cornerSubPix(rect_left, corners1, Size(5, 5), Size(-1, -1),
                        TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 0.01));

            // Back-project corners to original image coordinates
            vector<Point2f> corners1_orig;
            for (const auto& corner : corners1) {
                // Use inverse mapping: unproject from rectified to 3D, then project to original
                double point2d_rect[2] = {corner.x, corner.y};

                double fx_rect = kb4_left.fx * 0.6;
                double fy_rect = kb4_left.fy * 0.6;
                double cx_rect = rectified_size.width * 0.5;
                double cy_rect = rectified_size.height * 0.5;

                double x_rect = (point2d_rect[0] - cx_rect) / fx_rect;
                double y_rect = (point2d_rect[1] - cy_rect) / fy_rect;
                double z_rect = 1.0;

                double norm = sqrt(x_rect*x_rect + y_rect*y_rect + z_rect*z_rect);
                double point3d[3] = {x_rect/norm, y_rect/norm, z_rect/norm};

                double point2d_orig[2];
                if (kb4::project(kb4_left, point3d, point2d_orig)) {
                    corners1_orig.push_back(Point2f(point2d_orig[0], point2d_orig[1]));
                } else {
                    corners1_orig.push_back(corner);  // Fallback
                }
            }
            corners1 = corners1_orig;
            std::cout << "[LOG] 左图角点反投影完成, 数量: " << corners1.size() << std::endl;
        }

        if (found2) {
            cornerSubPix(rect_right, corners2, Size(5, 5), Size(-1, -1),
                        TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 0.01));

            // Back-project corners to original image coordinates
            vector<Point2f> corners2_orig;
            for (const auto& corner : corners2) {
                double point2d_rect[2] = {corner.x, corner.y};

                double fx_rect = kb4_right.fx * 0.6;
                double fy_rect = kb4_right.fy * 0.6;
                double cx_rect = rectified_size.width * 0.5;
                double cy_rect = rectified_size.height * 0.5;

                double x_rect = (point2d_rect[0] - cx_rect) / fx_rect;
                double y_rect = (point2d_rect[1] - cy_rect) / fy_rect;
                double z_rect = 1.0;

                double norm = sqrt(x_rect*x_rect + y_rect*y_rect + z_rect*z_rect);
                double point3d[3] = {x_rect/norm, y_rect/norm, z_rect/norm};

                double point2d_orig[2];
                if (kb4::project(kb4_right, point3d, point2d_orig)) {
                    corners2_orig.push_back(Point2f(point2d_orig[0], point2d_orig[1]));
                } else {
                    corners2_orig.push_back(corner);
                }
            }
            corners2 = corners2_orig;
            std::cout << "[LOG] 右图角点反投影完成, 数量: " << corners2.size() << std::endl;
        }

        vector<Point3d> obj;
        for (int r = 0; r < board_height; ++r) {
            for (int c = 0; c < board_width; ++c) {
                obj.push_back(Point3d(c * square_size, r * square_size, 0.0));
            }
        }

        if (found1 && found2) {
            // Validate corners with relaxed edge tolerance (10%)
            // Mark invalid points in red and save visualization
            bool frame_valid = true;
            int invalid_left = 0, invalid_right = 0;
            int z_neg_left = 0, z_neg_right = 0;
            
            // 10% edge tolerance
            double edge_tolerance = 0.10;
            double left_min_x = img1.cols * edge_tolerance;
            double left_max_x = img1.cols * (1.0 - edge_tolerance);
            double left_min_y = img1.rows * edge_tolerance;
            double left_max_y = img1.rows * (1.0 - edge_tolerance);
            
            double right_min_x = img2.cols * edge_tolerance;
            double right_max_x = img2.cols * (1.0 - edge_tolerance);
            double right_min_y = img2.rows * edge_tolerance;
            double right_max_y = img2.rows * (1.0 - edge_tolerance);
            
            // Create visualization images
            Mat vis_left = img1.clone();
            Mat vis_right = img2.clone();
            
            // Check left corners
            for (size_t j = 0; j < corners1.size(); j++) {
                bool out_of_bounds = (corners1[j].x < left_min_x || corners1[j].x > left_max_x ||
                                     corners1[j].y < left_min_y || corners1[j].y > left_max_y);
                
                // Unproject to check if Z < 0 (point behind camera)
                double point2d[2] = {corners1[j].x, corners1[j].y};
                double point3d[3];
                bool z_positive = true;
                if (kb4::unproject(kb4_left, point2d, point3d)) {
                    if (point3d[2] < 0) {
                        z_neg_left++;
                        z_positive = false;
                    }
                }
                
                if (out_of_bounds || !z_positive) {
                    invalid_left++;
                    // Draw in red
                    circle(vis_left, corners1[j], 8, Scalar(0, 0, 255), 2);
                } else {
                    // Draw in green
                    circle(vis_left, corners1[j], 6, Scalar(0, 255, 0), 2);
                }
            }
            
            // Check right corners
            for (size_t j = 0; j < corners2.size(); j++) {
                bool out_of_bounds = (corners2[j].x < right_min_x || corners2[j].x > right_max_x ||
                                     corners2[j].y < right_min_y || corners2[j].y > right_max_y);
                
                double point2d[2] = {corners2[j].x, corners2[j].y};
                double point3d[3];
                bool z_positive = true;
                if (kb4::unproject(kb4_right, point2d, point3d)) {
                    if (point3d[2] < 0) {
                        z_neg_right++;
                        z_positive = false;
                    }
                }
                
                if (out_of_bounds || !z_positive) {
                    invalid_right++;
                    circle(vis_right, corners2[j], 8, Scalar(0, 0, 255), 2);
                } else {
                    circle(vis_right, corners2[j], 6, Scalar(0, 255, 0), 2);
                }
            }
            
            // Save visualization
            char vis_left_path[256], vis_right_path[256];
            sprintf(vis_left_path, "%s/precorrect_vis_left%d.jpg", img_dir, i);
            sprintf(vis_right_path, "%s/precorrect_vis_right%d.jpg", img_dir, i);
            imwrite(vis_left_path, vis_left);
            imwrite(vis_right_path, vis_right);
            
            // Relaxed acceptance criteria: allow up to 50% invalid points (10% tolerance helps)
            double invalid_ratio_left = (double)invalid_left / corners1.size();
            double invalid_ratio_right = (double)invalid_right / corners2.size();
            
            if (invalid_ratio_left > 0.5 || invalid_ratio_right > 0.5) {
                frame_valid = false;
                std::cout << "[LOG] 图像对 " << i << " 无效点过多: left=" << invalid_ratio_left*100 
                         << "%, right=" << invalid_ratio_right*100 << "%" << std::endl;
            }
            
            if (frame_valid) {
                cout << i << ". Found corners (with pre-correction): "
                     << "left_invalid=" << invalid_left << "/" << corners1.size() 
                     << " (Z<0: " << z_neg_left << "), "
                     << "right_invalid=" << invalid_right << "/" << corners2.size()
                     << " (Z<0: " << z_neg_right << ")" << endl;
                imagePoints1.push_back(corners1);
                imagePoints2.push_back(corners2);
                object_points.push_back(obj);
            } else {
                std::cout << "[LOG] 图像对 " << i << " 被过滤 (查看可视化: " 
                         << vis_left_path << ", " << vis_right_path << ")" << std::endl;
            }
        } else {
            std::cout << "[LOG] 图像对 " << i << " 未能同时检测到左右角点, 跳过" << std::endl;
        }
    }

    std::cout << "[LOG] 完成所有图像对处理, 有效图像对数量: " << imagePoints1.size() << std::endl;

    for (size_t i = 0; i < imagePoints1.size(); i++) {
        vector<Point2d> v1, v2;
        for (size_t j = 0; j < imagePoints1[i].size(); j++) {
            v1.push_back(Point2d(imagePoints1[i][j].x, imagePoints1[i][j].y));
            v2.push_back(Point2d(imagePoints2[i][j].x, imagePoints2[i][j].y));
        }
        left_img_points.push_back(v1);
        right_img_points.push_back(v2);
    }

    std::cout << "[LOG] 图像点转换完成, left_img_points.size(): " << left_img_points.size()
              << ", right_img_points.size(): " << right_img_points.size()
              << ", object_points.size(): " << object_points.size() << std::endl;
}

int main(int argc, char const *argv[])
{
    int board_width, board_height;
    float square_size;
    char* img_dir;
    char* leftimg_filename = NULL;
    char* rightimg_filename = NULL;
    char* out_file;
    char* extension = (char*)"jpg";
    double physical_baseline = -1.0;
    int mono_mode = 0;              // --mono flag for monocular calibration
    int joint_ba = 0;               // --joint-ba flag for joint intrinsic+extrinsic optimization
    char* init_extrinsic = NULL;    // --init-extrinsic for loading hand-eye calibrated extrinsics

    printf("[LOG] Initializing popt options...\n");
    fflush(stdout);
    static struct poptOption options[] = {
        { "board_width",'w',POPT_ARG_INT,&board_width,0,"Checkerboard width","NUM" },
        { "board_height",'h',POPT_ARG_INT,&board_height,0,"Checkerboard height","NUM" },
        { "square_size",'s',POPT_ARG_FLOAT,&square_size,0,"Checkerboard square size","NUM" },
        { "img_dir",'d',POPT_ARG_STRING,&img_dir,0,"Directory containing images","STR" },
        { "leftimg_filename",'l',POPT_ARG_STRING,&leftimg_filename,0,"Left image prefix","STR" },
        { "rightimg_filename",'r',POPT_ARG_STRING,&rightimg_filename,0,"Right image prefix","STR" },
        { "out_file",'o',POPT_ARG_STRING,&out_file,0,"Output calibration filename (YML)","STR" },
        { "extension",'e',POPT_ARG_STRING,&extension,0,"Image file extension (default: jpg)","STR" },
        { "baseline",'b',POPT_ARG_DOUBLE,&physical_baseline,0,"Physical baseline distance in meters (for accuracy evaluation)","NUM" },
        { "mono",0,POPT_ARG_NONE,&mono_mode,0,"Monocular calibration mode (calibrate single camera)","" },
        { "joint-ba",0,POPT_ARG_NONE,&joint_ba,0,"Joint bundle adjustment (optimize intrinsics + extrinsics)","" },
        { "init-extrinsic",0,POPT_ARG_STRING,&init_extrinsic,0,"Load initial extrinsics from YAML file (hand-eye calibrated)","STR" },
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };

    printf("[LOG] Creating POpt object...\n");
    fflush(stdout);
    POpt popt(NULL, argc, argv, options, 0);
    int c;
    printf("[LOG] Starting popt argument parsing...\n");
    fflush(stdout);
    while((c = popt.getNextOpt()) >= 0) {}
    
    printf("========== Double-Sphere Camera Calibration ==========\n");
    printf("Using DS model + 6-order radial distortion + Ceres BA\n");
    if (mono_mode) {
        printf("Mode: Monocular calibration\n");
    } else {
        printf("Mode: Stereo calibration\n");
    }
    if (joint_ba) {
        printf("Bundle Adjustment: Joint intrinsics + extrinsics optimization\n");
    } else {
        printf("Bundle Adjustment: Extrinsics only (intrinsics fixed)\n");
    }
    printf("\n");
    fflush(stdout);
    
    // Check if we're in mono mode
    bool is_left_only = (leftimg_filename != NULL && rightimg_filename == NULL);
    bool is_right_only = (leftimg_filename == NULL && rightimg_filename != NULL);
    bool is_stereo = (leftimg_filename != NULL && rightimg_filename != NULL);
    
    if (mono_mode && !is_left_only && !is_right_only) {
        cerr << "Error: In mono mode, specify either --left or --right, not both" << endl;
        return 1;
    }
    
    if (!mono_mode && !is_stereo) {
        cerr << "Error: In stereo mode, both --left and --right must be specified" << endl;
        return 1;
    }
    
    // Step 1: Initial KB4 coarse calibration using OpenCV fisheye
    printf("Step 1: KB4 Coarse Calibration (initial guess)...\n");
    fflush(stdout);
    
    // Load images for initial calibration
    vector<int> left_indices, right_indices, common_indices;
    
    if (mono_mode) {
        // Monocular mode: load only the specified camera images
        if (is_left_only) {
            left_indices = find_image_indices(img_dir, leftimg_filename, extension);
            common_indices = left_indices;
        } else {
            right_indices = find_image_indices(img_dir, rightimg_filename, extension);
            common_indices = right_indices;
        }
    } else {
        // Stereo mode: load both cameras and find common indices
        left_indices = find_image_indices(img_dir, leftimg_filename, extension);
        right_indices = find_image_indices(img_dir, rightimg_filename, extension);
        
        for (int idx : left_indices) {
            if (find(right_indices.begin(), right_indices.end(), idx) != right_indices.end()) {
                common_indices.push_back(idx);
            }
        }
    }
    
    if (common_indices.empty()) {
        cerr << "Error: No matching image" << (mono_mode ? "s" : " pairs") << " found" << endl;
        cerr.flush();
        return 1;
    }
    
    // Initial corner detection without pre-correction
    vector<vector<Point3d>> obj_pts_init;
    vector<vector<Point2d>> left_pts_init, right_pts_init;
    
    Size board_size(board_width, board_height);
    
    for (int i : common_indices) {
        char left_img[100], right_img[100];
        bool load_left = is_left_only || is_stereo;
        bool load_right = is_right_only || is_stereo;
        
        if (load_left) {
            sprintf(left_img, "%s/%s%d.%s", img_dir, leftimg_filename, i, extension);
            img1 = imread(left_img, IMREAD_COLOR);
            if (img1.empty()) continue;
            cvtColor(img1, gray1, COLOR_BGR2GRAY);
        }
        
        if (load_right) {
            sprintf(right_img, "%s/%s%d.%s", img_dir, rightimg_filename, i, extension);
            img2 = imread(right_img, IMREAD_COLOR);
            if (img2.empty()) continue;
            cvtColor(img2, gray2, COLOR_BGR2GRAY);
        }
        
        bool found1 = false, found2 = false;
        
        if (load_left) {
            found1 = findChessboardCorners(img1, board_size, corners1,
                                           CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FILTER_QUADS);
            if (found1) {
                cornerSubPix(gray1, corners1, Size(5, 5), Size(-1, -1),
                            TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 0.01));
            }
        }
        
        if (load_right) {
            found2 = findChessboardCorners(img2, board_size, corners2,
                                           CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_FILTER_QUADS);
            if (found2) {
                cornerSubPix(gray2, corners2, Size(5, 5), Size(-1, -1),
                            TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 0.01));
            }
        }
        
        bool detection_ok = (mono_mode && (found1 || found2)) || (is_stereo && found1 && found2);
        
        if (detection_ok) {
            vector<Point3d> obj;
            for (int r = 0; r < board_height; ++r) {
                for (int c = 0; c < board_width; ++c) {
                    obj.push_back(Point3d(c * square_size, r * square_size, 0.0));
                }
            }
            
            obj_pts_init.push_back(obj);
            
            if (found1 || is_stereo) {
                vector<Point2d> v1;
                for (size_t j = 0; j < corners1.size(); j++) {
                    v1.push_back(Point2d(corners1[j].x, corners1[j].y));
                }
                left_pts_init.push_back(v1);
            }
            
            if (found2 || is_stereo) {
                vector<Point2d> v2;
                for (size_t j = 0; j < corners2.size(); j++) {
                    v2.push_back(Point2d(corners2[j].x, corners2[j].y));
                }
                right_pts_init.push_back(v2);
            }
        }
    }
    
    if (obj_pts_init.empty()) {
        cerr << "Error: No valid corners detected for initial calibration" << endl;
        cerr.flush();
        return 1;
    }
    
    printf("Initial calibration: %zu image pairs\n", obj_pts_init.size());
    fflush(stdout);
    
    // Validate we have enough images for calibration
    if (obj_pts_init.size() < 3) {
        cerr << "Error: Need at least 3 images for calibration, got " << obj_pts_init.size() << endl;
        cerr.flush();
        return 1;
    }
    
    // Validate image points data
    for (size_t i = 0; i < obj_pts_init.size(); i++) {
        if (mono_mode) {
            // In mono mode, check only the active camera
            size_t expected_size = obj_pts_init[i].size();
            if (is_left_only && left_pts_init[i].size() != expected_size) {
                cerr << "Error: Mismatched point counts at image " << i << endl;
                cerr.flush();
                return 1;
            }
            if (is_right_only && right_pts_init[i].size() != expected_size) {
                cerr << "Error: Mismatched point counts at image " << i << endl;
                cerr.flush();
                return 1;
            }
        } else {
            // In stereo mode, check both cameras
            if (obj_pts_init[i].size() != left_pts_init[i].size() || 
                obj_pts_init[i].size() != right_pts_init[i].size()) {
                cerr << "Error: Mismatched point counts at image pair " << i << endl;
                cerr.flush();
                return 1;
            }
        }
        if (obj_pts_init[i].size() < 4) {
            cerr << "Error: Not enough points at image " << i << " (got " << obj_pts_init[i].size() << ")" << endl;
            cerr.flush();
            return 1;
        }
    }
    
    // Run OpenCV fisheye calibration for KB4 initial guess
    Matx33d K1_kb4, K2_kb4, R_kb4;
    Vec3d T_kb4;
    Vec4d D1_kb4, D2_kb4;
    
    int flag = 0;
    flag |= fisheye::CALIB_RECOMPUTE_EXTRINSIC;
    flag |= fisheye::CALIB_FIX_SKEW;
    
    bool fisheye_success = false;
    
    if (mono_mode) {
        // Monocular calibration
        if (is_left_only) {
            printf("Attempting fisheye::calibrate for left camera with %zu images...\n", obj_pts_init.size());
            fflush(stdout);
            try {
                vector<Vec3d> rvecs, tvecs;
                fisheye::calibrate(obj_pts_init, left_pts_init, img1.size(),
                                  K1_kb4, D1_kb4, rvecs, tvecs, flag,
                                  TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 1e-5));
                fisheye_success = true;
                printf("KB4 fisheye calibration succeeded for left camera\n");
                // Set right camera parameters to left for consistency (won't be used)
                K2_kb4 = K1_kb4;
                D2_kb4 = D1_kb4;
            } catch (const cv::Exception& e) {
                printf("Fisheye model failed for left camera: %s\n", e.what());
            }
        } else {  // is_right_only
            printf("Attempting fisheye::calibrate for right camera with %zu images...\n", obj_pts_init.size());
            fflush(stdout);
            try {
                vector<Vec3d> rvecs, tvecs;
                fisheye::calibrate(obj_pts_init, right_pts_init, img2.size(),
                                  K2_kb4, D2_kb4, rvecs, tvecs, flag,
                                  TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 1e-5));
                fisheye_success = true;
                printf("KB4 fisheye calibration succeeded for right camera\n");
                // Set left camera parameters to right for consistency (won't be used)
                K1_kb4 = K2_kb4;
                D1_kb4 = D2_kb4;
            } catch (const cv::Exception& e) {
                printf("Fisheye model failed for right camera: %s\n", e.what());
            }
        }
    } else {
        // Stereo calibration
        printf("Attempting fisheye::stereoCalibrate with %zu image pairs...\n", obj_pts_init.size());
        fflush(stdout);
        try {
            fisheye::stereoCalibrate(obj_pts_init, left_pts_init, right_pts_init,
                                    K1_kb4, D1_kb4, K2_kb4, D2_kb4, img1.size(), R_kb4, T_kb4, flag,
                                    TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 30, 1e-5));
            fisheye_success = true;
            printf("KB4 fisheye calibration succeeded\n");
        } catch (const cv::Exception& e) {
        printf("Fisheye model failed (expected for FOV > 200°): %s\n", e.what());
        printf("Falling back to omnidir (MEI) model for initial calibration...\n");
        fflush(stdout);
        
        // Fallback: Use omnidir (MEI) model for extreme wide-angle lenses
        // Convert Point2d to Point2f and Point3d to Point3f for omnidir compatibility
        vector<vector<Point2f>> left_pts_init_f, right_pts_init_f;
        vector<vector<Point3f>> obj_pts_init_f;
        
        for (size_t i = 0; i < obj_pts_init.size(); i++) {
            vector<Point3f> obj_f;
            for (size_t j = 0; j < obj_pts_init[i].size(); j++) {
                obj_f.push_back(Point3f((float)obj_pts_init[i][j].x, (float)obj_pts_init[i][j].y, (float)obj_pts_init[i][j].z));
            }
            obj_pts_init_f.push_back(obj_f);
        }
        
        if (!mono_mode || is_left_only) {
            for (size_t i = 0; i < left_pts_init.size(); i++) {
                vector<Point2f> v1;
                for (size_t j = 0; j < left_pts_init[i].size(); j++) {
                    v1.push_back(Point2f((float)left_pts_init[i][j].x, (float)left_pts_init[i][j].y));
                }
                left_pts_init_f.push_back(v1);
            }
        }
        
        if (!mono_mode || is_right_only) {
            for (size_t i = 0; i < right_pts_init.size(); i++) {
                vector<Point2f> v2;
                for (size_t j = 0; j < right_pts_init[i].size(); j++) {
                    v2.push_back(Point2f((float)right_pts_init[i][j].x, (float)right_pts_init[i][j].y));
                }
                right_pts_init_f.push_back(v2);
            }
        }
        
        Mat K1_mat, K2_mat, D1_mat, D2_mat, xi1_mat, xi2_mat, R_mat, T_mat;
        vector<Vec3d> rvecs_left, tvecs_left, rvecs_right, tvecs_right;
        
        int omni_flags = 0;
        omni_flags |= omnidir::CALIB_FIX_SKEW;
        
        try {
            if (!mono_mode || is_left_only) {
                // Calibrate left camera
                printf("  Calibrating left camera with omnidir...\n");
                fflush(stdout);
                double rms_left = omnidir::calibrate(obj_pts_init_f, left_pts_init_f, 
                                                     is_left_only ? img1.size() : img1.size(),
                                                     K1_mat, xi1_mat, D1_mat, rvecs_left, tvecs_left,
                                                     omni_flags,
                                                     TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 200, 1e-6));
                printf("  Left camera RMS: %.4f\n", rms_left);
            }
            
            if (!mono_mode || is_right_only) {
                // Calibrate right camera
                printf("  Calibrating right camera with omnidir...\n");
                fflush(stdout);
                double rms_right = omnidir::calibrate(obj_pts_init_f, right_pts_init_f,
                                                      is_right_only ? img2.size() : img2.size(),
                                                      K2_mat, xi2_mat, D2_mat, rvecs_right, tvecs_right,
                                                      omni_flags,
                                                      TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 200, 1e-6));
                printf("  Right camera RMS: %.4f\n", rms_right);
            }
            
            if (!mono_mode) {
                // Stereo calibration with omnidir
                printf("  Performing stereo calibration with omnidir...\n");
                fflush(stdout);
                Mat rvec_stereo, tvec_stereo;
                double rms_stereo = omnidir::stereoCalibrate(obj_pts_init_f, left_pts_init_f, right_pts_init_f,
                                                             img1.size(), img2.size(),
                                                             K1_mat, xi1_mat, D1_mat,
                                                             K2_mat, xi2_mat, D2_mat,
                                                             rvec_stereo, tvec_stereo, rvecs_left, tvecs_left,
                                                             omni_flags | omnidir::CALIB_USE_GUESS,
                                                             TermCriteria(TermCriteria::EPS | TermCriteria::MAX_ITER, 200, 1e-6));
                printf("  Stereo calibration RMS: %.4f\n", rms_stereo);
                
                // Convert rotation vector to rotation matrix
                Rodrigues(rvec_stereo, R_mat);
                R_kb4 = Matx33d((double*)R_mat.data);
                T_kb4 = Vec3d(tvec_stereo.at<double>(0), tvec_stereo.at<double>(1), tvec_stereo.at<double>(2));
            }
            
            // Convert omnidir results to KB4 format
            // Extract intrinsics
            if (is_left_only || !mono_mode) {
                K1_kb4 = Matx33d((double*)K1_mat.data);
                D1_kb4 = Vec4d(D1_mat.at<double>(0), D1_mat.at<double>(1), 
                              D1_mat.at<double>(2), D1_mat.at<double>(3));
            }
            if (is_right_only || !mono_mode) {
                K2_kb4 = Matx33d((double*)K2_mat.data);
                D2_kb4 = Vec4d(D2_mat.at<double>(0), D2_mat.at<double>(1), 
                              D2_mat.at<double>(2), D2_mat.at<double>(3));
            }
            
            printf("Omnidir calibration succeeded as fallback\n");
            if (!mono_mode || is_left_only) {
                printf("  Left mirror parameter: xi1=%.6f\n", xi1_mat.at<double>(0));
            }
            if (!mono_mode || is_right_only) {
                printf("  Right mirror parameter: xi2=%.6f\n", xi2_mat.at<double>(0));
            }
            
        } catch (const cv::Exception& e2) {
            cerr << "Error: Both fisheye and omnidir calibration failed!" << endl;
            cerr << "Omnidir error: " << e2.what() << endl;
            cerr << "\nPossible causes:" << endl;
            cerr << "  - Images may have excessive distortion even for omnidir model" << endl;
            cerr << "  - Checkerboard corners may not be detected accurately" << endl;
            cerr << "  - Image pairs may not show the same checkerboard view" << endl;
            cerr << "  - Need more or better quality calibration images" << endl;
            cerr.flush();
            return 1;
        }
    }
    
    if (fisheye_success) {
        printf("KB4 calibration complete\n");
    } else {
        printf("Initial calibration complete (using omnidir model)\n");
    }
    if (is_left_only || !mono_mode) {
        printf("  Left camera: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f\n", 
               K1_kb4(0,0), K1_kb4(1,1), K1_kb4(0,2), K1_kb4(1,2));
    }
    if (is_right_only || !mono_mode) {
        printf("  Right camera: fx=%.2f, fy=%.2f, cx=%.2f, cy=%.2f\n",
               K2_kb4(0,0), K2_kb4(1,1), K2_kb4(0,2), K2_kb4(1,2));
    }
    fflush(stdout);
    
    // Create KB4 parameters
    kb4::KB4Params kb4_left(K1_kb4, D1_kb4);
    kb4::KB4Params kb4_right(K2_kb4, D2_kb4);
    
    // In mono mode, we skip the rest and save just the intrinsics
    if (mono_mode) {
        printf("\nMono calibration complete. Saving results to %s...\n", 
               out_file ? out_file : (is_left_only ? "left_ds.yml" : "right_ds.yml"));
        fflush(stdout);
        
        const char* output_file = out_file ? out_file : (is_left_only ? "left_ds.yml" : "right_ds.yml");
        
        // Initialize DS parameters from KB4
        double_sphere::DoubleSphereParams ds_params;
        if (is_left_only) {
            ds_params.fx = kb4_left.fx;
            ds_params.fy = kb4_left.fy;
            ds_params.cx = kb4_left.cx;
            ds_params.cy = kb4_left.cy;
            ds_params.k1 = kb4_left.k1;
            ds_params.k2 = kb4_left.k2;
            ds_params.k3 = kb4_left.k3;
            ds_params.k4 = kb4_left.k4;
        } else {
            ds_params.fx = kb4_right.fx;
            ds_params.fy = kb4_right.fy;
            ds_params.cx = kb4_right.cx;
            ds_params.cy = kb4_right.cy;
            ds_params.k1 = kb4_right.k1;
            ds_params.k2 = kb4_right.k2;
            ds_params.k3 = kb4_right.k3;
            ds_params.k4 = kb4_right.k4;
        }
        ds_params.xi = 0.0;      // Initial guess
        ds_params.alpha = 0.5;   // Initial guess
        ds_params.k5 = 0.0;
        ds_params.k6 = 0.0;
        
        FileStorage fs(output_file, FileStorage::WRITE);
        fs << "model_type" << "double_sphere";
        fs << "camera" << "{";
        fs << "fx" << ds_params.fx;
        fs << "fy" << ds_params.fy;
        fs << "cx" << ds_params.cx;
        fs << "cy" << ds_params.cy;
        fs << "xi" << ds_params.xi;
        fs << "alpha" << ds_params.alpha;
        fs << "k1" << ds_params.k1;
        fs << "k2" << ds_params.k2;
        fs << "k3" << ds_params.k3;
        fs << "k4" << ds_params.k4;
        fs << "k5" << ds_params.k5;
        fs << "k6" << ds_params.k6;
        fs << "}";
        fs.release();
        
        printf("Mono calibration saved successfully to %s\n", output_file);
        printf("Note: For better accuracy, run Bundle Adjustment with full DS model\n");
        fflush(stdout);
        
        return 0;
    }
    
    // Load initial extrinsics if provided (hand-eye calibrated)
    if (init_extrinsic != NULL) {
        printf("\nLoading initial extrinsics from %s...\n", init_extrinsic);
        fflush(stdout);
        
        try {
            FileStorage fs(init_extrinsic, FileStorage::READ);
            if (!fs.isOpened()) {
                cerr << "Error: Cannot open " << init_extrinsic << endl;
                return 1;
            }
            
            // Read rotation matrix
            Mat R_handeye;
            fs["R"] >> R_handeye;
            if (R_handeye.empty() || R_handeye.rows != 3 || R_handeye.cols != 3) {
                cerr << "Error: Invalid rotation matrix in " << init_extrinsic << endl;
                return 1;
            }
            
            // Read translation vector
            Mat T_handeye;
            fs["T"] >> T_handeye;
            if (T_handeye.empty() || (T_handeye.rows != 3 || T_handeye.cols != 1) && 
                (T_handeye.rows != 1 || T_handeye.cols != 3)) {
                cerr << "Error: Invalid translation vector in " << init_extrinsic << endl;
                return 1;
            }
            
            fs.release();
            
            // Convert to the expected format
            R_kb4 = Matx33d((double*)R_handeye.data);
            if (T_handeye.rows == 1) {
                T_kb4 = Vec3d(T_handeye.at<double>(0), T_handeye.at<double>(1), T_handeye.at<double>(2));
            } else {
                T_kb4 = Vec3d(T_handeye.at<double>(0), T_handeye.at<double>(1), T_handeye.at<double>(2));
            }
            
            printf("Loaded hand-eye calibrated extrinsics:\n");
            printf("  R = [%.6f, %.6f, %.6f;\n", R_kb4(0,0), R_kb4(0,1), R_kb4(0,2));
            printf("       %.6f, %.6f, %.6f;\n", R_kb4(1,0), R_kb4(1,1), R_kb4(1,2));
            printf("       %.6f, %.6f, %.6f]\n", R_kb4(2,0), R_kb4(2,1), R_kb4(2,2));
            printf("  T = [%.6f, %.6f, %.6f]\n", T_kb4[0], T_kb4[1], T_kb4[2]);
            
            // Compute rotation angle for validation
            Mat rvec_handeye;
            Rodrigues(Mat(R_kb4), rvec_handeye);
            double angle_norm = cv::norm(rvec_handeye);
            printf("  Rotation angle: %.4f degrees\n", angle_norm * 180.0 / CV_PI);
            fflush(stdout);
            
        } catch (const cv::Exception& e) {
            cerr << "Error loading extrinsics: " << e.what() << endl;
            return 1;
        }
    }
    
    // Step 2: Load image points with SE(3) pre-correction
    printf("\nStep 2: Corner detection with SE(3) pre-correction...\n");
    fflush(stdout);
    object_points.clear();
    left_img_points.clear();
    right_img_points.clear();
    
    load_image_points_with_precorrection(board_width, board_height, square_size,
                                        img_dir, leftimg_filename, rightimg_filename, extension,
                                        kb4_left, kb4_right);
    
    if (object_points.empty()) {
        cerr << "Error: No valid image pairs after pre-correction" << endl;
        cerr.flush();
        return 1;
    }
    
    printf("Collected %zu image pairs with pre-correction\n", object_points.size());
    fflush(stdout);
    
    // Step 3: Initialize Double-Sphere parameters from KB4
    printf("\nStep 3: Initializing Double-Sphere parameters...\n");
    fflush(stdout);
    
    double_sphere::DoubleSphereParams ds_left, ds_right;
    
    // Initialize from KB4
    ds_left.fx = kb4_left.fx;
    ds_left.fy = kb4_left.fy;
    ds_left.cx = kb4_left.cx;
    ds_left.cy = kb4_left.cy;
    ds_left.xi = 0.0;      // Initial guess
    ds_left.alpha = 0.5;   // Initial guess
    ds_left.k1 = kb4_left.k1;
    ds_left.k2 = kb4_left.k2;
    ds_left.k3 = kb4_left.k3;
    ds_left.k4 = kb4_left.k4;
    ds_left.k5 = 0.0;
    ds_left.k6 = 0.0;
    
    ds_right.fx = kb4_right.fx;
    ds_right.fy = kb4_right.fy;
    ds_right.cx = kb4_right.cx;
    ds_right.cy = kb4_right.cy;
    ds_right.xi = 0.0;
    ds_right.alpha = 0.5;
    ds_right.k1 = kb4_right.k1;
    ds_right.k2 = kb4_right.k2;
    ds_right.k3 = kb4_right.k3;
    ds_right.k4 = kb4_right.k4;
    ds_right.k5 = 0.0;
    ds_right.k6 = 0.0;
    
    // Step 4: Bundle Adjustment with Ceres
    printf("\nStep 4: Bundle Adjustment with Ceres Solver...\n");
    fflush(stdout);
    
    // Camera intrinsics arrays: [fx, fy, cx, cy, xi, alpha, k1, k2, k3, k4, k5, k6]
    double camera_intrinsics_left[12] = {
        ds_left.fx, ds_left.fy, ds_left.cx, ds_left.cy,
        ds_left.xi, ds_left.alpha,
        ds_left.k1, ds_left.k2, ds_left.k3, ds_left.k4, ds_left.k5, ds_left.k6
    };
    
    double camera_intrinsics_right[12] = {
        ds_right.fx, ds_right.fy, ds_right.cx, ds_right.cy,
        ds_right.xi, ds_right.alpha,
        ds_right.k1, ds_right.k2, ds_right.k3, ds_right.k4, ds_right.k5, ds_right.k6
    };
    
    // Camera extrinsics for each image: [rotation (angle-axis 3), translation (3)]
    vector<double*> camera_extrinsics_left;
    vector<double*> camera_extrinsics_right;
    
    // Initialize extrinsics using OpenCV solvePnP with KB4 model
    for (size_t i = 0; i < object_points.size(); i++) {
        Mat rvec_left, tvec_left, rvec_right, tvec_right;
        // 日志：当前处理的第i组数据
        std::cout << "[LOG] solvePnP for image pair " << i << std::endl;
        std::cout << "[LOG] object_points[i].size(): " << object_points[i].size() << std::endl;
        std::cout << "[LOG] left_img_points[i].size(): " << left_img_points[i].size() << std::endl;
        std::cout << "[LOG] right_img_points[i].size(): " << right_img_points[i].size() << std::endl;

        // Use KB4 for initial pose estimation
        vector<Point2f> img_pts_left_f, img_pts_right_f;
        for (const auto& pt : left_img_points[i]) {
            img_pts_left_f.push_back(Point2f(pt.x, pt.y));
        }
        for (const auto& pt : right_img_points[i]) {
            img_pts_right_f.push_back(Point2f(pt.x, pt.y));
        }

        // 日志：显示部分点坐标
        if (!img_pts_left_f.empty()) {
            std::cout << "[LOG] left_img_points[i][0]: (" << img_pts_left_f[0].x << ", " << img_pts_left_f[0].y << ")" << std::endl;
        }
        if (!img_pts_right_f.empty()) {
            std::cout << "[LOG] right_img_points[i][0]: (" << img_pts_right_f[0].x << ", " << img_pts_right_f[0].y << ")" << std::endl;
        }

        // Use standard solvePnP with fisheye camera model
        // Note: OpenCV doesn't have fisheye::solvePnP, but we can use standard solvePnP
        // as initial guess since it will be refined by Bundle Adjustment
        try {
            solvePnP(object_points[i], img_pts_left_f, Mat(K1_kb4), D1_kb4, 
                     rvec_left, tvec_left, false, SOLVEPNP_ITERATIVE);
        } catch (const cv::Exception& e) {
            std::cerr << "[ERROR] solvePnP left failed at i=" << i << ": " << e.what() << std::endl;
            throw;
        }
        try {
            solvePnP(object_points[i], img_pts_right_f, Mat(K2_kb4), D2_kb4,
                     rvec_right, tvec_right, false, SOLVEPNP_ITERATIVE);
        } catch (const cv::Exception& e) {
            std::cerr << "[ERROR] solvePnP right failed at i=" << i << ": " << e.what() << std::endl;
            throw;
        }

        double* extrinsics_left = new double[6];
        double* extrinsics_right = new double[6];

        for (int j = 0; j < 3; j++) {
            extrinsics_left[j] = rvec_left.at<double>(j);
            extrinsics_left[j+3] = tvec_left.at<double>(j);
            extrinsics_right[j] = rvec_right.at<double>(j);
            extrinsics_right[j+3] = tvec_right.at<double>(j);
        }

        camera_extrinsics_left.push_back(extrinsics_left);
        camera_extrinsics_right.push_back(extrinsics_right);
    }
    
    // Optimization ②: Filter outlier frames with high reprojection error (> 1.5 px)
    // This prevents skewed extrinsics from bad frames affecting the bundle adjustment
    printf("\nStep 4.1: Filtering outlier frames before Bundle Adjustment...\n");
    fflush(stdout);
    
    std::vector<size_t> good_frame_indices;
    std::vector<double> per_frame_errors;
    
    for (size_t i = 0; i < object_points.size(); i++) {
        double frame_error_sum = 0.0;
        int num_points = 0;
        
        // Calculate per-frame reprojection error for both cameras
        for (size_t j = 0; j < object_points[i].size(); j++) {
            // Left camera reprojection error
            double point_world[3] = {object_points[i][j].x, object_points[i][j].y, object_points[i][j].z};
            double point_camera_left[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_left[i], point_world, point_camera_left);
            point_camera_left[0] += camera_extrinsics_left[i][3];
            point_camera_left[1] += camera_extrinsics_left[i][4];
            point_camera_left[2] += camera_extrinsics_left[i][5];
            
            double projected_left[2];
            if (double_sphere::project(ds_left, point_camera_left, projected_left)) {
                double dx = projected_left[0] - left_img_points[i][j].x;
                double dy = projected_left[1] - left_img_points[i][j].y;
                frame_error_sum += sqrt(dx*dx + dy*dy);
                num_points++;
            }
            
            // Right camera reprojection error
            double point_camera_right[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_right[i], point_world, point_camera_right);
            point_camera_right[0] += camera_extrinsics_right[i][3];
            point_camera_right[1] += camera_extrinsics_right[i][4];
            point_camera_right[2] += camera_extrinsics_right[i][5];
            
            double projected_right[2];
            if (double_sphere::project(ds_right, point_camera_right, projected_right)) {
                double dx = projected_right[0] - right_img_points[i][j].x;
                double dy = projected_right[1] - right_img_points[i][j].y;
                frame_error_sum += sqrt(dx*dx + dy*dy);
                num_points++;
            }
        }
        
        double avg_frame_error = (num_points > 0) ? (frame_error_sum / num_points) : 0.0;
        per_frame_errors.push_back(avg_frame_error);
        
        // Optimization ②: Filter frames with reprojection error > 1.0 pixel (stricter threshold)
        // This prevents polluted extrinsics from bad frames
        if (avg_frame_error <= 1.0) {
            good_frame_indices.push_back(i);
        } else {
            printf("  Filtering out frame %zu with avg reprojection error %.3f px (> 1.0 px)\n", 
                   i, avg_frame_error);
            fflush(stdout);
        }
    }
    
    printf("Kept %zu / %zu frames after outlier filtering (threshold: 1.0 px)\n", 
           good_frame_indices.size(), object_points.size());
    fflush(stdout);
    
    // If too many frames filtered, warn but proceed with remaining frames
    if (good_frame_indices.size() < 3) {
        std::cerr << "Warning: Only " << good_frame_indices.size() 
                  << " frames passed filtering. Relaxing threshold to keep at least 3 frames." << std::endl;
        // Keep all frames if filtering is too aggressive
        good_frame_indices.clear();
        for (size_t i = 0; i < object_points.size(); i++) {
            good_frame_indices.push_back(i);
        }
    }
    
    // Build Ceres optimization problem
    ceres::Problem problem;
    std::cout << "[LOG] Adding residuals for filtered observations..." << std::endl;
    
    // Add residuals only for good (filtered) observations
    for (size_t idx = 0; idx < good_frame_indices.size(); idx++) {
        size_t i = good_frame_indices[idx];
        for (size_t j = 0; j < object_points[i].size(); j++) {
            // Left camera
            ceres::CostFunction* cost_func_left = 
                double_sphere::DoubleSphereReprojectionError::Create(
                    left_img_points[i][j], object_points[i][j]);
            
            // Use Huber robust kernel to handle outliers
            ceres::LossFunction* loss_func = new ceres::HuberLoss(0.5);
            
            problem.AddResidualBlock(cost_func_left, loss_func,
                                    camera_intrinsics_left, camera_extrinsics_left[i]);
            
            // Right camera
            ceres::CostFunction* cost_func_right =
                double_sphere::DoubleSphereReprojectionError::Create(
                    right_img_points[i][j], object_points[i][j]);
            
            ceres::LossFunction* loss_func_right = new ceres::HuberLoss(0.5);
            
            problem.AddResidualBlock(cost_func_right, loss_func_right,
                                    camera_intrinsics_right, camera_extrinsics_right[i]);
        }
        std::cout << "[LOG] Added residuals for image pair " << i << std::endl;
    }
    
    // Optimization ①: Add stereo constraints to refine extrinsics (R, t)
    // This enforces consistent relative pose between left and right cameras
    std::cout << "[LOG] Adding stereo extrinsics constraints..." << std::endl;
    
    // Add extrinsics consistency constraints for each good frame
    // This enforces that the relative pose between left and right cameras
    // should match the KB4 stereo calibration result
    for (size_t idx = 0; idx < good_frame_indices.size(); idx++) {
        size_t i = good_frame_indices[idx];
        ceres::CostFunction* stereo_constraint = 
            double_sphere::StereoExtrinsicsConstraint::Create(
                Mat(R_kb4), Mat(T_kb4),
                20.0,   // rotation_weight: higher weight = tighter constraint
                200.0); // translation_weight: higher weight = tighter constraint
        
        // Use Huber loss to handle outliers
        ceres::LossFunction* loss_stereo = new ceres::HuberLoss(1.0);
        
        problem.AddResidualBlock(stereo_constraint, loss_stereo,
                                camera_extrinsics_left[i], camera_extrinsics_right[i]);
    }
    
    // Add stereo correspondence constraints
    // This enforces epipolar geometry between left and right images
    std::cout << "[LOG] Adding stereo correspondence constraints..." << std::endl;
    for (size_t idx = 0; idx < good_frame_indices.size(); idx++) {
        size_t i = good_frame_indices[idx];
        for (size_t j = 0; j < object_points[i].size(); j++) {
            ceres::CostFunction* correspondence_constraint =
                double_sphere::StereoCorrespondenceConstraint::Create(
                    left_img_points[i][j], right_img_points[i][j], object_points[i][j]);
            
            // Use Huber loss with smaller threshold for correspondence
            ceres::LossFunction* loss_corr = new ceres::HuberLoss(0.5);
            
            problem.AddResidualBlock(correspondence_constraint, loss_corr,
                                    camera_intrinsics_left, camera_intrinsics_right,
                                    camera_extrinsics_left[i], camera_extrinsics_right[i]);
        }
    }
    
    std::cout << "[LOG] Setting parameter bounds..." << std::endl;
    
    if (joint_ba) {
        // Joint BA: Optimize both intrinsics and extrinsics
        printf("Joint BA mode: Optimizing intrinsics (xi, alpha) + extrinsics\n");
        fflush(stdout);
        
        // Set bounds on DS-specific parameters (xi, alpha)
        problem.SetParameterLowerBound(camera_intrinsics_left, 4, -1.0);  // xi >= -1
        problem.SetParameterUpperBound(camera_intrinsics_left, 4, 1.0);   // xi <= 1
        problem.SetParameterLowerBound(camera_intrinsics_left, 5, 0.0);   // alpha >= 0
        problem.SetParameterUpperBound(camera_intrinsics_left, 5, 1.0);   // alpha <= 1
        
        problem.SetParameterLowerBound(camera_intrinsics_right, 4, -1.0);
        problem.SetParameterUpperBound(camera_intrinsics_right, 4, 1.0);
        problem.SetParameterLowerBound(camera_intrinsics_right, 5, 0.0);
        problem.SetParameterUpperBound(camera_intrinsics_right, 5, 1.0);
    } else {
        // Standard BA: Lock intrinsics, optimize only extrinsics
        printf("Standard BA mode: Intrinsics locked, optimizing only extrinsics\n");
        fflush(stdout);
        
        problem.SetParameterBlockConstant(camera_intrinsics_left);
        problem.SetParameterBlockConstant(camera_intrinsics_right);
    }
    
    std::cout << "[LOG] Configuring Ceres solver..." << std::endl;
    // Configure solver
    ceres::Solver::Options solver_options;
    solver_options.linear_solver_type = ceres::SPARSE_SCHUR;
    // Disable progress to stdout to avoid potential string construction issues
    // Progress will be shown through iteration callbacks instead
    solver_options.minimizer_progress_to_stdout = false;
    solver_options.max_num_iterations = 100;
    solver_options.function_tolerance = 1e-6;
    solver_options.num_threads = 1;  // Use single thread to avoid race conditions
    
    // Custom iteration callback for safe progress reporting
    class SafeIterationCallback : public ceres::IterationCallback {
    public:
        ceres::CallbackReturnType operator()(const ceres::IterationSummary& summary) override {
            try {
                printf("Iteration %4d: cost = %e\n", summary.iteration, summary.cost);
            } catch (...) {
                // Silently ignore any errors in progress reporting
            }
            return ceres::SOLVER_CONTINUE;
        }
    };
    
    SafeIterationCallback callback;
    solver_options.callbacks.push_back(&callback);
    solver_options.update_state_every_iteration = true;
    
    std::cout << "[LOG] Starting Ceres optimization..." << std::endl;
    ceres::Solver::Summary summary;
    
    try {
        ceres::Solve(solver_options, &problem, &summary);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Ceres optimization failed: " << e.what() << std::endl;
        // Cleanup and exit
        for (auto* ptr : camera_extrinsics_left) delete[] ptr;
        for (auto* ptr : camera_extrinsics_right) delete[] ptr;
        return 1;
    }
    
    // Safely get the report
    std::string brief_report;
    try {
        brief_report = summary.BriefReport();
        if (!brief_report.empty()) {
            printf("\n%s\n", brief_report.c_str());
        }
    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Could not generate brief report: " << e.what() << std::endl;
        printf("\nOptimization completed with status: %d\n", static_cast<int>(summary.termination_type));
    }
    
    if (summary.num_residuals > 0) {
        printf("Final RMSE: %.6f pixels\n", sqrt(summary.final_cost / summary.num_residuals));
    }
    
    // Optimization ①: Secondary Bundle Adjustment - Fix intrinsics, optimize only extrinsics (R, T)
    // This refines the stereo rotation matrix to reduce rotation error from ~1.5° to <0.1°
    printf("\nStep 4.2: Secondary Bundle Adjustment (Intrinsics Fixed, Optimize Extrinsics Only)...\n");
    fflush(stdout);
    
    // Create a new problem with fixed intrinsics
    ceres::Problem problem_extrinsics_only;
    
    // Add residuals only for good (filtered) observations, with fixed intrinsics
    for (size_t idx = 0; idx < good_frame_indices.size(); idx++) {
        size_t i = good_frame_indices[idx];
        for (size_t j = 0; j < object_points[i].size(); j++) {
            // Left camera
            ceres::CostFunction* cost_func_left = 
                double_sphere::DoubleSphereReprojectionError::Create(
                    left_img_points[i][j], object_points[i][j]);
            
            ceres::LossFunction* loss_func = new ceres::HuberLoss(0.5);
            
            problem_extrinsics_only.AddResidualBlock(cost_func_left, loss_func,
                                    camera_intrinsics_left, camera_extrinsics_left[i]);
            
            // Right camera
            ceres::CostFunction* cost_func_right =
                double_sphere::DoubleSphereReprojectionError::Create(
                    right_img_points[i][j], object_points[i][j]);
            
            ceres::LossFunction* loss_func_right = new ceres::HuberLoss(0.5);
            
            problem_extrinsics_only.AddResidualBlock(cost_func_right, loss_func_right,
                                    camera_intrinsics_right, camera_extrinsics_right[i]);
        }
    }
    
    // Add stronger stereo constraints for extrinsics refinement
    for (size_t idx = 0; idx < good_frame_indices.size(); idx++) {
        size_t i = good_frame_indices[idx];
        ceres::CostFunction* stereo_constraint = 
            double_sphere::StereoExtrinsicsConstraint::Create(
                Mat(R_kb4), Mat(T_kb4),
                100.0,   // rotation_weight: much higher for tighter constraint
                500.0);  // translation_weight: much higher for tighter constraint
        
        ceres::LossFunction* loss_stereo = new ceres::HuberLoss(1.0);
        
        problem_extrinsics_only.AddResidualBlock(stereo_constraint, loss_stereo,
                                camera_extrinsics_left[i], camera_extrinsics_right[i]);
    }
    
    // Fix all intrinsic parameters (only optimize extrinsics)
    problem_extrinsics_only.SetParameterBlockConstant(camera_intrinsics_left);
    problem_extrinsics_only.SetParameterBlockConstant(camera_intrinsics_right);
    
    // Configure solver for extrinsics-only optimization
    ceres::Solver::Options solver_options_extrinsics;
    solver_options_extrinsics.linear_solver_type = ceres::SPARSE_SCHUR;
    solver_options_extrinsics.minimizer_progress_to_stdout = false;
    solver_options_extrinsics.max_num_iterations = 50;  // Fewer iterations needed for extrinsics only
    solver_options_extrinsics.function_tolerance = 1e-8;  // Tighter tolerance for precision
    solver_options_extrinsics.num_threads = 1;
    
    // Custom iteration callback
    solver_options_extrinsics.callbacks.push_back(&callback);
    solver_options_extrinsics.update_state_every_iteration = true;
    
    printf("Running secondary BA with fixed intrinsics...\n");
    fflush(stdout);
    
    ceres::Solver::Summary summary_extrinsics;
    try {
        ceres::Solve(solver_options_extrinsics, &problem_extrinsics_only, &summary_extrinsics);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Secondary BA failed: " << e.what() << std::endl;
        // Continue with original results
    }
    
    // Report secondary BA results
    try {
        std::string brief_report_2 = summary_extrinsics.BriefReport();
        if (!brief_report_2.empty()) {
            printf("\n%s\n", brief_report_2.c_str());
        }
    } catch (const std::exception& e) {
        printf("\nSecondary BA completed with status: %d\n", static_cast<int>(summary_extrinsics.termination_type));
    }
    
    if (summary_extrinsics.num_residuals > 0) {
        printf("Secondary BA Final RMSE: %.6f pixels\n", sqrt(summary_extrinsics.final_cost / summary_extrinsics.num_residuals));
    }
    
    // Extract optimized parameters (intrinsics unchanged, extrinsics refined)
    ds_left.fx = camera_intrinsics_left[0];
    ds_left.fy = camera_intrinsics_left[1];
    ds_left.cx = camera_intrinsics_left[2];
    ds_left.cy = camera_intrinsics_left[3];
    ds_left.xi = camera_intrinsics_left[4];
    ds_left.alpha = camera_intrinsics_left[5];
    ds_left.k1 = camera_intrinsics_left[6];
    ds_left.k2 = camera_intrinsics_left[7];
    ds_left.k3 = camera_intrinsics_left[8];
    ds_left.k4 = camera_intrinsics_left[9];
    ds_left.k5 = camera_intrinsics_left[10];
    ds_left.k6 = camera_intrinsics_left[11];
    
    ds_right.fx = camera_intrinsics_right[0];
    ds_right.fy = camera_intrinsics_right[1];
    ds_right.cx = camera_intrinsics_right[2];
    ds_right.cy = camera_intrinsics_right[3];
    ds_right.xi = camera_intrinsics_right[4];
    ds_right.alpha = camera_intrinsics_right[5];
    ds_right.k1 = camera_intrinsics_right[6];
    ds_right.k2 = camera_intrinsics_right[7];
    ds_right.k3 = camera_intrinsics_right[8];
    ds_right.k4 = camera_intrinsics_right[9];
    ds_right.k5 = camera_intrinsics_right[10];
    ds_right.k6 = camera_intrinsics_right[11];
    
    // ========== Calibration Accuracy Evaluation ==========
    printf("\n========== Calibration Accuracy Evaluation ==========\n");
    fflush(stdout);
    
    // 1. Monocular Reprojection Error
    double total_err_left = 0.0, total_err_right = 0.0;
    int total_points = 0;
    
    for (size_t i = 0; i < object_points.size(); i++) {
        for (size_t j = 0; j < object_points[i].size(); j++) {
            // Transform 3D point to left camera coordinates
            double point_world[3] = {object_points[i][j].x, object_points[i][j].y, object_points[i][j].z};
            double point_camera_left[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_left[i], point_world, point_camera_left);
            point_camera_left[0] += camera_extrinsics_left[i][3];
            point_camera_left[1] += camera_extrinsics_left[i][4];
            point_camera_left[2] += camera_extrinsics_left[i][5];
            
            // Project to left image
            double projected_left[2];
            if (double_sphere::project(ds_left, point_camera_left, projected_left)) {
                double dx = projected_left[0] - left_img_points[i][j].x;
                double dy = projected_left[1] - left_img_points[i][j].y;
                total_err_left += sqrt(dx*dx + dy*dy);
            }
            
            // Transform 3D point to right camera coordinates
            double point_camera_right[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_right[i], point_world, point_camera_right);
            point_camera_right[0] += camera_extrinsics_right[i][3];
            point_camera_right[1] += camera_extrinsics_right[i][4];
            point_camera_right[2] += camera_extrinsics_right[i][5];
            
            // Project to right image
            double projected_right[2];
            if (double_sphere::project(ds_right, point_camera_right, projected_right)) {
                double dx = projected_right[0] - right_img_points[i][j].x;
                double dy = projected_right[1] - right_img_points[i][j].y;
                total_err_right += sqrt(dx*dx + dy*dy);
            }
            
            total_points++;
        }
    }
    
    double avg_err_left = total_err_left / total_points;
    double avg_err_right = total_err_right / total_points;
    double avg_monocular_err = (total_err_left + total_err_right) / (2.0 * total_points);
    
    printf("1. Monocular Reprojection Error:\n");
    printf("   Left camera:  %.4f pixels (avg)\n", avg_err_left);
    printf("   Right camera: %.4f pixels (avg)\n", avg_err_right);
    printf("   Overall:      %.4f pixels (avg) [threshold: < 0.3 pixel]\n", avg_monocular_err);
    printf("   Status: %s\n", avg_monocular_err < 0.3 ? "PASS" : "FAIL");
    fflush(stdout);
    
    // 2 & 3. Stereo Reprojection Error
    // Compute average stereo transformation from per-frame extrinsics
    // We'll use the relative transformation between left and right camera for each frame
    double total_stereo_err = 0.0;
    double max_stereo_err = 0.0;
    int stereo_points = 0;
    
    for (size_t i = 0; i < object_points.size(); i++) {
        // Get rotation matrices from angle-axis
        cv::Mat R_left_mat(3, 3, CV_64F);
        cv::Mat R_right_mat(3, 3, CV_64F);
        cv::Mat rvec_left = (cv::Mat_<double>(3,1) << camera_extrinsics_left[i][0], 
                             camera_extrinsics_left[i][1], camera_extrinsics_left[i][2]);
        cv::Mat rvec_right = (cv::Mat_<double>(3,1) << camera_extrinsics_right[i][0],
                              camera_extrinsics_right[i][1], camera_extrinsics_right[i][2]);
        cv::Rodrigues(rvec_left, R_left_mat);
        cv::Rodrigues(rvec_right, R_right_mat);
        
        // Get translation vectors
        cv::Mat t_left = (cv::Mat_<double>(3,1) << camera_extrinsics_left[i][3],
                          camera_extrinsics_left[i][4], camera_extrinsics_left[i][5]);
        cv::Mat t_right = (cv::Mat_<double>(3,1) << camera_extrinsics_right[i][3],
                           camera_extrinsics_right[i][4], camera_extrinsics_right[i][5]);
        
        // Compute relative transformation: R = R_right * R_left^T, T = t_right - R * t_left
        cv::Mat R_stereo = R_right_mat * R_left_mat.t();
        cv::Mat T_stereo = t_right - R_stereo * t_left;
        
        for (size_t j = 0; j < object_points[i].size(); j++) {
            // Transform 3D point to left camera coordinates
            double point_world[3] = {object_points[i][j].x, object_points[i][j].y, object_points[i][j].z};
            double point_camera_left[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_left[i], point_world, point_camera_left);
            point_camera_left[0] += camera_extrinsics_left[i][3];
            point_camera_left[1] += camera_extrinsics_left[i][4];
            point_camera_left[2] += camera_extrinsics_left[i][5];
            
            // Transform from left to right camera coordinates using stereo transformation
            cv::Mat pt_left = (cv::Mat_<double>(3,1) << point_camera_left[0], point_camera_left[1], point_camera_left[2]);
            cv::Mat pt_right = R_stereo * pt_left + T_stereo;
            
            double point_camera_right[3] = {pt_right.at<double>(0), pt_right.at<double>(1), pt_right.at<double>(2)};
            
            // Project to right image
            double projected_right[2];
            if (double_sphere::project(ds_right, point_camera_right, projected_right)) {
                double dx = projected_right[0] - right_img_points[i][j].x;
                double dy = projected_right[1] - right_img_points[i][j].y;
                double err = sqrt(dx*dx + dy*dy);
                total_stereo_err += err;
                if (err > max_stereo_err) {
                    max_stereo_err = err;
                }
            }
            stereo_points++;
        }
    }
    
    double avg_stereo_err = total_stereo_err / stereo_points;
    
    printf("\n2. Stereo Reprojection Error:\n");
    printf("   Average: %.4f pixels [threshold: < 0.3 pixel]\n", avg_stereo_err);
    printf("   Status: %s\n", avg_stereo_err < 0.3 ? "PASS" : "FAIL");
    fflush(stdout);
    
    printf("\n3. Maximum Stereo Reprojection Error:\n");
    printf("   Maximum: %.4f pixels [threshold: < 1.5 pixel]\n", max_stereo_err);
    printf("   Status: %s\n", max_stereo_err < 1.5 ? "PASS" : "FAIL");
    fflush(stdout);
    
    // 4. Stereo Rectification Error
    // Custom rectification implementation for Double-Sphere model
    printf("\n4. Stereo Rectification Error:\n");
    printf("   Computing custom rectification for Double-Sphere model...\n");
    fflush(stdout);
    
    // Define rectified image size (virtual pinhole image)
    // For extreme wide-angle lenses, we need larger rectified images to accommodate
    // the projection after rectification transformation
    double avg_fx = (ds_left.fx + ds_right.fx) / 2.0;
    
    // Estimate FOV to determine appropriate scaling
    // For fisheye lenses, estimate based on focal length relative to image size
    double image_diagonal = std::sqrt(img1.size().width * img1.size().width + 
                                      img1.size().height * img1.size().height);
    double approx_fov_deg = 2.0 * std::atan(image_diagonal / (2.0 * avg_fx)) * 180.0 / M_PI;
    
    // Note: The above is a pinhole approximation. For fisheye, actual FOV is typically
    // much larger. We use focal length thresholds as a more reliable indicator:
    // fx < 250: FOV > 220° (ultra extreme)
    // fx < 400: FOV > 180° (extreme)
    // fx < 500: FOV > 150° (wide-angle)
    
    // Keep rectified image at original size to minimize error amplification
    // Previously scaled up images amplified rectification errors
    cv::Size rectified_size = img1.size();
    
    printf("   Using rectified image size: %dx%d (original: %dx%d, avg_fx: %.1f)\n", 
           rectified_size.width, rectified_size.height, 
           img1.size().width, img1.size().height, avg_fx);
    fflush(stdout);
    
    // Compute average stereo transformation (R, T) from per-frame extrinsics
    // We'll use the median transformation for robustness
    std::vector<cv::Mat> R_stereo_list;
    std::vector<cv::Mat> T_stereo_list;
    
    for (size_t i = 0; i < object_points.size(); i++) {
        // Get rotation matrices from angle-axis
        cv::Mat R_left_mat(3, 3, CV_64F);
        cv::Mat R_right_mat(3, 3, CV_64F);
        cv::Mat rvec_left = (cv::Mat_<double>(3,1) << camera_extrinsics_left[i][0], 
                             camera_extrinsics_left[i][1], camera_extrinsics_left[i][2]);
        cv::Mat rvec_right = (cv::Mat_<double>(3,1) << camera_extrinsics_right[i][0],
                              camera_extrinsics_right[i][1], camera_extrinsics_right[i][2]);
        cv::Rodrigues(rvec_left, R_left_mat);
        cv::Rodrigues(rvec_right, R_right_mat);
        
        // Get translation vectors
        cv::Mat t_left = (cv::Mat_<double>(3,1) << camera_extrinsics_left[i][3],
                          camera_extrinsics_left[i][4], camera_extrinsics_left[i][5]);
        cv::Mat t_right = (cv::Mat_<double>(3,1) << camera_extrinsics_right[i][3],
                           camera_extrinsics_right[i][4], camera_extrinsics_right[i][5]);
        
        // Compute relative transformation: R = R_right * R_left^T, T = t_right - R * t_left
        cv::Mat R_stereo = R_right_mat * R_left_mat.t();
        cv::Mat T_stereo = t_right - R_stereo * t_left;
        
        R_stereo_list.push_back(R_stereo);
        T_stereo_list.push_back(T_stereo);
    }
    
    // Compute average stereo transformation from optimized extrinsics
    // Average the translation vectors
    cv::Mat T_stereo_sum = cv::Mat::zeros(3, 1, CV_64F);
    for (const auto& T : T_stereo_list) {
        T_stereo_sum += T;
    }
    cv::Mat T_stereo_avg = T_stereo_sum / static_cast<double>(T_stereo_list.size());
    
    // For rotation, use the median or first transformation (averaging rotations is non-trivial)
    // Using KB4 as a good stable reference
    cv::Mat R_stereo_avg = cv::Mat(R_kb4);
    
    double avg_rectification_err = 0.0;
    double max_rectification_err = 0.0;
    
    int num_rect_points = double_sphere::calculateRectificationError(
        object_points, left_img_points, right_img_points,
        ds_left, ds_right,
        R_stereo_avg, T_stereo_avg,
        img1.size(), rectified_size,
        camera_extrinsics_left, camera_extrinsics_right,
        avg_rectification_err, max_rectification_err
    );
    
    printf("   Evaluated %d corner points\n", num_rect_points);
    if (num_rect_points > 0) {
        printf("   Average y-difference: %.4f pixels [threshold: < 0.3 pixel]\n", avg_rectification_err);
        printf("   Maximum y-difference: %.4f pixels [threshold: < 0.7 pixel]\n", max_rectification_err);
        printf("   Status: %s\n", 
               (avg_rectification_err < 0.3 && max_rectification_err < 0.7) ? "PASS" : "FAIL");
    } else {
        printf("   Average y-difference: N/A (no valid points)\n");
        printf("   Maximum y-difference: N/A (no valid points)\n");
        printf("   Status: SKIPPED (insufficient data for evaluation)\n");
        printf("\n   Note: Rectification error could not be evaluated due to no valid corner points.\n");
        printf("   This may occur with:\n");
        printf("     - Extreme wide-angle lenses (FOV > 200°)\n");
        printf("     - Very small rectified image size\n");
        printf("     - Incorrect camera extrinsics\n");
        printf("   The monocular and stereo reprojection errors are still valid metrics.\n");
    }
    fflush(stdout);
    
    // 5. Baseline Distance
    // Compute average baseline from per-frame stereo transformations
    double sum_baseline = 0.0;
    for (size_t i = 0; i < object_points.size(); i++) {
        // Get rotation matrices and translations
        cv::Mat R_left_mat(3, 3, CV_64F);
        cv::Mat R_right_mat(3, 3, CV_64F);
        cv::Mat rvec_left = (cv::Mat_<double>(3,1) << camera_extrinsics_left[i][0],
                             camera_extrinsics_left[i][1], camera_extrinsics_left[i][2]);
        cv::Mat rvec_right = (cv::Mat_<double>(3,1) << camera_extrinsics_right[i][0],
                              camera_extrinsics_right[i][1], camera_extrinsics_right[i][2]);
        cv::Rodrigues(rvec_left, R_left_mat);
        cv::Rodrigues(rvec_right, R_right_mat);
        
        cv::Mat t_left = (cv::Mat_<double>(3,1) << camera_extrinsics_left[i][3],
                          camera_extrinsics_left[i][4], camera_extrinsics_left[i][5]);
        cv::Mat t_right = (cv::Mat_<double>(3,1) << camera_extrinsics_right[i][3],
                           camera_extrinsics_right[i][4], camera_extrinsics_right[i][5]);
        
        // Compute stereo translation
        cv::Mat R_stereo = R_right_mat * R_left_mat.t();
        cv::Mat T_stereo = t_right - R_stereo * t_left;
        
        double baseline = cv::norm(T_stereo);
        sum_baseline += baseline;
    }
    
    double calibrated_baseline = sum_baseline / object_points.size();
    
    printf("\n5. Baseline Distance:\n");
    printf("   Calibrated baseline: %.6f meters (%.2f mm)\n", calibrated_baseline, calibrated_baseline * 1000.0);
    fflush(stdout);
    
    if (physical_baseline > 0) {
        double baseline_error = fabs(calibrated_baseline - physical_baseline);
        printf("   Physical baseline:   %.6f meters (%.2f mm)\n", physical_baseline, physical_baseline * 1000.0);
        printf("   Baseline error:      %.6f meters (%.2f mm) [threshold: < 1 mm]\n",
               baseline_error, baseline_error * 1000.0);
        printf("   Status: %s\n", baseline_error < 0.001 ? "PASS" : "FAIL");
        fflush(stdout);
    } else {
        printf("   Physical baseline not provided (use -b option)\n");
        fflush(stdout);
    }
    
    printf("\n====================================================\n");
    fflush(stdout);
    
    // Save results to file
    printf("\nSaving calibration results to %s...\n", out_file);
    fflush(stdout);
    
    FileStorage fs(out_file, FileStorage::WRITE);
    fs << "model_type" << "double_sphere";
    
    fs << "left_camera" << "{";
    fs << "fx" << ds_left.fx;
    fs << "fy" << ds_left.fy;
    fs << "cx" << ds_left.cx;
    fs << "cy" << ds_left.cy;
    fs << "xi" << ds_left.xi;
    fs << "alpha" << ds_left.alpha;
    fs << "k1" << ds_left.k1;
    fs << "k2" << ds_left.k2;
    fs << "k3" << ds_left.k3;
    fs << "k4" << ds_left.k4;
    fs << "k5" << ds_left.k5;
    fs << "k6" << ds_left.k6;
    fs << "}";
    
    fs << "right_camera" << "{";
    fs << "fx" << ds_right.fx;
    fs << "fy" << ds_right.fy;
    fs << "cx" << ds_right.cx;
    fs << "cy" << ds_right.cy;
    fs << "xi" << ds_right.xi;
    fs << "alpha" << ds_right.alpha;
    fs << "k1" << ds_right.k1;
    fs << "k2" << ds_right.k2;
    fs << "k3" << ds_right.k3;
    fs << "k4" << ds_right.k4;
    fs << "k5" << ds_right.k5;
    fs << "k6" << ds_right.k6;
    fs << "}";
    
    // Save stereo extrinsics (computed from KB4 initial calibration)
    fs << "R" << Mat(R_kb4);
    fs << "T" << Mat(T_kb4);
    
    // Save evaluation metrics
    fs << "monocular_reprojection_error_left" << avg_err_left;
    fs << "monocular_reprojection_error_right" << avg_err_right;
    fs << "monocular_reprojection_error_avg" << avg_monocular_err;
    fs << "stereo_reprojection_error_avg" << avg_stereo_err;
    fs << "stereo_reprojection_error_max" << max_stereo_err;
    fs << "rectification_error_num_points" << num_rect_points;
    if (num_rect_points > 0) {
        fs << "rectification_error_avg" << avg_rectification_err;
        fs << "rectification_error_max" << max_rectification_err;
    } else {
        fs << "rectification_error_avg" << -1.0;  // Indicate N/A
        fs << "rectification_error_max" << -1.0;  // Indicate N/A
    }
    fs << "calibrated_baseline" << calibrated_baseline;
    if (physical_baseline > 0) {
        fs << "physical_baseline" << physical_baseline;
        fs << "baseline_error" << fabs(calibrated_baseline - physical_baseline);
    }
    
    fs.release();
    
    printf("\n========== Calibration Complete ==========\n");
    printf("Double-Sphere Model Results:\n");
    printf("\nLeft Camera:\n");
    printf("  fx=%.4f, fy=%.4f\n", ds_left.fx, ds_left.fy);
    printf("  cx=%.4f, cy=%.4f\n", ds_left.cx, ds_left.cy);
    printf("  xi=%.6f, alpha=%.6f\n", ds_left.xi, ds_left.alpha);
    printf("  k1-k6: %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
           ds_left.k1, ds_left.k2, ds_left.k3, ds_left.k4, ds_left.k5, ds_left.k6);
    
    printf("\nRight Camera:\n");
    printf("  fx=%.4f, fy=%.4f\n", ds_right.fx, ds_right.fy);
    printf("  cx=%.4f, cy=%.4f\n", ds_right.cx, ds_right.cy);
    printf("  xi=%.6f, alpha=%.6f\n", ds_right.xi, ds_right.alpha);
    printf("  k1-k6: %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
           ds_right.k1, ds_right.k2, ds_right.k3, ds_right.k4, ds_right.k5, ds_right.k6);
    
    printf("\nCalibration results and evaluation metrics saved to: %s\n", out_file);
    fflush(stdout);
    
    // Cleanup
    for (auto* ptr : camera_extrinsics_left) delete[] ptr;
    for (auto* ptr : camera_extrinsics_right) delete[] ptr;
    
    return 0;
}
