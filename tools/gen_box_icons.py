"""Generator for the box-state icons used by the ZL/ZR all-boxes overview.

Produces three 128x128 RGBA PNGs in romfs/icons/:
  - box_full.png      (all slots occupied)
  - box_empty.png     (no Pokemon)
  - box_nonempty.png  (at least one Pokemon)

Style: thick-outline cartoon box with belt/buckle; nonempty variant has an
enlarged Poke Ball peeking over the top-right corner.
"""
from PIL import Image, ImageDraw
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "romfs", "icons")
OUT = os.path.normpath(OUT)
S = 128

BLACK = (22, 22, 22, 255)
WHITE = (248, 248, 248, 255)
RED   = (224, 58, 62, 255)


def fade(c, a=0.55):
    r, g, b, al = c
    bg = 230
    return (int(r + (bg - r) * a), int(g + (bg - g) * a),
            int(b + (bg - b) * a), al)


def pokeball(img, cx, cy, r, outline=BLACK):
    d = ImageDraw.Draw(img, "RGBA")
    bbox = (cx - r, cy - r, cx + r, cy + r)
    ow = 3 if r >= 18 else 2
    bw = 3 if r >= 18 else 2
    d.pieslice(bbox, 180, 360, fill=RED,   outline=outline, width=ow)
    d.pieslice(bbox, 0,   180, fill=WHITE, outline=outline, width=ow)
    d.rectangle((cx - r, cy - (3 if r >= 18 else 2),
                 cx + r, cy + (3 if r >= 18 else 2)), fill=outline)
    br = max(5 if r >= 18 else 4, r // 3)
    d.ellipse((cx - br, cy - br, cx + br, cy + br), fill=WHITE,
              outline=outline, width=bw)
    inr = max(3 if r >= 18 else 2, br // 2)
    d.ellipse((cx - inr, cy - inr, cx + inr, cy + inr), fill=WHITE,
              outline=outline, width=2)


def draw_box(state):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    body = (120, 200, 95, 255)
    lid  = (165, 225, 115, 255)
    if state == 'empty':
        body = fade(body); lid = fade(lid)
    # body + lid (same geometry across all three states)
    d.rounded_rectangle((18, 50, 110, 110), 10, fill=body, outline=BLACK, width=5)
    d.rounded_rectangle((12, 30, 116, 60), 8, fill=lid,  outline=BLACK, width=5)
    # belt
    d.rectangle((18, 74, 110, 82), fill=BLACK)
    # buckle
    d.rounded_rectangle((55, 70, 73, 86), 3,
                        fill=(240, 200, 60, 255), outline=BLACK, width=3)
    if state == 'empty':
        # hollow buckle
        d.rounded_rectangle((55, 70, 73, 86), 3,
                            fill=body, outline=BLACK, width=3)
    if state == 'nonempty':
        # larger Poke Ball peeking over the top-right corner
        pokeball(img, 100, 24, 24)
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    for state, name in [('full', 'box_full.png'),
                        ('empty', 'box_empty.png'),
                        ('nonempty', 'box_nonempty.png')]:
        draw_box(state).save(os.path.join(OUT, name))
    print("wrote icons to", OUT)


if __name__ == '__main__':
    main()
