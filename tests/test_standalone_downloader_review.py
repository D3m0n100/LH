"""Protocol and input regressions, independent of compiler implementation."""
import importlib.util
from pathlib import Path
import struct
import json
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'third_party/custom_dsp_language/compile/lh_download_v3.py'
spec = importlib.util.spec_from_file_location('downloader', SCRIPT)
downloader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(downloader)


def frame(payload):
    return payload + struct.pack('<H', downloader.modbus_crc16(payload))


class SerialStub:
    timeout = 0.1
    def __init__(self, response):
        self.response = response
    def reset_input_buffer(self):
        pass
    def write(self, request):
        return len(request)
    def read(self, size):
        # Fragmentation must be supported, including exception frames.
        chunk, self.response = self.response[:1], self.response[1:]
        return chunk


class DownloaderReviewTest(unittest.TestCase):
    def test_shared_protocol_vectors(self):
        vectors = json.loads((Path(__file__).with_name('protocol_golden_vectors.json')).read_text())
        for vector in vectors['crc16_modbus']:
            self.assertEqual(downloader.modbus_crc16(bytes.fromhex(vector['hex'])), vector['crc'])

    def client(self, response):
        client = downloader.ModbusRTU.__new__(downloader.ModbusRTU)
        client.device_addr = 65
        client.ser = SerialStub(response)
        return client

    def test_write_identity(self):
        valid = downloader.build_write_single(65, 0, 15)
        self.assertTrue(self.client(valid).write_register(40001, 15))
        for payload in [bytes.fromhex('42060000000f'), bytes.fromhex('41060001000f'),
                        bytes.fromhex('410600000010'), bytes.fromhex('41040000000f')]:
            self.assertFalse(self.client(frame(payload)).write_register(40001, 15))
        for response in [valid[:-1], valid[:-1] + b'\x00', frame(bytes.fromhex('418602'))]:
            self.assertFalse(self.client(response).write_register(40001, 15))

    def test_read_identity_and_exception(self):
        self.assertEqual(self.client(frame(bytes.fromhex('410302000f'))).read_register(40001), 15)
        for payload in ['420302000f', '410402000f', '410301000f', '418302']:
            self.assertIsNone(self.client(frame(bytes.fromhex(payload))).read_register(40001))
        client = self.client(frame(bytes.fromhex('418302')))
        self.assertIsNone(client.read_register(40001))
        self.assertEqual(client.last_error, 'Modbus exception 2')

    def test_deadline_is_total(self):
        client = self.client(frame(bytes.fromhex('410302000f')))
        with patch.object(downloader.time, 'monotonic', side_effect=[0, 0.01, 0.11]):
            self.assertIsNone(client.read_register(40001))
        self.assertEqual(client.ser.timeout, 0.1)

    def test_file_validation_before_serial_open(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.code'
            path.write_text('// comment\n40001 0 1\n40002 0 2 65535\n', encoding='ascii')
            self.assertEqual(downloader.parse_code_file(path), [(40001, 1), (40004, 65535)])
            for bad in ['INVALID', '49999 0 100000 1', '40001 0 -1 1',
                        '40001 0 70000', '40001 0 -1', '40000 0 1']:
                path.write_text('40001 0 1\n' + bad + '\n', encoding='ascii')
                with patch.object(downloader.serial, 'Serial') as serial_open:
                    with self.assertRaises(SystemExit):
                        downloader.download(str(path))
                    serial_open.assert_not_called()

    def test_no_implicit_value_truncation(self):
        for value in [-1, 65536, 70000]:
            with self.assertRaises(ValueError):
                downloader.build_write_single(65, 0, value)


if __name__ == '__main__':
    unittest.main()
