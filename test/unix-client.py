"""Independent local 9P peer: negotiation fallback and ixpc directory decoding.

Run from the repository root after make: python3 test/unix-client.py
"""
import socket
import struct
import subprocess
import tempfile
from pathlib import Path


def string(value):
    data = value.encode()
    return struct.pack('<H', len(data)) + data


def qid(directory=False):
    return struct.pack('<BIQ', 128 if directory else 0, 0, 1)


def stat(name, extended, directory=False):
    data = struct.pack('<HI', 0, 0) + qid(directory)
    data += struct.pack('<IIIQ', (0x80000000 if directory else 0) | 0o755, 0, 0, 0)
    data += string(name) + string('user') + string('group') + string('user')
    if extended:
        data += string('') + struct.pack('<III', 1000, 1000, 1000)
    return struct.pack('<H', len(data)) + data


def read_exact(conn, n):
    data = b''
    while len(data) < n:
        part = conn.recv(n - len(data))
        if not part:
            raise EOFError
        data += part
    return data


def check(mode):
    extended = mode == 'unix'
    with tempfile.TemporaryDirectory(prefix='ixp-client-') as tmp:
        path = str(Path(tmp) / 'socket')
        with socket.socket(socket.AF_UNIX) as listener:
            listener.bind(path)
            listener.listen(1)
            listener.settimeout(5)
            process = subprocess.Popen(['cmd/ixpc.out', '-a', 'unix!' + path, 'ls', '/'],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            versions = []
            try:
                conn, _ = listener.accept()
                with conn:
                    conn.settimeout(5)
                    while True:
                        try:
                            size, = struct.unpack('<I', read_exact(conn, 4))
                            packet = read_exact(conn, size - 4)
                        except EOFError:
                            break
                        kind, tag = struct.unpack_from('<BH', packet)
                        body = packet[3:]
                        if kind == 100:
                            versions.append(body[6:].decode())
                            version = '9P2000.u' if extended else '9P2000'
                            if mode == 'fallback' and len(versions) == 1:
                                version = 'unknown'
                            response = struct.pack('<I', 8192) + string(version)
                        elif kind == 104:
                            response = qid(True)
                        elif kind == 110:
                            response = struct.pack('<H', 0)
                        elif kind == 124:
                            data = stat('/', extended, True)
                            response = struct.pack('<H', len(data)) + data
                        elif kind == 112:
                            response = qid(True) + struct.pack('<I', 4096)
                        elif kind == 116:
                            offset, = struct.unpack_from('<Q', body, 4)
                            data = b'' if offset else stat('alpha', extended) + stat('beta', extended)
                            response = struct.pack('<I', len(data)) + data
                        elif kind == 120:
                            response = b''
                        else:
                            raise AssertionError('Unexpected request: ' + str(kind))
                        conn.sendall(struct.pack('<IBH', 7 + len(response), kind + 1, tag) + response)
                stdout, stderr = process.communicate(timeout=5)
                assert process.returncode == 0, stderr.decode()
                assert b'alpha' in stdout and b'beta' in stdout, stdout
                expected = ['9P2000.u', '9P2000'] if mode == 'fallback' else ['9P2000.u']
                assert versions == expected, versions
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()


for mode in ('unix', 'legacy', 'fallback'):
    check(mode)
print('UNIX client interoperability tests passed')
