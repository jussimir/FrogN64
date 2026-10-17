#!/usr/bin/env python3
"""Compare captured frames in an explicit region, ignoring alpha."""
import argparse
import json
from PIL import Image, ImageChops, ImageStat


def compare(left, right, crop=None):
    with Image.open(left) as a, Image.open(right) as b:
        a=a.convert('RGB');b=b.convert('RGB')
        if a.size != b.size: raise ValueError(f'Resoluções diferentes: {a.size} / {b.size}')
        if crop:
            if not (0<=crop[0]<crop[2]<=a.width and 0<=crop[1]<crop[3]<=a.height):
                raise ValueError('Recorte fora da imagem.')
            a=a.crop(crop);b=b.crop(crop)
        difference=ImageChops.difference(a,b)
        stats=ImageStat.Stat(difference)
        return {'mse':sum(x*x for x in stats.rms)/3,'max_error':max(e[1] for e in difference.getextrema()),'size':list(a.size)}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('left');parser.add_argument('right')
    parser.add_argument('--crop',type=int,nargs=4)
    parser.add_argument('--max-mse',type=float,default=0)
    args=parser.parse_args()
    result=compare(args.left,args.right,args.crop)
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result['mse']<=args.max_mse else 1)
