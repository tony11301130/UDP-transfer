# file_receiver_debug.py
import os
import socket
import binascii
from datetime import datetime

SOCK_PATH = "/tmp/udp_stream.sock"
MAX_LEN = 2048

if os.path.exists(SOCK_PATH):
    os.remove(SOCK_PATH)

sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
sock.bind(SOCK_PATH)

print(f"🔍 Debug Receiver listening on {SOCK_PATH}")
print("⏳ 等待封包中...\n")

while True:
    data, _ = sock.recvfrom(MAX_LEN)
    now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{now}] Received packet: {len(data)} bytes")
    print(binascii.hexlify(data).decode())
    print("-" * 60)
