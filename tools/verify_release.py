#!/usr/bin/env python3
"""Verify the local release payload and pinned engine sources (does not publish).
Usage: python3 tools/verify_release.py dist/Cue-V-Setup.exe ENGINE_SOURCE_ZIP [build/Cue]
"""
import hashlib
import io
import json
import lzma
from pathlib import Path, PurePosixPath
import struct
import sys
import tarfile
import zipfile
import zlib


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify(installer, engine, stage=None):
    repo = Path(__file__).resolve().parent.parent
    manifest = json.loads((repo / 'third_party/libmpv/manifest.json').read_text())
    sources = json.loads((repo / 'third_party/libmpv/sources.json').read_text())
    binary = installer.read_bytes()
    expected_line = f'{digest(binary)}  {installer.name}'
    if installer.with_suffix(installer.suffix + '.sha256').read_text().strip() != expected_line:
        raise ValueError('Installer checksum does not match')
    if len(binary) < 48 or binary[-48:-40] != b'CUESETUP':
        raise ValueError('Installer footer missing')
    start, size, raw_size, count, crc, reserved = struct.unpack('<QQQQII', binary[-40:])
    if start + size != len(binary) - 48 or reserved or binary[:2] != b'MZ':
        raise ValueError('Invalid installer bounds or executable stub')
    raw = lzma.decompress(binary[start:start+size], format=lzma.FORMAT_ALONE)
    if len(raw) != raw_size or zlib.crc32(raw) != crc:
        raise ValueError('Payload size or CRC mismatch')
    entries = {}
    offset = 0
    for _ in range(count):
        length, = struct.unpack_from('<H', raw, offset)
        offset += 2
        name = raw[offset:offset+length].decode('utf-8')
        offset += length
        length, = struct.unpack_from('<Q', raw, offset)
        offset += 8
        if name in entries or PurePosixPath(name).is_absolute() or '..' in PurePosixPath(name).parts or offset + length > len(raw):
            raise ValueError(f'Invalid payload entry: {name}')
        data = raw[offset:offset+length]
        entries[name] = data
        offset += length
        if stage and name != 'sources/README.txt' and (stage / name).read_bytes() != data:
            raise ValueError(f'Staged bytes differ: {name}')
    if offset != len(raw) or not entries.get('Cue.exe', b'').startswith(b'MZ'):
        raise ValueError('Payload trailing bytes or application executable missing')
    dll = entries[manifest['dll_name']]
    if digest(dll) != manifest['sha256'] or len(dll) != manifest['size_bytes']:
        raise ValueError('Engine DLL does not match repository manifest')
    note = entries['sources/README.txt'].decode('utf-8')
    if manifest['source_package'] not in note or 'releases/tag/v' not in note:
        raise ValueError('Installed source link is incomplete')
    with zipfile.ZipFile(engine) as archive:
        if digest(engine.read_bytes()) != manifest['source_package_sha256']:
            raise ValueError('Engine source ZIP does not match repository manifest')
        if archive.testzip():
            raise ValueError('Engine source ZIP has corrupt entries')
        for source in sources:
            data = archive.read('archives/' + source['archive'])
            if digest(data) != source['sha256']:
                raise ValueError(f"Source archive hash mismatch: {source['name']}")
            with tarfile.open(fileobj=io.BytesIO(data)) as tar:
                if not tar.getmembers():
                    raise ValueError('Empty source archive')
    print(f'Verified: installer SHA256, LZMA, CRC32, {count} payload files, staged byte comparisons, DLL manifest and {len(sources)} source archives')


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    try:
        verify(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) == 4 else None)
    except (OSError, ValueError, KeyError, struct.error, lzma.LZMAError, zipfile.BadZipFile, tarfile.TarError) as error:
        sys.exit(f'Release verification failed: {error}')
