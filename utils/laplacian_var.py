#!/usr/bin/env python3
"""
Laplacian Variance Blur Detection Utility

This script analyzes images for blur using the Laplacian variance method.
Blurry images should be removed from calibration datasets as they lead to poor corner detection.

Usage:
    python laplacian_var.py <image_directory> [--threshold 100] [--delete]
    
Arguments:
    image_directory: Directory containing calibration images
    --threshold: Laplacian variance threshold (default: 100 for 8-bit grayscale)
                 Images below this threshold are considered blurry
    --delete: Actually delete blurry images (default: just report them)
    --extension: Image file extension to check (default: jpg)

Example:
    # Check all JPG images and report blurry ones
    python laplacian_var.py ../imgs/
    
    # Check BMP images with custom threshold
    python laplacian_var.py ../imgs/ --threshold 80 --extension bmp
    
    # Delete blurry images (use with caution!)
    python laplacian_var.py ../imgs/ --delete

Recommended threshold:
    - For 8-bit grayscale: σ² > 100
    - For high-quality images: σ² > 150
    - Lower threshold = more permissive (keeps more images)
"""

import cv2
import numpy as np
import os
import sys
import argparse
from pathlib import Path


def calculate_laplacian_variance(image_path):
    """
    Calculate Laplacian variance for an image.
    
    Args:
        image_path: Path to the image file
        
    Returns:
        Laplacian variance value (higher = sharper image)
    """
    # Read image
    img = cv2.imread(str(image_path))
    if img is None:
        return None
    
    # Convert to grayscale
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    
    # Calculate Laplacian
    laplacian = cv2.Laplacian(gray, cv2.CV_64F)
    
    # Calculate variance
    variance = laplacian.var()
    
    return variance


def analyze_images(image_dir, threshold=100.0, delete_blurry=False, extension='jpg'):
    """
    Analyze all images in a directory for blur.
    
    Args:
        image_dir: Directory containing images
        threshold: Laplacian variance threshold
        delete_blurry: Whether to delete blurry images
        extension: Image file extension to check
    """
    image_dir = Path(image_dir)
    
    if not image_dir.exists():
        print(f"Error: Directory {image_dir} does not exist")
        return
    
    # Find all images with the specified extension
    image_files = sorted(image_dir.glob(f'*.{extension}'))
    
    if not image_files:
        print(f"No .{extension} images found in {image_dir}")
        return
    
    print(f"Analyzing {len(image_files)} images in {image_dir}")
    print(f"Blur threshold: Laplacian variance < {threshold}")
    print("-" * 70)
    
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
    
    print("-" * 70)
    print(f"\nSummary:")
    print(f"  Sharp images:  {len(sharp_images)} / {len(image_files)}")
    print(f"  Blurry images: {len(blurry_images)} / {len(image_files)}")
    
    if blurry_images:
        print(f"\nBlurry images detected:")
        for img in blurry_images:
            print(f"  - {img.name}")
        
        if delete_blurry:
            print(f"\nDeleting {len(blurry_images)} blurry images...")
            for img in blurry_images:
                img.unlink()
                print(f"  Deleted: {img.name}")
            print("Done.")
        else:
            print("\nTo delete these images, run again with --delete flag")
            print("WARNING: Deletion is permanent! Make a backup first.")
    else:
        print("\n✓ All images pass blur check!")
    
    # Calculate and display statistics
    if len(image_files) > 1:
        variances = [calculate_laplacian_variance(img) for img in image_files 
                     if calculate_laplacian_variance(img) is not None]
        if variances:
            print(f"\nLaplacian variance statistics:")
            print(f"  Min:    {min(variances):.2f}")
            print(f"  Max:    {max(variances):.2f}")
            print(f"  Mean:   {np.mean(variances):.2f}")
            print(f"  Median: {np.median(variances):.2f}")
            print(f"  Std:    {np.std(variances):.2f}")


def main():
    parser = argparse.ArgumentParser(
        description='Detect and optionally remove blurry images using Laplacian variance',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument('image_dir', type=str,
                        help='Directory containing calibration images')
    parser.add_argument('--threshold', type=float, default=100.0,
                        help='Laplacian variance threshold (default: 100)')
    parser.add_argument('--delete', action='store_true',
                        help='Delete blurry images (USE WITH CAUTION)')
    parser.add_argument('--extension', type=str, default='jpg',
                        help='Image file extension to check (default: jpg)')
    
    args = parser.parse_args()
    
    if args.delete:
        response = input(f"⚠️  WARNING: This will permanently delete blurry images from {args.image_dir}\n"
                        f"Are you sure? Type 'yes' to confirm: ")
        if response.lower() != 'yes':
            print("Aborted.")
            return
    
    analyze_images(args.image_dir, args.threshold, args.delete, args.extension)


if __name__ == '__main__':
    main()
