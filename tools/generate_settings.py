#!/usr/bin/env python3
"""
Settings Schema Generator for Sumi Firmware

Generates consistent settings definitions across:
- C++ structs (for firmware)
- JSON schema (for portal validation)
- TypeScript interfaces (for portal development)
- Documentation (markdown)

Usage:
    python generate_settings.py                    # Generate all
    python generate_settings.py --cpp              # Generate C++ only
    python generate_settings.py --json             # Generate JSON schema only
    python generate_settings.py --docs             # Generate docs only

The schema is defined in settings_schema.yaml in this directory.
"""

import os
import sys
import json
import argparse
from pathlib import Path
from datetime import datetime

# Schema definition (could also be loaded from YAML file)
SETTINGS_SCHEMA = {
    "version": 3,
    "groups": {
        "display": {
            "description": "Display and UI settings",
            "settings": {
                "orientation": {
                    "type": "int",
                    "default": 1,
                    "min": 0,
                    "max": 1,
                    "description": "Screen orientation (0=landscape, 1=portrait)"
                },
                "sleepMinutes": {
                    "type": "int",
                    "default": 15,
                    "min": 0,
                    "max": 120,
                    "description": "Minutes of inactivity before sleep (0=disabled)"
                },
                "fullRefreshPages": {
                    "type": "int",
                    "default": 10,
                    "min": 0,
                    "max": 50,
                    "description": "Pages between full display refreshes"
                },
                "buttonShape": {
                    "type": "int",
                    "default": 0,
                    "min": 0,
                    "max": 2,
                    "description": "Home screen button shape (0=rounded, 1=pill, 2=square)"
                },
                "bgTheme": {
                    "type": "int",
                    "default": 0,
                    "min": 0,
                    "max": 3,
                    "description": "Background theme (0=light, 1=gray, 2=sepia, 3=dark)"
                },
                "showStatusBar": {
                    "type": "bool",
                    "default": True,
                    "description": "Show status bar on home screen"
                },
                "showBatteryHome": {
                    "type": "bool",
                    "default": True,
                    "description": "Show battery indicator on home screen"
                },
                "showClockHome": {
                    "type": "bool",
                    "default": True,
                    "description": "Show clock on home screen"
                },
                "invertColors": {
                    "type": "bool",
                    "default": False,
                    "description": "Invert display colors (dark mode)"
                },
                "hItemsPerRow": {
                    "type": "int",
                    "default": 4,
                    "min": 3,
                    "max": 5,
                    "description": "Items per row in landscape mode"
                },
                "vItemsPerRow": {
                    "type": "int",
                    "default": 2,
                    "min": 2,
                    "max": 3,
                    "description": "Items per row in portrait mode"
                }
            }
        },
        "reader": {
            "description": "E-book reader settings",
            "settings": {
                "fontSize": {
                    "type": "int",
                    "default": 18,
                    "min": 12,
                    "max": 32,
                    "description": "Font size in pixels"
                },
                "lineHeight": {
                    "type": "int",
                    "default": 150,
                    "min": 100,
                    "max": 200,
                    "description": "Line height percentage"
                },
                "margins": {
                    "type": "int",
                    "default": 20,
                    "min": 5,
                    "max": 50,
                    "description": "Page margins in pixels"
                },
                "textAlign": {
                    "type": "int",
                    "default": 1,
                    "min": 0,
                    "max": 1,
                    "description": "Text alignment (0=left, 1=justify)"
                },
                "hyphenation": {
                    "type": "bool",
                    "default": True,
                    "description": "Enable word hyphenation"
                },
                "showProgress": {
                    "type": "bool",
                    "default": True,
                    "description": "Show reading progress"
                }
            }
        },
        "flashcards": {
            "description": "Flashcard/SRS settings",
            "settings": {
                "newPerDay": {
                    "type": "int",
                    "default": 20,
                    "min": 0,
                    "max": 100,
                    "description": "New cards per day"
                },
                "reviewLimit": {
                    "type": "int",
                    "default": 200,
                    "min": 0,
                    "max": 500,
                    "description": "Maximum reviews per day"
                },
                "useFsrs": {
                    "type": "bool",
                    "default": True,
                    "description": "Use FSRS algorithm"
                },
                "shuffle": {
                    "type": "bool",
                    "default": True,
                    "description": "Shuffle card order"
                }
            }
        },
        "weather": {
            "description": "Weather widget settings",
            "settings": {
                "latitude": {
                    "type": "float",
                    "default": 0.0,
                    "min": -90.0,
                    "max": 90.0,
                    "description": "Location latitude"
                },
                "longitude": {
                    "type": "float",
                    "default": 0.0,
                    "min": -180.0,
                    "max": 180.0,
                    "description": "Location longitude"
                },
                "location": {
                    "type": "string",
                    "default": "New York",
                    "maxLength": 32,
                    "description": "Location name"
                },
                "celsius": {
                    "type": "bool",
                    "default": False,
                    "description": "Use Celsius (false=Fahrenheit)"
                },
                "updateHours": {
                    "type": "int",
                    "default": 1,
                    "min": 1,
                    "max": 24,
                    "description": "Hours between updates"
                }
            }
        },
        "bluetooth": {
            "description": "Bluetooth keyboard settings",
            "settings": {
                "enabled": {
                    "type": "bool",
                    "default": False,
                    "description": "Enable Bluetooth"
                },
                "autoConnect": {
                    "type": "bool",
                    "default": True,
                    "description": "Auto-connect to paired devices"
                },
                "keyboardLayout": {
                    "type": "int",
                    "default": 0,
                    "min": 0,
                    "max": 5,
                    "description": "Keyboard layout (0=US, 1=UK, 2=DE, 3=FR, 4=ES, 5=IT)"
                }
            }
        }
    }
}


