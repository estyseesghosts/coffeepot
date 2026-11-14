#!/usr/bin/env python3
"""Portable host analysis for PICA Lab outputs; no dependencies."""
import argparse, csv, json, statistics, zlib
from collections import defaultdict
from pathlib import Path
SIZE=256; BYTES=SIZE*SIZE*4; CLEAR=0x000000FF

def crc32(data):
    '''Image identity: CRC-32, the value the zip stores and the CSV crc32 column holds.'''
    return zlib.crc32(data)&0xffffffff

def row_crc(row):
    '''CSV image identity, or None for old CSVs (fnv1a32 column): FNV-1a is not an identity.'''
    v=row.get('crc32')
    return v.upper() if v else None

def resolve_csv(path):
    """Accepts results.csv, a run directory, or a run_NNN.zip (extracted beside it)."""
    import zipfile
    p=Path(path)
    if p.suffix=='.zip':
        out=p.with_name(p.stem+'.unzipped')   # not run_NNN/, which may hold text files only
        with zipfile.ZipFile(p) as z:
            if not any(out.rglob('results.csv')) or len([f for f in out.rglob('*') if f.is_file()])!=len(z.namelist()):
                bad=z.testzip()
                if bad: raise ValueError(f'{p}: corrupt entry {bad}')
                z.extractall(out)
        found=sorted(out.rglob('results.csv'))
        if not found: raise ValueError(f'{p}: no results.csv inside')
        return found[0]
    if p.is_dir(): return p/'results.csv'
    return p

def load(path):
    raw=Path(path).read_bytes()
    if len(raw)!=BYTES: raise ValueError(f'{path}: expected {BYTES}, got {len(raw)}')
    return raw

def counts(raw):
    marked=sum(int.from_bytes(raw[i:i+4],'little')!=CLEAR for i in range(0,len(raw),4))
    p=((SIZE//2)*SIZE+SIZE//2)*4
    return marked,int.from_bytes(raw[p:p+4],'little')

def ppm(raw,output,rotate=False):
    out=bytearray(SIZE*SIZE*3)
    for y in range(SIZE):
        for x in range(SIZE):
            src=(y*SIZE+x)*4
            dx,dy=(SIZE-1-y,x) if rotate else (x,y)
            dst=(dy*SIZE+dx)*3
            out[dst:dst+3]=bytes((raw[src+3],raw[src+2],raw[src+1]))
    Path(output).write_bytes(b'P6\n256 256\n255\n'+out)

def diff(a,b,output=None):
    a,b=load(a),load(b)
    changed=sum(a[i:i+4]!=b[i:i+4] for i in range(0,BYTES,4))
    if output:
        Path(output).write_bytes(bytes(abs(x-y) for x,y in zip(a,b)))
    return changed

def image_id(row,base,images=None):
    '''CRC-32 of the image: from the CSV (new runs) or computed from the .rgba (old runs).'''
    c=row_crc(row)
    if c: return c
    f=(images or base)/f"{row['family']}_{int(row['case_id']):04d}.rgba"
    return f'{crc32(f.read_bytes()):08X}' if f.exists() else None

def summarize(path,images=None):
    path=resolve_csv(path)
    rows=list(csv.DictReader(path.open(newline='',encoding='utf-8')))
    groups=defaultdict(list); issues=[]
    for r in rows:
        groups[(r['family'],r['variant'])].append(r)
        if r['family'].startswith('perf'): continue
        image=(images or path.parent)/f"{r['family']}_{int(r['case_id']):04d}.rgba"
        if not image.exists(): issues.append(f'Missing {image}');continue
        raw=load(image); n,c=counts(raw)
        if row_crc(r) and f'{crc32(raw):08X}'!=row_crc(r): issues.append(f'{image}: crc32 mismatch')
        if n!=int(r['nonclear_pixels']): issues.append(f'{image}: pixel count mismatch')
        if f'{c:08X}'!=r['center_abgr_u32'].upper(): issues.append(f'{image}: center mismatch')
    stats={}
    for (family,variant),rs in groups.items():
        result={'count':len(rs)}
        if family.startswith('perf') and family!='perf2_cov':
            values=[float(r['gpu_queue_ms']) for r in rs]
            result.update(median_ms=statistics.median(values),minimum_ms=min(values),maximum_ms=max(values))
            p3d=[float(r['p3d_us']) for r in rs if r.get('p3d_us') not in (None,'','-1')]
            if p3d: result.update(median_p3d_us=statistics.median(p3d))
        else:
            result.update(unique_images=len(set(image_id(r,path.parent,images) for r in rs)),min_pixels=min(int(r['nonclear_pixels']) for r in rs),max_pixels=max(int(r['nonclear_pixels']) for r in rs))
        stats[f'{family}/{variant}']=result
    clear=[r for r in rows if r['family']=='selftest' and r['variant']=='clear']
    rect=[r for r in rows if r['family']=='selftest' and r['variant']=='rect']
    if clear and any(int(r['nonclear_pixels']) for r in clear): issues.append('Selftest: clear contained changed pixels')
    if rect and any(int(r['nonclear_pixels']) < 100 or int(r['nonclear_pixels']) >= SIZE*SIZE for r in rect):
        issues.append('Selftest: rectangle coverage invalid or image blank')
    if len(rect)>1 and len(set(image_id(r,path.parent,images) for r in rect))!=1:
        issues.append('Selftest: repeated rectangle differs')
    print(json.dumps({'rows':len(rows),'issues':issues,'groups':stats},indent=2))
    return int(bool(issues))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='command',required=True)
    q=sub.add_parser('summary');q.add_argument('csv');q.add_argument('--image-dir',type=Path)
    q=sub.add_parser('diff');q.add_argument('a');q.add_argument('b');q.add_argument('--out')
    q=sub.add_parser('ppm');q.add_argument('rgba');q.add_argument('out');q.add_argument('--rotate',action='store_true')
    a=p.parse_args()
    if a.command=='summary': return summarize(a.csv,a.image_dir)
    if a.command=='diff':print(json.dumps({'changed_pixels':diff(a.a,a.b,a.out)}));return 0
    if a.command=='ppm':ppm(load(a.rgba),a.out,a.rotate);return 0
    return 1
if __name__=='__main__':raise SystemExit(main())
