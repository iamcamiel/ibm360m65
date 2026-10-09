#!/usr/bin/env python3
"""Read raw input frames from the CPU-free PDI1 PCIe image; never write BAR0."""
import argparse,datetime,json,mmap,os,struct,time
from pathlib import Path

def capture(read32):
    if read32(0)!=0x03602065 or read32(0x3FC)!=0x50444931 or read32(0x7F8)!=0:
        raise ValueError('The loaded image is not the PDI1 input diagnostic')
    if read32(0x7FC)!=0x00010004:
        raise ValueError('Unexpected diagnostic interface revision')
    for attempt in range(10):
        if read32(0x400)!=0x504E4C31:raise ValueError('PNL1 snapshot signature missing')
        generation=read32(0x404)
        flags=read32(0x408);divider=read32(0x40C)
        banks=[read32(0x410+4*b)&0xFFFFFF for b in range(8)]
        if generation==read32(0x404):
            return {'checked_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    'generation':generation,'frame_valid':bool(flags&8),
                    'enable_divider':divider,'banks':banks,
                    'serial_sync':(flags>>8)&255,'serial_meta':(flags>>16)&255,
                    'build_date':f'{read32(0x7F4):08x}','build_time':f'{read32(0x7F0):08x}',
                    'polarity_assumed':False}
    raise ValueError('Another reader repeatedly changed the held frame')

def find_resource():
    devices=[p/'resource0' for p in Path('/sys/bus/pci/devices').iterdir()
      if (p/'vendor').read_text().strip()=='0x0360' and (p/'device').read_text().strip()=='0x2065']
    if len(devices)!=1:raise ValueError('Expected exactly one diagnostic PCIe device')
    return devices[0]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--watch',type=float,default=0)
    args=parser.parse_args()
    if args.watch<0:parser.error('watch must be nonnegative')
    resource=find_resource()
    with (resource.parent/'config').open('rb') as config:
        config.seek(4)
        command=int.from_bytes(config.read(2),'little')
    if not command&2:
        raise ValueError('PCIe memory decoding is disabled; enable the endpoint before reading BAR0')
    fd=os.open(resource,os.O_RDONLY|os.O_SYNC)
    try:
        with mmap.mmap(fd,2048,mmap.MAP_SHARED,mmap.PROT_READ) as mm:
            while True:
                print(json.dumps(capture(lambda o:struct.unpack_from('<I',mm,o)[0])),flush=True)
                if not args.watch:break
                time.sleep(args.watch)
    finally:os.close(fd)
if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:pass
