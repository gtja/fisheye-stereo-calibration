#!/usr/bin/env python3
"""
Script to read and display calibration results from the generated YAML file.
Displays both the camera parameters and the detailed accuracy evaluation metrics.
"""

import sys
import os
from pathlib import Path
import yaml


def read_calibration_results(yaml_file):
    """
    Read and display calibration results from YAML file.
    
    Args:
        yaml_file: Path to the calibration YAML file (e.g., cam_stereo.yml)
    """
    if not os.path.exists(yaml_file):
        print(f"Error: File not found: {yaml_file}")
        return False
    
    try:
        with open(yaml_file, 'r') as f:
            data = yaml.safe_load(f)
        
        print("\n" + "="*70)
        print("FISHEYE STEREO CALIBRATION RESULTS")
        print("="*70)
        
        # Read model type
        model_type = data.get("model_type", "Unknown")
        print(f"\nModel Type: {model_type}")
        
        # Read left camera parameters
        print("\n" + "-"*70)
        print("LEFT CAMERA PARAMETERS")
        print("-"*70)
        left_camera = data.get("left_camera", {})
        if left_camera:
            print(f"  fx: {left_camera.get('fx', 0):.4f}")
            print(f"  fy: {left_camera.get('fy', 0):.4f}")
            print(f"  cx: {left_camera.get('cx', 0):.4f}")
            print(f"  cy: {left_camera.get('cy', 0):.4f}")
            print(f"  xi: {left_camera.get('xi', 0):.6f}")
            print(f"  alpha: {left_camera.get('alpha', 0):.6f}")
            print(f"  k1: {left_camera.get('k1', 0):.6f}")
            print(f"  k2: {left_camera.get('k2', 0):.6f}")
            print(f"  k3: {left_camera.get('k3', 0):.6f}")
            print(f"  k4: {left_camera.get('k4', 0):.6f}")
            print(f"  k5: {left_camera.get('k5', 0):.6f}")
            print(f"  k6: {left_camera.get('k6', 0):.6f}")
        
        # Read right camera parameters
        print("\n" + "-"*70)
        print("RIGHT CAMERA PARAMETERS")
        print("-"*70)
        right_camera = data.get("right_camera", {})
        if right_camera:
            print(f"  fx: {right_camera.get('fx', 0):.4f}")
            print(f"  fy: {right_camera.get('fy', 0):.4f}")
            print(f"  cx: {right_camera.get('cx', 0):.4f}")
            print(f"  cy: {right_camera.get('cy', 0):.4f}")
            print(f"  xi: {right_camera.get('xi', 0):.6f}")
            print(f"  alpha: {right_camera.get('alpha', 0):.6f}")
            print(f"  k1: {right_camera.get('k1', 0):.6f}")
            print(f"  k2: {right_camera.get('k2', 0):.6f}")
            print(f"  k3: {right_camera.get('k3', 0):.6f}")
            print(f"  k4: {right_camera.get('k4', 0):.6f}")
            print(f"  k5: {right_camera.get('k5', 0):.6f}")
            print(f"  k6: {right_camera.get('k6', 0):.6f}")
        
        # Read calibration accuracy evaluation
        print("\n" + "="*70)
        print("CALIBRATION ACCURACY EVALUATION")
        print("="*70)
        
        eval_data = data.get("calibration_accuracy_evaluation", {})
        if eval_data:
            # Monocular Reprojection Error
            print("\n1. MONOCULAR REPROJECTION ERROR")
            print("-"*70)
            mono = eval_data.get("monocular_reprojection_error", {})
            if mono:
                left_avg = mono.get("left_camera_avg", 0)
                right_avg = mono.get("right_camera_avg", 0)
                overall_avg = mono.get("overall_avg", 0)
                threshold = mono.get("threshold", 0)
                status = mono.get("status", "N/A")
                
                print(f"  Left Camera Average:   {left_avg:.4f} pixels")
                print(f"  Right Camera Average:  {right_avg:.4f} pixels")
                print(f"  Overall Average:       {overall_avg:.4f} pixels")
                print(f"  Threshold:             {threshold:.4f} pixels")
                print(f"  Status:                {status}")
            
            # Stereo Reprojection Error
            print("\n2. STEREO REPROJECTION ERROR")
            print("-"*70)
            stereo = eval_data.get("stereo_reprojection_error", {})
            if stereo:
                avg = stereo.get("average", 0)
                maximum = stereo.get("maximum", 0)
                threshold_avg = stereo.get("threshold_avg", 0)
                threshold_max = stereo.get("threshold_max", 0)
                status = stereo.get("status", "N/A")
                
                print(f"  Average:               {avg:.4f} pixels")
                print(f"  Maximum:               {maximum:.4f} pixels")
                print(f"  Threshold (avg):       {threshold_avg:.4f} pixels")
                print(f"  Threshold (max):       {threshold_max:.4f} pixels")
                print(f"  Status:                {status}")
            
            # Rectification Error
            print("\n3. STEREO RECTIFICATION ERROR")
            print("-"*70)
            rect = eval_data.get("rectification_error", {})
            if rect:
                num_points = rect.get("num_points_evaluated", 0)
                avg = rect.get("average_y_difference", -1)
                maximum = rect.get("maximum_y_difference", -1)
                status = rect.get("status", "N/A")
                
                print(f"  Points Evaluated:      {num_points}")
                if num_points > 0 and avg >= 0:
                    threshold_avg = rect.get("threshold_avg", 0)
                    threshold_max = rect.get("threshold_max", 0)
                    print(f"  Average Y-Difference:  {avg:.4f} pixels")
                    print(f"  Maximum Y-Difference:  {maximum:.4f} pixels")
                    print(f"  Threshold (avg):       {threshold_avg:.4f} pixels")
                    print(f"  Threshold (max):       {threshold_max:.4f} pixels")
                else:
                    reason = rect.get("reason", "N/A")
                    print(f"  Reason:                {reason}")
                print(f"  Status:                {status}")
            
            # Baseline Distance
            print("\n4. BASELINE DISTANCE")
            print("-"*70)
            baseline = eval_data.get("baseline", {})
            if baseline:
                cal_baseline_m = baseline.get("calibrated_baseline_m", 0)
                cal_baseline_mm = baseline.get("calibrated_baseline_mm", 0)
                
                print(f"  Calibrated Baseline:   {cal_baseline_m:.6f} m ({cal_baseline_mm:.2f} mm)")
                
                phys_baseline_m = baseline.get("physical_baseline_m", -1)
                if phys_baseline_m > 0:
                    phys_baseline_mm = baseline.get("physical_baseline_mm", 0)
                    error_m = baseline.get("error_m", 0)
                    error_mm = baseline.get("error_mm", 0)
                    threshold_m = baseline.get("threshold_m", 0)
                    threshold_mm = baseline.get("threshold_mm", 0)
                    status = baseline.get("status", "N/A")
                    
                    print(f"  Physical Baseline:     {phys_baseline_m:.6f} m ({phys_baseline_mm:.2f} mm)")
                    print(f"  Baseline Error:        {error_m:.6f} m ({error_mm:.2f} mm)")
                    print(f"  Threshold:             {threshold_m:.6f} m ({threshold_mm:.2f} mm)")
                    print(f"  Status:                {status}")
                else:
                    status = baseline.get("status", "N/A")
                    print(f"  Status:                {status}")
            
            # Overall Summary
            print("\n" + "="*70)
            print("OVERALL SUMMARY")
            print("="*70)
            summary = eval_data.get("overall_summary", {})
            if summary:
                monocular_pass = summary.get("monocular_pass", False)
                stereo_pass = summary.get("stereo_pass", False)
                rectification_pass = summary.get("rectification_pass", False)
                baseline_pass = summary.get("baseline_pass", False)
                all_pass = summary.get("all_tests_pass", False)
                
                print(f"  Monocular Reprojection:  {('✓ PASS' if monocular_pass else '✗ FAIL')}")
                print(f"  Stereo Reprojection:     {('✓ PASS' if stereo_pass else '✗ FAIL')}")
                print(f"  Rectification Error:     {('✓ PASS' if rectification_pass else '✗ FAIL')}")
                print(f"  Baseline Distance:       {('✓ PASS' if baseline_pass else '✗ FAIL')}")
                print("\n" + "-"*70)
                print(f"  OVERALL CALIBRATION:     {('✓ PASS' if all_pass else '✗ FAIL')}")
                print("-"*70)
        
        print("\n" + "="*70 + "\n")
        return True
        
    except Exception as e:
        print(f"Error reading calibration results: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    if len(sys.argv) < 2:
        # Default to output/cam_stereo.yml
        yaml_file = "output/cam_stereo.yml"
        print(f"Usage: {sys.argv[0]} <calibration_yaml_file>")
        print(f"Using default file: {yaml_file}\n")
    else:
        yaml_file = sys.argv[1]
    
    success = read_calibration_results(yaml_file)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
