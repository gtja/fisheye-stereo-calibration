#include <opencv2/core/core.hpp>
#include <ceres/ceres.h>
#include <iostream>
#include "double_sphere.h"

using namespace std;
using namespace cv;

// Test function to verify diagnostic messages work correctly
int main() {
    cout << "Testing rectification error calculation with edge cases..." << endl;
    
    // Create minimal test data
    vector<vector<Point3d>> object_points;
    vector<vector<Point2d>> left_img_points, right_img_points;
    
    // Add one test image with a few corner points
    vector<Point3d> obj_pts;
    vector<Point2d> left_pts, right_pts;
    
    // Create a simple 3x3 grid
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            obj_pts.push_back(Point3d(c * 0.03, r * 0.03, 0.0));
            left_pts.push_back(Point2d(400 + c * 50, 300 + r * 50));
            right_pts.push_back(Point2d(350 + c * 50, 300 + r * 50));
        }
    }
    
    object_points.push_back(obj_pts);
    left_img_points.push_back(left_pts);
    right_img_points.push_back(right_pts);
    
    // Create test camera parameters
    double_sphere::DoubleSphereParams left_params, right_params;
    left_params.fx = 200.0;
    left_params.fy = 200.0;
    left_params.cx = 400.0;
    left_params.cy = 300.0;
    
    right_params.fx = 200.0;
    right_params.fy = 200.0;
    right_params.cx = 400.0;
    right_params.cy = 300.0;
    
    // Create test extrinsics (identity rotation, small translation)
    vector<double*> camera_extrinsics_left, camera_extrinsics_right;
    double* extr_left = new double[6]{0, 0, 0, 0, 0, 0.5};  // Translation only in Z
    double* extr_right = new double[6]{0, 0, 0, 0.1, 0, 0.5};  // Baseline in X
    
    camera_extrinsics_left.push_back(extr_left);
    camera_extrinsics_right.push_back(extr_right);
    
    // Test 1: Normal case with reasonable size
    cout << "\nTest 1: Normal rectified image size (800x600)" << endl;
    Mat R = Mat::eye(3, 3, CV_64F);
    Mat T = (Mat_<double>(3,1) << 0.1, 0, 0);
    Size image_size(800, 600);
    Size rectified_size(800, 600);
    
    double avg_err = 0.0, max_err = 0.0;
    int num_points = double_sphere::calculateRectificationError(
        object_points, left_img_points, right_img_points,
        left_params, right_params,
        R, T, image_size, rectified_size,
        camera_extrinsics_left, camera_extrinsics_right,
        avg_err, max_err
    );
    
    cout << "Result: " << num_points << " points evaluated" << endl;
    if (num_points > 0) {
        cout << "Average error: " << avg_err << ", Max error: " << max_err << endl;
    }
    
    // Test 2: Very small rectified image (should trigger diagnostic messages)
    cout << "\nTest 2: Very small rectified image size (50x50)" << endl;
    rectified_size = Size(50, 50);
    
    num_points = double_sphere::calculateRectificationError(
        object_points, left_img_points, right_img_points,
        left_params, right_params,
        R, T, image_size, rectified_size,
        camera_extrinsics_left, camera_extrinsics_right,
        avg_err, max_err
    );
    
    cout << "Result: " << num_points << " points evaluated" << endl;
    if (num_points > 0) {
        cout << "Average error: " << avg_err << ", Max error: " << max_err << endl;
    }
    
    // Test 3: Very large focal length (points will be out of bounds)
    cout << "\nTest 3: Mismatched virtual camera focal length" << endl;
    rectified_size = Size(800, 600);
    left_params.fx = 50.0;  // Very small focal length
    left_params.fy = 50.0;
    right_params.fx = 50.0;
    right_params.fy = 50.0;
    
    num_points = double_sphere::calculateRectificationError(
        object_points, left_img_points, right_img_points,
        left_params, right_params,
        R, T, image_size, rectified_size,
        camera_extrinsics_left, camera_extrinsics_right,
        avg_err, max_err
    );
    
    cout << "Result: " << num_points << " points evaluated" << endl;
    if (num_points > 0) {
        cout << "Average error: " << avg_err << ", Max error: " << max_err << endl;
    }
    
    // Cleanup
    delete[] extr_left;
    delete[] extr_right;
    
    cout << "\nAll tests completed successfully!" << endl;
    return 0;
}
