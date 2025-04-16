import time

def get_udp_socket_buffer_usage(port):
    try:
        with open("/proc/net/udp", "r") as f:
            lines = f.readlines()[1:]  # skip header
            for line in lines:
                parts = line.split()
                local_address = parts[1]
                queue = parts[4]  # tx_queue:rx_queue
                local_port = int(local_address.split(":")[1], 16)
                if local_port == port:
                    rx_queue = int(queue.split(":")[1], 16)
                    return rx_queue
    except Exception as e:
        print(f"⚠️ 無法讀取 /proc/net/udp: {e}")
    return None

def monitor_udp_port(port, interval=1):
    print(f"📡 開始監控 UDP port {port} 的接收 buffer 使用量")
    while True:
        usage = get_udp_socket_buffer_usage(port)
        if usage is not None:
            print(f"[{time.strftime('%H:%M:%S')}] UDP port {port} 的接收 buffer 使用量：約 {usage} bytes")
        else:
            print(f"[{time.strftime('%H:%M:%S')}] 找不到 UDP port {port} 的 buffer 使用資料")
        time.sleep(interval)

if __name__ == "__main__":
    monitor_udp_port(5005)
