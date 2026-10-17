#!/usr/bin/env python3
"""Run the production audio buffer's MIPS tests and inspect its offline PCM output."""
import argparse
import array
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import wave
from headless import ROOT, digest, freeze_emulator


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--emulator',type=Path,default=ROOT.parent/'pessoal-nfsu2-psp/repositories/ppsspp/build/PPSSPPHeadless')
    ap.add_argument('--assets',type=Path)
    ap.add_argument('--cpu',choices=['jit','interpreter','ir','jit-ir'],default='jit')
    ap.add_argument('--tag',default=time.strftime('audio-%Y%m%d-%H%M%S'))
    args=ap.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',args.tag): raise ValueError('Tag inválida.')
    prx=ROOT/'tests/audio/audio-tests.prx'
    if not prx.is_file(): raise ValueError('Compile tests/audio com PSPSDK antes de executar.')
    emulator,info=freeze_emulator(args.emulator,args.assets,Path('/Applications/PPSSPPSDL.app/Contents/Frameworks/libMoltenVK.dylib'))
    folder=ROOT/'dist/headless'/args.tag
    folder.mkdir(parents=True,exist_ok=False)
    program=folder/'audio-tests.prx';shutil.copy2(prx,program)
    config=folder/'ppsspp.ini'
    config.write_text('[Sound]\nEnable = True\n[SystemParam]\nPSPModel = 1\n')
    command=[str(emulator),'--graphics=null','--cpu='+args.cpu,'--cpu-mhz=333','--timeout-wall=30',
        '--timeout-emulated=10','--frame-stats','--memstick='+str(folder/'memstick'),
        '--appendconfig='+str(config),'--dump-audio='+str(folder/'audio.wav'),str(program)]
    result={'command':command,'emulator':info,'prx_sha256':digest(program),'psp_hardware_used':False}
    start=time.monotonic()
    try:
        with (folder/'process.log').open('w') as log:
            proc=subprocess.run(command,cwd=folder,stdout=log,stderr=subprocess.STDOUT,timeout=35)
        exit_code=proc.returncode
    except subprocess.TimeoutExpired:
        exit_code=None
        result['error']='Timeout de parede; execução sintética não concluída.'
    result.update(exit_code=exit_code,wall_seconds=time.monotonic()-start)
    log=(folder/'process.log').read_text(errors='replace')
    result['mips_cases_passed']='FROGN64_AUDIO_TESTS_PASS samples=44099' in log and exit_code==0
    try:
        with wave.open(str(folder/'audio.wav'),'rb') as wav:
            if wav.getnchannels()!=2 or wav.getsampwidth()!=2: raise ValueError('PCM estéreo 16 bits esperado.')
            rate=wav.getframerate();data=array.array('h',wav.readframes(wav.getnframes()))
            if sys.byteorder!='little':data.byteswap()
        left=data[::2];right=data[1::2]
        active=[x for x in left if abs(x)>10]
        metrics={'sample_rate':rate,'frames':len(left),'duration_seconds':len(left)/rate,
            'peak':max((abs(x) for x in left),default=0),
            'rms_active':math.sqrt(sum(x*x for x in active)/len(active)) if active else 0,
            'max_adjacent_jump':max((abs(left[i]-left[i-1]) for i in range(1,len(left))),default=0),
            'stereo_equal':left==right}
        result['waveform']=metrics
        result['waveform_passed']=1000<metrics['peak']<=13000 and metrics['max_adjacent_jump']<2000 and metrics['stereo_equal']
    except Exception as error:
        result['waveform_error']=str(error);result['waveform_passed']=False
    (folder/'result.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({'folder':str(folder),**result},indent=2))
    return 0 if result['mips_cases_passed'] and result['waveform_passed'] else 1

if __name__=='__main__':sys.exit(main())
