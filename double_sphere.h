#ifndef DOUBLE_SPHERE_H
#define DOUBLE_SPHERE_H

#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <vector>

namespace double_sphere {

// Double-Sphere camera model parameters
// Reference: "The Double Sphere Camera Model" by Usenko et al., 2018
struct DoubleSphereParams {
    // Intrinsics
    double fx, fy;  // Focal length
    double cx, cy;  // Principal point
    double xi;      // First projection parameter (mirror parameter)
    double alpha;   // Second projection parameter (distinction from UCM)
    
    // 6-order radial distortion coefficients
    double k1, k2, k3, k4, k5, k6;
    
    DoubleSphereParams() :
        fx(400.0), fy(400.0), cx(320.0), cy(240.0),
        xi(0.0), alpha(0.5),
        k1(0.0), k2(0.0), k3(0.0), k4(0.0), k5(0.0), k6(0.0) {}
};

// Project a 3D point to 2D using Double-Sphere model
inline bool project(const DoubleSphereParams& params,
                   const double* point3d,
                   double* point2d) {
    const double& fx = params.fx;
    const double& fy = params.fy;
    const double& cx = params.cx;
    const double& cy = params.cy;
    const double& xi = params.xi;
    const double& alpha = params.alpha;
    const double& k1 = params.k1;
    const double& k2 = params.k2;
    const double& k3 = params.k3;
    const double& k4 = params.k4;
    const double& k5 = params.k5;
    const double& k6 = params.k6;
    
    double x = point3d[0];
    double y = point3d[1];
    double z = point3d[2];
    
    // Check for points behind camera
    if (z <= 0.0) {
        return false;
    }
    
    // Double-Sphere projection
    double d1 = sqrt(x*x + y*y + z*z);
    double d2 = sqrt(x*x + y*y + (xi*d1 + z)*(xi*d1 + z));
    
    // Denominator check
    double denom = alpha*d2 + (1.0 - alpha)*(xi*d1 + z);
    if (fabs(denom) < 1e-10) {
        return false;
    }
    
    // Normalized coordinates
    double mx = x / denom;
    double my = y / denom;
    
    // Radial distance squared
    double r2 = mx*mx + my*my;
    double r4 = r2*r2;
    double r6 = r4*r2;
    
    // 6-order radial distortion
    double radial = 1.0 + k1*r2 + k2*r4 + k3*r6 + k4*r2*r4 + k5*r4*r4 + k6*r2*r6;
    
    // Distorted coordinates
    double mx_dist = mx * radial;
    double my_dist = my * radial;
    
    // Apply focal length and principal point
    point2d[0] = fx * mx_dist + cx;
    point2d[1] = fy * my_dist + cy;
    
    return true;
}

// Unproject a 2D point to 3D unit sphere using Double-Sphere model (iterative)
inline bool unproject(const DoubleSphereParams& params,
                     const double* point2d,
                     double* point3d) {
    const double& fx = params.fx;
    const double& fy = params.fy;
    const double& cx = params.cx;
    const double& cy = params.cy;
    const double& xi = params.xi;
    const double& alpha = params.alpha;
    const double& k1 = params.k1;
    const double& k2 = params.k2;
    const double& k3 = params.k3;
    const double& k4 = params.k4;
    const double& k5 = params.k5;
    const double& k6 = params.k6;
    
    // Normalize pixel coordinates
    double mx_dist = (point2d[0] - cx) / fx;
    double my_dist = (point2d[1] - cy) / fy;
    
    // Iteratively remove radial distortion (Newton-Raphson)
    double mx = mx_dist;
    double my = my_dist;
    
    for (int iter = 0; iter < 10; ++iter) {
        double r2 = mx*mx + my*my;
        double r4 = r2*r2;
        double r6 = r4*r2;
        
        double radial = 1.0 + k1*r2 + k2*r4 + k3*r6 + k4*r2*r4 + k5*r4*r4 + k6*r2*r6;
        double d_radial = k1 + 2.0*k2*r2 + 3.0*k3*r4 + 4.0*k4*r2*r2 + 5.0*k5*r4*r2 + 6.0*k6*r6;
        
        double dx = mx_dist - mx * radial;
        double dy = my_dist - my * radial;
        
        double denom = radial + 2.0 * d_radial * (mx*mx + my*my);
        if (fabs(denom) < 1e-10) break;
        
        mx += dx / denom;
        my += dy / denom;
        
        if (dx*dx + dy*dy < 1e-10) break;
    }
    
    // Reverse Double-Sphere unprojection
    double r2 = mx*mx + my*my;
    double mz2 = 1.0 / (alpha*alpha * r2 + 1.0);
    if (mz2 < 0.0) return false;
    
    double mz = sqrt(mz2);
    double w1 = (mz * alpha + (1.0 - alpha)) / (mz * alpha + (1.0 - alpha) * xi);
    
    point3d[0] = w1 * mx;
    point3d[1] = w1 * my;
    point3d[2] = w1 * mz - xi;
    
    // Normalize to unit vector
    double norm = sqrt(point3d[0]*point3d[0] + point3d[1]*point3d[1] + point3d[2]*point3d[2]);
    if (norm < 1e-10) return false;
    
    point3d[0] /= norm;
    point3d[1] /= norm;
    point3d[2] /= norm;
    
    return true;
}

// Ceres cost functor for reprojection error with Double-Sphere model
struct DoubleSphereReprojectionError {
    DoubleSphereReprojectionError(const cv::Point2d& observed_point,
                                   const cv::Point3d& world_point)
        : observed_x(observed_point.x),
          observed_y(observed_point.y),
          world_x(world_point.x),
          world_y(world_point.y),
          world_z(world_point.z) {}
    