def generate_cpp_header():
    """Generate C++ header with settings structs."""
    
    lines = [
        "/**",
        " * @file SettingsSchema.h",
        " * @brief Auto-generated settings schema",
        f" * @generated {datetime.now().isoformat()}",
        " * ",
        " * DO NOT EDIT - Generated by tools/generate_settings.py",
        " */",
        "",
        "#ifndef SUMI_SETTINGS_SCHEMA_H",
        "#define SUMI_SETTINGS_SCHEMA_H",
        "",
        "#include <Arduino.h>",
        "",
        f"#define SETTINGS_VERSION {SETTINGS_SCHEMA['version']}",
        "",
    ]
    
    # Generate structs for each group
    for group_name, group in SETTINGS_SCHEMA["groups"].items():
        struct_name = f"{group_name.capitalize()}Settings"
        lines.append(f"// {group['description']}")
        lines.append(f"struct {struct_name} {{")
        
        for setting_name, setting in group["settings"].items():
            cpp_type = {
                "int": "int",
                "float": "float", 
                "bool": "bool",
                "string": "char"
            }[setting["type"]]
            
            comment = f"  // {setting['description']}"
            
            if setting["type"] == "string":
                max_len = setting.get("maxLength", 32)
                lines.append(f"    {cpp_type} {setting_name}[{max_len}];{comment}")
            else:
                default = setting["default"]
                if setting["type"] == "bool":
                    default = "true" if default else "false"
                elif setting["type"] == "float":
                    default = f"{default}f"
                lines.append(f"    {cpp_type} {setting_name} = {default};{comment}")
        
        lines.append("};")
        lines.append("")
    
    # Generate validation function
    lines.append("// Validation limits")
    for group_name, group in SETTINGS_SCHEMA["groups"].items():
        for setting_name, setting in group["settings"].items():
            if "min" in setting:
                const_name = f"{group_name.upper()}_{setting_name.upper()}"
                lines.append(f"#define {const_name}_MIN {setting['min']}")
                lines.append(f"#define {const_name}_MAX {setting['max']}")
    
    lines.append("")
    lines.append("#endif // SUMI_SETTINGS_SCHEMA_H")
    
    return "\n".join(lines)


def generate_json_schema():
    """Generate JSON schema for portal validation."""
    
    schema = {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "title": "Sumi Settings",
        "description": "Settings schema for Sumi e-reader firmware",
        "type": "object",
        "properties": {},
        "additionalProperties": False
    }
    
    for group_name, group in SETTINGS_SCHEMA["groups"].items():
        group_schema = {
            "type": "object",
            "description": group["description"],
            "properties": {},
            "additionalProperties": False
        }
        
        for setting_name, setting in group["settings"].items():
            prop = {
                "description": setting["description"]
            }
            
            if setting["type"] == "int":
                prop["type"] = "integer"
                prop["default"] = setting["default"]
                if "min" in setting:
                    prop["minimum"] = setting["min"]
                if "max" in setting:
                    prop["maximum"] = setting["max"]
            elif setting["type"] == "float":
                prop["type"] = "number"
                prop["default"] = setting["default"]
                if "min" in setting:
                    prop["minimum"] = setting["min"]
                if "max" in setting:
                    prop["maximum"] = setting["max"]
            elif setting["type"] == "bool":
                prop["type"] = "boolean"
                prop["default"] = setting["default"]
            elif setting["type"] == "string":
                prop["type"] = "string"
                prop["default"] = setting["default"]
                if "maxLength" in setting:
                    prop["maxLength"] = setting["maxLength"]
            
            group_schema["properties"][setting_name] = prop
        
        schema["properties"][group_name] = group_schema
    
    return json.dumps(schema, indent=2)


