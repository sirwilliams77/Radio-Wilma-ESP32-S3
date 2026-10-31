#!/usr/bin/env python3
"""Generates the graphics headers of the ESP32-S3 radio from the files in tools/ and assets/.

Usage (from the sketch folder, Python 3 with Pillow: pip install pillow):
    python tools/make_ui_assets.py

Inputs                                   Output (sketch folder)
  tools/fonts/Roboto-*.ttf               UiFonts.h     antialiased fonts (4-bit alpha)
  (drawn in this script)                 UiIcons.h     small UI icons (4-bit alpha, tinted)
  (drawn in this script)                 UiImages.h    logo/cover placeholders (RGB565)
  tools/weather_icons/<kind>.png         UiWeather.h   weather icons (RGB565 on black)
  assets/radio_wilma_logo.jpg           WebLogo.h     logo of the web panel (JPEG bytes)
  (test graphic)                         StationLogos.h test station logo copied to flash on first start

Weather icons: replace any PNG in tools/weather_icons (any size and colours, transparent
background; it is scaled to fit 78x60) and run the script again. Missing PNGs are
recreated with the default design, so deleting a file restores the original icon.
"""
import math
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.dirname(TOOLS_DIR)
FONT_DIR = os.path.join(TOOLS_DIR, "fonts")
WEATHER_DIR = os.path.join(TOOLS_DIR, "weather_icons")
WEB_LOGO = os.path.join(OUT_DIR, "assets", "radio_wilma_logo.jpg")
LOGO_FILES = sys.argv[1:]
PREVIEW_DIR = os.environ.get("UI_PREVIEW_DIR")

LOGO_W, LOGO_H = 88, 66
COVER_W, COVER_H = 76, 68

TEXT_CHARS = [chr(c) for c in range(32, 127)]
TEXT_CHARS += list("ąćęłńóśźżĄĆĘŁŃÓŚŹŻ")
TEXT_CHARS += [chr(c) for c in range(0xC0, 0x100) if chr(c) not in TEXT_CHARS and c not in (0xD7, 0xF7)]
TEXT_CHARS += list("°…–—’‘“”•")
TEXT_CHARS = sorted(set(TEXT_CHARS), key=ord)

# name, ttf file, pixel size, character set
FONTS = [
    ("FONT_LABEL", "Roboto-Medium.ttf", 11, TEXT_CHARS),
    ("FONT_SMALL", "Roboto-Regular.ttf", 14, TEXT_CHARS),
    ("FONT_TOP", "Roboto-Medium.ttf", 17, TEXT_CHARS),
    ("FONT_TITLE", "Roboto-Medium.ttf", 22, TEXT_CHARS),
    ("FONT_ARTIST", "Roboto-Bold.ttf", 24, TEXT_CHARS),
    ("FONT_STATION_SMALL", "Roboto-Bold.ttf", 28, TEXT_CHARS),
    ("FONT_STATION", "Roboto-Bold.ttf", 40, TEXT_CHARS),
    ("FONT_CLOCK", "Roboto-Bold.ttf", 93, list("-0123456789:")),
    ("FONT_TEMP", "Roboto-Bold.ttf", 30, list(" -0123456789\u00b0C")),
]


