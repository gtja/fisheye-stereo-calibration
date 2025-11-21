#!/usr/bin/env python3
"""
Script to rectify/undistort images using calibration results.
Supports both fisheye and double-sphere camera models.

Usage:
    # Rectify stereo images
    python3 rectify_images.py -c cam_stereo.yml -i imgs/ -o output_rectified/ -l left -r right
    
    # Rectify single camera images
    python3 rectify_images.py -c cam_stereo.yml -i imgs/ -o output_rectified/ -l left --mono
    
    # Specify image extension
    python3 rectify_images.py -c cam_stereo.yml -i imgs/ -o output_rectified/ -l left -r right -e jpg
"""

import sys
import os
import argparse
import yaml
import cv2
import numpy as np
from pathlib import Path
import glob


def load_calibration(yaml_file):
    """
    Load calibration parameters from YAML file.
    
    Args:
        yaml_file: Path to calibration YAML file
        
    Returns:
        Dictionary containing calibration parameters
    """
    if not os.path.exists(yaml_file):
        print(f"Error: Calibration file not found: {yaml_file}")
        return None
    
    try:
        with open(yaml_file, 'r') as f:
            data = yaml.safe_load(f)
        
        model_type = data.get("model_type", "fisheye")
        print(f"Loaded calibration file: {yaml_file}")
        print(f"Model type: {model_type}")
        
        return data
    except Exception as e:
        print(f"Error loading calibration file: {e}")
        return None


def parse_opencv_matrix(mat_data):
    """
    Parse OpenCV matrix from YAML data (supports both dict and list formats).
    
    Args:
        mat_data: Matrix data (can be dict with 'data', 'rows', 'cols' or numpy array)
        
    Returns:
        numpy array
    """
    if isinstance(mat_data, dict):
        # OpenCV YAML format with 'data', 'rows', 'cols'
        data = np.array(mat_data['data'])
        rows = mat_data['rows']
        cols = mat_data['cols']
        return data.reshape(rows, cols)
    else:
        # Already a numpy array or list
        return np.array(mat_data)


def get_fisheye_params(calib_data, camera='left'):
    """
    Extract fisheye camera parameters from calibration data.
    
    Args:
        calib_data: Calibration data dictionary
        camera: 'left' or 'right'
        
    Returns:
        Tuple of (K, D, R, P) matrices
    """
    if camera == 'left':
        K = parse_opencv_matrix(calib_data['K1'])
        D = parse_opencv_matrix(calib_data['D1']).reshape(-1, 1)
        R = parse_opencv_matrix(calib_data['R1']) if 'R1' in calib_data else np.eye(3)
        P = parse_opencv_matrix(calib_data['P1']) if 'P1' in calib_data else K
    else:
        K = parse_opencv_matrix(calib_data['K2'])
        D = parse_opencv_matrix(calib_data['D2']).reshape(-1, 1)
        R = parse_opencv_matrix(calib_data['R2']) if 'R2' in calib_data else np.eye(3)
        P = parse_opencv_matrix(calib_data['P2']) if 'P2' in calib_data else K
    
    # Ensure correct data types
    K = K.astype(np.float64)
    D = D.astype(np.float64)
    R = R.astype(np.float64)
    P = P.astype(np.float64)
    
    return K, D, R, P


def get_omnidir_params(calib_data, camera='left'):
    """
    Extract omnidirectional camera parameters from calibration data.
    
    Args:
        calib_data: Calibration data dictionary
        camera: 'left' or 'right'
        
    Returns:
        Tuple of (K, D, xi, R, P) matrices/values
    """
    if camera == 'left':
        K = parse_opencv_matrix(calib_data['K1'])
        D = parse_opencv_matrix(calib_data['D1']).reshape(1, -1)
        xi = calib_data.get('xi1', 0.0)
        R = parse_opencv_matrix(calib_data['R1']) if 'R1' in calib_data else np.eye(3)
        P = parse_opencv_matrix(calib_data['P1']) if 'P1' in calib_data else K
    else:
        K = parse_opencv_matrix(calib_data['K2'])
        D = parse_opencv_matrix(calib_data['D2']).reshape(1, -1)
        xi = calib_data.get('xi2', 0.0)
        R = parse_opencv_matrix(calib_data['R2']) if 'R2' in calib_data else np.eye(3)
        P = parse_opencv_matrix(calib_data['P2']) if 'P2' in calib_data else K
    
    # Ensure correct data types
    K = K.astype(np.float64)
    D = D.astype(np.float64)
    R = R.astype(np.float64)
    P = P.astype(np.float64)
    
    return K, D, xi, R, P


