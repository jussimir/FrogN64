# SPDX-License-Identifier: GPL-2.0-or-later
import struct
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'Tools'))
from headless import rom_id


class RomHeaderTests(unittest.TestCase):
    def test_daedalus_id_for_all_byte_orders(self):
        header=bytearray(64)
        header[:4]=bytes.fromhex('80371240')
        struct.pack_into('>II',header,16,0x096a40ea,0x8abe0a10)
        header[62]=0x45
        versions=[header,b''.join(header[i:i+2][::-1] for i in range(0,64,2)),
                  b''.join(header[i:i+4][::-1] for i in range(0,64,4))]
        with tempfile.TemporaryDirectory() as folder:
            for i,data in enumerate(versions):
                path=Path(folder)/str(i);path.write_bytes(data)
                self.assertEqual(rom_id(path),'ea406a09100abe8a-45')

    def test_invalid_and_short_headers(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'bad'
            for data in [b'',b'\x80\x37\x12\x40',bytes(64)]:
                path.write_bytes(data)
                with self.assertRaises(ValueError):rom_id(path)

if __name__=='__main__':unittest.main()
