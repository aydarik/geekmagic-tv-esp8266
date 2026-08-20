import struct
from PIL import Image, ImageFont

FONT_PATH = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"

FALLBACK_FONT_PATHS = (
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
)

CHARACTERS = (
    "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~"
    "ÄÅÆÇÈÉÑÖØÜßàáääåæçèéñöøü"
    "ĄąĆćĘęŁłŃńŚśŹźŻżČčĚěŇňŘřŠšŤťŽžŐőŰűŞşŢţ"
    "АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
    "абвгдежзийклмнопрстуфхцчшщъыьэюя"
    "Ёё№…«»€₽£¥°℃℉←↑→↓↔↕↖↗↘↙♥✔✖♪♫₂㎳"
)


def generate_vlw_header(
        font_size: int,
        output_file: str = "src/fonts/NotoSans_Regular24.h",
        array_name: str = "NotoSans_Regular24",
        font_name: str = "NotoSans-Regular",
        postscript_name: str = "NotoSans-Regular",
        fallback_font_paths: tuple[str, ...] = FALLBACK_FONT_PATHS,
):
    try:
        font = ImageFont.truetype(FONT_PATH, font_size)
    except IOError:
        print(f"Error: font file '{FONT_PATH}' not found.")
        return

    fallback_fonts = []
    for fallback_path in fallback_font_paths:
        try:
            fallback_fonts.append(ImageFont.truetype(fallback_path, font_size))
        except IOError:
            # Fallbacks are optional; the primary font is still usable if a
            # particular system font is not installed.
            pass

    def font_for_char(char: str):
        """Return the first font that contains char.

        Pillow renders a missing glyph as the same small crossed box for a
        number of fonts.  U+FFFF is guaranteed to be absent from these fonts,
        so comparing against its mask lets us detect that placeholder without
        requiring fontTools or parsing the font's cmap table.
        """
        for candidate in (font, *fallback_fonts):
            mask = candidate.getmask(char)
            missing = candidate.getmask("\uffff")
            if mask.size != missing.size or bytes(mask) != bytes(missing):
                return candidate
        return font

    unique_chars = sorted(list(dict.fromkeys(CHARACTERS)), key=lambda c: ord(c))

    ascent, descent = font.getmetrics()
    glyph_headers = []
    # VLW stores all glyph records first and all bitmap data after them.
    # Each record is seven 32-bit values (the last one is reserved padding).
    # Keep the bitmap separate so the offset calculation used by TFT_eSPI
    # remains valid.
    glyph_bitmaps = []

    for char in unique_chars:
        code_point = ord(char)
        # getbbox() describes the font's logical layout box.  It includes
        # antialiasing margins and its y origin is not the VLW baseline
        # offset.  VLW glyphs must instead use the non-zero part of the
        # rendered mask, just like the original TFT_eSPI converter does.
        glyph_font = font_for_char(char)
        mask = glyph_font.getmask(char)
        mask_image = Image.frombytes("L", mask.size, bytes(mask))
        mask_bbox = mask_image.getbbox()
        layout_bbox = glyph_font.getbbox(char)

        if not mask_bbox or not layout_bbox:
            w, h = 0, 0
            x_offset, y_offset = 0, 0
        else:
            w = mask_bbox[2] - mask_bbox[0]
            h = mask_bbox[3] - mask_bbox[1]
            # getmask() is relative to layout_bbox's left edge, so retain
            # the font's left side bearing when the visible mask is cropped.
            x_offset = layout_bbox[0] + mask_bbox[0]
            # dY is the distance from the glyph bitmap's top to the baseline.
            # Pillow's bbox is relative to the selected font's baseline, and
            # the mask has been cropped to its non-zero pixels.  Use the
            # glyph's top, rather than its bottom: using bbox[3] - descent
            # makes every glyph start at the same height and causes short
            # letters such as e, s and r to float above the baseline.
            glyph_ascent = glyph_font.getmetrics()[0]
            y_offset = glyph_ascent - layout_bbox[1]

        advance = int(glyph_font.getlength(char)) if hasattr(glyph_font, 'getlength') else w

        glyph_bitmap = bytearray()
        if w > 0 and h > 0:
            img = mask_image.crop(mask_bbox)
            glyph_bitmap.extend(img.tobytes())

        glyph_headers.append({
            'code': code_point,
            'height': h,
            'width': w,
            'advance': advance,
            'dY': y_offset,
            'dX': x_offset
        })
        glyph_bitmaps.append(glyph_bitmap)

    vlw_bytes = bytearray()

    vlw_bytes.extend(struct.pack(">I", len(unique_chars)))
    vlw_bytes.extend(struct.pack(">I", 0x000B))
    vlw_bytes.extend(struct.pack(">I", font_size))
    vlw_bytes.extend(struct.pack(">I", 0))
    vlw_bytes.extend(struct.pack(">I", ascent))
    vlw_bytes.extend(struct.pack(">I", descent))

    for g in glyph_headers:
        vlw_bytes.extend(struct.pack(">I", g['code']))
        vlw_bytes.extend(struct.pack(">I", g['height']))
        vlw_bytes.extend(struct.pack(">I", g['width']))
        vlw_bytes.extend(struct.pack(">I", g['advance']))
        vlw_bytes.extend(struct.pack(">i", g['dY']))
        vlw_bytes.extend(struct.pack(">i", g['dX']))
        vlw_bytes.extend(struct.pack(">I", 0))  # reserved/padding

    for glyph_bitmap in glyph_bitmaps:
        vlw_bytes.extend(glyph_bitmap)

    # TFT_eSPI expects the VLW metadata footer, even when the font is embedded
    # in a PROGMEM array rather than loaded from a .vlw file.
    for name in (font_name, postscript_name):
        encoded_name = name.encode("ascii")
        if len(encoded_name) > 255:
            raise ValueError("VLW font names must be at most 255 bytes long")
        vlw_bytes.append(len(encoded_name))
        vlw_bytes.extend(encoded_name)
        vlw_bytes.append(0)
    vlw_bytes.append(1)  # anti-aliased/smoothed font

    with open(output_file, "w", encoding="utf-8") as f:
        f.write("#include <pgmspace.h>\n\n")
        f.write(f"const uint8_t {array_name}[] PROGMEM = {{\n")

        line_buffer = []
        for b in vlw_bytes:
            line_buffer.append(f"0x{b:02X}")
            if len(line_buffer) == 16:
                f.write("    " + ", ".join(line_buffer) + ",\n")
                line_buffer = []

        if line_buffer:
            f.write("    " + ", ".join(line_buffer) + "\n")

        f.write("};\n")

    print(f"Generated {output_file} ({len(vlw_bytes)} byte).")


if __name__ == "__main__":
    generate_vlw_header(24)
