"""Minimal SGI RGB to PNG converter.

SGI RGB format (IRIS image):
  - 512-byte header
  - Pixel data: either verbatim or RLE-compressed scanlines
  - Channel-interleaved (all R scanlines, then G, then B)

This module converts SGI RGB bytes to PNG bytes using only the
standard library (struct + zlib). No Pillow dependency required.
"""

from __future__ import annotations

import struct
import zlib


def sgi_to_png(data: bytes) -> bytes:
    """Convert SGI RGB image bytes to PNG bytes.

    Supports VERBATIM (uncompressed) and RLE-compressed SGI files,
    1 or 3 channels, 1 byte per pixel.
    """
    if len(data) < 512:
        raise ValueError("data too short for SGI RGB header")

    # Parse 512-byte SGI header (big-endian)
    magic = struct.unpack_from(">H", data, 0)[0]
    if magic != 474:
        raise ValueError(f"not an SGI RGB file (magic={magic:#x}, expected 0x01da)")

    storage = struct.unpack_from("B", data, 2)[0]  # 0=verbatim, 1=RLE
    bpc = struct.unpack_from("B", data, 3)[0]  # bytes per channel (1 or 2)
    dimension = struct.unpack_from(">H", data, 4)[0]
    xsize = struct.unpack_from(">H", data, 6)[0]
    ysize = struct.unpack_from(">H", data, 8)[0]
    zsize = struct.unpack_from(">H", data, 10)[0]  # channels

    if bpc != 1:
        raise ValueError(f"only 1-byte-per-channel SGI files supported (got {bpc})")

    if storage == 0:
        pixels = _read_verbatim(data, xsize, ysize, zsize)
    elif storage == 1:
        pixels = _read_rle(data, xsize, ysize, zsize)
    else:
        raise ValueError(f"unknown SGI storage type: {storage}")

    return _encode_png(pixels, xsize, ysize, zsize)


def _read_verbatim(data: bytes, xsize: int, ysize: int, zsize: int) -> list[list[bytes]]:
    """Read verbatim (uncompressed) SGI pixel data.

    Returns list of scanlines (bottom-to-top), each a list of channel bytes.
    """
    offset = 512
    # channels[z] is a flat bytes block of ysize*xsize pixels
    channels: list[bytes] = []
    plane_size = xsize * ysize
    for z in range(zsize):
        start = offset + z * plane_size
        channels.append(data[start : start + plane_size])

    # Build scanlines top-to-bottom, interleaving channels
    scanlines: list[bytes] = []
    for y in range(ysize - 1, -1, -1):  # SGI is bottom-to-top
        row = bytearray()
        for x in range(xsize):
            for z in range(zsize):
                row.append(channels[z][y * xsize + x])
        scanlines.append(bytes(row))
    return scanlines


def _read_rle(data: bytes, xsize: int, ysize: int, zsize: int) -> list[bytes]:
    """Read RLE-compressed SGI pixel data."""
    n_scanlines = ysize * zsize

    # Offset and length tables start at byte 512
    table_offset = 512
    offsets = struct.unpack_from(f">{n_scanlines}I", data, table_offset)
    lengths = struct.unpack_from(f">{n_scanlines}I", data, table_offset + n_scanlines * 4)

    # Decode each scanline
    channels: list[list[bytes]] = [[] for _ in range(zsize)]
    for z in range(zsize):
        for y in range(ysize):
            idx = y + z * ysize
            scanline = _rle_decode(data, offsets[idx], lengths[idx], xsize)
            channels[z].append(scanline)

    # Build scanlines top-to-bottom, interleaving channels
    scanlines: list[bytes] = []
    for y in range(ysize - 1, -1, -1):  # SGI is bottom-to-top
        row = bytearray()
        for x in range(xsize):
            for z in range(zsize):
                row.append(channels[z][y][x])
        scanlines.append(bytes(row))
    return scanlines


def _rle_decode(data: bytes, offset: int, length: int, xsize: int) -> bytes:
    """Decode one RLE-compressed scanline."""
    result = bytearray()
    end = offset + length
    pos = offset
    while pos < end:
        pixel = data[pos]
        pos += 1
        count = pixel & 0x7F
        if count == 0:
            break
        if pixel & 0x80:
            # Copy `count` literal bytes
            result.extend(data[pos : pos + count])
            pos += count
        else:
            # Repeat next byte `count` times
            result.extend(bytes([data[pos]]) * count)
            pos += 1
    # Pad or truncate to xsize
    if len(result) < xsize:
        result.extend(b"\x00" * (xsize - len(result)))
    return bytes(result[:xsize])


def _encode_png(scanlines: list[bytes], width: int, height: int, channels: int) -> bytes:
    """Encode raw scanlines as a PNG file."""
    if channels == 1:
        color_type = 0  # grayscale
    elif channels == 3:
        color_type = 2  # RGB
    elif channels == 4:
        color_type = 6  # RGBA
    else:
        raise ValueError(f"unsupported channel count: {channels}")

    def chunk(chunk_type: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)

    # IHDR
    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    ihdr = chunk(b"IHDR", ihdr_data)

    # IDAT — filter type 0 (None) for each row
    raw = bytearray()
    for row in scanlines:
        raw.append(0)  # filter byte
        raw.extend(row)
    compressed = zlib.compress(bytes(raw))
    idat = chunk(b"IDAT", compressed)

    # IEND
    iend = chunk(b"IEND", b"")

    return b"\x89PNG\r\n\x1a\n" + ihdr + idat + iend
