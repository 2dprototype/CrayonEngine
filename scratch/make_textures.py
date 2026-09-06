import struct
import math
import os

def write_bmp(filename, width, height, pixels):
    # 32-bit RGBA BMP
    row_size = width * 4
    image_size = row_size * height
    file_size = 54 + image_size
    
    header = bytearray([
        0x42, 0x4D,             # 'BM'
        *struct.pack('<I', file_size),
        0, 0, 0, 0,             # Reserved
        *struct.pack('<I', 54), # Offset to pixel data
        *struct.pack('<I', 40), # DIB header size (BITMAPINFOHEADER)
        *struct.pack('<i', width),
        *struct.pack('<i', height), # Bottom-up
        *struct.pack('<H', 1),  # Planes
        *struct.pack('<H', 32), # Bits per pixel
        *struct.pack('<I', 0),  # Compression (BI_RGB)
        *struct.pack('<I', image_size),
        *struct.pack('<i', 2835), # X pixels per meter
        *struct.pack('<i', 2835), # Y pixels per meter
        0, 0, 0, 0,             # Colors in color table
        0, 0, 0, 0              # Important color count
    ])
    
    pixel_bytes = bytearray()
    for y in range(height): # bottom to top
        for x in range(width):
            r, g, b, a = pixels[y][x]
            pixel_bytes.extend([b, g, r, a]) # BGRA order
            
    with open(filename, 'wb') as f:
        f.write(header)
        f.write(pixel_bytes)
    print(f"Generated {filename}")

os.makedirs("game/assets/textures", exist_ok=True)

# 1. Crate (32x32)
crate = []
for y in range(32):
    row = []
    for x in range(32):
        border = (x < 3 or x >= 29 or y < 3 or y >= 29)
        cross = abs(x - y) <= 2 or abs(x - (31 - y)) <= 2
        grain = (x * 7 + y * 13) % 17 - 8
        if border:
            r = min(255, max(0, 110 + grain))
            g = min(255, max(0, 75 + grain))
            b = min(255, max(0, 45 + grain))
        elif cross:
            r = min(255, max(0, 140 + grain))
            g = min(255, max(0, 95 + grain))
            b = min(255, max(0, 55 + grain))
        else:
            r = min(255, max(0, 180 + grain))
            g = min(255, max(0, 125 + grain))
            b = min(255, max(0, 75 + grain))
        row.append((r, g, b, 255))
    crate.append(row)
write_bmp("game/assets/textures/crate.bmp", 32, 32, crate)

# 2. Cobblestone / Brick (32x32)
brick = []
for y in range(32):
    row = []
    for x in range(32):
        by = y % 8
        offset = 8 if (y // 8) % 2 == 1 else 0
        bx = (x + offset) % 16
        is_mortar = (by == 0 or bx == 0)
        noise = (x * 11 + y * 23) % 25 - 12
        if is_mortar:
            row.append((40, 40, 45, 255))
        else:
            base = 130 + noise
            row.append((base, base - 10, base - 5, 255))
    brick.append(row)
write_bmp("game/assets/textures/brick.bmp", 32, 32, brick)

# 3. Grass (32x32)
grass = []
for y in range(32):
    row = []
    for x in range(32):
        n = (x * 17 + y * 37) % 35
        r = 35 + n // 2
        g = 130 + n
        b = 40 + n // 3
        row.append((r, g, b, 255))
    grass.append(row)
write_bmp("game/assets/textures/grass.bmp", 32, 32, grass)

# 4. Item / Coin (16x16)
coin = []
for y in range(16):
    row = []
    for x in range(16):
        dx = x - 7.5
        dy = y - 7.5
        dist = math.sqrt(dx * dx + dy * dy)
        if dist > 7.0:
            row.append((0, 0, 0, 0)) # transparent
        elif dist > 5.5:
            row.append((180, 130, 10, 255)) # dark gold edge
        elif dist > 3.0:
            row.append((255, 215, 0, 255)) # bright gold
        else:
            row.append((255, 240, 120, 255)) # inner gleam
    coin.append(row)
write_bmp("game/assets/textures/coin.bmp", 16, 16, coin)
