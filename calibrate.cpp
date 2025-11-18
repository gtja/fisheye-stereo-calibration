#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <stdio.h>
#include <iostream>
#include "popt_pp.h"

using namespace std;
using namespace cv;

vector< vector< Point3d > > object_points;
vector< vector< Point2f > > imagePoints1, imagePoints2;
vector< Point2f > corners1, corners2;
vector< vector< Point2d > > left_img_points, right_img_points;

Mat img1, img2, gray1, gray2, spl1, spl2;

void load_image_points(int board_width, int board_height, float square_size, int num_imgs, 
                      char* img_dir, char* leftimg_filename, char* rightimg_filename) {
  Size board_size = Size(board_width, board_height);
  int board_n = board_width * board_height;

  for (int i = 1; i <= num_imgs; i++) {
    char left_img[100], right_img[100];
    sprintf(left_img, "%s%s%d.jpg", img_dir, leftimg_filename, i);
    sprintf(right_img, "%s%s%d.jpg", img_dir, rightimg_filename, i);
    img1 = imread(left_img, cv::IMREAD_COLOR);
    img2 = imread(right_img, cv::IMREAD_COLOR);
    cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);

    bool found1 = false, found2 = false;

    found1 = cv::findChessboardCorners(img1, board_size, corners1,
  cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FILTER_QUADS);
    found2 = cv::findChessboardCorners(img2, board_size, corners2,
  cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FILTER_QUADS);

    if (found1)
    {
      cv::cornerSubPix(gray1, corners1, cv::Size(5, 5), cv::Size(-1, -1),
  cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.1));
      cv::drawChessboardCorners(gray1, board_size, corners1, found1);
    }
    if (found2)
    {
      cv::cornerSubPix(gray2, corners2, cv::Size(5, 5), cv::Size(-1, -1),
  cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.1));
      cv::drawChessboardCorners(gray2, board_size, corners2, found2);
    }

    vector<cv::Point3d> obj;
    for( int i = 0; i < board_height; ++i )
      for( int j = 0; j < board_width; ++j )
        obj.push_back(Point3d(double( (float)j * square_size ), double( (float)i * square_size ), 0));

    if (found1 && found2) {
      cout << i << ". Found corners!" << endl;
      imagePoints1.push_back(corners1);
      imagePoints2.push_back(corners2);
      object_points.push_back(obj);
    }
  }
  for (int i = 0; i < imagePoints1.size(); i++) {
    vector< Point2d > v1, v2;
    for (int j = 0; j < imagePoints1[i].size(); j++) {
      v1.push_back(Point2d((double)imagePoints1[i][j].x, (double)imagePoints1[i][j].y));
      v2.push_back(Point2d((double)imagePoints2[i][j].x, (double)imagePoints2[i][j].y));
    }
    left_img_points.push_back(v1);
    right_img_points.push_back(v2);
  }
}