    // Parameters:
    // camera_intrinsics[0-9]: fx, fy, cx, cy, xi, alpha, k1, k2, k3, k4
    // camera_intrinsics[10-11]: k5, k6
    // camera_extrinsics[0-5]: rotation (angle-axis), translation
    template <typename T>
    bool operator()(const T* const camera_intrinsics,
                   const T* const camera_extrinsics,
                   T* residuals) const {
        // Extract camera parameters
        T fx = camera_intrinsics[0];
        T fy = camera_intrinsics[1];
        T cx = camera_intrinsics[2];
        T cy = camera_intrinsics[3];
        T xi = camera_intrinsics[4];
        T alpha = camera_intrinsics[5];
        T k1 = camera_intrinsics[6];
        T k2 = camera_intrinsics[7];
        T k3 = camera_intrinsics[8];
        T k4 = camera_intrinsics[9];
        T k5 = camera_intrinsics[10];
        T k6 = camera_intrinsics[11];
        
        // Transform 3D point from world to camera coordinates
        T point_world[3] = {T(world_x), T(world_y), T(world_z)};
        T point_camera[3];
        
        // Apply rotation (angle-axis)
        ceres::AngleAxisRotatePoint(camera_extrinsics, point_world, point_camera);
        
        // Apply translation
        point_camera[0] += camera_extrinsics[3];
        point_camera[1] += camera_extrinsics[4];
        point_camera[2] += camera_extrinsics[5];
        
        // Check if point is behind camera
        if (point_camera[2] <= T(0.0)) {
            residuals[0] = T(1000.0);
            residuals[1] = T(1000.0);
            return true;
        }
        
        // Double-Sphere projection
        T x = point_camera[0];
        T y = point_camera[1];
        T z = point_camera[2];
        
        T d1 = ceres::sqrt(x*x + y*y + z*z);
        T d2 = ceres::sqrt(x*x + y*y + (xi*d1 + z)*(xi*d1 + z));
        
        T denom = alpha*d2 + (T(1.0) - alpha)*(xi*d1 + z);
        if (ceres::abs(denom) < T(1e-10)) {
            residuals[0] = T(1000.0);
            residuals[1] = T(1000.0);
            return true;
        }
        
        T mx = x / denom;
        T my = y / denom;
        
        // Apply radial distortion
        T r2 = mx*mx + my*my;
        T r4 = r2*r2;
        T r6 = r4*r2;
        
        T radial = T(1.0) + k1*r2 + k2*r4 + k3*r6 + k4*r2*r4 + k5*r4*r4 + k6*r2*r6;
        
        T mx_dist = mx * radial;
        T my_dist = my * radial;
        
        // Project to pixel coordinates
        T predicted_x = fx * mx_dist + cx;
        T predicted_y = fy * my_dist + cy;
        
        // Compute residuals
        residuals[0] = predicted_x - T(observed_x);
        residuals[1] = predicted_y - T(observed_y);
        
        return true;
    }
    
