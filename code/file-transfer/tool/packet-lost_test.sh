#!/bin/bash
echo "[INFO] Monitoring UDP packet receive errors..."
initial=$(netstat -su | grep "packet receive errors" | awk '{print $1}')
last_rx=$(netstat -su | grep "packets received" | awk '{print $1}')
echo "[INFO] Initial error count: $initial"

while true; do
    sleep 1
    current=$(netstat -su | grep "packet receive errors" | awk '{print $1}')
    rx_now=$(netstat -su | grep "packets received" | awk '{print $1}')
    pps=$((rx_now - last_rx))
    last_rx=$rx_now

    if [[ "$current" -gt "$initial" ]]; then
        timestamp=$(date +"%Y-%m-%d %H:%M:%S")
        echo -e "\033[0;31m[ALERT] [$timestamp] UDP packet receive errors increased: $initial → $current\033[0m"
        echo -e "\033[0;33m        ➜ Receive rate: ${pps} packets/sec\033[0m"
        initial=$current
    else
        echo "[INFO] $(date +%H:%M:%S) - No new errors, receive rate: ${pps} pps"
    fi
done
