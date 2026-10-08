"""Read CKD/CCKD volume labels, dataset extents and PDS directories.

The image is opened read-only. Inspect stopped or offline volumes for a
consistent view; active CCKD writes may otherwise change its index tables.
"""
import argparse
import bz2
import json
from pathlib import Path
import struct
import zlib


class Image:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        self.kind = self.data[:8]
        self.heads, self.track_size = struct.unpack_from('<II', self.data, 8)
        if self.kind == b'CKD_C370':
            self.endian = '>' if self.data[515] & 2 else '<'
            self.cylinders = int.from_bytes(self.data[552:556], 'little')
            self.l1_count = struct.unpack_from(self.endian + 'I', self.data, 516)[0]
        elif self.kind != b'CKD_P370':
            raise ValueError(f'Unsupported image header: {self.kind!r}')

    def track(self, number):
        if self.kind == b'CKD_P370':
            start = 512 + number * self.track_size
            return self.data[start:start + self.track_size]
        if number // 256 >= self.l1_count:
            return b''
        l2 = struct.unpack_from(self.endian + 'I', self.data, 1024 + 4 * (number // 256))[0]
        if l2 in (0, 0xffffffff):
            return b''
        start, length, _ = struct.unpack_from(self.endian + 'IHH', self.data, l2 + 8 * (number % 256))
        if start in (0, 0xffffffff):
            return b''
        compressed = self.data[start:start + length]
        method = compressed[0] & 3
        if method == 1:
            return compressed[:5] + zlib.decompress(compressed[5:])
        if method == 2:
            return compressed[:5] + bz2.decompress(compressed[5:])
        if method:
            raise ValueError(f'Unknown track compression {method}')
        return compressed

    def records(self, number):
        track = self.track(number)
        position = 5
        while position + 8 <= len(track):
            header = track[position:position + 8]
            if header == b'\xff' * 8:
                break
            cylinder, head, record, key_length, data_length = struct.unpack('>HHBBH', header)
            end = position + 8 + key_length + data_length
            if end > len(track):
                raise ValueError(f'Truncated record on track {number}')
            key = track[position + 8:position + 8 + key_length]
            data = track[position + 8 + key_length:end]
            yield cylinder, head, record, key, data
            position = end

    def record(self, cylinder, head, record):
        for _, _, number, key, data in self.records(cylinder * self.heads + head):
            if number == record:
                return key, data
        raise ValueError(f'Record {cylinder}/{head}/{record} absent')

    def catalog(self):
        _, label = self.record(0, 0, 3)
        if label[:4].decode('cp037') != 'VOL1':
            raise ValueError('VOL1 label absent')
        cylinder, head, record = struct.unpack_from('>HHB', label, 11)
        key, data = self.record(cylinder, head, record)
        f4 = key + data
        if len(f4) != 140 or f4[44] != 0xf4:
            raise ValueError('Invalid VTOC format 4 descriptor')
        _, _, begin_c, begin_h, end_c, end_h = struct.unpack_from('>BBHHHH', f4, 105)
        datasets = {}
        for track in range(begin_c * self.heads + begin_h, end_c * self.heads + end_h + 1):
            for _, _, _, key, data in self.records(track):
                if len(key) != 44 or len(data) != 96 or data[0] != 0xf1:
                    continue
                descriptor = key + data
                name = key.decode('cp037').strip()
                extents = []
                for offset in (105, 115, 125):
                    kind, _, bc, bh, ec, eh = struct.unpack_from('>BBHHHH', descriptor, offset)
                    if kind:
                        extents.append([bc * self.heads + bh, ec * self.heads + eh])
                datasets[name] = {'extents': extents, 'organization': descriptor[82:84].hex(),
                                  'block_size': int.from_bytes(descriptor[86:88], 'big')}
        return label[4:10].decode('cp037'), datasets

    def members(self, dataset):
        result = {}
        for begin, end in dataset['extents']:
            for track in range(begin, end + 1):
                for _, _, record, _, data in self.records(track):
                    if record == 0:
                        continue
                    if len(data) != 256:
                        return result
                    used = int.from_bytes(data[:2], 'big')
                    if not 2 <= used <= 256:
                        return result
                    position = 2
                    while position + 8 <= used:
                        name = data[position:position + 8]
                        if name == b'\xff' * 8:
                            return result
                        if position + 12 > used:
                            raise ValueError('Truncated PDS directory entry')
                        entry = data[position:position + 12]
                        result[name.decode('cp037').strip()] = {'ttr': entry[8:11].hex(), 'alias': bool(entry[11] & 0x80)}
                        position += 12 + 2 * (entry[11] & 31)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--members')
    args = parser.parse_args()
    image = Image(args.image)
    volume, datasets = image.catalog()
    result = {'volume': volume, 'datasets': datasets}
    if args.members:
        result['members'] = image.members(datasets[args.members.upper()])
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