def get_double_sphere_params(calib_data, camera='left'):
    """
    Extract double-sphere camera parameters from calibration data.
    
    Args:
        calib_data: Calibration data dictionary
        camera: 'left' or 'right'
        
    Returns:
        Dictionary containing camera parameters
    """
    cam_key = 'left_camera' if camera == 'left' else 'right_camera'
    
    if cam_key not in calib_data:
        print(f"Error: {cam_key} parameters not found in calibration file")
        return None
    
    cam_params = calib_data[cam_key]
    
    # For double-sphere model, we approximate using fisheye model
    # since OpenCV doesn't have built-in double-sphere undistortion
    K = np.array([
        [cam_params['fx'], 0, cam_params['cx']],
        [0, cam_params['fy'], cam_params['cy']],
        [0, 0, 1]
    ], dtype=np.float64)
    
    # Use the distortion coefficients (k1-k4 for fisheye approximation)
    D = np.array([
        cam_params.get('k1', 0.0),
        cam_params.get('k2', 0.0),
        cam_params.get('k3', 0.0),
        cam_params.get('k4', 0.0)
    ]).reshape(-1, 1)
    
    # Note: xi and alpha parameters are not directly used in OpenCV fisheye undistortion
    # For accurate double-sphere undistortion, a custom implementation would be needed
    
    return {'K': K, 'D': D, 'xi': cam_params.get('xi', 0.0), 'alpha': cam_params.get('alpha', 0.5)}


def rectify_fisheye_image(img, K, D, R, P, img_size=None):
    """
    Rectify a fisheye image.
    
    Args:
        img: Input image
        K: Camera matrix
        D: Distortion coefficients
        R: Rectification rotation matrix
        P: Projection matrix
        img_size: Output image size (if None, uses input image size)
        
    Returns:
        Rectified image
    """
    if img_size is None:
        img_size = (img.shape[1], img.shape[0])
    
    # Compute rectification maps
    map1, map2 = cv2.fisheye.initUndistortRectifyMap(
        K, D, R, P, img_size, cv2.CV_16SC2
    )
    
    # Apply rectification
    rectified = cv2.remap(img, map1, map2, cv2.INTER_LINEAR)
    
    return rectified


def rectify_omnidir_image(img, K, D, xi, R, P, img_size=None):
    """
    Rectify an omnidirectional image.
    
    Args:
        img: Input image
        K: Camera matrix
        D: Distortion coefficients
        xi: Mirror parameter
        R: Rectification rotation matrix
        P: Projection matrix
        img_size: Output image size (if None, uses input image size)
        
    Returns:
        Rectified image
    """
    if img_size is None:
        img_size = (img.shape[1], img.shape[0])
    
    # Compute rectification maps
    # xi must be a 1x1 array for OpenCV's omnidir functions
    xi_mat = np.array([[xi]], dtype=np.float64)
    # Use RECTIFY_LONGLATI projection for omnidirectional rectification
    map1, map2 = cv2.omnidir.initUndistortRectifyMap(
        K, D, xi_mat, R, P, img_size, cv2.CV_16SC2, cv2.omnidir.RECTIFY_LONGLATI
    )
    
    # Apply rectification
    rectified = cv2.remap(img, map1, map2, cv2.INTER_LINEAR)
    
    return rectified


