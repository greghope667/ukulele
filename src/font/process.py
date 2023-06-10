from PIL import Image, ImageDraw

ROWS = 6
COLS = 16
CHAR_WIDTH = 8
CHAR_HEIGHT = 8
BORDER = 1
BLANK_PIXEL = {1, 215}

SIZE = (COLS * (CHAR_WIDTH + BORDER), ROWS * (CHAR_HEIGHT + BORDER))

def blank(path):
    image = Image.new(mode='P', size=SIZE, color=1)
    draw = ImageDraw.Draw(image)

    for r in range(ROWS):
        y = r * (CHAR_HEIGHT + BORDER)
        x = size[0]
        draw.line([(0,y), (x,y)], fill=0, width=1)

    for c in range(COLS):
        x = c * (CHAR_WIDTH + BORDER)
        y = size[1]
        draw.line([(x,0), (x,y)], fill=0, width=1)

    image.save(path)


def export(path, out):
    image = Image.open(path)

    out.write("""\
#include "font.h"

const font_t font_8x8 = {
""")

    def readcharline(x0, y0):
        num = 0
        out.write(" ")
        for x in reversed(range(x0, x0+CHAR_WIDTH)):
            p = image.getpixel((x, y0))
            num *= 2
            num += (p not in BLANK_PIXEL)
        out.write(f"0x{num:02x},")

    def readchar(x0, y0):
        out.write("\t{")
        for y in range(y0, y0+CHAR_HEIGHT):
            readcharline(x0, y)
        out.write("},\n")

    for r in range(ROWS):
        y0 = r * (CHAR_HEIGHT + BORDER) + 1
        for c in range(COLS):
            x0 = c * (CHAR_WIDTH + BORDER) + 1
            readchar(x0, y0)

    out.write("};\n")

if __name__ == "__main__":
    import sys
    export("font.png", sys.stdout)
