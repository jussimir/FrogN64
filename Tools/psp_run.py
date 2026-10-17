#!/usr/bin/env python3
"""Run this fork through the existing PSPLink server and shared hardware lock.

The NFSU2 controller supplies only the shared lock and pspsh executable. Its
game loader, volume pokes and assets are deliberately not used for FrogN64.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
STATE = ROOT / 'dist/psplink/session.json'
OWNER = f'FrogN64: {ROOT}'


def shared_controller(path):
    spec = importlib.util.spec_from_file_location('shared_psp_run', path / 'tools/psp_run.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def command(shared, text, folder=None, timeout=12):
    result = subprocess.run([str(shared.p.BIN / 'pspsh'), '-n', '-e', text],
                            capture_output=True, timeout=timeout)
    answer = (result.stdout + result.stderr).decode('utf-8', errors='replace')
    if folder:
        with (folder / 'commands.log').open('a') as log:
            log.write(f'\n[{time.strftime("%Y-%m-%dT%H:%M:%S%z")}] {text}\n{answer}\n')
    print(f'$ {text}\n{answer}', flush=True)
    if result.returncode:
        raise RuntimeError(f'pspsh retornou {result.returncode}: {text}')
    return answer


def quote_path(path):
    if any(ch in path for ch in ['"', '\\', '\n', '\r', '\0']):
        raise ValueError('Caminho PSP contém caracteres não suportados.')
    return '"' + path + '"'


def read_state(shared):
    state = json.loads(STATE.read_text())
    lock = shared.lock_read()
    if not lock or lock.get('owner') != OWNER:
        raise RuntimeError('Esta sessão não possui a trava compartilhada do PSP.')
    return state, Path(state['folder'])


def capture_tty(port, path, ready, stop, errors):
    try:
        with socket.create_connection(('127.0.0.1', port), timeout=4) as sock, path.open('wb') as log:
            sock.settimeout(0.25)
            ready.set()
            while not stop.is_set():
                try:
                    data = sock.recv(65536)
                except socket.timeout:
                    continue
                if not data:
                    break
                log.write(data)
                log.flush()
    except OSError as error:
        errors.append(str(error))
    finally:
        ready.set()


def screenshot(shared, state, folder):
    filename = f'screen-{time.strftime("%H%M%S")}.bmp'
    command(shared, f'scrshot {state["remote"]}/{filename}', folder, timeout=30)
    source = folder / 'game' / filename
    if not source.is_file():
        raise RuntimeError('PSPLink não produziu o screenshot.')
    target = folder / (source.stem + '.png')
    subprocess.run(['sips', '-s', 'format', 'png', str(source), '--out', str(target)],
                   check=True, capture_output=True)
    print(target)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=['inspect', 'start', 'status', 'probe', 'shot', 'stop', 'recover-directory'])
    ap.add_argument('--shared-root', type=Path, default=ROOT.parent / 'pessoal-nfsu2-psp')
    ap.add_argument('--host-root', type=Path)
    ap.add_argument('--tag', default=time.strftime('%Y%m%d-%H%M%S'))
    ap.add_argument('--list-path', help='probe/inspect: list one PSP directory')
    ap.add_argument('--rom-directory', help='start: initial ROM browser folder (no ROMs copied)')
    ap.add_argument('--rom', help='start: load a ROM directly from a PSP path')
    a = ap.parse_args()
    shared = shared_controller(a.shared_root)

    if a.action == 'recover-directory':
        shared.lock_take('hardware-info', OWNER)
        try:
            command(shared, 'cd host0:/')
            command(shared, 'pwd')
            command(shared, 'modlist')
        finally:
            shared.lock_free()
        return

    if a.action == 'status':
        info = shared.lock_read()
        print(json.dumps(info, indent=2))
        if not shared.lock_alive(info) or info.get('owner') == OWNER:
            command(shared, 'modlist')
        return

    if a.action == 'inspect':
        shared.lock_take('hardware-info', OWNER)
        try:
            for text in ['pspver', 'ver', 'meminfo', 'modlist', 'pwd', 'ls ms0:/PSP/GAME']:
                command(shared, text)
            if a.list_path:
                command(shared, 'ls ' + quote_path(a.list_path))
        finally:
            shared.lock_free()
        return

    if a.action == 'start':
        if a.rom:
            quote_path(a.rom)
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', a.tag):
            raise ValueError('Tag inválida.')
        if not a.host_root or not a.host_root.is_dir():
            raise ValueError('Informe --host-root com a raiz já servida pelo usbhostfs_pc.')
        shared.lock_take('start', OWNER)
        launched = False
        try:
            modules = command(shared, 'modlist')
            if 'UID' not in modules or 'NFSU2 PSP Viewer' in modules or 'DaedalusX64' in modules or 'FrogN64' in modules:
                raise RuntimeError('PSPLink não está livre para carregar o FrogN64.')
            folder = ROOT / 'dist/psplink' / a.tag
            folder.mkdir(parents=True, exist_ok=False)
            game = folder / 'game'
            shutil.copytree(ROOT / 'dist/package/PSP/GAME/FrogN64', game)
            if a.rom_directory:
                quote_path(a.rom_directory)
                (game / 'frogn64-directory.txt').write_text(a.rom_directory + '\n')
            prx = (ROOT / 'build/Source/daedalus.prx').read_bytes()
            pbp = (game / 'EBOOT.PBP').read_bytes()
            offsets = struct.unpack_from('<8I', pbp, 8)
            if pbp[offsets[6]:offsets[7]] != prx:
                raise RuntimeError('O PRX não corresponde ao EBOOT empacotado.')
            (game / 'run.prx').write_bytes(prx)
            shutil.copy2(ROOT / 'build/Source/daedalus', folder / 'daedalus.elf')
            build_info = (game / 'BUILD-INFO.txt').read_text()
            match = re.search(r'^Revision: ([0-9a-f]{40})$', build_info, re.MULTILINE)
            if not match:
                raise RuntimeError('O pacote não informa sua revisão de build.')
            link = a.host_root / ('pspn64-' + a.tag)
            link.symlink_to(game.resolve())
            state = {'owner': OWNER, 'folder': str(folder), 'host_link': str(link),
                     'remote': 'host0:/' + link.name,
                     'revision': match.group(1),
                     'controller_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                     'prx_sha256': hashlib.sha256(prx).hexdigest(), 'started_at': time.time(),
                     'rom': a.rom, 'audio': 'upstream default (disabled on PSP)'}
            STATE.parent.mkdir(parents=True, exist_ok=True)
            STATE.write_text(json.dumps(state, indent=2))
            (folder / 'session.json').write_text(json.dumps(state, indent=2))
            for text in ['pspver', 'ver', 'meminfo', 'pwd']:
                command(shared, text, folder)
            command(shared, 'cd ' + state['remote'], folder)
            if state['remote'] not in command(shared, 'pwd', folder):
                raise RuntimeError('PSPLink não confirmou o diretório isolado.')
            stop = threading.Event()
            threads = []
            errors = []
            try:
                for port, name in [(10002, 'stdout.log'), (10003, 'stderr.log')]:
                    ready = threading.Event()
                    thread = threading.Thread(target=capture_tty, args=(port, folder / name, ready, stop, errors))
                    thread.start()
                    threads.append(thread)
                    if not ready.wait(5) or errors:
                        raise RuntimeError(f'Não foi possível conectar a captura de logs: {errors}')
                launched = True
                load = state['remote'] + '/run.prx'
                if a.rom:
                    load += ' ' + quote_path(a.rom)
                command(shared, load, folder, timeout=30)
                time.sleep(8)
                for text in ['modlist', 'meminfo', 'exprint', 'power', 'thlist']:
                    command(shared, text, folder)
                screenshot(shared, state, folder)
            finally:
                stop.set()
                for thread in threads:
                    thread.join(timeout=2)
            print(f'Sessão ativa; evidências: {folder}. Use stop para liberar o PSP.')
        except BaseException:
            # Once load was attempted, keep ownership until an explicit reset.
            if not launched:
                shared.lock_free()
            raise
        return

    state, folder = read_state(shared)
    if a.action == 'shot':
        screenshot(shared, state, folder)
    elif a.action == 'probe':
        for text in ['modlist', 'meminfo', 'exprint', 'power', 'thlist']:
            command(shared, text, folder)
        if a.list_path:
            command(shared, 'ls ' + quote_path(a.list_path), folder)
    elif a.action == 'stop':
        try:
            command(shared, 'reset', folder, timeout=6)
        except (subprocess.TimeoutExpired, RuntimeError):
            print('Reset enviado; aguardando reconexão sem reenviar reset.')
        # Wait for the USB reconnect once; never resend reset during recovery.
        time.sleep(5)
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            try:
                answer = command(shared, 'modlist', folder, timeout=3)
                if 'UID' in answer and 'DaedalusX64' not in answer and 'FrogN64' not in answer:
                    command(shared, 'cd host0:/', folder)
                    if 'host0:/' not in command(shared, 'pwd', folder):
                        raise RuntimeError('O diretório do shell não foi restaurado.')
                    Path(state['host_link']).unlink(missing_ok=True)
                    (folder / 'stop.json').write_text(json.dumps({'stopped_at': time.time(), 'psplink_reconnected': True}, indent=2))
                    shared.lock_free()
                    STATE.unlink(missing_ok=True)
                    print('PSPLink pronto, sessão liberada.')
                    return
            except (subprocess.TimeoutExpired, RuntimeError):
                pass
            time.sleep(5)
        raise RuntimeError('PSPLink não reconectou. Trava preservada para recuperação.')


if __name__ == '__main__':
    main()