int main(int argc, char const *argv[])
{
  int board_width, board_height, num_imgs;
  float square_size;
  float physical_baseline = -1.0; // Physical baseline in meters (optional)
  char* img_dir;
  char* leftimg_filename;
  char* rightimg_filename;
  char* out_file;

  static struct poptOption options[] = {
    { "board_width",'w',POPT_ARG_INT,&board_width,0,"Checkerboard width","NUM" },
    { "board_height",'h',POPT_ARG_INT,&board_height,0,"Checkerboard height","NUM" },
    { "square_size",'s',POPT_ARG_FLOAT,&square_size,0,"Checkerboard square size","NUM" },
    { "num_imgs",'n',POPT_ARG_INT,&num_imgs,0,"Number of checkerboard images","NUM" },
    { "img_dir",'d',POPT_ARG_STRING,&img_dir,0,"Directory containing images","STR" },
    { "leftimg_filename",'l',POPT_ARG_STRING,&leftimg_filename,0,"Left image prefix","STR" },
    { "rightimg_filename",'r',POPT_ARG_STRING,&rightimg_filename,0,"Right image prefix","STR" },
    { "out_file",'o',POPT_ARG_STRING,&out_file,0,"Output calibration filename (YML)","STR" },
    { "physical_baseline",'b',POPT_ARG_FLOAT,&physical_baseline,0,"Physical baseline distance in meters (optional)","NUM" },
    POPT_AUTOHELP
    { NULL, 0, 0, NULL, 0, NULL, NULL }
  };

  POpt popt(NULL, argc, argv, options, 0);
  int c;
  while((c = popt.getNextOpt()) >= 0) {}

  load_image_points(board_width, board_height, square_size, num_imgs, img_dir, leftimg_filename, rightimg_filename);

  printf("Starting Calibration\n");
  cv::Matx33d K1, K2, R;
  cv::Vec3d T;
  cv::Vec4d D1, D2;
  int flag = 0;
  flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
  flag |= cv::fisheye::CALIB_CHECK_COND;
  flag |= cv::fisheye::CALIB_FIX_SKEW;
  //flag |= cv::fisheye::CALIB_FIX_K2;
  //flag |= cv::fisheye::CALIB_FIX_K3;
  //flag |= cv::fisheye::CALIB_FIX_K4;
  cv::fisheye::stereoCalibrate(object_points, left_img_points, right_img_points,
      K1, D1, K2, D2, img1.size(), R, T, flag,
      cv::TermCriteria(3, 12, 0));

  cv::FileStorage fs1(out_file, cv::FileStorage::WRITE);
  fs1 << "K1" << Mat(K1);
  fs1 << "K2" << Mat(K2);
  fs1 << "D1" << D1;
  fs1 << "D2" << D2;
  fs1 << "R" << Mat(R);
  fs1 << "T" << T;
  printf("Done Calibration\n");

  printf("Starting Rectification\n");

  cv::Mat R1, R2, P1, P2, Q;
  cv::fisheye::stereoRectify(K1, D1, K2, D2, img1.size(), R, T, R1, R2, P1, P2, 
Q, cv::CALIB_ZERO_DISPARITY, img1.size(), 0.0, 1.1);

  fs1 << "R1" << R1;
  fs1 << "R2" << R2;
  fs1 << "P1" << P1;
  fs1 << "P2" << P2;
  fs1 << "Q" << Q;

  printf("Done Rectification\n");

  // ========== Calibration Accuracy Evaluation ==========
  printf("\n========== Calibration Accuracy Evaluation ==========\n");
  
  // First, we need to get the extrinsic parameters for each image
  // Re-calibrate individual cameras to get per-image extrinsics
  vector<cv::Vec3d> rvecs_left, tvecs_left, rvecs_right, tvecs_right;
  
  // Calibrate left camera to get extrinsics
  cv::fisheye::calibrate(object_points, left_img_points, img1.size(), 
                         K1, D1, rvecs_left, tvecs_left, 
                         cv::fisheye::CALIB_USE_INTRINSIC_GUESS);
  
  // Calibrate right camera to get extrinsics
  cv::fisheye::calibrate(object_points, right_img_points, img2.size(), 
                         K2, D2, rvecs_right, tvecs_right,
                         cv::fisheye::CALIB_USE_INTRINSIC_GUESS);
  
  // 1. Monocular Reprojection Error
  double total_err_left = 0.0, total_err_right = 0.0;
  int total_points = 0;
  
  for (size_t i = 0; i < object_points.size(); i++) {
    // Project object points to left camera with its extrinsics
    vector<Point2d> projected_left;
    cv::fisheye::projectPoints(object_points[i], projected_left, rvecs_left[i], tvecs_left[i], K1, D1);
    
    // Calculate error for left camera
    for (size_t j = 0; j < projected_left.size(); j++) {
      double dx = projected_left[j].x - left_img_points[i][j].x;
      double dy = projected_left[j].y - left_img_points[i][j].y;
      total_err_left += sqrt(dx*dx + dy*dy);
    }
    
    // Project object points to right camera with its extrinsics
    vector<Point2d> projected_right;
    cv::fisheye::projectPoints(object_points[i], projected_right, rvecs_right[i], tvecs_right[i], K2, D2);
    
    // Calculate error for right camera
    for (size_t j = 0; j < projected_right.size(); j++) {
      double dx = projected_right[j].x - right_img_points[i][j].x;
      double dy = projected_right[j].y - right_img_points[i][j].y;
      total_err_right += sqrt(dx*dx + dy*dy);
    }
    
    total_points += object_points[i].size();
  }
  
  double avg_err_left = total_err_left / total_points;
  double avg_err_right = total_err_right / total_points;
  double avg_monocular_err = (total_err_left + total_err_right) / (2.0 * total_points);
  
  printf("1. Monocular Reprojection Error:\n");
  printf("   Left camera:  %.4f pixels (avg)\n", avg_err_left);
  printf("   Right camera: %.4f pixels (avg)\n", avg_err_right);
  printf("   Overall:      %.4f pixels (avg) [threshold: < 0.3 pixel]\n", avg_monocular_err);
  printf("   Status: %s\n", avg_monocular_err < 0.3 ? "PASS" : "FAIL");
  
  // 2. Stereo Reprojection Error (left -> transform -> right)
  // For stereo reprojection, we project from left camera through stereo transformation to right camera
  double total_stereo_err = 0.0;
  double max_stereo_err = 0.0;
  int stereo_points = 0;
  
  // Convert stereo rotation matrix to rotation vector
  cv::Vec3d rvec_stereo;
  cv::Rodrigues(Mat(R), rvec_stereo);
  
  for (size_t i = 0; i < object_points.size(); i++) {
    // For stereo reprojection: we use left camera pose and transform to right
    // Transform object points from left camera frame to right camera frame
    vector<Point3d> transformed_points;
    Mat R_mat(R);
    
    for (size_t j = 0; j < object_points[i].size(); j++) {
      // First transform to left camera coordinates
      Mat rvec_left_mat;
      cv::Rodrigues(rvecs_left[i], rvec_left_mat);
      Mat tvec_left_mat(tvecs_left[i]);
      
      // Point in world coordinates
      Mat pt_world = (Mat_<double>(3,1) << object_points[i][j].x, object_points[i][j].y, object_points[i][j].z);
      
      // Transform to left camera coordinates
      Mat pt_left = rvec_left_mat * pt_world + tvec_left_mat;
      
      // Transform from left to right camera coordinates using stereo extrinsics
      Mat pt_right = R_mat * pt_left + Mat(T);
      
      transformed_points.push_back(Point3d(pt_right.at<double>(0), pt_right.at<double>(1), pt_right.at<double>(2)));
    }
    
    // Project transformed points to right camera (with zero extrinsics since points are already in right camera frame)
    vector<Point2d> projected_right;
    cv::fisheye::projectPoints(transformed_points, projected_right, cv::Vec3d(0,0,0), cv::Vec3d(0,0,0), K2, D2);
    
    // Calculate stereo reprojection error
    for (size_t j = 0; j < projected_right.size(); j++) {
      double dx = projected_right[j].x - right_img_points[i][j].x;
      double dy = projected_right[j].y - right_img_points[i][j].y;
      double err = sqrt(dx*dx + dy*dy);
      total_stereo_err += err;
      if (err > max_stereo_err) {
        max_stereo_err = err;
      }
    }
    stereo_points += object_points[i].size();
  }
  
  double avg_stereo_err = total_stereo_err / stereo_points;
  
  printf("\n2. Stereo Reprojection Error:\n");
  printf("   Average: %.4f pixels [threshold: < 0.3 pixel]\n", avg_stereo_err);
  printf("   Status: %s\n", avg_stereo_err < 0.3 ? "PASS" : "FAIL");
  
  printf("\n3. Maximum Stereo Reprojection Error:\n");
  printf("   Maximum: %.4f pixels [threshold: < 1.5 pixel]\n", max_stereo_err);
  printf("   Status: %s\n", max_stereo_err < 1.5 ? "PASS" : "FAIL");
  
  // 4. Stereo Rectification Error
  double total_rectify_err = 0.0;
  int rectify_points = 0;
  
  // Compute rectification maps
  cv::Mat map1x, map1y, map2x, map2y;
  cv::fisheye::initUndistortRectifyMap(K1, D1, R1, P1, img1.size(), CV_32FC1, map1x, map1y);
  cv::fisheye::initUndistortRectifyMap(K2, D2, R2, P2, img2.size(), CV_32FC1, map2x, map2y);
  
  // Check rectification error for detected corners
  for (size_t i = 0; i < left_img_points.size(); i++) {
    for (size_t j = 0; j < left_img_points[i].size(); j++) {
      // Get original point coordinates
      Point2f left_pt = Point2f((float)left_img_points[i][j].x, (float)left_img_points[i][j].y);
      Point2f right_pt = Point2f((float)right_img_points[i][j].x, (float)right_img_points[i][j].y);
      
      // Apply rectification transformation
      float left_rect_x = map1x.at<float>(cvRound(left_pt.y), cvRound(left_pt.x));
      float left_rect_y = map1y.at<float>(cvRound(left_pt.y), cvRound(left_pt.x));
      float right_rect_x = map2x.at<float>(cvRound(right_pt.y), cvRound(right_pt.x));
      float right_rect_y = map2y.at<float>(cvRound(right_pt.y), cvRound(right_pt.x));
      
      // Calculate y-coordinate difference
      if (left_rect_x >= 0 && left_rect_y >= 0 && right_rect_x >= 0 && right_rect_y >= 0) {
        double y_diff = fabs(left_rect_y - right_rect_y);
        total_rectify_err += y_diff;
        rectify_points++;
      }
    }
  }
  
  double avg_rectify_err = (rectify_points > 0) ? (total_rectify_err / rectify_points) : 0.0;
  
  printf("\n4. Stereo Rectification Error:\n");
  printf("   Average Y-coordinate difference: %.4f pixels [threshold: < 0.3 pixel]\n", avg_rectify_err);
  printf("   Status: %s\n", avg_rectify_err < 0.3 ? "PASS" : "FAIL");
  
  // 5. Baseline Distance
  double calibrated_baseline = cv::norm(T); // in meters
  
  printf("\n5. Baseline Distance:\n");
  printf("   Calibrated baseline: %.6f meters (%.2f mm)\n", calibrated_baseline, calibrated_baseline * 1000.0);
  
  if (physical_baseline > 0) {
    double baseline_error = fabs(calibrated_baseline - physical_baseline);
    printf("   Physical baseline:   %.6f meters (%.2f mm)\n", physical_baseline, physical_baseline * 1000.0);
    printf("   Baseline error:      %.6f meters (%.2f mm) [threshold: < 1 mm]\n", 
           baseline_error, baseline_error * 1000.0);
    printf("   Status: %s\n", baseline_error < 0.001 ? "PASS" : "FAIL");
  } else {
    printf("   Physical baseline not provided (use -b option)\n");
  }
  
  // Write evaluation metrics to output file
  fs1 << "monocular_reprojection_error_left" << avg_err_left;
  fs1 << "monocular_reprojection_error_right" << avg_err_right;
  fs1 << "monocular_reprojection_error_avg" << avg_monocular_err;
  fs1 << "stereo_reprojection_error_avg" << avg_stereo_err;
  fs1 << "stereo_reprojection_error_max" << max_stereo_err;
  fs1 << "stereo_rectification_error_avg" << avg_rectify_err;
  fs1 << "calibrated_baseline" << calibrated_baseline;
  if (physical_baseline > 0) {
    fs1 << "physical_baseline" << physical_baseline;
    fs1 << "baseline_error" << fabs(calibrated_baseline - physical_baseline);
  }
  
  printf("\n====================================================\n");
  printf("Evaluation metrics saved to: %s\n", out_file);
  
  return 0;
}
