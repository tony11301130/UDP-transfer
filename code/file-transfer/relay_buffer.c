// relay_buffer.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#define INPUT_PATH "/tmp/relay_input.sock"
#define OUTPUT_PATH "/tmp/udp_stream.sock"
#define CHUNK_SIZE 1024
#define MAX_PACKET_SIZE (CHUNK_SIZE + 100)
#define QUEUE_SIZE 65536

typedef struct {
    char data[MAX_PACKET_SIZE];
    ssize_t len;
} Packet;

Packet queue[QUEUE_SIZE];
int head = 0;
int tail = 0;
int count = 0;
long total_recv = 0;
long total_sent = 0;
long retry_count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int enqueue(const char *data, ssize_t len) {
    pthread_mutex_lock(&lock);
    if (count >= QUEUE_SIZE) {
        retry_count++;
        pthread_mutex_unlock(&lock);
        return -1;
    }
    if (len > MAX_PACKET_SIZE || len <= 0) {
        fprintf(stderr, "[ERROR] invalid packet length: %zd\n", len);
        pthread_mutex_unlock(&lock);
        return -1;
    }
    memset(queue[tail].data, 0, MAX_PACKET_SIZE);
    memcpy(queue[tail].data, data, len);
    queue[tail].len = len;
    tail = (tail + 1) % QUEUE_SIZE;
    count++;
    total_recv++;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
    return 0;
}

int dequeue(Packet *pkt) {
    pthread_mutex_lock(&lock);
    while (count == 0) {
        pthread_cond_wait(&cond, &lock);
    }
    *pkt = queue[head];
    head = (head + 1) % QUEUE_SIZE;
    count--;
    total_sent++;
    pthread_mutex_unlock(&lock);
    return 0;
}

void *receiver_thread(void *arg) {
    int recv_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (recv_fd < 0) {
        perror("socket (recv)");
        exit(1);
    }

    unlink(INPUT_PATH);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, INPUT_PATH, sizeof(addr.sun_path) - 1);

    if (bind(recv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("[INFO] 接收執行緒已啟動，綁定於 %s\n", INPUT_PATH);

    while (1) {
        char buf[MAX_PACKET_SIZE];
        ssize_t len = recvfrom(recv_fd, buf, sizeof(buf), 0, NULL, NULL);
        if (len <= 0) continue;
        enqueue(buf, len);
    }
    return NULL;
}

void *sender_thread(void *arg) {
    int send_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (send_fd < 0) {
        perror("socket (send)");
        exit(1);
    }

    unlink(OUTPUT_PATH);
    struct sockaddr_un dest = {0};
    dest.sun_family = AF_UNIX;
    strncpy(dest.sun_path, OUTPUT_PATH, sizeof(dest.sun_path) - 1);

    printf("[INFO] 傳送執行緒已啟動，目標為 %s\n", OUTPUT_PATH);

    while (1) {
        Packet pkt;
        dequeue(&pkt);

        if (pkt.len <= 0 || pkt.len > MAX_PACKET_SIZE) {
            fprintf(stderr, "[WARN] 跳過非法封包，長度 = %zd\n", pkt.len);
            continue;
        }

        sendto(send_fd, pkt.data, pkt.len, 0,
               (struct sockaddr*)&dest, sizeof(dest));
    }
    return NULL;
}

void *monitor_thread(void *arg) {
    while (1) {
        sleep(1);
        pthread_mutex_lock(&lock);
        int qlen = count;
        long recv = total_recv;
        long sent = total_sent;
        long retry = retry_count;
        pthread_mutex_unlock(&lock);

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[9];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

        printf("[MON] Queue: %d pkts | Total Recv: %ld | Sent: %ld | Retry: %ld\n",
               qlen, recv, sent, retry);
        fflush(stdout);
    }
    return NULL;
}

int main() {
    pthread_t t_recv, t_send, t_mon;
    pthread_create(&t_recv, NULL, receiver_thread, NULL);
    pthread_create(&t_send, NULL, sender_thread, NULL);
    pthread_create(&t_mon, NULL, monitor_thread, NULL);
    pthread_join(t_recv, NULL);
    pthread_join(t_send, NULL);
    pthread_join(t_mon, NULL);
    return 0;
}
