#!/usr/bin/env python3
"""
Rectify images using Double-Sphere calibration (full model).
Supports monocular and stereo rectification.

Usage:
  python3 rectify_images.py -c cam_stereo.yml -i imgs/ -o output_rectified/ -l left --mono -e bmp
  python3 rectify_images.py -c cam_stereo.yml -i imgs/ -o output_rectified/ -l left -r right -e bmp
"""
import sys
import os
import argparse
import yaml
import cv2
import numpy as np
import glob

class DoubleSphereCamera:
    def __init__(self, params):
        self.fx = float(params['fx'])
        self.fy = float(params['fy'])
        self.cx = float(params['cx'])
        self.cy = float(params['cy'])
        self.xi = float(params['xi'])
        self.alpha = float(params['alpha'])
        self.k1 = float(params.get('k1', 0))
        self.k2 = float(params.get('k2', 0))
        self.k3 = float(params.get('k3', 0))
        self.k4 = float(params.get('k4', 0))
        self.k5 = float(params.get('k5', 0))
        self.k6 = float(params.get('k6', 0))

    def project(self, points_3d):
        """
        Project 3D points (N, 3) or (H, W, 3) to 2D pixel coordinates.
        """
        x = points_3d[..., 0]
        y = points_3d[..., 1]
        z = points_3d[..., 2]

        d1 = np.sqrt(x**2 + y**2 + z**2)
        d2 = np.sqrt(x**2 + y**2 + (self.xi * d1 + z)**2)
        
        denom = self.alpha * d2 + (1.0 - self.alpha) * (self.xi * d1 + z)
        
        # Avoid division by zero
        mask = np.abs(denom) > 1e-8
        mx = np.zeros_like(x)
        my = np.zeros_like(y)
        
        mx[mask] = x[mask] / denom[mask]
        my[mask] = y[mask] / denom[mask]
        
        r2 = mx**2 + my**2
        r4 = r2**2
        r6 = r4 * r2 # Matches C++ implementation
        
        # Distortion model from C++ code:
        # radial = 1.0 + k1*r2 + k2*r4 + k3*r6 + k4*r2*r4 + k5*r4*r4 + k6*r2*r6
        # Note: r2*r4 = r6, r4*r4 = r8, r2*r6 = r8
        radial = 1.0 + self.k1*r2 + self.k2*r4 + self.k3*r6 + \
                 self.k4*r6 + self.k5*(r4*r4) + self.k6*(r2*r6)
                 
        u = self.fx * mx * radial + self.cx
        v = self.fy * my * radial + self.cy
        
        return np.stack([u, v], axis=-1).astype(np.float32)

def load_calib(yaml_file):
    # Skip OpenCV header and parse YAML
    with open(yaml_file, 'r') as f:
        lines = f.readlines()
    
    # Filter out OpenCV specific tags that PyYAML can't handle
    filtered_lines = []
    skip_block = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('%YAML'):
            continue
        if '!!opencv-matrix' in stripped:
            # We will parse R and T manually if needed, or assume they are simple lists if formatted differently
            # But usually OpenCV YAML puts data in a nested block.
            # Let's try to parse the structure by removing the tag
            line = line.replace('!!opencv-matrix', '')
        filtered_lines.append(line)
        
    try:
        data = yaml.safe_load(''.join(filtered_lines))
    except yaml.YAMLError as e:
        print(f"Error parsing YAML: {e}")
        sys.exit(1)
        
    left = data['left_camera']
    right = data.get('right_camera', None)
    
    # Parse R and T
    R = None
    T = None
    if 'R' in data and data['R'] is not None:
        R_data = data['R']['data']
        R = np.array(R_data).reshape(3, 3)
    if 'T' in data and data['T'] is not None:
        T_data = data['T']['data']
        T = np.array(T_data).reshape(3, 1)
        
    return left, right, R, T

def find_images(img_dir, prefix, ext):
    pattern = os.path.join(img_dir, f"{prefix}*.{ext}")
    files = glob.glob(pattern)
    def extract_num(f):
        base = os.path.basename(f)
        num = ''.join(filter(str.isdigit, base[len(prefix):]))
        return int(num) if num else 0
    files.sort(key=extract_num)
    return files

def init_rectify_map(cam_model, R_rect, P_rect, size):
    """
    Generate rectification map for Double Sphere model.
    
    Args:
        cam_model: DoubleSphereCamera instance
        R_rect: Rotation matrix (3x3) from Camera to Rectified Frame
        P_rect: Projection matrix (3x4) of the Rectified Virtual Camera
        size: (width, height)
    """
    w, h = size
    
    # 1. Create grid of pixels in Rectified Image
    u, v = np.meshgrid(np.arange(w), np.arange(h))
    
    # 2. Unproject from Rectified Pinhole to 3D rays (in Rectified Frame)
    # P_rect = [K_rect | 0] usually
    fx_r = P_rect[0, 0]
    fy_r = P_rect[1, 1]
    cx_r = P_rect[0, 2]
    cy_r = P_rect[1, 2]
    
    x_rect = (u - cx_r) / fx_r
    y_rect = (v - cy_r) / fy_r
    z_rect = np.ones_like(x_rect)
    
    # Stack to (H, W, 3)
    rays_rect = np.stack([x_rect, y_rect, z_rect], axis=-1)
    
    # Normalize rays (optional, but good for rotation)
    norms = np.linalg.norm(rays_rect, axis=-1, keepdims=True)
    rays_rect = rays_rect / norms
    
    # 3. Rotate rays back to Original Camera Frame
    # ray_cam = R_rect.T * ray_rect
    # We use tensordot or matmul. 
    # rays_rect is (H, W, 3). R_rect.T is (3, 3).
    # result[i,j] = R_rect.T @ rays_rect[i,j]
    rays_cam = rays_rect @ R_rect # Equivalent to (R_rect.T @ rays_rect.T).T = rays_rect @ R_rect
    
    # 4. Project rays using Double Sphere Model
    uv_dist = cam_model.project(rays_cam)
    
    map_x = uv_dist[..., 0]
    map_y = uv_dist[..., 1]
    
    return map_x, map_y

