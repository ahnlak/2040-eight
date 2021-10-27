#!/bin/env python3
#
# Tool to convert PNG files into usuable C structs for the PicoSystem API
# to handle. I *really* hate Python, so this will be very un-pythony.

from PIL import Image
import sys
import pathlib
import math

# Run with `png2argb.py <image.png>`
image_path = pathlib.Path(sys.argv[1])
code_path = image_path.with_suffix('.hpp')

# Fetch the image first
img = Image.open(image_path).convert('RGBA')

print(f"Loaded image file with width {img.width} and height {img.height}\n")

# Create the new file, output the basic structure
code_file = open(code_path, 'w')
code_file.write(f'const picosystem::color_t {image_path.stem}_data[{(img.width*img.height)}] = {{\n')

# Slow but easy to follow; we work through the image a pixel at a time
span = 0;
for row in range(0, img.height):
  for col in range(0, img.width):

    # Fetch the RGBA values for the current pixel
    (r,g,b,a) = img.getpixel((col,row))

    # Convert these values down to 4 bits a piece
    br = min(15, math.ceil(r/16))
    bg = min(15, math.ceil(g/16))
    bb = min(15, math.ceil(b/16))
    ba = min(15, math.ceil(a/16))

    # And jam them together horribly
    c = (bg<<12)|(bb<<8)|(ba<<4)|br

    code_file.write(f'0x{c:04x}, ')

    # Keep lines shortish
    if span >= 7:
      span = 0
      code_file.write('\n')
    else:
      span += 1

# End close up the output array
code_file.write('\n};\n')
code_file.write(f'picosystem::buffer_t {image_path.stem}_buffer{{.w = {img.width}, .h = {img.height}, .data = (picosystem::color_t *){image_path.stem}_data}};\n')