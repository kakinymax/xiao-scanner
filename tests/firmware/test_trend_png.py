"""Decode firmware-generated fixtures with independent standard PNG/zlib/MIME readers."""
import binascii
import email.policy
import email.parser
import json
from pathlib import Path
import struct
import unittest
import zlib

OUTPUT = Path(__file__).resolve().parents[2] / "build" / "host-tests"


def decode_png(data: bytes) -> list[bytes]:
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("PNG signature")
    offset, compressed, kinds, dimensions = 8, bytearray(), [], None
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        content = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        if crc != binascii.crc32(kind + content):
            raise ValueError("PNG CRC")
        kinds.append(kind)
        if kind == b"IHDR":
            dimensions = struct.unpack(">IIBBBBB", content)
        elif kind == b"IDAT":
            compressed.extend(content)
        offset += length + 12
    if offset != len(data) or kinds != [b"IHDR", b"IDAT", b"IEND"] or dimensions != (640, 400, 1, 0, 0, 0, 0):
        raise ValueError("PNG format")
    decoded = zlib.decompress(compressed)  # Also verifies DEFLATE and Adler32.
    if len(decoded) != 81 * 400 or any(decoded[i * 81] != 0 for i in range(400)):
        raise ValueError("PNG scanline")
    return [decoded[y * 81 + 1:(y + 1) * 81] for y in range(400)]


class TrendPngTests(unittest.TestCase):
    def test_png_is_independently_decodable(self):
        rows = decode_png((OUTPUT / "trend-sample.png").read_bytes())
        self.assertEqual(len(rows), 400)
        self.assertTrue(any(row != bytes([255]) * 80 for row in rows[85:190]))
        self.assertTrue(any(row != bytes([255]) * 80 for row in rows[260:365]))

    def test_missing_interval_is_not_connected(self):
        rows = decode_png((OUTPUT / "trend-sample.png").read_bytes())
        # 07:00--12:00 is absent; only the dotted axes/grid may appear there.
        for y in range(86, 190):
            for x in range(240, 330):
                if y not in (112, 138, 164) and x != 343:
                    self.assertTrue(rows[y][x // 8] & (128 >> (x % 8)), (x, y))

    def test_empty_graph_is_valid_without_invented_points(self):
        rows = decode_png((OUTPUT / "trend-empty.png").read_bytes())
        self.assertEqual(len(rows), 400)
        self.assertNotEqual(rows, decode_png((OUTPUT / "trend-sample.png").read_bytes()))

    def test_crc_detects_corrupted_image(self):
        data = bytearray((OUTPUT / "trend-sample.png").read_bytes())
        data[500] ^= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            decode_png(data)

    def test_discord_multipart_is_standard_mime(self):
        body = (OUTPUT / "trend-multipart.bin").read_bytes()
        headers = b"MIME-Version: 1.0\r\nContent-Type: multipart/form-data; boundary=XiaoTrendBoundary1\r\n\r\n"
        message = email.parser.BytesParser(policy=email.policy.default).parsebytes(headers + body)
        parts = list(message.iter_parts())
        self.assertEqual(len(parts), 2)
        self.assertEqual(parts[0].get_param("name", header="content-disposition"), "payload_json")
        self.assertEqual(json.loads(parts[0].get_payload(decode=True))["allowed_mentions"], {"parse": []})
        self.assertEqual(parts[1].get_param("name", header="content-disposition"), "files[0]")
        self.assertEqual(parts[1].get_filename(), "trend.png")
        self.assertEqual(parts[1].get_content_type(), "image/png")
        self.assertEqual(parts[1].get_payload(decode=True), (OUTPUT / "trend-sample.png").read_bytes())


if __name__ == "__main__":
    unittest.main()
