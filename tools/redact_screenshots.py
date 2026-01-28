#!/usr/bin/env python3
"""
Screenshot Redaction Tool for SUMI Documentation
Blacks out WiFi network names and IP addresses for privacy

Usage:
  python redact_screenshots.py input.png output.png [--wifi] [--ip]
  
Or edit the REDACT_AREAS dictionary below for specific coordinates.
"""
from PIL import Image, ImageDraw
import sys
import os

def redact_image(input_path, output_path, areas):
    """Draw black rectangles over specified areas"""
    img = Image.open(input_path)
    draw = ImageDraw.Draw(img)
    
    for area in areas:
        draw.rectangle(area, fill='black')
    
    img.save(output_path, quality=95)
    print(f"Redacted: {input_path} -> {output_path}")

# Common redaction areas for SUMI portal screenshots (829x1567 resolution)
# Adjust these coordinates based on your actual screenshots
REDACT_PRESETS = {
    # Top banner area - "NetworkName • 192.168.x.x"
    'banner_right': (620, 102, 829, 125),
    
    # WiFi settings page - network list
    'wifi_network_1': (207, 633, 310, 660),  # First network name
    'wifi_network_2': (207, 678, 310, 705),  # Second network name
    
    # Banner with network instructions
    'banner_ssid': (350, 166, 450, 190),     # SSID in instruction text
    'banner_ip': (580, 166, 680, 190),       # IP in instruction text
    
    # Connection status area
    'connection_ssid': (350, 525, 530, 560), # "Connected to NetworkName"
    'connection_ip': (370, 690, 475, 720),   # IP address in instructions
}

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python redact_screenshots.py input.png output.png [area1] [area2] ...")
        print("\nAvailable preset areas:")
        for name, coords in REDACT_PRESETS.items():
            print(f"  {name}: {coords}")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    # Collect areas to redact
    areas = []
    if len(sys.argv) > 3:
        for area_name in sys.argv[3:]:
            if area_name in REDACT_PRESETS:
                areas.append(REDACT_PRESETS[area_name])
            else:
                print(f"Unknown area: {area_name}")
    else:
        # Default: redact banner_right (most common)
        areas.append(REDACT_PRESETS['banner_right'])
    
    redact_image(input_file, output_file, areas)