def rectify_images(calib_file, input_dir, output_dir, left_prefix, right_prefix=None, ext='bmp', mono=True):
    left_params, right_params, R, T = load_calib(calib_file)
    
    left_cam = DoubleSphereCamera(left_params)
    right_cam = None
    if right_params:
        right_cam = DoubleSphereCamera(right_params)
        
    os.makedirs(output_dir, exist_ok=True)
    
    left_imgs = find_images(input_dir, left_prefix, ext)
    if not left_imgs:
        print(f"No left images found.")
        return

    # Read first image to get size
    img0 = cv2.imread(left_imgs[0])
    if img0 is None:
        print(f"Cannot read {left_imgs[0]}")
        return
    h, w = img0.shape[:2]
    img_size = (w, h)
    
    # Compute Rectification Transforms
    if not mono and right_cam and R is not None and T is not None:
        print("Configuring Stereo Rectification...")
        # Use cv2.stereoRectify to compute rotations R1, R2
        # We pass dummy K and D because we only care about R1, R2 derived from R, T
        K_dummy = np.eye(3)
        K_dummy[0,0] = w
        K_dummy[1,1] = h
        K_dummy[0,2] = w/2
        K_dummy[1,2] = h/2
        D_dummy = np.zeros(5)
        
        R1, R2, P1, P2, Q, roi1, roi2 = cv2.stereoRectify(
            K_dummy, D_dummy, K_dummy, D_dummy, img_size, R, T, 
            flags=cv2.CALIB_ZERO_DISPARITY, alpha=0
        )
        
        # Custom P_rect to ensure good FOV
        # P1 and P2 from stereoRectify might be too zoomed in or out
        # Let's define a custom P_rect with reasonable FOV (e.g. 100 degrees)
        # f = w / (2 * tan(100/2 * pi/180)) ~= w / 2.38
        f_new = w / 2.5 
        P1_new = np.array([
            [f_new, 0, w/2, 0],
            [0, f_new, h/2, 0],
            [0, 0, 1, 0]
        ])
        P2_new = P1_new.copy()
        # P2_new[0, 3] should be P2[0, 3] scaled? 
        # P2[0, 3] = T_x * f. We can keep P2 from stereoRectify but replace intrinsics part
        # But for pure image rectification (not disparity), we just need the maps.
        # If we want valid disparity, P2_new[0,3] must be correct.
        # P2_new[0, 3] = P1_new[0, 0] * (T[0] if T is horizontal baseline)
        # Let's just use P1_new for both for image appearance, 
        # but strictly P2 should have the baseline offset.
        # Since we are just saving images, P1_new is fine for both if we just want to see rectified images.
        # But let's try to respect the baseline for P2.
        baseline = np.linalg.norm(T)
        P2_new[0, 3] = -f_new * baseline # Standard right camera shift
        
        # Generate Maps
        print("Generating Left Map...")
        map1_l, map2_l = init_rectify_map(left_cam, R1, P1_new, img_size)
        print("Generating Right Map...")
        map1_r, map2_r = init_rectify_map(right_cam, R2, P2_new, img_size)
        
    else:
        print("Configuring Monocular Rectification...")
        # Identity rotation
        R1 = np.eye(3)
        # New camera matrix
        f_new = w / 3.0 # Wide FOV
        P1_new = np.array([
            [f_new, 0, w/2, 0],
            [0, f_new, h/2, 0],
            [0, 0, 1, 0]
        ])
        print("Generating Left Map...")
        map1_l, map2_l = init_rectify_map(left_cam, R1, P1_new, img_size)
        map1_r, map2_r = None, None

    # Process Left Images
    print(f"Processing {len(left_imgs)} left images...")
    for i, p in enumerate(left_imgs):
        img = cv2.imread(p)
        if img is None: continue
        rect = cv2.remap(img, map1_l, map2_l, cv2.INTER_LINEAR)
        out_name = os.path.splitext(os.path.basename(p))[0] + '_rectified.' + ext
        cv2.imwrite(os.path.join(output_dir, out_name), rect)
        if (i+1)%10==0: print(f"  {i+1}/{len(left_imgs)}")

    # Process Right Images
    if not mono and right_prefix and right_cam and map1_r is not None:
        right_imgs = find_images(input_dir, right_prefix, ext)
        print(f"Processing {len(right_imgs)} right images...")
        for i, p in enumerate(right_imgs):
            img = cv2.imread(p)
            if img is None: continue
            rect = cv2.remap(img, map1_r, map2_r, cv2.INTER_LINEAR)
            out_name = os.path.splitext(os.path.basename(p))[0] + '_rectified.' + ext
            cv2.imwrite(os.path.join(output_dir, out_name), rect)
            if (i+1)%10==0: print(f"  {i+1}/{len(right_imgs)}")
            
    print("Done.")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--calib', required=True)
    parser.add_argument('-i', '--input', required=True)
    parser.add_argument('-o', '--output', required=True)
    parser.add_argument('-l', '--left', required=True)
    parser.add_argument('-r', '--right', default=None)
    parser.add_argument('-e', '--extension', default='bmp')
    parser.add_argument('--mono', action='store_true')
    args = parser.parse_args()
    rectify_images(args.calib, args.input, args.output, args.left, args.right, args.extension, args.mono)

if __name__ == '__main__':
    main()

