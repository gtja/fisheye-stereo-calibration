#!/usr/bin/env python3
"""
Pre-Calibration Image Quality Check Script

This script automatically checks calibration images for quality issues and removes
problematic images before running calibration. It integrates:
1. Blur detection using Laplacian variance
2. Corner detection quality analysis

Usage:
    python3 pre_calibration_check.py <image_dir> --width W --height H [options]

Arguments:
    image_dir: Directory containing calibration images
    --width: Checkerboard width (number of inner corners)
    --height: Checkerboard height (number of inner corners)
    --blur-threshold: Laplacian variance threshold (default: 100)
    --extension: Image file extension (default: jpg)
    --left-prefix: Left camera image prefix (default: left)
    --right-prefix: Right camera image prefix (default: right)
    --backup: Create backup of removed images (default: True)

The script will:
1. Check all images for blur
2. Remove blurry images
3. Check remaining images for corner detection quality
4. Remove images with poor corner detection
5. Report the final count of valid images for calibration
"""

import cv2
import numpy as np
import os
import sys
import argparse
from pathlib import Path
import shutil

# Import functions from the utility scripts
sys.path.insert(0, str(Path(__file__).parent))
from laplacian_var import calculate_laplacian_variance
from corner_analysis import analyze_image as analyze_corners


def backup_image(image_path, backup_dir):
    """Create a backup of an image before removing it."""
    backup_dir = Path(backup_dir)
    backup_dir.mkdir(parents=True, exist_ok=True)
    backup_path = backup_dir / image_path.name
    shutil.copy2(image_path, backup_path)
    return backup_path


def check_blur(image_dir, threshold, extension, backup_dir=None):
    """
    Check all images for blur and remove blurry ones.
    
    Returns:
        List of images that passed the blur check
    """
    image_dir = Path(image_dir)
    image_files = sorted(image_dir.glob(f'*.{extension}'))
    
    if not image_files:
        print(f"No .{extension} images found in {image_dir}")
        return []
    
    print("=" * 80)
    print("STEP 1: BLUR DETECTION")
    print("=" * 80)
    print(f"Analyzing {len(image_files)} images for blur")
    print(f"Blur threshold: Laplacian variance < {threshold}")
    print("-" * 80)
    
    blurry_images = []
    sharp_images = []
    
    for img_path in image_files:
        variance = calculate_laplacian_variance(img_path)
        
        if variance is None:
            print(f"ERROR: Could not read {img_path.name}")
            continue
        
        status = "SHARP" if variance >= threshold else "BLURRY"
        symbol = "✓" if variance >= threshold else "✗"
        
        print(f"{symbol} {img_path.name:30s} Laplacian var: {variance:8.2f} [{status}]")
        
        if variance < threshold:
            blurry_images.append(img_path)
        else:
            sharp_images.append(img_path)
    
    print("-" * 80)
    print(f"Sharp images:  {len(sharp_images)} / {len(image_files)}")
    print(f"Blurry images: {len(blurry_images)} / {len(image_files)}")
    
    # Remove blurry images
    if blurry_images:
        print(f"\nRemoving {len(blurry_images)} blurry images...")
        for img in blurry_images:
            if backup_dir:
                backup_path = backup_image(img, backup_dir)
                print(f"  Backed up and removed: {img.name} -> {backup_path}")
            else:
                print(f"  Removed: {img.name}")
            img.unlink()
        print("Blur check complete.\n")
    else:
        print("✓ All images pass blur check!\n")
    
    return sharp_images


