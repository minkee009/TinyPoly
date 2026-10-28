"""src/icon.c의 도트 그림으로 assets/icon.ico(Windows exe 아이콘)와 docs/icon.png를 만든다.

    python tools/make_icon.py

그림을 바꾸면 이 스크립트를 다시 돌려서 결과 파일을 함께 커밋할 것. 표준 라이브러리만 쓴다.
"""
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COLORS = {
    'K': (24, 20, 36, 255),
    'H': (255, 244, 200, 255),
    'T': (255, 214, 100, 255),
    'U': (236, 172, 62, 255),
    'L': (232, 150, 48, 255),
    'M': (194, 108, 36, 255),
    'R': (150, 84, 44, 255),
}


def read_art():
    src = (ROOT / 'src' / 'icon.c').read_text(encoding='utf-8')
    block = src[src.index('ICON_ART_BEGIN'):src.index('ICON_ART_END')]
    rows = re.findall(r'"([^"]*)"', block)
    assert len(rows) == 16 and all(len(r) == 16 for r in rows), 'icon art must be 16x16'
    return rows


def png(rows, scale):
    n = 16 * scale
    raw = bytearray()
    for y in range(n):
        raw.append(0)
        for x in range(n):
            raw += bytes(COLORS.get(rows[y // scale][x // scale], (0, 0, 0, 0)))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', n, n, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(bytes(raw), 9))
            + chunk(b'IEND', b''))


def ico(rows, scales):
    images = [png(rows, s) for s in scales]
    out = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for s, data in zip(scales, images):
        n = 16 * s
        out += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    return out + b''.join(images)


def main():
    rows = read_art()
    (ROOT / 'assets').mkdir(exist_ok=True)
    (ROOT / 'assets' / 'icon.ico').write_bytes(ico(rows, [1, 2, 3, 4, 8, 16]))   # 16 ~ 256px
    (ROOT / 'docs' / 'icon.png').write_bytes(png(rows, 8))                        # 128px (README)


if __name__ == '__main__':
    main()
