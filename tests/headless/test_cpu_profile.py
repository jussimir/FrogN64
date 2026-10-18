# SPDX-License-Identifier: GPL-2.0-or-later
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Tools'))
from cpu_profile import load_symbols


class ProfileSymbolsTests(unittest.TestCase):
    def test_assembly_labels_and_globals(self):
        listing = '''000b194c T _ReturnFromDynaRecIfStuffToDo
000b1954 T _ReturnFromDynaRec
000b1984 T _DirectExitCheckNoDelay
000b19b0 T _DirectExitCheckDelay
000b19e4 t _DirectExitCheckCheckCount
000b1a04 00000030 T sized_function
002999d0 00000004 B gRDPFrame
'''
        with patch('cpu_profile.subprocess.check_output', return_value=listing):
            symbols, addresses = load_symbols(Path('guest.elf'), Path('psp-nm'))
        spans = {name: (start, size) for start, size, name in symbols}
        self.assertEqual(spans['_DirectExitCheckNoDelay'], (0xb1984, 0x2c))
        self.assertEqual(spans['_DirectExitCheckCheckCount'], (0xb19e4, 0x20))
        self.assertEqual(spans['sized_function'], (0xb1a04, 0x30))
        self.assertEqual(addresses['gRDPFrame'], 0x2999d0)
        self.assertNotIn('gRDPFrame', spans)


if __name__ == '__main__': unittest.main()