def generate_typescript():
    """Generate TypeScript interfaces for portal development."""
    
    lines = [
        "/**",
        " * Auto-generated TypeScript interfaces for Sumi settings",
        f" * Generated: {datetime.now().isoformat()}",
        " */",
        "",
    ]
    
    for group_name, group in SETTINGS_SCHEMA["groups"].items():
        interface_name = f"{group_name.capitalize()}Settings"
        lines.append(f"/** {group['description']} */")
        lines.append(f"export interface {interface_name} {{")
        
        for setting_name, setting in group["settings"].items():
            ts_type = {
                "int": "number",
                "float": "number",
                "bool": "boolean",
                "string": "string"
            }[setting["type"]]
            
            lines.append(f"  /** {setting['description']} */")
            lines.append(f"  {setting_name}: {ts_type};")
        
        lines.append("}")
        lines.append("")
    
    # Combined interface
    lines.append("/** Complete settings object */")
    lines.append("export interface SumiSettings {")
    for group_name in SETTINGS_SCHEMA["groups"].keys():
        interface_name = f"{group_name.capitalize()}Settings"
        lines.append(f"  {group_name}: {interface_name};")
    lines.append("}")
    
    return "\n".join(lines)


def generate_markdown():
    """Generate markdown documentation."""
    
    lines = [
        "# Sumi Settings Reference",
        "",
        f"*Auto-generated: {datetime.now().isoformat()}*",
        "",
        f"Settings version: {SETTINGS_SCHEMA['version']}",
        "",
        "---",
        "",
    ]
    
    for group_name, group in SETTINGS_SCHEMA["groups"].items():
        lines.append(f"## {group_name.capitalize()}")
        lines.append("")
        lines.append(group["description"])
        lines.append("")
        lines.append("| Setting | Type | Default | Range | Description |")
        lines.append("|---------|------|---------|-------|-------------|")
        
        for setting_name, setting in group["settings"].items():
            type_str = setting["type"]
            default = setting["default"]
            
            if setting["type"] == "bool":
                default = "true" if default else "false"
            
            range_str = ""
            if "min" in setting and "max" in setting:
                range_str = f"{setting['min']}-{setting['max']}"
            elif "maxLength" in setting:
                range_str = f"max {setting['maxLength']} chars"
            
            lines.append(f"| `{setting_name}` | {type_str} | {default} | {range_str} | {setting['description']} |")
        
        lines.append("")
    
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate Sumi settings schema files")
    parser.add_argument("--cpp", action="store_true", help="Generate C++ header only")
    parser.add_argument("--json", action="store_true", help="Generate JSON schema only")
    parser.add_argument("--ts", action="store_true", help="Generate TypeScript only")
    parser.add_argument("--docs", action="store_true", help="Generate docs only")
    parser.add_argument("--output", "-o", default=".", help="Output directory")
    args = parser.parse_args()
    
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    generate_all = not (args.cpp or args.json or args.ts or args.docs)
    
    if generate_all or args.cpp:
        path = output_dir / "SettingsSchema.h"
        path.write_text(generate_cpp_header())
        print(f"Generated: {path}")
    
    if generate_all or args.json:
        path = output_dir / "settings_schema.json"
        path.write_text(generate_json_schema())
        print(f"Generated: {path}")
    
    if generate_all or args.ts:
        path = output_dir / "settings.types.ts"
        path.write_text(generate_typescript())
        print(f"Generated: {path}")
    
    if generate_all or args.docs:
        path = output_dir / "SETTINGS_REFERENCE.md"
        path.write_text(generate_markdown())
        print(f"Generated: {path}")


if __name__ == "__main__":
    main()
