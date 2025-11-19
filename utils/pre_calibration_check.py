#!/usr/bin/env python3
"""
Pre-Calibration Image Quality Check Script

This script automatically checks calibration images for quality issues and creates
a new directory with filtered, sequentially-named images for calibration. It integrates:
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
    --output-dir: Output directory for filtered images (default: <image_dir>_filtered)

The script will:
1. Check all images for blur
2. Check images for corner detection quality
3. Create a new directory with only good images
4. Rename images sequentially (left1.ext, left2.ext, ..., leftN.ext)
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


def check_blur(image_dir, threshold, extension):
    """
    Check all images for blur.
    
    Returns:
        Dictionary mapping image paths to their blur status (True = sharp, False = blurry)
    """
    image_dir = Path(image_dir)
    image_files = sorted(image_dir.glob(f'*.{extension}'))
    
    if not image_files:
        print(f"No .{extension} images found in {image_dir}")
        return {}
    
    print("=" * 80)
    print("STEP 1: BLUR DETECTION")
    print("=" * 80)
    print(f"Analyzing {len(image_files)} images for blur")
    print(f"Blur threshold: Laplacian variance < {threshold}")
    print("-" * 80)
    
    blur_status = {}
    blurry_count = 0
    sharp_count = 0
    
    for img_path in image_files:
        variance = calculate_laplacian_variance(img_path)
        
        if variance is None:
            print(f"ERROR: Could not read {img_path.name}")
            blur_status[img_path] = False
            continue
        
        is_sharp = variance >= threshold
        blur_status[img_path] = is_sharp
        
        status = "SHARP" if is_sharp else "BLURRY"
        symbol = "✓" if is_sharp else "✗"
        
        print(f"{symbol} {img_path.name:30s} Laplacian var: {variance:8.2f} [{status}]")
        
        if is_sharp:
            sharp_count += 1
        else:
            blurry_count += 1
    
    print("-" * 80)
    print(f"Sharp images:  {sharp_count} / {len(image_files)}")
    print(f"Blurry images: {blurry_count} / {len(image_files)}")
    print()
    
    return blur_status


def check_corners(image_dir, board_width, board_height, extension, left_prefix, right_prefix, blur_status):
    """
    Check images for corner detection quality.
    
    Returns:
        Dictionary mapping image paths to corner quality status ('good', 'warning', 'failed')
    """
    image_dir = Path(image_dir)
    
    print("=" * 80)
    print("STEP 2: CORNER DETECTION QUALITY ANALYSIS")
    print("=" * 80)
    print(f"Chessboard size: {board_width}x{board_height} (inner corners)")
    print(f"Expected corners per image: {board_width * board_height}")
    
    corner_status = {}
    
    for prefix in [left_prefix, right_prefix]:
        image_files = sorted(image_dir.glob(f"{prefix}*.{extension}"))
        
        if not image_files:
            print(f"\nWarning: No images found with prefix '{prefix}'")
            continue
        
        print(f"\n--- Analyzing {len(image_files)} {prefix} camera images ---")
        print("-" * 80)
        
        failed_count = 0
        warning_count = 0
        good_count = 0
        
        for img_path in image_files:
            # Skip blurry images
            if not blur_status.get(img_path, False):
                corner_status[img_path] = 'failed'
                failed_count += 1
                print(f"✗ {img_path.name:30s} SKIPPED (BLURRY)")
                continue
            
            result = analyze_corners(img_path, board_width, board_height, False, False, None)
            
            if 'error' in result:
                print(f"✗ {img_path.name:30s} ERROR: {result['error']}")
                corner_status[img_path] = 'failed'
                failed_count += 1
            elif not result['found']:
                print(f"✗ {img_path.name:30s} NO CORNERS DETECTED")
                corner_status[img_path] = 'failed'
                failed_count += 1
            else:
                dist = result['distribution']
                edge_status = f"{dist['edge_count']}/4 edges"
                
                if result['detection_rate'] < 100:
                    status = "INCOMPLETE"
                    symbol = "⚠"
                    corner_status[img_path] = 'warning'
                    warning_count += 1
                elif dist['edge_count'] < 3:
                    status = "POOR DIST"
                    symbol = "⚠"
                    corner_status[img_path] = 'warning'
                    warning_count += 1
                else:
                    status = "GOOD"
                    symbol = "✓"
                    corner_status[img_path] = 'good'
                    good_count += 1
                
                print(f"{symbol} {img_path.name:30s} {result['corners_detected']:3d}/{result['corners_expected']:3d} corners "
                      f"({edge_status}) [{status}]")
        
        print("-" * 80)
        print(f"Good images:    {good_count}")
        print(f"Warning images: {warning_count}")
        print(f"Failed images:  {failed_count}")
    
    print()
    return corner_status


def extract_image_number(filename, prefix):
    """Extract the numeric index from an image filename."""
    # Remove prefix and extension
    name_without_ext = filename.stem
    if name_without_ext.startswith(prefix):
        number_str = name_without_ext[len(prefix):]
        try:
            return int(number_str)
        except ValueError:
            return None
    return None


def create_filtered_directory(image_dir, corner_status, extension, left_prefix, right_prefix, output_dir):
    """
    Create a new directory with filtered images, renamed sequentially.
    
    Returns:
        Number of valid image pairs copied
    """
    image_dir = Path(image_dir)
    output_dir = Path(output_dir)
    
    print("=" * 80)
    print("STEP 3: CREATE FILTERED IMAGE DIRECTORY")
    print("=" * 80)
    print(f"Output directory: {output_dir}")
    
    # Create output directory
    if output_dir.exists():
        print(f"Removing existing filtered directory...")
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Collect good and warning images (both are usable)
    left_images = {}
    right_images = {}
    
    for img_path, status in corner_status.items():
        if status in ['good', 'warning']:
            if img_path.name.startswith(left_prefix):
                img_num = extract_image_number(img_path, left_prefix)
                if img_num is not None:
                    left_images[img_num] = img_path
            elif img_path.name.startswith(right_prefix):
                img_num = extract_image_number(img_path, right_prefix)
                if img_num is not None:
                    right_images[img_num] = img_path
    
    # Find matching pairs (images with same index in both left and right)
    left_indices = set(left_images.keys())
    right_indices = set(right_images.keys())
    common_indices = sorted(left_indices & right_indices)
    
    if not common_indices:
        print("✗ ERROR: No matching image pairs found")
        return 0
    
    print(f"\nFound {len(common_indices)} valid image pairs")
    print(f"Copying and renaming images...")
    print("-" * 80)
    
    # Copy images with sequential naming
    for new_idx, orig_idx in enumerate(common_indices, start=1):
        left_src = left_images[orig_idx]
        right_src = right_images[orig_idx]
        
        left_dst = output_dir / f"{left_prefix}{new_idx}.{extension}"
        right_dst = output_dir / f"{right_prefix}{new_idx}.{extension}"
        
        shutil.copy2(left_src, left_dst)
        shutil.copy2(right_src, right_dst)
        
        print(f"  {left_src.name:30s} -> {left_dst.name:30s}")
        print(f"  {right_src.name:30s} -> {right_dst.name:30s}")
    
    print("-" * 80)
    print(f"✓ Copied {len(common_indices)} image pairs to {output_dir}")
    print()
    
    return len(common_indices)


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
    parser.add_argument('--output-dir', type=str, default=None,
                        help='Output directory for filtered images (default: <image_dir>_filtered)')
    
    args = parser.parse_args()
    
    image_dir = Path(args.image_dir)
    if not image_dir.exists():
        print(f"Error: Directory {image_dir} does not exist")
        return 1
    
    # Determine output directory
    if args.output_dir:
        output_dir = Path(args.output_dir)
    else:
        # Default: create a _filtered subdirectory inside the image directory
        # This ensures it's accessible when the image directory is mounted in Docker
        output_dir = image_dir / 'imgs_filtered'
    
    print(f"Input directory:  {image_dir}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Step 1: Check for blur
    blur_status = check_blur(image_dir, args.blur_threshold, args.extension)
    
    if not any(blur_status.values()):
        print("Error: No images passed blur check")
        return 1
    
    # Step 2: Check corner detection quality
    corner_status = check_corners(
        image_dir, args.width, args.height, args.extension,
        args.left_prefix, args.right_prefix, blur_status
    )
    
    # Step 3: Create filtered directory with renamed images
    num_pairs = create_filtered_directory(
        image_dir, corner_status, args.extension,
        args.left_prefix, args.right_prefix, output_dir
    )
    
    # Final summary
    print("=" * 80)
    print("FINAL SUMMARY")
    print("=" * 80)
    
    if num_pairs == 0:
        print("\n✗ ERROR: No valid image pairs available for calibration")
        return 1
    elif num_pairs < 20:
        print(f"\n⚠ WARNING: Only {num_pairs} valid image pairs - recommend at least 30 for best results")
    else:
        print(f"\n✓ SUCCESS: {num_pairs} valid image pairs ready for calibration")
    
    print(f"\nFiltered images saved to: {output_dir}")
    print(f"Number of valid pairs: {num_pairs}")
    
    print(f"\nRecommended calibration command:")
    print(f"  ./calibrate -w {args.width} -h {args.height} -n {num_pairs} "
          f"-d {output_dir}/ -l {args.left_prefix} -r {args.right_prefix} "
          f"-e {args.extension} -o output.yml")
    
    # Write the filtered directory path and count for the entrypoint script to read
    info_file = image_dir / ".filtered_info"
    with open(info_file, 'w') as f:
        f.write(f"{output_dir}\n")
        f.write(f"{num_pairs}\n")
    
    print(f"\nFiltered directory info written to: {info_file}")
    print("=" * 80)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
