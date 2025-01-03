import png
import math

width = 1024
height = 1024
img = []
for y in range(height):
    row = []
    for x in range(width):
        row.append(21)
        row.append(21)
        row.append(21)
    img.append(row)

for fr in range(6, 1, -1):
    for x in range(2,width-2):
        s = math.sin(fr * 2 * 3.14159 * x / width) * 0.95
        y1 = int((-s+1) * 0.5 * height)
        s = math.sin(fr * 2 * 3.14159 * (x-1) / width) * 0.95
        y0 = int((-s+1) * 0.5 * height)
        if (y0 < y1):
            t = y1
            y1 = y0
            y0 = t
        for p in range(y1, y0):
            for xo in range(-2,2):
                img[p][3 * (x+xo)] = 255 - 30 * fr
                img[p][3 * (x+xo)+1] = 255 - 30 * fr
                img[p][3 * (x+xo)+2] = 255 - 30 * fr

with open('gradient.png', 'wb') as f:
    w = png.Writer(width, height, greyscale=False)
    w.write(f, img)