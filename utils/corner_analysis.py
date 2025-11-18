#!/usr/bin/env python3
"""
Corner Detection Quality Analysis Utility

This script analyzes chessboard corner detection quality for calibration images.
It checks for:
1. Number of corners detected (should match expected count)
2. Corner distribution (corners should reach edge regions)
3. Visualization of corner locations

Usage:
    python corner_analysis.py <image_directory> --width 9 --height 6 [options]
    
Arguments:
    image_directory: Directory containing calibration images
    --width: Chessboard width (number of inner corners)
    --height: Chessboard height (number of inner corners)
    --prefix: Image filename prefix (default: checks all images)
    --extension: Image file extension (default: jpg)
    --show: Display images with corners marked (requires display)
    --save: Save corner visualization images to output directory

Example:
    # Analyze all images in directory
    python corner_analysis.py ../imgs/ --width 9 --height 6
    
    # Analyze only left camera images and save visualizations
    python corner_analysis.py ../imgs/ --width 9 --height 6 --prefix left --save
    
    # Check BMP images with visualization
    python corner_analysis.py ../imgs/ --width 9 --height 6 --extension bmp --show

Quality criteria:
    - All expected corners should be detected (100% detection rate)
    - Corners should cover edge regions (within 20% border from each edge)
    - Corner distribution should be uniform across the image
"""

import cv2
import numpy as np
import os
import sys
import argparse
from pathlib import Path


def check_corner_distribution(corners, image_shape, border_percent=0.2):
    """
    Check if corners reach the edge regions of the image.
    
    Args:
        corners: Detected corner points
        image_shape: (height, width) of the image
        border_percent: Percentage of image dimension to define edge region
        
    Returns:
        Dictionary with edge coverage information
    """
    height, width = image_shape[:2]
    
    border_x = int(width * border_percent)
    border_y = int(height * border_percent)
    
    has_left = False
    has_right = False
    has_top = False
    has_bottom = False
    
    for corner in corners:
        x, y = corner.ravel()
        if x < border_x:
            has_left = True
        if x > width - border_x:
            has_right = True
        if y < border_y:
            has_top = True
        if y > height - border_y:
            has_bottom = True
    
    edge_count = sum([has_left, has_right, has_top, has_bottom])
    
    return {
        'left': has_left,
        'right': has_right,
        'top': has_top,
        'bottom': has_bottom,
        'edge_count': edge_count,
        'border_percent': border_percent
    }


def analyze_image(image_path, board_width, board_height, show=False, save_vis=False, output_dir=None):
    """
    Analyze corner detection quality for a single image.
    
    Args:
        image_path: Path to the image file
        board_width: Chessboard width (inner corners)
        board_height: Chessboard height (inner corners)
        show: Whether to display the image
        save_vis: Whether to save visualization
        output_dir: Directory to save visualizations
        
    Returns:
        Dictionary with analysis results
    """
    img = cv2.imread(str(image_path))
    if img is None:
        return {'error': 'Could not read image'}
    
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    board_size = (board_width, board_height)
    expected_corners = board_width * board_height
    
    # Find chessboard corners
    found, corners = cv2.findChessboardCorners(
        img, board_size,
        cv2.CALIB_CB_ADAPTIVE_THRESH | cv2.CALIB_CB_FILTER_QUADS
    )
    
    result = {
        'found': found,
        'corners_detected': len(corners) if found else 0,
        'corners_expected': expected_corners,
        'detection_rate': (len(corners) / expected_corners * 100) if found else 0
    }
    
    if found:
        # Refine corner positions
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.01)
        cv2.cornerSubPix(gray, corners, (5, 5), (-1, -1), criteria)
        
        # Check distribution
        dist_info = check_corner_distribution(corners, gray.shape)
        result['distribution'] = dist_info
        
        # Create visualization
        if show or save_vis:
            vis_img = img.copy()
            cv2.drawChessboardCorners(vis_img, board_size, corners, found)
            
            # Add text overlay
            text = f"Corners: {len(corners)}/{expected_corners}"
            cv2.putText(vis_img, text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 
                       1, (0, 255, 0), 2)
            
            edge_text = f"Edges: {dist_info['edge_count']}/4"
            cv2.putText(vis_img, edge_text, (10, 70), cv2.FONT_HERSHEY_SIMPLEX, 
                       1, (0, 255, 0) if dist_info['edge_count'] >= 3 else (0, 0, 255), 2)
            
            if show:
                cv2.imshow(f'Corner Analysis - {image_path.name}', vis_img)
                cv2.waitKey(500)  # Show for 500ms
            
            if save_vis and output_dir:
                output_path = output_dir / f"{image_path.stem}_corners.jpg"
                cv2.imwrite(str(output_path), vis_img)
                result['visualization'] = str(output_path)
    
    return result