    // Factory method
    static ceres::CostFunction* Create(const cv::Point2d& observed_point,
                                       const cv::Point3d& world_point) {
        return new ceres::AutoDiffCostFunction<DoubleSphereReprojectionError, 2, 12, 6>(
            new DoubleSphereReprojectionError(observed_point, world_point));
    }
    
private:
    double observed_x, observed_y;
    double world_x, world_y, world_z;
};

// Virtual pinhole camera parameters for rectification
struct VirtualPinholeParams {
    double fx, fy;  // Virtual focal length
    double cx, cy;  // Virtual principal point (usually image center)
    
    VirtualPinholeParams(int width, int height, double focal_length = 300.0) :
        fx(focal_length), fy(focal_length),
        cx(width / 2.0), cy(height / 2.0) {}
};

// Create stereo rectification maps for Double-Sphere model
// This implements custom rectification as standard OpenCV rectification doesn't support DS model
// Approach: For each pixel in virtual rectified image, unproject to 3D ray, then project to original DS image
inline void createStereoRectificationMaps(
    const DoubleSphereParams& left_params,
    const DoubleSphereParams& right_params,
    const cv::Mat& R,  // Rotation from left to right camera (3x3)
    const cv::Mat& T,  // Translation from left to right camera (3x1)
    const cv::Size& image_size,
    const cv::Size& rectified_size,
    cv::Mat& map_left_x, cv::Mat& map_left_y,
    cv::Mat& map_right_x, cv::Mat& map_right_y)
{
    // Create virtual pinhole camera parameters
    VirtualPinholeParams virtual_cam(rectified_size.width, rectified_size.height);
    
    // Initialize output maps
    map_left_x.create(rectified_size, CV_32FC1);
    map_left_y.create(rectified_size, CV_32FC1);
    map_right_x.create(rectified_size, CV_32FC1);
    map_right_y.create(rectified_size, CV_32FC1);
    
    // Compute rectification transforms
    // For stereo rectification, we want both cameras to look in the same direction
    // Standard approach: align with average optical axis
    
    // Compute rotation to align left camera with rectified coordinate system
    // Rectified system: X-axis along baseline, Z-axis forward, Y-axis down
    cv::Mat baseline = T.clone();
    double baseline_norm = cv::norm(baseline);
    if (baseline_norm < 1e-10) {
        std::cerr << "Warning: Baseline too small for rectification" << std::endl;
        return;
    }
    
    // Standard stereo rectification approach:
    // We want to create a rectified coordinate system where:
    // - e1 (new X) points along the baseline (left to right)
    // - e3 (new Z) points forward
    // - e2 (new Y) completes the right-handed system
    
    // e1: normalized baseline direction (pointing from left to right)
    cv::Mat e1 = baseline / baseline_norm;
    
    // We want e3 to point forward. Original forward is [0, 0, 1]
    // e3 should be perpendicular to e1 and close to [0, 0, 1]
    // Use Gram-Schmidt: e3 = forward - (forward·e1)e1, then normalize
    cv::Mat forward = (cv::Mat_<double>(3,1) << 0, 0, 1);
    double dot_product = e1.dot(forward);
    cv::Mat e3 = forward - dot_product * e1;
    double e3_norm = cv::norm(e3);
    
    if (e3_norm < 1e-10) {
        // Baseline parallel to Z-axis, choose perpendicular direction
        // Try Y-axis
        forward = (cv::Mat_<double>(3,1) << 0, 1, 0);
        dot_product = e1.dot(forward);
        e3 = forward - dot_product * e1;
        e3_norm = cv::norm(e3);
    }
    e3 = e3 / e3_norm;
    
    // Ensure e3 points forward (positive Z component)
    if (e3.at<double>(2) < 0) {
        e3 = -e3;
    }
    
    // e2: completes right-handed system, e2 = e3 × e1
    cv::Mat e2 = e3.cross(e1);
    e2 = e2 / cv::norm(e2);
    
    // Build rectification rotation matrix for left camera
    // R_rect maps from left camera coords to rectified coords
    cv::Mat R_rect_left = cv::Mat::zeros(3, 3, CV_64F);
    for (int i = 0; i < 3; i++) {
        R_rect_left.at<double>(0, i) = e1.at<double>(i);
        R_rect_left.at<double>(1, i) = e2.at<double>(i);
        R_rect_left.at<double>(2, i) = e3.at<double>(i);
    }
    
    // Rectification rotation for right camera: R_rect_right = R_rect_left * R
    cv::Mat R_rect_right = R_rect_left * R;
    
    // Generate remapping for left camera
    for (int v = 0; v < rectified_size.height; v++) {
        for (int u = 0; u < rectified_size.width; u++) {
            // Unproject from virtual pinhole to 3D ray in rectified coordinate system
            double x_rect = (u - virtual_cam.cx) / virtual_cam.fx;
            double y_rect = (v - virtual_cam.cy) / virtual_cam.fy;
            double z_rect = 1.0;
            
            // Normalize to unit ray
            double norm = std::sqrt(x_rect*x_rect + y_rect*y_rect + z_rect*z_rect);
            cv::Mat ray_rect = (cv::Mat_<double>(3,1) << x_rect/norm, y_rect/norm, z_rect/norm);
            
            // Transform ray from rectified to left camera coordinate system
            cv::Mat ray_left = R_rect_left.t() * ray_rect;
            
            // Project ray to left camera using Double-Sphere model
            double ray_left_data[3] = {ray_left.at<double>(0), ray_left.at<double>(1), ray_left.at<double>(2)};
            double point2d[2];
            
            if (project(left_params, ray_left_data, point2d)) {
                // Check if projection is within image bounds
                if (point2d[0] >= 0 && point2d[0] < image_size.width &&
                    point2d[1] >= 0 && point2d[1] < image_size.height) {
                    map_left_x.at<float>(v, u) = static_cast<float>(point2d[0]);
                    map_left_y.at<float>(v, u) = static_cast<float>(point2d[1]);
                } else {
                    map_left_x.at<float>(v, u) = -1.0f;
                    map_left_y.at<float>(v, u) = -1.0f;
                }
            } else {
                map_left_x.at<float>(v, u) = -1.0f;
                map_left_y.at<float>(v, u) = -1.0f;
            }
        }
    }
    
    // Generate remapping for right camera
    for (int v = 0; v < rectified_size.height; v++) {
        for (int u = 0; u < rectified_size.width; u++) {
            // Unproject from virtual pinhole to 3D ray in rectified coordinate system
            double x_rect = (u - virtual_cam.cx) / virtual_cam.fx;
            double y_rect = (v - virtual_cam.cy) / virtual_cam.fy;
            double z_rect = 1.0;
            
            // Normalize to unit ray
            double norm = std::sqrt(x_rect*x_rect + y_rect*y_rect + z_rect*z_rect);
            cv::Mat ray_rect = (cv::Mat_<double>(3,1) << x_rect/norm, y_rect/norm, z_rect/norm);
            
            // Transform ray from rectified to right camera coordinate system
            cv::Mat ray_right = R_rect_right.t() * ray_rect;
            
            // Project ray to right camera using Double-Sphere model
            double ray_right_data[3] = {ray_right.at<double>(0), ray_right.at<double>(1), ray_right.at<double>(2)};
            double point2d[2];
            
            if (project(right_params, ray_right_data, point2d)) {
                // Check if projection is within image bounds
                if (point2d[0] >= 0 && point2d[0] < image_size.width &&
                    point2d[1] >= 0 && point2d[1] < image_size.height) {
                    map_right_x.at<float>(v, u) = static_cast<float>(point2d[0]);
                    map_right_y.at<float>(v, u) = static_cast<float>(point2d[1]);
                } else {
                    map_right_x.at<float>(v, u) = -1.0f;
                    map_right_y.at<float>(v, u) = -1.0f;
                }
            } else {
                map_right_x.at<float>(v, u) = -1.0f;
                map_right_y.at<float>(v, u) = -1.0f;
            }
        }
    }
}

// Calculate stereo rectification error using observed corner points
// Returns average and maximum y-coordinate differences after rectification
// This measures how well the rectification aligns corresponding points horizontally
inline int calculateRectificationError(
    const std::vector<std::vector<cv::Point3d>>& object_points,
    const std::vector<std::vector<cv::Point2d>>& left_img_points,
    const std::vector<std::vector<cv::Point2d>>& right_img_points,
    const DoubleSphereParams& left_params,
    const DoubleSphereParams& right_params,
    const cv::Mat& R,
    const cv::Mat& T,
    const cv::Size& image_size,
    const cv::Size& rectified_size,
    const std::vector<double*>& camera_extrinsics_left,
    const std::vector<double*>& camera_extrinsics_right,
    double& avg_error,
    double& max_error)
{
    avg_error = 0.0;
    max_error = 0.0;
    int valid_points = 0;
    
    // Diagnostic counters for debugging
    int total_points = 0;
    int points_behind_camera = 0;
    int points_negative_z_rect = 0;
    int points_out_of_bounds = 0;
    
    // Compute rectification rotation matrices
    cv::Mat baseline = T.clone();
    double baseline_norm = cv::norm(baseline);
    if (baseline_norm < 1e-10) {
        std::cerr << "Warning: Baseline too small for rectification" << std::endl;
        return 0;
    }
    
    // Standard stereo rectification: e1 along baseline, e3 forward, e2 completes system
    cv::Mat e1 = baseline / baseline_norm;
    
    // Use Gram-Schmidt to get e3 perpendicular to e1 and close to forward [0,0,1]
    cv::Mat forward = (cv::Mat_<double>(3,1) << 0, 0, 1);
    double dot_product = e1.dot(forward);
    cv::Mat e3 = forward - dot_product * e1;
    double e3_norm = cv::norm(e3);
    
    if (e3_norm < 1e-10) {
        // Baseline parallel to Z, use Y-axis instead
        forward = (cv::Mat_<double>(3,1) << 0, 1, 0);
        dot_product = e1.dot(forward);
        e3 = forward - dot_product * e1;
        e3_norm = cv::norm(e3);
    }
    e3 = e3 / e3_norm;
    
    // Ensure e3 points forward (positive Z component)
    if (e3.at<double>(2) < 0) {
        e3 = -e3;
    }
    
    // e2 completes right-handed system
    cv::Mat e2 = e3.cross(e1);
    e2 = e2 / cv::norm(e2);
    
    // Build rectification rotation matrix for left camera
    // R_rect maps from left camera coordinates to rectified coordinates
    cv::Mat R_rect_left_full = cv::Mat::zeros(3, 3, CV_64F);
    for (int i = 0; i < 3; i++) {
        R_rect_left_full.at<double>(0, i) = e1.at<double>(i);
        R_rect_left_full.at<double>(1, i) = e2.at<double>(i);
        R_rect_left_full.at<double>(2, i) = e3.at<double>(i);
    }
    
    // For extreme wide-angle lenses, use partial rectification to avoid negative Z
    // Estimate the field of view from the focal length and image size
    double avg_fx_check = (left_params.fx + right_params.fx) / 2.0;
    double image_diagonal = std::sqrt(image_size.width * image_size.width + 
                                      image_size.height * image_size.height);
    double approx_fov_deg = 2.0 * std::atan(image_diagonal / (2.0 * avg_fx_check)) * 180.0 / M_PI;
    
    cv::Mat R_rect_left;
    double rect_strength = 1.0;  // 1.0 = full rectification, 0.0 = no rectification
    
    if (approx_fov_deg > 170.0) {
        // Extreme wide-angle lens: use partial rectification (40%)
        rect_strength = 0.4;
    } else if (approx_fov_deg > 130.0) {
        // Wide-angle lens: use partial rectification (45%)
        // Reduced from 0.7 to prevent points going behind camera (Z<0)
        rect_strength = 0.45;
    } else if (approx_fov_deg > 100.0) {
        // Moderate wide-angle lens: use partial rectification (70%)
        rect_strength = 0.7;
    }
    
    if (rect_strength < 1.0) {
        // Apply partial rectification using angle-axis interpolation
        cv::Mat rvec_full;
        cv::Rodrigues(R_rect_left_full, rvec_full);
        cv::Mat rvec_partial = rvec_full * rect_strength;
        cv::Rodrigues(rvec_partial, R_rect_left);
    } else {
        R_rect_left = R_rect_left_full.clone();
    }
    
    // Rectification rotation for right camera
    cv::Mat R_rect_right = R_rect_left * R;
    
    // Virtual pinhole camera parameters
    // Use average focal length from the calibrated cameras to better match the projection
    double avg_fx = (left_params.fx + right_params.fx) / 2.0;
    double avg_fy = (left_params.fy + right_params.fy) / 2.0;
    
    // For wide-angle lenses, keep virtual focal length >= original focal length
    // This prevents excessive distortion and keeps errors manageable
    // Scaling down (< 1.0) causes the virtual camera to have wider FOV than the fisheye,
    // leading to extreme distortions at image boundaries (600+ pixel errors)
    double focal_scale = 1.0;
    if (avg_fx < 300.0) {
        // Extreme wide-angle lens detected (FOV > ~200°)
        focal_scale = 1.1;  // Slightly increase to crop to common FOV
    } else if (avg_fx < 500.0) {
        // Wide-angle lens (FOV > ~150°)
        focal_scale = 1.0;  // Keep same as original to match projection
    } else if (avg_fx < 700.0) {
        // Moderate wide-angle
        focal_scale = 0.95; // Slight decrease is acceptable for narrower FOV
    } else {
        // Standard lens
        focal_scale = 0.9;
    }
    
    VirtualPinholeParams virtual_cam(rectified_size.width, rectified_size.height, 
                                     avg_fx * focal_scale);
    virtual_cam.fy = avg_fy * focal_scale;
    
    // For each 3D point, project through both cameras and measure rectification error
    for (size_t i = 0; i < object_points.size(); i++) {
        for (size_t j = 0; j < object_points[i].size(); j++) {
            total_points++;
            
            // Transform 3D point to left camera coordinates using optimized extrinsics
            double point_world[3] = {object_points[i][j].x, object_points[i][j].y, object_points[i][j].z};
            double point_camera_left[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_left[i], point_world, point_camera_left);
            point_camera_left[0] += camera_extrinsics_left[i][3];
            point_camera_left[1] += camera_extrinsics_left[i][4];
            point_camera_left[2] += camera_extrinsics_left[i][5];
            
            // Transform 3D point to right camera coordinates using optimized extrinsics
            double point_camera_right[3];
            ceres::AngleAxisRotatePoint(camera_extrinsics_right[i], point_world, point_camera_right);
            point_camera_right[0] += camera_extrinsics_right[i][3];
            point_camera_right[1] += camera_extrinsics_right[i][4];
            point_camera_right[2] += camera_extrinsics_right[i][5];
            
            // Skip points behind camera
            if (point_camera_left[2] <= 0 || point_camera_right[2] <= 0) {
                points_behind_camera++;
                continue;
            }
            
            // Transform 3D points from camera coordinates to rectified coordinate systems
            // R_rect maps from camera to rectified, so we use it directly
            cv::Mat pt_left_cam = (cv::Mat_<double>(3,1) << point_camera_left[0], point_camera_left[1], point_camera_left[2]);
            cv::Mat pt_right_cam = (cv::Mat_<double>(3,1) << point_camera_right[0], point_camera_right[1], point_camera_right[2]);
            
            // Apply rectification: pt_rect = R_rect * pt_cam
            cv::Mat pt_left_rect = R_rect_left * pt_left_cam;
            cv::Mat pt_right_rect = R_rect_right * pt_right_cam;
            

            // Project to virtual pinhole image
            if (pt_left_rect.at<double>(2) > 0 && pt_right_rect.at<double>(2) > 0) {
                double u_left = virtual_cam.fx * (pt_left_rect.at<double>(0) / pt_left_rect.at<double>(2)) + virtual_cam.cx;
                double v_left = virtual_cam.fy * (pt_left_rect.at<double>(1) / pt_left_rect.at<double>(2)) + virtual_cam.cy;
                
                double u_right = virtual_cam.fx * (pt_right_rect.at<double>(0) / pt_right_rect.at<double>(2)) + virtual_cam.cx;
                double v_right = virtual_cam.fy * (pt_right_rect.at<double>(1) / pt_right_rect.at<double>(2)) + virtual_cam.cy;
                
                // Check if points are within rectified image bounds
                if (v_left >= 0 && v_left < rectified_size.height &&
                    v_right >= 0 && v_right < rectified_size.height &&
                    u_left >= 0 && u_left < rectified_size.width &&
                    u_right >= 0 && u_right < rectified_size.width) {
                    
                    // Compute y-coordinate difference (epipolar error)
                    // After perfect rectification, corresponding points should have same y-coordinate
                    double y_diff = std::abs(v_left - v_right);
                    
                    avg_error += y_diff;
                    if (y_diff > max_error) {
                        max_error = y_diff;
                    }
                    valid_points++;
                } else {
                    points_out_of_bounds++;
                }
            } else {
                points_negative_z_rect++;
            }
        }
    }
    

    
    if (valid_points > 0) {
        avg_error /= valid_points;
    }
    
    // Always print diagnostics if we have fewer than 50% valid points
    // This helps users understand what's happening with extreme wide-angle lenses
    if (total_points > 0 && valid_points < total_points / 2) {
        double valid_percentage = 100.0 * valid_points / total_points;
        std::cerr << "\nRectification diagnostic information (only " << valid_percentage 
                  << "% of points were valid):" << std::endl;
        std::cerr << "   Total corner points: " << total_points << std::endl;
        std::cerr << "   Valid points: " << valid_points << " (" << valid_percentage << "%)" << std::endl;
        std::cerr << "   Points behind camera: " << points_behind_camera 
                  << " (" << 100.0 * points_behind_camera / total_points << "%)" << std::endl;
        std::cerr << "   Points with negative Z after rectification: " << points_negative_z_rect 
                  << " (" << 100.0 * points_negative_z_rect / total_points << "%)" << std::endl;
        std::cerr << "   Points outside rectified image bounds: " << points_out_of_bounds 
                  << " (" << 100.0 * points_out_of_bounds / total_points << "%)" << std::endl;
        std::cerr << "   Rectified image size: " << rectified_size.width << "x" << rectified_size.height << std::endl;
        std::cerr << "   Calibrated focal lengths: left_fx=" << left_params.fx << ", right_fx=" << right_params.fx << std::endl;
        std::cerr << "   Estimated FOV: " << approx_fov_deg << "° (rectification strength: " << rect_strength << ")" << std::endl;
        std::cerr << "   Virtual camera: fx=" << virtual_cam.fx << ", fy=" << virtual_cam.fy 
                  << ", cx=" << virtual_cam.cx << ", cy=" << virtual_cam.cy << std::endl;
        
        if (valid_points == 0) {
            std::cerr << "Suggestions:" << std::endl;
            if (points_out_of_bounds > total_points / 2) {
                std::cerr << "   - Most points are out of bounds: rectified image size may be too small" << std::endl;
            }
            if (points_negative_z_rect > total_points / 2) {
                std::cerr << "   - Most points have negative Z: this is expected for extreme wide-angle lenses (FOV > 180°)" << std::endl;
                std::cerr << "   - Consider using the monocular and stereo reprojection errors as primary metrics" << std::endl;
            }
            if (points_behind_camera > total_points / 2) {
                std::cerr << "   - Most points are behind camera: extrinsics may be incorrect" << std::endl;
            }
        }
    }
    
    return valid_points;
}

} // namespace double_sphere

#endif // DOUBLE_SPHERE_H
