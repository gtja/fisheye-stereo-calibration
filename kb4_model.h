#ifndef KB4_MODEL_H
#define KB4_MODEL_H

#include <opencv2/core/core.hpp>
#include <cmath>

namespace kb4 {

// Kannala-Brandt fisheye model (KB4) with 4 distortion parameters
// This is used for initial coarse calibration
struct KB4Params {
    // Intrinsics
    double fx, fy;  // Focal length
    double cx, cy;  // Principal point
    
    // 4 distortion coefficients (k1, k2, k3, k4)
    double k1, k2, k3, k4;
    
    KB4Params() :
        fx(400.0), fy(400.0), cx(320.0), cy(240.0),
        k1(0.0), k2(0.0), k3(0.0), k4(0.0) {}
    
    KB4Params(const cv::Matx33d& K, const cv::Vec4d& D) :
        fx(K(0,0)), fy(K(1,1)), cx(K(0,2)), cy(K(1,2)),
        k1(D[0]), k2(D[1]), k3(D[2]), k4(D[3]) {}
};

// Project a 3D point to 2D using KB4 model (same as OpenCV fisheye)
inline bool project(const KB4Params& params,
                   const double* point3d,
                   double* point2d) {
    const double& fx = params.fx;
    const double& fy = params.fy;
    const double& cx = params.cx;
    const double& cy = params.cy;
    const double& k1 = params.k1;
    const double& k2 = params.k2;
    const double& k3 = params.k3;
    const double& k4 = params.k4;
    
    double x = point3d[0];
    double y = point3d[1];
    double z = point3d[2];
    
    // Use atan2 to handle full sphere (including z <= 0)
    double r_3d = sqrt(x*x + y*y);
    double theta = atan2(r_3d, z);
    
    // Apply distortion
    double theta2 = theta * theta;
    double theta4 = theta2 * theta2;
    double theta6 = theta4 * theta2;
    double theta8 = theta4 * theta4;
    
    double theta_d = theta * (1.0 + k1*theta2 + k2*theta4 + k3*theta6 + k4*theta8);
    
    // Calculate scaling factor
    // We want to map (x,y) direction to radius theta_d
    // x_img = theta_d * cos(phi) = theta_d * (x / r_3d)
    // y_img = theta_d * sin(phi) = theta_d * (y / r_3d)
    // scale = theta_d / r_3d
    
    double scale = (r_3d > 1e-8) ? (theta_d / r_3d) : 1.0;
    
    double mx = x * scale;
    double my = y * scale;
    
    // Apply focal length and principal point
    point2d[0] = fx * mx + cx;
    point2d[1] = fy * my + cy;
    
    return true;
}

// Unproject a 2D point to 3D unit sphere using KB4 model (iterative)
inline bool unproject(const KB4Params& params,
                     const double* point2d,
                     double* point3d) {
    const double& fx = params.fx;
    const double& fy = params.fy;
    const double& cx = params.cx;
    const double& cy = params.cy;
    const double& k1 = params.k1;
    const double& k2 = params.k2;
    const double& k3 = params.k3;
    const double& k4 = params.k4;
    
    // Normalize pixel coordinates
    double mx = (point2d[0] - cx) / fx;
    double my = (point2d[1] - cy) / fy;
    
    double r_dist = sqrt(mx*mx + my*my);
    
    // Iteratively solve for theta (undistorted angle)
    double theta = r_dist;  // Initial guess
    
    for (int iter = 0; iter < 10; ++iter) {
        double theta2 = theta * theta;
        double theta4 = theta2 * theta2;
        double theta6 = theta4 * theta2;
        double theta8 = theta4 * theta4;
        
        // f(theta) = theta * (1 + k1*theta^2 + k2*theta^4 + k3*theta^6 + k4*theta^8) - r_dist
        double f = theta * (1.0 + k1*theta2 + k2*theta4 + k3*theta6 + k4*theta8) - r_dist;
        
        // f'(theta) = 1 + 3*k1*theta^2 + 5*k2*theta^4 + 7*k3*theta^6 + 9*k4*theta^8
        double df = 1.0 + 3.0*k1*theta2 + 5.0*k2*theta4 + 7.0*k3*theta6 + 9.0*k4*theta8;
        
        if (fabs(df) < 1e-10) break;
        
        double delta = f / df;
        theta -= delta;
        
        if (fabs(delta) < 1e-10) break;
    }
    
    // Convert back to 3D point
    double sin_theta = sin(theta);
    double cos_theta = cos(theta);
    
    if (r_dist > 1e-8) {
        double scale = sin_theta / r_dist;
        point3d[0] = mx * scale;
        point3d[1] = my * scale;
        point3d[2] = cos_theta;
    } else {
        point3d[0] = 0.0;
        point3d[1] = 0.0;
        point3d[2] = 1.0;
    }
    
    return true;
}

// Create a rectification map for SE(3) pre-correction
// This warps the image to a virtual plane for better corner detection
inline void createRectificationMap(const KB4Params& params,
                                   const cv::Size& image_size,
                                   const cv::Size& rectified_size,
                                   cv::Mat& map_x,
                                   cv::Mat& map_y,
                                   const cv::Mat& R = cv::Mat::eye(3, 3, CV_64F)) {
    map_x.create(rectified_size, CV_32FC1);
    map_y.create(rectified_size, CV_32FC1);
    
    // Virtual camera parameters (pinhole) for rectified image
    double fx_rect = params.fx * 0.6;  // Scale down for better coverage
    double fy_rect = params.fy * 0.6;
    double cx_rect = rectified_size.width * 0.5;
    double cy_rect = rectified_size.height * 0.5;
    
    for (int v = 0; v < rectified_size.height; ++v) {
        for (int u = 0; u < rectified_size.width; ++u) {
            // Pixel in rectified (pinhole) image
            double x_rect = (u - cx_rect) / fx_rect;
            double y_rect = (v - cy_rect) / fy_rect;
            double z_rect = 1.0;
            
            // Normalize to unit vector
            double norm = sqrt(x_rect*x_rect + y_rect*y_rect + z_rect*z_rect);
            double p_virt[3] = {x_rect/norm, y_rect/norm, z_rect/norm};
            
            // Apply rotation: P_cam = R * P_virt
            double point3d[3];
            point3d[0] = R.at<double>(0,0)*p_virt[0] + R.at<double>(0,1)*p_virt[1] + R.at<double>(0,2)*p_virt[2];
            point3d[1] = R.at<double>(1,0)*p_virt[0] + R.at<double>(1,1)*p_virt[1] + R.at<double>(1,2)*p_virt[2];
            point3d[2] = R.at<double>(2,0)*p_virt[0] + R.at<double>(2,1)*p_virt[1] + R.at<double>(2,2)*p_virt[2];
            
            // Project to original fisheye image
            double point2d[2];
            if (project(params, point3d, point2d)) {
                // Check if within image bounds
                if (point2d[0] >= 0 && point2d[0] < image_size.width &&
                    point2d[1] >= 0 && point2d[1] < image_size.height) {
                    map_x.at<float>(v, u) = static_cast<float>(point2d[0]);
                    map_y.at<float>(v, u) = static_cast<float>(point2d[1]);
                } else {
                    map_x.at<float>(v, u) = -1.0f;
                    map_y.at<float>(v, u) = -1.0f;
                }
            } else {
                map_x.at<float>(v, u) = -1.0f;
                map_y.at<float>(v, u) = -1.0f;
            }
        }
    }
}

} // namespace kb4

#endif // KB4_MODEL_H
