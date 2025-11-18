#ifndef DOUBLE_SPHERE_H
#define DOUBLE_SPHERE_H

#include <opencv2/core/core.hpp>
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

} // namespace double_sphere

#endif // DOUBLE_SPHERE_H
