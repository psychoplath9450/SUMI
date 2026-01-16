#!/usr/bin/env python3
"""
Portal Build Script for Sumi Firmware

Combines the split portal development files into src/core/portal_html.h
which is embedded directly in the firmware.

Usage:
    python build.py               # Build portal_html.h
    python build.py --watch       # Watch for changes and rebuild
    python build.py --minify      # Build with minification

The portal is embedded in the firmware, NOT stored on SD card.
The SD card is only for user content (books, images, flashcards, maps, notes).
"""

import os
import sys
import re
import time
import argparse
from pathlib import Path

# Paths
PORTAL_DIR = Path(__file__).parent
CSS_DIR = PORTAL_DIR / "css"
JS_DIR = PORTAL_DIR / "js"
TEMPLATE_DIR = PORTAL_DIR / "templates"
OUTPUT_FILE = PORTAL_DIR.parent / "src" / "core" / "portal_html.h"

# Files to include in order
CSS_FILES = [
    "variables.css",
    "base.css",
    "layout.css",
    "components.css",
    "pages.css",
    "responsive.css",
]

JS_FILES = [
    "config.js",
    "api.js",
    "navigation.js",
    "toast.js",
    "status.js",
    "builder.js",
    "plugins.js",
    "wifi.js",
    "files.js",
    "reader.js",
    "bluetooth.js",
    "system.js",
    "settings.js",
    "main.js",
]


def read_file(path):
    """Read a file and return its contents."""
    if path.exists():
        return path.read_text(encoding='utf-8')
    return ""


def collect_css():
    """Collect all CSS files into a single string."""
    css_parts = []
    
    for filename in CSS_FILES:
        filepath = CSS_DIR / filename
        if filepath.exists():
            content = read_file(filepath)
            css_parts.append(f"/* === {filename} === */\n{content}")
        else:
            print(f"Warning: CSS file not found: {filename}")
    
    # Also include any CSS files not in the explicit list
    for filepath in sorted(CSS_DIR.glob("*.css")):
        if filepath.name not in CSS_FILES:
            content = read_file(filepath)
            css_parts.append(f"/* === {filepath.name} === */\n{content}")
    
    return "\n\n".join(css_parts)


def collect_js():
    """Collect all JavaScript files into a single string."""
    js_parts = []
    
    for filename in JS_FILES:
        filepath = JS_DIR / filename
        if filepath.exists():
            content = read_file(filepath)
            js_parts.append(f"// === {filename} ===\n{content}")
        else:
            print(f"Warning: JS file not found: {filename}")
    
    # Also include any JS files not in the explicit list
    for filepath in sorted(JS_DIR.glob("*.js")):
        if filepath.name not in JS_FILES:
            content = read_file(filepath)
            js_parts.append(f"// === {filepath.name} ===\n{content}")
    
    return "\n\n".join(js_parts)


def read_template():
    """Read the HTML template."""
    template_path = TEMPLATE_DIR / "index.html"
    if template_path.exists():
        return read_file(template_path)
    
    # Fallback: Generate minimal template
    return """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Sumi Setup</title>
<style>
{{CSS}}
</style>
</head>
<body>
{{BODY}}
<script>
{{JS}}
</script>
</body>
</html>
"""


def minify_css(css):
    """Simple CSS minification."""
    # Remove comments
    css = re.sub(r'/\*[\s\S]*?\*/', '', css)
    # Remove whitespace around special characters
    css = re.sub(r'\s*([{}:;,>+~])\s*', r'\1', css)
    # Remove multiple spaces
    css = re.sub(r'\s+', ' ', css)
    # Remove leading/trailing whitespace
    css = css.strip()
    return css


def minify_js(js):
    """Simple JavaScript minification (preserves strings)."""
    lines = js.split('\n')
    result = []
    
    for line in lines:
        # Remove single-line comments (but not in strings)
        stripped = line.strip()
        if stripped.startswith('//'):
            continue
        result.append(stripped)
    
    return '\n'.join(result)


def build_portal(minify=False):
    """Build the portal_html.h file."""
    print("Building portal_html.h...")
    
    # Collect CSS
    css = collect_css()
    if minify:
        css = minify_css(css)
    
    # Collect JavaScript
    js = collect_js()
    if minify:
        js = minify_js(js)
    
    # Read template
    template = read_template()
    
    # Read body content
    body_path = TEMPLATE_DIR / "body.html"
    body = read_file(body_path) if body_path.exists() else ""
    
    # Combine HTML
    html = template.replace("{{CSS}}", css)
    html = html.replace("{{JS}}", js)
    html = html.replace("{{BODY}}", body)
    
    # Wrap in C header format
    header = '''/**
 * @file portal_html.h
 * @brief Embedded web portal for Sumi - Auto-generated
 * 
 * This file is auto-generated from portal/templates, portal/css, and portal/js
 * Do not edit directly - edit the source files and run: python portal/build.py
 */

#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

#include <pgmspace.h>

const char PORTAL_HTML[] PROGMEM = R"rawliteral('''
    
    footer = ''')rawliteral";

#endif // PORTAL_HTML_H
'''
    
    output = header + html + footer
    
    # Write output
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_FILE.write_text(output, encoding='utf-8')
    
    size_kb = OUTPUT_FILE.stat().st_size / 1024
    print(f"Built: {OUTPUT_FILE} ({size_kb:.1f} KB)")
    
    return True


def watch_and_build():
    """Watch for file changes and rebuild."""
    print("Watching for changes... (Ctrl+C to stop)")
    
    last_build = 0
    
    def get_mtime():
        """Get most recent modification time."""
        mtimes = []
        for directory in [CSS_DIR, JS_DIR, TEMPLATE_DIR]:
            if directory.exists():
                for f in directory.glob("*"):
                    mtimes.append(f.stat().st_mtime)
        return max(mtimes) if mtimes else 0
    
    while True:
        try:
            current_mtime = get_mtime()
            if current_mtime > last_build:
                build_portal()
                last_build = time.time()
            time.sleep(1)
        except KeyboardInterrupt:
            print("\nStopped watching.")
            break


def main():
    parser = argparse.ArgumentParser(description="Build Sumi portal.html")
    parser.add_argument("--watch", action="store_true", help="Watch for changes")
    parser.add_argument("--minify", action="store_true", help="Minify output")
    args = parser.parse_args()
    
    if args.watch:
        watch_and_build()
    else:
        build_portal(minify=args.minify)


if __name__ == "__main__":
    main()
