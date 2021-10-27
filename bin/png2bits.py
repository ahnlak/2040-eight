#!/bin/env python3
#
# Tool to convert PNG files into usuable C bit arrays for PicoSystem stuff.
# Hacked together from a script from IronScorpion (but the terrible bit of
# Python are all my fault!)

from PIL import Image
from bitarray import bitarray
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
code_file.write(f'const uint8_t {image_path.stem}_data[{int((img.width*img.height)/8)}] = {{\n')

# And use a bitarray to assemble the data
data = bitarray(img.height * img.width)

# Work through the image one pixel at a time
index = 0
for row in range(0, img.height):
  for col in range(0, img.width):

    # Fetch the RGBA values for the current pixel
    (r,g,b,a) = img.getpixel((col,row))

    # If transparent, the pixel is empty - otherwise it's set
    if a == 0:
        data[index] = 0
    else:
        data[index] = 1

    index += 1

# Now write out the bytes of this bitarray
span = 0
for byte in data.tobytes():
    code_file.write(f'0x{byte:02x}, ')
    if span >= 16:
        span = 0
        code_file.write('\n')
    else:
        span += 1

# And close up the output array
code_file.write('\n};\n')
