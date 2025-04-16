# file_receiver_streaming_unix.py
import os
import socket
import hashlib
from datetime import datetime

CHUNK_SIZE = 1024
SOCK_PATH = "/tmp/udp_stream.sock"
RECEIVE_DIR = "/home/tony/code/file-transfer/received"
LOG_DIR = "./logs"
IDLE_TIMEOUT = 10  # seconds

file_buffers = {}

os.makedirs(RECEIVE_DIR, exist_ok=True)
os.makedirs(LOG_DIR, exist_ok=True)
if os.path.exists(SOCK_PATH):
    os.remove(SOCK_PATH)

def log(info, msg):
    ts = datetime.now().strftime("%H:%M:%S")
    line = f"[{ts}] {msg}"
    print(line)
    if info and 'log_path' in info:
        with open(info['log_path'], 'a') as f:
            f.write(line + "\n")

def finalize_and_verify(info):
    tmp = info['tmp_path']
    real = os.path.join(RECEIVE_DIR, info['real_name'])
    info['file'].close()
    os.rename(tmp, real)
    sha256 = hashlib.sha256(open(real, 'rb').read()).hexdigest()
    if sha256 == info['hash']:
        log(info, f"✅ File '{info['real_name']}' verified.")
    else:
        log(info, f"❌ Hash mismatch for '{info['real_name']}'!\n  Expected: {info['hash']}\n  Got     : {sha256}")

def report_missing_chunks(info):
    missing = sorted(set(range(info['total'])) - info['received_chunks'])
    if missing:
        log(info, f"❌ Missing chunks: {missing}")
    else:
        log(info, "✅ All chunks received successfully.")

def flush_buffered_chunks(info):
    f = info['file']
    while info['expected_seq'] in info['buffer']:
        data = info['buffer'].pop(info['expected_seq'])
        f.seek(info['expected_seq'] * CHUNK_SIZE)
        f.write(data)
        info['received_chunks'].add(info['expected_seq'])
        info['expected_seq'] += 1

def handle_packet(packet):
    packet_type = packet[0]
    if packet_type == 0x00:
        file_id = packet[1:5]
        total = int.from_bytes(packet[5:9], 'big')
        name_len = packet[9]
        filename = packet[10:10 + name_len].decode()
        sha256 = packet[10 + name_len:].decode()
        tmp_path = os.path.join(RECEIVE_DIR, filename + ".tmp")
        now = datetime.now()
        log_path = os.path.join(LOG_DIR, f"{filename}-{now.strftime('%H%M%S')}.log")
        f = open(tmp_path, 'wb')
        file_buffers[file_id] = {
            'file': f,
            'tmp_path': tmp_path,
            'real_name': filename,
            'hash': sha256,
            'received_chunks': set(),
            'expected_seq': 0,
            'buffer': {},
            'last_update': now,
            'log_path': log_path,
            'total': total
        }
        log(file_buffers[file_id], f"📄 Metadata: {filename}, chunks={total}")
    elif packet_type == 0x01:
        file_id = packet[1:5]
        seq = int.from_bytes(packet[5:9], 'big')
        data = packet[9:]
        info = file_buffers.get(file_id)
        if not info:
            log(None, f"⚠️ Unknown file_id {file_id.hex()}")
            return
        info['last_update'] = datetime.now()
        if seq in info['received_chunks'] or seq in info['buffer']:
            return
        if seq >= info['total']:
            return
        info['buffer'][seq] = data
        flush_buffered_chunks(info)
        if len(info['received_chunks']) == info['total']:
            report_missing_chunks(info)
            finalize_and_verify(info)
            del file_buffers[file_id]

def main():
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.bind(SOCK_PATH)
    print(f"🟢 Python listener on {SOCK_PATH}")
    sock.settimeout(1.0)
    while True:
        try:
            packet, _ = sock.recvfrom(CHUNK_SIZE + 100)
            handle_packet(packet)
        except socket.timeout:
            now = datetime.now()
            for fid in list(file_buffers):
                info = file_buffers[fid]
                if (now - info['last_update']).total_seconds() > IDLE_TIMEOUT:
                    log(info, f"⏳ Idle timeout: {info['real_name']}")
                    report_missing_chunks(info)
                    finalize_and_verify(info)
                    del file_buffers[fid]

if __name__ == "__main__":
    main()