def pack4(pixels):
    nib = [(p * 15 + 127) // 255 for p in pixels]
    if len(nib) % 2:
        nib.append(0)
    return bytes((nib[i] << 4) | nib[i + 1] for i in range(0, len(nib), 2))


def c_bytes(data, indent="    "):
    lines = []
    for i in range(0, len(data), 20):
        lines.append(indent + ",".join("0x%02X" % b for b in data[i:i + 20]) + ",")
    return "\n".join(lines)


def render_font(name, ttf, size, chars):
    font = ImageFont.truetype(os.path.join(FONT_DIR, ttf), size)
    ascent, descent = font.getmetrics()
    glyphs, data = [], bytearray()
    canvas = size * 3
    ox, oy = size, size * 2
    for ch in chars:
        img = Image.new("L", (canvas, canvas), 0)
        ImageDraw.Draw(img).text((ox, oy), ch, font=font, fill=255, anchor="ls")
        box = img.getbbox()
        adv = round(font.getlength(ch))
        if box is None:
            glyphs.append((ord(ch), len(data), 0, 0, 0, 0, adv))
            continue
        crop = img.crop(box)
        w, h = crop.size
        glyphs.append((ord(ch), len(data), w, h, box[0] - ox, box[1] - oy, adv))
        data += pack4(list(crop.getdata()))
    cap = -font.getbbox("H", anchor="ls")[1]
    lines = ["// %s: %s %dpx, %d glyphs, %d bytes" % (name, ttf, size, len(glyphs), len(data)),
             "static const uint8_t %s_BITMAP[] PROGMEM = {" % name, c_bytes(data), "};",
             "static const UiGlyph %s_GLYPHS[] PROGMEM = {" % name]
    for g in glyphs:
        lines.append("    {%d, 0x%04X, %d, %d, %d, %d, %d}," % (g[1], g[0], g[2], g[3], g[4], g[5], g[6]))
    lines += ["};",
              "static const UiFont %s = {%s_GLYPHS, %s_BITMAP, %d, %d, %d, %d};" % (
                  name, name, name, len(glyphs), ascent, descent, cap), ""]
    return "\n".join(lines), len(data) + len(glyphs) * 12


def mask_entry(name, img):
    w, h = img.size
    data = pack4(list(img.getdata()))
    return ("static const uint8_t %s_BITS[] PROGMEM = {\n%s\n};\n"
            "static const UiMask %s = {%s_BITS, %d, %d};\n" % (name, c_bytes(data), name, name, w, h))


def draw_icon(w, h, painter, scale=4):
    big = Image.new("L", (w * scale, h * scale), 0)
    painter(ImageDraw.Draw(big), scale)
    return big.resize((w, h), Image.LANCZOS)


def icon_wifi(d, s):
    cx, cy = 11 * s, 15 * s
    for r, wdt in ((13, 2.4), (8.5, 2.4)):
        d.arc((cx - r * s, cy - r * s, cx + r * s, cy + r * s), 225, 315, fill=255, width=int(wdt * s))
    d.ellipse((cx - 2.2 * s, cy - 3.6 * s, cx + 2.2 * s, cy + 0.8 * s), fill=255)


def icon_globe(d, s):
    w = int(1.5 * s)
    d.ellipse((1 * s, 1 * s, 17 * s, 17 * s), outline=255, width=w)
    d.ellipse((5.5 * s, 1 * s, 12.5 * s, 17 * s), outline=255, width=w)
    d.line((1.5 * s, 9 * s, 16.5 * s, 9 * s), fill=255, width=w)


def icon_heart(d, s):
    w = int(1.6 * s)
    d.arc((1 * s, 1 * s, 9.5 * s, 9.5 * s), 135, 360, fill=255, width=w)
    d.arc((7.5 * s, 1 * s, 16 * s, 9.5 * s), 180, 45, fill=255, width=w)
    d.line((2.3 * s, 8.3 * s, 8.5 * s, 14.5 * s), fill=255, width=w)
    d.line((14.7 * s, 8.3 * s, 8.5 * s, 14.5 * s), fill=255, width=w)


def icon_bars(d, s):
    for x, top in ((1, 7), (5.5, 2), (10, 5), (14.5, 12)):
        d.rectangle((x * s, top * s, (x + 2) * s, 16 * s), fill=255)


def icon_menu(d, s):
    for y in (2, 7, 12):
        d.rectangle((1 * s, y * s, 16 * s, (y + 1.6) * s), fill=255)


def icon_clock(d, s):
    w = int(1.5 * s)
    d.ellipse((1 * s, 1 * s, 17 * s, 17 * s), outline=255, width=w)
    d.line((9 * s, 4.5 * s, 9 * s, 9.5 * s), fill=255, width=w)
    d.line((9 * s, 9 * s, 12.5 * s, 11 * s), fill=255, width=w)


def make_icons():
    out = ["// Generated by tools/make_ui_assets.py - 4-bit alpha icons, tinted at draw time.",
           "#pragma once", '#include "UiTypes.h"', ""]
    for name, w, h, fn in (("ICON_WIFI", 22, 17, icon_wifi), ("ICON_GLOBE", 18, 18, icon_globe),
                           ("ICON_HEART", 17, 16, icon_heart), ("ICON_BITRATE", 18, 17, icon_bars),
                           ("ICON_GENRE", 17, 15, icon_menu), ("ICON_TIMEZONE", 18, 18, icon_clock)):
        out.append(mask_entry(name, draw_icon(w, h, fn)))
    return "\n".join(out)


def rounded(img, radius, border=(70, 76, 86)):
    """Black outside rounded corners plus a thin border (the UI background is black)."""
    w, h = img.size
    s = 4
    mask = Image.new("L", (w * s, h * s), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w * s - 1, h * s - 1), radius * s, fill=255)
    mask = mask.resize((w, h), Image.LANCZOS)
    ring = Image.new("L", (w * s, h * s), 0)
    rd = ImageDraw.Draw(ring)
    rd.rounded_rectangle((0, 0, w * s - 1, h * s - 1), radius * s, fill=255)
    rd.rounded_rectangle((s * 1.3, s * 1.3, w * s - 1 - s * 1.3, h * s - 1 - s * 1.3), (radius - 1) * s, fill=0)
    ring = ring.resize((w, h), Image.LANCZOS)
    base = Image.composite(Image.new("RGB", (w, h), border), img.convert("RGB"), ring)
    return Image.composite(base, Image.new("RGB", (w, h), (0, 0, 0)), mask)