def analyze_images(image_dir, board_width, board_height, prefix=None, extension='jpg', 
                   show=False, save_vis=False):
    """
    Analyze all images in a directory.
    
    Args:
        image_dir: Directory containing images
        board_width: Chessboard width (inner corners)
        board_height: Chessboard height (inner corners)
        prefix: Optional filename prefix to filter images
        extension: Image file extension
        show: Whether to display images
        save_vis: Whether to save visualizations
    """
    image_dir = Path(image_dir)
    
    if not image_dir.exists():
        print(f"Error: Directory {image_dir} does not exist")
        return
    
    # Find matching images
    if prefix:
        pattern = f"{prefix}*.{extension}"
    else:
        pattern = f"*.{extension}"
    
    image_files = sorted(image_dir.glob(pattern))
    
    if not image_files:
        print(f"No images found matching pattern: {pattern}")
        return
    
    # Create output directory if saving visualizations
    output_dir = None
    if save_vis:
        output_dir = image_dir / "corner_analysis"
        output_dir.mkdir(exist_ok=True)
        print(f"Saving visualizations to: {output_dir}")
    
    print(f"Analyzing {len(image_files)} images")
    print(f"Chessboard size: {board_width}x{board_height} (inner corners)")
    print(f"Expected corners per image: {board_width * board_height}")
    print("-" * 80)
    
    results = []
    good_images = []
    warning_images = []
    failed_images = []
    
    for img_path in image_files:
        result = analyze_image(img_path, board_width, board_height, show, save_vis, output_dir)
        results.append((img_path, result))
        
        if 'error' in result:
            print(f"✗ {img_path.name:30s} ERROR: {result['error']}")
            failed_images.append(img_path)
        elif not result['found']:
            print(f"✗ {img_path.name:30s} NO CORNERS DETECTED")
            failed_images.append(img_path)
        else:
            dist = result['distribution']
            edge_status = f"{dist['edge_count']}/4 edges"
            
            if result['detection_rate'] < 100:
                status = "INCOMPLETE"
                symbol = "⚠"
                warning_images.append(img_path)
            elif dist['edge_count'] < 3:
                status = "POOR DIST"
                symbol = "⚠"
                warning_images.append(img_path)
            else:
                status = "GOOD"
                symbol = "✓"
                good_images.append(img_path)
            
            print(f"{symbol} {img_path.name:30s} {result['corners_detected']:3d}/{result['corners_expected']:3d} corners "
                  f"({edge_status}) [{status}]")
    
    print("-" * 80)
    print(f"\nSummary:")
    print(f"  Good images:    {len(good_images)} / {len(image_files)}")
    print(f"  Warning images: {len(warning_images)} / {len(image_files)}")
    print(f"  Failed images:  {len(failed_images)} / {len(image_files)}")
    
    if warning_images:
        print(f"\n⚠ Images with warnings (poor distribution or incomplete corners):")
        for img in warning_images:
            print(f"  - {img.name}")
        print("\nConsider:")
        print("  - Recapturing images with better coverage of image edges")
        print("  - Ensuring the checkerboard is visible at all 4 corners when possible")
    
    if failed_images:
        print(f"\n✗ Images with no corners detected:")
        for img in failed_images:
            print(f"  - {img.name}")
        print("\nConsider:")
        print("  - Checking if images are properly focused")
        print("  - Verifying the checkerboard pattern is fully visible")
        print("  - Ensuring adequate lighting")
    
    if save_vis:
        print(f"\nCorner visualizations saved to: {output_dir}")
    
    if show:
        cv2.destroyAllWindows()
    
    # Recommendations
    if len(good_images) < len(image_files):
        print("\n" + "=" * 80)
        print("RECOMMENDATIONS:")
        print("=" * 80)
        if len(failed_images) > 0:
            print(f"1. Remove or replace {len(failed_images)} failed image(s)")
        if len(warning_images) > 0:
            print(f"2. Review {len(warning_images)} warning image(s) - consider recapturing")
        if len(good_images) < 20:
            print(f"3. You have {len(good_images)} good images - aim for at least 30 for best results")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze chessboard corner detection quality',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument('image_dir', type=str,
                        help='Directory containing calibration images')
    parser.add_argument('--width', '-w', type=int, required=True,
                        help='Checkerboard width (number of inner corners)')
    parser.add_argument('--height', '-ht', type=int, required=True,
                        help='Checkerboard height (number of inner corners)')
    parser.add_argument('--prefix', type=str, default=None,
                        help='Image filename prefix to filter (e.g., "left")')
    parser.add_argument('--extension', type=str, default='jpg',
                        help='Image file extension (default: jpg)')
    parser.add_argument('--show', action='store_true',
                        help='Display images with corners marked')
    parser.add_argument('--save', action='store_true',
                        help='Save corner visualization images')
    
    args = parser.parse_args()
    
    analyze_images(args.image_dir, args.width, args.height, args.prefix, 
                   args.extension, args.show, args.save)


if __name__ == '__main__':
    main()
