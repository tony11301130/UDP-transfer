// udp_receiver_relay.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define UDP_PORT 5005
#define SOCK_PATH "/tmp/relay_input.sock"
#define CHUNK_SIZE 1024
#define MAX_PACKET_SIZE (CHUNK_SIZE + 100)

int main() {
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        perror("UDP socket");
        exit(1);
    }

    struct sockaddr_in udp_addr = {0};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);

    if (bind(udp_fd, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("UDP bind");
        exit(1);
    }

    int unix_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_fd < 0) {
        perror("UNIX socket");
        exit(1);
    }

    struct sockaddr_un dest = {0};
    dest.sun_family = AF_UNIX;
    strncpy(dest.sun_path, SOCK_PATH, sizeof(dest.sun_path) - 1);

    char buf[MAX_PACKET_SIZE];

    printf("C receiver running on UDP %d → UNIX socket %s\n", UDP_PORT, SOCK_PATH);
    while (1) {
        ssize_t len = recvfrom(udp_fd, buf, sizeof(buf), 0, NULL, NULL);
        if (len <= 0) continue;
        sendto(unix_fd, buf, len, 0, (struct sockaddr*)&dest, sizeof(dest));
    }
    return 0;
}