def check_corners(image_dir, board_width, board_height, extension, left_prefix, right_prefix, backup_dir=None):
    """
    Check images for corner detection quality and remove problematic ones.
    
    Returns:
        Dictionary with counts of valid images per camera
    """
    image_dir = Path(image_dir)
    
    print("=" * 80)
    print("STEP 2: CORNER DETECTION QUALITY ANALYSIS")
    print("=" * 80)
    print(f"Chessboard size: {board_width}x{board_height} (inner corners)")
    print(f"Expected corners per image: {board_width * board_height}")
    
    results = {'left': 0, 'right': 0}
    
    for prefix in [left_prefix, right_prefix]:
        image_files = sorted(image_dir.glob(f"{prefix}*.{extension}"))
        
        if not image_files:
            print(f"\nWarning: No images found with prefix '{prefix}'")
            continue
        
        print(f"\n--- Analyzing {len(image_files)} {prefix} camera images ---")
        print("-" * 80)
        
        failed_images = []
        warning_images = []
        good_images = []
        
        for img_path in image_files:
            result = analyze_corners(img_path, board_width, board_height, False, False, None)
            
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
        print(f"Good images:    {len(good_images)}")
        print(f"Warning images: {len(warning_images)}")
        print(f"Failed images:  {len(failed_images)}")
        
        # Remove failed images (no corners detected or errors)
        images_to_remove = failed_images
        if images_to_remove:
            print(f"\nRemoving {len(images_to_remove)} images with failed corner detection...")
            for img in images_to_remove:
                if backup_dir:
                    backup_path = backup_image(img, backup_dir)
                    print(f"  Backed up and removed: {img.name} -> {backup_path}")
                else:
                    print(f"  Removed: {img.name}")
                img.unlink()
        
        # Keep warning images but notify user
        if warning_images:
            print(f"\n⚠ Keeping {len(warning_images)} images with warnings (suboptimal but usable):")
            for img in warning_images:
                print(f"  - {img.name}")
        
        valid_count = len(good_images) + len(warning_images)
        results[prefix] = valid_count
        print(f"\n✓ {prefix} camera: {valid_count} valid images")
    
    print()
    return results


def main():
    parser = argparse.ArgumentParser(
        description='Pre-calibration image quality check',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument('image_dir', type=str,
                        help='Directory containing calibration images')
    parser.add_argument('--width', '-w', type=int, required=True,
                        help='Checkerboard width (number of inner corners)')
    parser.add_argument('--height', '-ht', type=int, required=True,
                        help='Checkerboard height (number of inner corners)')
    parser.add_argument('--blur-threshold', type=float, default=100.0,
                        help='Laplacian variance threshold for blur detection (default: 100)')
    parser.add_argument('--extension', type=str, default='jpg',
                        help='Image file extension (default: jpg)')
    parser.add_argument('--left-prefix', type=str, default='left',
                        help='Left camera image prefix (default: left)')
    parser.add_argument('--right-prefix', type=str, default='right',
                        help='Right camera image prefix (default: right)')
    parser.add_argument('--no-backup', action='store_true',
                        help='Do not create backup of removed images')
    
    args = parser.parse_args()
    
    image_dir = Path(args.image_dir)
    if not image_dir.exists():
        print(f"Error: Directory {image_dir} does not exist")
        return 1
    
    # Create backup directory
    backup_dir = None
    if not args.no_backup:
        backup_dir = image_dir / ".removed_images_backup"
        backup_dir.mkdir(exist_ok=True)
        print(f"Backup directory: {backup_dir}\n")
    
    # Step 1: Check for blur
    sharp_images = check_blur(image_dir, args.blur_threshold, args.extension, backup_dir)
    
    if not sharp_images:
        print("Error: No images passed blur check")
        return 1
    
    # Step 2: Check corner detection quality
    valid_counts = check_corners(
        image_dir, args.width, args.height, args.extension,
        args.left_prefix, args.right_prefix, backup_dir
    )
    
    # Final summary
    print("=" * 80)
    print("FINAL SUMMARY")
    print("=" * 80)
    
    left_count = valid_counts.get(args.left_prefix, 0)
    right_count = valid_counts.get(args.right_prefix, 0)
    
    print(f"Valid {args.left_prefix} camera images:  {left_count}")
    print(f"Valid {args.right_prefix} camera images: {right_count}")
    
    # Determine minimum count for stereo calibration
    min_count = min(left_count, right_count) if left_count > 0 and right_count > 0 else 0
    
    if min_count == 0:
        print("\n✗ ERROR: No valid image pairs available for calibration")
        return 1
    elif min_count < 20:
        print(f"\n⚠ WARNING: Only {min_count} valid image pairs - recommend at least 30 for best results")
    else:
        print(f"\n✓ SUCCESS: {min_count} valid image pairs ready for calibration")
    
    print(f"\nRecommended calibration command:")
    print(f"  ./calibrate -w {args.width} -h {args.height} -n {min_count} "
          f"-d {args.image_dir}/ -l {args.left_prefix} -r {args.right_prefix} "
          f"-e {args.extension} -o output.yml")
    
    # Write the count to a file for the entrypoint script to read
    count_file = image_dir / ".valid_image_count"
    with open(count_file, 'w') as f:
        f.write(str(min_count))
    
    print(f"\nValid image count written to: {count_file}")
    print("=" * 80)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