def find_images(img_dir, prefix, extension):
    """
    Find all images with given prefix and extension in directory.
    
    Args:
        img_dir: Directory containing images
        prefix: Image filename prefix
        extension: Image file extension (without dot)
        
    Returns:
        List of image paths sorted by numeric index
    """
    pattern = os.path.join(img_dir, f"{prefix}*.{extension}")
    files = glob.glob(pattern)
    
    # Sort by numeric index if possible
    def extract_number(filename):
        base = os.path.basename(filename)
        # Remove prefix and extension
        num_str = base[len(prefix):-len(extension)-1]
        try:
            return int(num_str)
        except ValueError:
            return 0
    
    files.sort(key=extract_number)
    return files


def rectify_images(calib_file, input_dir, output_dir, left_prefix, right_prefix=None, 
                   extension='jpg', mono=False):
    """
    Rectify images using calibration parameters.
    
    Args:
        calib_file: Path to calibration YAML file
        input_dir: Directory containing input images
        output_dir: Directory to save rectified images
        left_prefix: Prefix for left camera images
        right_prefix: Prefix for right camera images (None for mono)
        extension: Image file extension
        mono: If True, only rectify left camera images
    """
    # Load calibration
    calib_data = load_calibration(calib_file)
    if calib_data is None:
        return False
    
    model_type = calib_data.get("model_type", "fisheye")
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Find input images
    left_images = find_images(input_dir, left_prefix, extension)
    if not left_images:
        print(f"Error: No images found with prefix '{left_prefix}' in {input_dir}")
        return False
    
    print(f"\nFound {len(left_images)} left images")
    
    right_images = []
    if not mono and right_prefix:
        right_images = find_images(input_dir, right_prefix, extension)
        print(f"Found {len(right_images)} right images")
        
        if len(left_images) != len(right_images):
            print(f"Warning: Number of left and right images don't match")
    
    # Load first image to get size
    first_img = cv2.imread(left_images[0])
    if first_img is None:
        print(f"Error: Cannot read image {left_images[0]}")
        return False
    
    img_size = (first_img.shape[1], first_img.shape[0])
    print(f"Image size: {img_size[0]}x{img_size[1]}")
    
    # Prepare rectification parameters based on model type
    if model_type == "fisheye":
        print("\nUsing fisheye model for rectification")
        K_left, D_left, R_left, P_left = get_fisheye_params(calib_data, 'left')
        
        if not mono and right_prefix:
            K_right, D_right, R_right, P_right = get_fisheye_params(calib_data, 'right')
    
    elif model_type == "omnidir":
        print("\nUsing omnidirectional model for rectification")
        K_left, D_left, xi_left, R_left, P_left = get_omnidir_params(calib_data, 'left')
        
        if not mono and right_prefix:
            K_right, D_right, xi_right, R_right, P_right = get_omnidir_params(calib_data, 'right')
    
    elif model_type == "double_sphere":
        print("\nUsing double-sphere model (approximated with fisheye)")
        print("Note: This uses fisheye approximation (k1-k4 only, xi/alpha not used).")
        print("      For high-precision undistortion with double-sphere model, a custom")
        print("      C++ implementation using all 6 distortion coefficients would be needed.")
        
        params_left = get_double_sphere_params(calib_data, 'left')
        if params_left is None:
            return False
        K_left, D_left = params_left['K'], params_left['D']
        R_left = np.eye(3)  # No rectification rotation for monocular
        P_left = K_left
        
        if not mono and right_prefix:
            params_right = get_double_sphere_params(calib_data, 'right')
            if params_right is None:
                return False
            K_right, D_right = params_right['K'], params_right['D']
            R_right = np.eye(3)
            P_right = K_right
    
    else:
        print(f"Error: Unknown model type: {model_type}")
        return False
    
    # Rectify left images
    print(f"\nRectifying left images...")
    for i, img_path in enumerate(left_images):
        img = cv2.imread(img_path)
        if img is None:
            print(f"Warning: Cannot read image {img_path}, skipping")
            continue
        
        # Rectify based on model type
        if model_type == "omnidir":
            rectified = rectify_omnidir_image(img, K_left, D_left, xi_left, R_left, P_left, img_size)
        else:  # fisheye or double_sphere (approximated)
            rectified = rectify_fisheye_image(img, K_left, D_left, R_left, P_left, img_size)
        
        # Save rectified image (preserve original basename with _rectified suffix)
        original_basename = os.path.basename(img_path)
        name_without_ext = os.path.splitext(original_basename)[0]
        output_filename = f"{name_without_ext}_rectified.{extension}"
        output_path = os.path.join(output_dir, output_filename)
        cv2.imwrite(output_path, rectified)
        
        if (i + 1) % 5 == 0 or i == len(left_images) - 1:
            print(f"  Processed {i+1}/{len(left_images)} images")
    
    print(f"Left images saved to: {output_dir}")
    
    # Rectify right images
    if not mono and right_prefix and right_images:
        print(f"\nRectifying right images...")
        for i, img_path in enumerate(right_images):
            img = cv2.imread(img_path)
            if img is None:
                print(f"Warning: Cannot read image {img_path}, skipping")
                continue
            
            # Rectify based on model type
            if model_type == "omnidir":
                rectified = rectify_omnidir_image(img, K_right, D_right, xi_right, R_right, P_right, img_size)
            else:  # fisheye or double_sphere (approximated)
                rectified = rectify_fisheye_image(img, K_right, D_right, R_right, P_right, img_size)
            
            # Save rectified image (preserve original basename with _rectified suffix)
            original_basename = os.path.basename(img_path)
            name_without_ext = os.path.splitext(original_basename)[0]
            output_filename = f"{name_without_ext}_rectified.{extension}"
            output_path = os.path.join(output_dir, output_filename)
            cv2.imwrite(output_path, rectified)
            
            if (i + 1) % 5 == 0 or i == len(right_images) - 1:
                print(f"  Processed {i+1}/{len(right_images)} images")
        
        print(f"Right images saved to: {output_dir}")
    
    print(f"\n✓ Rectification complete!")
    print(f"  Total images rectified: {len(left_images) + len(right_images)}")
    print(f"  Output directory: {output_dir}")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Rectify/undistort images using calibration results',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Rectify stereo images
  python3 rectify_images.py -c output/cam_stereo.yml -i imgs/ -o imgs_rectified/ -l left -r right
  
  # Rectify only left camera images
  python3 rectify_images.py -c output/cam_stereo.yml -i imgs/ -o imgs_rectified/ -l left --mono
  
  # Specify image extension
  python3 rectify_images.py -c output/cam_stereo.yml -i imgs/ -o imgs_rectified/ -l left -r right -e bmp
        """
    )
    
    parser.add_argument('-c', '--calib', required=True,
                        help='Path to calibration YAML file (e.g., cam_stereo.yml)')
    parser.add_argument('-i', '--input', required=True,
                        help='Input directory containing images to rectify')
    parser.add_argument('-o', '--output', required=True,
                        help='Output directory for rectified images')
    parser.add_argument('-l', '--left', required=True,
                        help='Prefix for left camera images (e.g., "left")')
    parser.add_argument('-r', '--right', default=None,
                        help='Prefix for right camera images (e.g., "right")')
    parser.add_argument('-e', '--extension', default='jpg',
                        help='Image file extension (default: jpg)')
    parser.add_argument('--mono', action='store_true',
                        help='Only rectify left camera images (monocular mode)')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not os.path.exists(args.calib):
        print(f"Error: Calibration file not found: {args.calib}")
        sys.exit(1)
    
    if not os.path.isdir(args.input):
        print(f"Error: Input directory not found: {args.input}")
        sys.exit(1)
    
    if not args.mono and not args.right:
        print("Error: Either specify --right prefix for stereo or use --mono flag")
        sys.exit(1)
    
    # Run rectification
    success = rectify_images(
        args.calib,
        args.input,
        args.output,
        args.left,
        args.right,
        args.extension,
        args.mono
    )
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
