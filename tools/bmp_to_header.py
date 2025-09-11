#!/usr/bin/env python3
"""Convert BMP file to C header with complete BMP data."""

import sys
import os

def bmp_to_header(input_file, output_file, var_name):
    """Convert BMP file to C header."""
    
    # Read the BMP file
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # Write header file
    with open(output_file, 'w') as f:
        f.write(f"// Generated from {input_file}\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n\n")
        
        # Write the data array
        f.write(f"inline constexpr unsigned char {var_name}_data[] = {{\n")
        
        # Write bytes in rows of 16
        for i in range(0, len(data), 16):
            f.write("    ")
            chunk_size = min(16, len(data) - i)
            for j in range(chunk_size):
                f.write(f"0x{data[i+j]:02x}")
                if i + j < len(data) - 1:
                    f.write(", ")
            f.write("\n")
        
        f.write("};\n\n")
        f.write(f"inline constexpr unsigned int {var_name}_len = sizeof({var_name}_data);\n")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input.bmp> <output.h> <var_name>")
        sys.exit(1)
    
    bmp_to_header(sys.argv[1], sys.argv[2], sys.argv[3])
    print(f"Generated {sys.argv[2]}")