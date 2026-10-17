#!/usr/bin/env python3
"""Run a frozen FrogN64 EBOOT in an isolated PPSSPPHeadless instance."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import threading

ROOT = Path(__file__).resolve().parent.parent


def digest(path):
    with Path(path).open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def freeze_emulator(binary, assets, moltenvk):
    binary = binary.resolve()
    identity = digest(binary)
    target = ROOT / 'dist/tools' / identity[:12]
    target.mkdir(parents=True, exist_ok=True)
    if not (target / 'PPSSPPHeadless').exists():
        shutil.copy2(binary, target / 'PPSSPPHeadless')
    if not assets:
        assets = next((p for p in [binary.parent / 'assets', binary.parent.parent / 'assets'] if p.is_dir()), None)
    if not assets:
        raise ValueError('Informe --assets com a pasta assets do PPSSPP.')
    if not (target / 'assets').exists():
        shutil.copytree(assets, target / 'assets')
    driver_info = None
    if sys.platform == 'darwin' and moltenvk and moltenvk.is_file():
        # PPSSPP's macOS loader looks in this bundle path next to the executable.
        driver_dir = target / 'PPSSPPSDL.app/Contents/Frameworks'
        driver_dir.mkdir(parents=True, exist_ok=True)
        driver = driver_dir / 'libMoltenVK.dylib'
        if not driver.exists():
            shutil.copy2(moltenvk, driver)
        driver_info = {'sha256': digest(driver), 'source': str(moltenvk)}
    assets_hash = hashlib.sha256()
    for asset in sorted((target / 'assets').rglob('*')):
        if asset.is_file():
            assets_hash.update(str(asset.relative_to(target / 'assets')).encode())
            assets_hash.update(bytes.fromhex(digest(asset)))
    info = {'assets_tree_sha256': assets_hash.hexdigest(), 'source': str(binary), 'sha256': identity, 'assets_source': str(assets.resolve()), 'moltenvk': driver_info}
    (target / 'manifest.json').write_text(json.dumps(info, indent=2))
    return target / 'PPSSPPHeadless', info


def rom_id(path):
    with path.open('rb') as source:
        header = source.read(64)
    if len(header) != 64:
        raise ValueError('ROM menor que o cabeçalho N64.')
    magic = header[:4]
    if magic == bytes.fromhex('37804012'):
        header = b''.join(header[i:i+2][::-1] for i in range(0, 64, 2))
    elif magic == bytes.fromhex('40123780'):
        header = b''.join(header[i:i+4][::-1] for i in range(0, 64, 4))
    elif magic != bytes.fromhex('80371240'):
        raise ValueError('Formato de ROM N64 não reconhecido; use ROM descomprimida.')
    # Match Daedalus's ROMHeader/roms.ini IDs, which read the normalized
    # header CRC fields as native little-endian words.
    crc1, crc2 = struct.unpack_from('<II', header, 16)
    return f'{crc1:08x}{crc2:08x}-{header[62]:02x}'


class Debugger:
    def __init__(self, port, pid, timeout):
        import websocket
        self.ws = websocket.create_connection(f'ws://127.0.0.1:{port}/debugger',
            subprotocols=['debugger.ppsspp.org'], timeout=timeout)
        self.ticket = 0
        self.events = []
        self.transcript = []
        version = self.request('version', name='FrogN64 headless', version='1')
        if version.get('pid') != pid:
            raise RuntimeError(f'PID do debugger não corresponde ao processo iniciado: {version}')
        self.version = version

    def receive(self):
        value = json.loads(self.ws.recv())
        # Keep the transcript small: screenshots are saved separately.
        self.transcript.append({k: v for k, v in value.items() if k not in ['uri', 'base64']})
        return value

    def request(self, event, **params):
        self.ticket += 1
        ticket = self.ticket
        self.ws.send(json.dumps({'event': event, 'ticket': ticket, **params}))
        while True:
            value = self.receive()
            if value.get('ticket') == ticket:
                if value.get('event') == 'error':
                    raise RuntimeError(f'{event}: {value}')
                return value
            self.events.append(value)

    def run(self, seconds):
        self.events = [e for e in self.events if e.get('event') != 'cpu.stepping']
        reply = self.request('cpu.runUntilTime', relativeUs=round(seconds * 1_000_000))
        while True:
            stopped = next((e for e in self.events if e.get('event') == 'cpu.stepping'), None)
            if stopped:
                self.events.remove(stopped)
                break
            self.events.append(self.receive())
        status = self.request('cpu.status')
        # PPSSPP exposes microseconds with limited precision on long runs.
        if status['us'] + 1000 < reply['targetUs']:
            raise RuntimeError(f'CPU parou antes do tempo solicitado: {status}')
        return status

    def press(self, button, duration=0.08, settle=0.12):
        self.request('input.buttons.send', buttons={button: True})
        self.run(duration)
        self.request('input.buttons.send', buttons={button: False})
        self.run(settle)

    def analog(self, x, y, duration=0.12, settle=0.3):
        self.request('input.analog.send', x=x, y=y)
        self.run(duration)
        self.request('input.analog.send', x=0, y=0)
        self.run(settle)

    def screenshot(self, target):
        result = self.request('gpu.buffer.screenshot', type='uri')
        uri = result.get('uri', '')
        if not uri.startswith('data:image/png;base64,'):
            raise RuntimeError(f'Captura PNG indisponível: {result.keys()}')
        target.write_bytes(base64.b64decode(uri.split(',', 1)[1]))
        return {k: v for k, v in result.items() if k != 'uri'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--emulator', type=Path, default=os.environ.get('PPSSPP_HEADLESS'))
    ap.add_argument('--assets', type=Path)
    ap.add_argument('--moltenvk', type=Path, default=Path('/Applications/PPSSPPSDL.app/Contents/Frameworks/libMoltenVK.dylib'))
    ap.add_argument('--package', type=Path, default=ROOT / 'dist/package/PSP/GAME/FrogN64')
    ap.add_argument('--rom', type=Path)
    ap.add_argument('--render-debug', type=int, choices=range(32768), metavar='0..32767', help='máscara da build experimental de renderização LEGO')
    ap.add_argument('--audio', choices=['disabled', 'sync'], default='sync', help='ME/Async exige validação no PSP real')
    ap.add_argument('--no-hle', action='store_true')
    ap.add_argument('--n64-cpu', choices=['dynarec', 'interpreter'], default='dynarec')
    ap.add_argument('--no-n64-optimizations', action='store_true')
    ap.add_argument('--cpu', choices=['jit', 'interpreter', 'ir', 'jit-ir'], default='jit')
    ap.add_argument('--backend', choices=['vulkan', 'software', 'gles', 'null'], default='vulkan')
    ap.add_argument('--tag', default=time.strftime('%Y%m%d-%H%M%S'))
    ap.add_argument('--seconds', type=float, default=10)
    ap.add_argument('--wall-seconds', type=float, default=30)
    ap.add_argument('--script', type=Path, help='lista JSON de wait, press e capture, a partir do boot')
    args = ap.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', args.tag):
        raise ValueError('Tag inválida.')
    if not args.emulator:
        candidate = ROOT.parent / 'pessoal-nfsu2-psp/repositories/ppsspp/build/PPSSPPHeadless'
        if candidate.is_file():
            args.emulator = candidate
        else:
            raise ValueError('Informe --emulator ou PPSSPP_HEADLESS.')
    if args.seconds <= 0 or args.wall_seconds <= 0:
        raise ValueError('Os tempos devem ser positivos.')
    if args.script:
        steps = json.loads(args.script.read_text())
    else:
        steps = [{'action': 'wait', 'seconds': 2.0}, {'action': 'capture', 'name': 'menu'}]
        if args.rom:
            steps += [{'action': 'press', 'button': 'cross'}, {'action': 'capture', 'name': 'browser'},
                      {'action': 'press', 'button': 'cross'}]
        steps += [{'action': 'wait', 'seconds': args.seconds}, {'action': 'capture', 'name': 'final'}]
    virtual_time = 0
    for step in steps:
        if step['action'] == 'wait':
            if step['seconds'] <= 0: raise ValueError('wait deve ser positivo.')
            virtual_time += step['seconds']
        elif step['action'] == 'press':
            virtual_time += step.get('seconds', 0.08) + step.get('settle', 0.12)
        elif step['action'] == 'analog':
            if not (-1 <= step['x'] <= 1 and -1 <= step['y'] <= 1): raise ValueError('Analógico fora de -1..1.')
            virtual_time += step.get('seconds', 0.12) + step.get('settle', 0.3)
        elif step['action'] == 'expect_log':
            if not step.get('text'): raise ValueError('expect_log precisa de text.')
        elif step['action'] == 'capture':
            if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', step['name']): raise ValueError('Nome de captura inválido.')
        elif step['action'] == 'compare':
            for key in ['left', 'right']:
                if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', step[key]): raise ValueError('Nome de captura inválido.')
        else:
            raise ValueError('Ação desconhecida: ' + step['action'])
    if args.backend == 'null' and any(s['action'] == 'capture' for s in steps):
        raise ValueError('Backend null não produz capturas; use roteiro sem capture.')
    emulator, info = freeze_emulator(args.emulator, args.assets, args.moltenvk)
    folder = ROOT / 'dist/headless' / args.tag
    folder.mkdir(parents=True, exist_ok=False)
    stick = folder / 'memstick'
    game = stick / 'PSP/GAME/FrogN64'
    shutil.copytree(args.package, game)
    nand = stick / 'PSP/NAND'
    shutil.copytree(emulator.parent / 'assets/flash0', nand / 'flash0')
    if args.render_debug is not None:
        (game / 'frogn64-render-debug.txt').write_text(str(args.render_debug) + '\n')
    rom_info = None
    if args.rom:
        source = args.rom.resolve()
        ident = rom_id(source)
        shutil.rmtree(game / 'Roms')
        (game / 'Roms').mkdir()
        target = game / 'Roms' / source.name
        shutil.copy2(source, target)
        rom_info = {'source': str(source), 'sha256': digest(target), 'id': ident}
        (game / 'Preferences.ini').write_text('DisplayFramerate=yes\nLargeROMBuffer=yes\nViewportType=2\nLanguage=English\n\n' +
            '{' + ident + '}\n' + f'PatchesEnabled={int(not args.no_hle)}\nSpeedSyncEnabled=1\n' +
            f'DynarecEnabled={int(args.n64_cpu == "dynarec")}\n' +
            f'DynarecLoopOptimisation={int(not args.no_n64_optimizations)}\n' +
            f'DynarecDoublesOptimisation={int(not args.no_n64_optimizations)}\n' +
            f'MemoryAccessOptimisation={int(not args.no_n64_optimizations)}\n' +
            'DoubleDisplayEnabled=1\nCheckTextureHashFrequency=0\nFrameskip=0\n' +
            'AudioEnabled=' + ('2' if args.audio == 'sync' else '0') + '\n')
    config = folder / 'ppsspp.ini'
    config.write_text('[SystemParam]\nPSPModel = 1\n[Sound]\nEnable = True\n[Graphics]\nInternalResolution = 1\nFrameSkip = 0\nAutoFrameSkip = False\n')
    command = [str(emulator), '--cpu=' + args.cpu, '--graphics=' + args.backend, '--log', '--loglevel=3',
        '--debugger=0', '--cpu-mhz=333', '--memstick=' + str(stick), '--nand=' + str(nand), '--root=' + str(game),
        '--appendconfig=' + str(config), '--timeout-wall=' + str(args.wall_seconds),
        '--timeout-emulated=' + str(virtual_time + 1.0), '--frame-stats',
        '--dump-audio=' + str(folder / 'audio.wav'), str(game / 'EBOOT.PBP')]
    result = {'command': command, 'emulator': info, 'eboot_sha256': digest(game / 'EBOOT.PBP'),
        'rom': rom_info, 'script': steps, 'backend': args.backend, 'audio': args.audio,
        'psp_mhz': 333, 'hle': not args.no_hle, 'n64_cpu': args.n64_cpu, 'n64_optimizations': not args.no_n64_optimizations, 'render_debug': args.render_debug,
        'plan_completed': False, 'captures': [], 'psp_hardware_used': False}
    debugger = None
    start = time.monotonic()
    with (folder / 'process.log').open('w') as log:
        child_env = dict(os.environ)
        for key in ['PPSSPP_DUMP_AV', 'PPSSPP_DUMP_AUDIO', 'PPSSPP_PSP_MODEL', 'PPSSPP_FRAMESKIP', 'PPSSPP_MEMSTICK']:
            child_env.pop(key, None)
        proc = subprocess.Popen(command, cwd=game, env=child_env, stdout=log, stderr=subprocess.STDOUT)
        watchdog = threading.Timer(args.wall_seconds + 3, proc.kill)
        watchdog.daemon = True
        watchdog.start()
        try:
            deadline = start + min(10, args.wall_seconds)
            port = None
            while time.monotonic() < deadline and proc.poll() is None:
                output = (folder / 'process.log').read_text(errors='replace')
                match = re.search(r'Listening on port (\d+)', output)
                if match:
                    port = int(match.group(1)); break
                time.sleep(0.05)
            if port is None:
                raise RuntimeError('Debugger não anunciou a porta. Consulte process.log.')
            debugger = Debugger(port, proc.pid, args.wall_seconds + 3)
            result['version'] = debugger.version
            initial = debugger.request('cpu.status')
            for step in steps:
                if step['action'] == 'wait': debugger.run(step['seconds'])
                elif step['action'] == 'press': debugger.press(step['button'], step.get('seconds', 0.08), step.get('settle', 0.12))
                elif step['action'] == 'analog': debugger.analog(step['x'], step['y'], step.get('seconds', 0.12), step.get('settle', 0.3))
                elif step['action'] == 'expect_log':
                    if step['text'] not in (folder / 'process.log').read_text(errors='replace'):
                        raise RuntimeError('Marcador esperado ausente: ' + step['text'])
                elif step['action'] == 'compare':
                    from compare_frames import compare
                    metrics=compare(folder / (step['left'] + '.png'), folder / (step['right'] + '.png'), step.get('crop'))
                    result.setdefault('comparisons', []).append({'left':step['left'], 'right':step['right'], **metrics})
                    if metrics['mse']>step.get('max_mse', 0): raise RuntimeError('Comparação de imagens falhou: ' + str(metrics))
                else:
                    result['captures'].append({'name': step['name'], **debugger.screenshot(folder / (step['name'] + '.png'))})
            final = debugger.request('cpu.status')
            result['emulated_seconds'] = (final['us'] - initial['us']) / 1_000_000
            result['scenario_wall_seconds'] = time.monotonic() - start
            result['speed_including_boot'] = result['emulated_seconds'] / result['scenario_wall_seconds']
            result['plan_completed'] = True
            # Let the emulated deadline close PPSSPP normally and finish the WAV header.
            debugger.ws.send(json.dumps({'event': 'cpu.resume', 'ticket': 'finish'}))
            proc.wait(timeout=max(1, args.wall_seconds - (time.monotonic() - start) + 3))
            if proc.returncode != 0: raise RuntimeError(f'PPSSPP retornou {proc.returncode}.')
            if args.render_debug is not None and f'Frog render diagnostics: {args.render_debug}\n' not in (folder / 'process.log').read_text(errors='replace'):
                raise RuntimeError('A máscara de diagnóstico não foi confirmada pelo EBOOT.')
        except Exception as error:
            result['error'] = str(error)
            if proc.poll() is None:
                proc.terminate()
                try: proc.wait(timeout=3)
                except subprocess.TimeoutExpired: proc.kill(); proc.wait()
        finally:
            watchdog.cancel()
            if debugger:
                debugger.ws.close()
                (folder / 'debugger.json').write_text(json.dumps(debugger.transcript, indent=2))
            result['exit_code'] = proc.returncode
            result['wall_seconds'] = time.monotonic() - start
            (folder / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({'folder': str(folder), **result}, indent=2))
    return 0 if result['plan_completed'] and 'error' not in result else 1


if __name__ == '__main__':
    sys.exit(main())