def fit_cover(path, w, h):
    src = Image.open(path).convert("RGB")
    scale = max(w / src.width, h / src.height)
    src = src.resize((max(w, round(src.width * scale)), max(h, round(src.height * scale))), Image.LANCZOS)
    left, top = (src.width - w) // 2, (src.height - h) // 2
    return src.crop((left, top, left + w, top + h))


def gradient(w, h, c1, c2):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            t = (x / w * 0.6 + y / h * 0.4)
            px[x, y] = tuple(int(a + (b - a) * t) for a, b in zip(c1, c2))
    return img


def test_logo():
    img = gradient(LOGO_W, LOGO_H, (255, 120, 40), (150, 40, 200))
    d = ImageDraw.Draw(img)
    big = ImageFont.truetype(os.path.join(FONT_DIR, "Roboto-Black.ttf"), 26)
    small = ImageFont.truetype(os.path.join(FONT_DIR, "Roboto-Bold.ttf"), 11)
    d.text((LOGO_W // 2, 30), "TEST", font=big, fill=(255, 255, 255), anchor="mm")
    d.text((LOGO_W // 2, 50), "LOGO 88x66", font=small, fill=(255, 235, 200), anchor="mm")
    return img


def placeholder(w, h, kind):
    img = gradient(w, h, (18, 30, 48), (8, 12, 22))
    d = ImageDraw.Draw(img)
    s = 4
    big = Image.new("L", (w * s, h * s), 0)
    bd = ImageDraw.Draw(big)
    cx, cy = w * s // 2, h * s // 2
    if kind == "note":
        bd.ellipse((cx - 15 * s, cy + 6 * s, cx - 3 * s, cy + 15 * s), fill=255)
        bd.ellipse((cx + 5 * s, cy + 2 * s, cx + 17 * s, cy + 11 * s), fill=255)
        bd.rectangle((cx - 5 * s, cy - 16 * s, cx - 2 * s, cy + 11 * s), fill=255)
        bd.rectangle((cx + 14 * s, cy - 20 * s, cx + 17 * s, cy + 7 * s), fill=255)
        bd.polygon([(cx - 5 * s, cy - 16 * s), (cx + 17 * s, cy - 20 * s),
                    (cx + 17 * s, cy - 13 * s), (cx - 5 * s, cy - 9 * s)], fill=255)
    else:  # radio
        bd.rounded_rectangle((cx - 24 * s, cy - 10 * s, cx + 24 * s, cy + 18 * s), 4 * s, outline=255, width=3 * s)
        bd.line((cx - 16 * s, cy - 12 * s, cx + 12 * s, cy - 24 * s), fill=255, width=3 * s)
        bd.ellipse((cx - 18 * s, cy - 3 * s, cx - 2 * s, cy + 13 * s), outline=255, width=3 * s)
        for y in (0, 6, 12):
            bd.line((cx + 5 * s, cy + (y - 2) * s, cx + 17 * s, cy + (y - 2) * s), fill=255, width=2 * s)
    m = big.resize((w, h), Image.LANCZOS)
    img = Image.composite(Image.new("RGB", (w, h), (30, 170, 240)), img, m)
    return img


def rgb565_entry(name, img):
    w, h = img.size
    vals = []
    for r, g, b in img.getdata():
        vals.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    lines = ["static const uint16_t %s[%d * %d] PROGMEM = {" % (name, w, h)]
    for i in range(0, len(vals), 12):
        lines.append("    " + ",".join("0x%04X" % v for v in vals[i:i + 12]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def write_images(path, header, imgs):
    out = [header, "#pragma once", '#include "UiTypes.h"', ""]
    for name, img in imgs:
        out.append(rgb565_entry(name, img))
        img.save(os.path.join(PREVIEW_DIR, name.lower() + ".png")) if PREVIEW_DIR else None
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(out))


def make_images():
    write_images(os.path.join(OUT_DIR, "UiImages.h"),
                 "// Generated by tools/make_ui_assets.py - placeholder images (RGB565).",
                 [("LOGO_PLACEHOLDER", rounded(placeholder(LOGO_W, LOGO_H, "radio"), 6)),
                  ("COVER_PLACEHOLDER", rounded(placeholder(COVER_W, COVER_H, "note"), 6))])
    if LOGO_FILES:
        logos = [("LOGO_STATION_%d" % i, rounded(fit_cover(p, LOGO_W, LOGO_H), 6))
                 for i, p in enumerate(LOGO_FILES, 1)]
    else:
        logos = [("LOGO_TEST", rounded(test_logo(), 6))]
    write_images(os.path.join(OUT_DIR, "StationLogos.h"),
                 "// Generated by tools/make_ui_assets.py - station logos %dx%d (RGB565).\n"
                 "// Include only from the .ino (station list)." % (LOGO_W, LOGO_H), logos)


def main():
    parts = ["// Generated by tools/make_ui_assets.py from Roboto (SIL Open Font License 1.1).",
             "#pragma once", '#include "UiTypes.h"', ""]
    total = 0
    for name, ttf, size, chars in FONTS:
        text, nbytes = render_font(name, ttf, size, chars)
        parts.append(text)
        total += nbytes
        print("%-20s %6d bytes" % (name, nbytes))
    print("fonts total", total)
    with open(os.path.join(OUT_DIR, "UiFonts.h"), "w", encoding="utf-8") as f:
        f.write("\n".join(parts))
    with open(os.path.join(OUT_DIR, "UiIcons.h"), "w", encoding="utf-8") as f:
        f.write(make_icons())
    make_images()
    make_weather()
    make_web_logo()


# ---------------------------------------------------------------------------
# Weather icons (user-replaceable PNGs in tools/weather_icons)
# ---------------------------------------------------------------------------
WEATHER_W, WEATHER_H = 78, 60
WX_WHITE = (245, 247, 250)
WX_ACCENT = (22, 168, 240)
WEATHER_KINDS = ["CLEAR_DAY", "CLEAR_NIGHT", "PARTLY_DAY", "PARTLY_NIGHT", "CLOUDY",
                 "FOG", "DRIZZLE", "RAIN", "SNOW", "STORM"]


def _cloud(d, s, ox, oy, k=1.0):
    e = lambda a, b, c, dd: d.ellipse(((ox + a * k) * s, (oy + b * k) * s, (ox + c * k) * s, (oy + dd * k) * s), fill=255)
    e(0, 10, 16, 26)
    e(8, 2, 30, 24)
    e(22, 8, 40, 26)
    d.rectangle(((ox + 8 * k) * s, (oy + 16 * k) * s, (ox + 32 * k) * s, (oy + 26 * k) * s), fill=255)


def _sun(d, s, cx, cy, r, rays=True):
    d.ellipse(((cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s), fill=255)
    if rays:
        for a in range(0, 360, 45):
            ra = math.radians(a)
            r1, r2 = r + 3, r + 6.5
            d.line(((cx + r1 * math.cos(ra)) * s, (cy + r1 * math.sin(ra)) * s,
                    (cx + r2 * math.cos(ra)) * s, (cy + r2 * math.sin(ra)) * s), fill=255, width=int(2.2 * s))


def _moon(d, s, cx, cy, r):
    d.ellipse(((cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s), fill=255)
    d.ellipse(((cx - r + r * 0.75) * s, (cy - r - r * 0.35) * s, (cx + r + r * 0.75) * s, (cy + r - r * 0.35) * s), fill=0)


def default_weather_icon(kind, W=WEATHER_W, H=WEATHER_H):
    """Default design (drawn on a 52x40 grid, scaled to W x H). Returns an RGBA image."""
    k = W / 52.0
    s = 4 * k
    gap_px = int(13 * k) | 1
    size = (int(W * 4), int(H * 4))
    white = Image.new("L", size, 0)
    accent = Image.new("L", size, 0)
    wd, ad = ImageDraw.Draw(white), ImageDraw.Draw(accent)
    if kind == "CLEAR_DAY":
        _sun(ad, s, 26, 20, 9)
    elif kind == "CLEAR_NIGHT":
        _moon(ad, s, 24, 20, 13)
    elif kind in ("PARTLY_DAY", "PARTLY_NIGHT"):
        if kind == "PARTLY_DAY":
            _sun(ad, s, 35, 12, 7)
        else:
            _moon(ad, s, 40, 12, 10)
        _cloud(wd, s, 4, 10)
        gap = white.filter(ImageFilter.MaxFilter(gap_px))  # dark gap between cloud and sun/moon
        accent = Image.composite(Image.new("L", size, 0), accent, gap)
    elif kind == "CLOUDY":
        _cloud(ad, s, 14, 2, 0.8)
        gapsrc = Image.new("L", size, 0)
        _cloud(ImageDraw.Draw(gapsrc), s, 4, 10)
        accent = Image.composite(Image.new("L", size, 0), accent, gapsrc.filter(ImageFilter.MaxFilter(gap_px)))
        _cloud(wd, s, 4, 10)
    elif kind == "FOG":
        _cloud(wd, s, 6, 0, 0.9)
        for i, y in enumerate((29, 35)):
            ad.line(((8 + 4 * i) * s, y * s, (44 - 4 * i) * s, y * s), fill=255, width=int(2.4 * s))
    else:  # precipitation: cloud on top
        _cloud(wd, s, 6, 0, 0.85)
        if kind == "DRIZZLE":
            for x in (17, 26, 35):
                ad.ellipse(((x - 1.4) * s, 31 * s, (x + 1.4) * s, 33.8 * s), fill=255)
        elif kind == "RAIN":
            for x in (16, 25, 34):
                ad.line(((x + 2) * s, 29 * s, (x - 1.5) * s, 37 * s), fill=255, width=int(2.4 * s))
        elif kind == "SNOW":
            for x, y in ((16, 31), (26, 35), (36, 31)):
                for a in (0, 60, 120):
                    ra = math.radians(a)
                    ad.line(((x - 3 * math.cos(ra)) * s, (y - 3 * math.sin(ra)) * s,
                             (x + 3 * math.cos(ra)) * s, (y + 3 * math.sin(ra)) * s), fill=255, width=int(1.6 * s))
        elif kind == "STORM":
            ad.polygon([(28 * s, 25 * s), (20 * s, 32 * s), (25.5 * s, 32 * s), (22 * s, 39 * s),
                        (33 * s, 29 * s), (27.5 * s, 29 * s), (32 * s, 25 * s)], fill=255)
    white = white.resize((W, H), Image.LANCZOS)
    accent = accent.resize((W, H), Image.LANCZOS)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(Image.new("RGBA", (W, H), WX_WHITE + (255,)), (0, 0), white)
    img.paste(Image.new("RGBA", (W, H), WX_ACCENT + (255,)), (0, 0), accent)
    return img


def fit_on_black(img, W, H):
    """Scales an icon to fit W x H (aspect kept), centred, transparency blended onto black."""
    img = img.convert("RGBA")
    scale = min(W / img.width, H / img.height)
    size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
    img = img.resize(size, Image.LANCZOS)
    out = Image.new("RGB", (W, H), (0, 0, 0))
    out.paste(img, ((W - size[0]) // 2, (H - size[1]) // 2), img)
    return out


def make_weather():
    os.makedirs(WEATHER_DIR, exist_ok=True)
    out = ["// Generated by tools/make_ui_assets.py from tools/weather_icons/*.png - do not edit.",
           "// To change an icon, replace its PNG and run the script again.",
           "#pragma once", '#include "UiTypes.h"', "",
           "static const int16_t WX_ICON_W = %d;" % WEATHER_W,
           "static const int16_t WX_ICON_H = %d;" % WEATHER_H, ""]
    for kind in WEATHER_KINDS:
        path = os.path.join(WEATHER_DIR, kind.lower() + ".png")
        if not os.path.exists(path):
            default_weather_icon(kind).save(path)
            print("created default", os.path.relpath(path, OUT_DIR))
        out.append(rgb565_entry("WX_" + kind, fit_on_black(Image.open(path), WEATHER_W, WEATHER_H)))
    out.append("// Order matches enum UiWeatherKind in RadioUi.h.")
    out.append("static const uint16_t *const WEATHER_ICONS[] = {\n%s\n};\n" %
               "\n".join("    WX_%s," % k for k in WEATHER_KINDS))
    with open(os.path.join(OUT_DIR, "UiWeather.h"), "w", encoding="utf-8") as f:
        f.write("\n".join(out))


def make_web_logo():
    with open(WEB_LOGO, "rb") as f:
        data = f.read()
    out = ["// Generated by tools/make_ui_assets.py from assets/radio_wilma_logo.jpg - do not edit.",
           "#pragma once", "#include <stdint.h>", "#include <stddef.h>", "",
           "static const uint8_t WEB_LOGO_JPG[] PROGMEM = {", c_bytes(data), "};",
           "static const size_t WEB_LOGO_JPG_LEN = sizeof(WEB_LOGO_JPG);", ""]
    with open(os.path.join(OUT_DIR, "WebLogo.h"), "w", encoding="utf-8") as f:
        f.write("\n".join(out))


if __name__ == "__main__":
    main()
