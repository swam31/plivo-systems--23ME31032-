#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>


static uint8_t payload_history[65536][160] = {};

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (::bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char harness_buf[2048];
    uint32_t bytes_sent = 0;
    uint32_t bytes_allowed = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, harness_buf, sizeof harness_buf, 0, NULL, NULL);
        if (n < 164) continue;

        uint32_t seq32;
        memcpy(&seq32, harness_buf, 4);
        seq32 = ntohl(seq32);
        
        uint16_t seq_idx = (uint16_t)seq32;
        memcpy(payload_history[seq_idx], harness_buf + 4, 160);

        bytes_allowed += 316;

        unsigned char wire_buf[400];
        uint8_t net_seq = (uint8_t)(seq32 & 0xFF);
        wire_buf[0] = net_seq;
        memcpy(wire_buf + 1, payload_history[seq_idx], 160);

        if (seq32 == 0) {
            sendto(out_fd, wire_buf, 161, 0, (struct sockaddr *)&relay, sizeof relay);
            bytes_sent += 161;
        } else {
            if (bytes_sent + 321 <= bytes_allowed) {
                uint16_t prev_idx = (uint16_t)(seq32 - 1);
                memcpy(wire_buf + 161, payload_history[prev_idx], 160);
                
                sendto(out_fd, wire_buf, 321, 0, (struct sockaddr *)&relay, sizeof relay);
                bytes_sent += 321;
            } else {
                sendto(out_fd, wire_buf, 161, 0, (struct sockaddr *)&relay, sizeof relay);
                bytes_sent += 161;
            }
        }
    }
    return 0;
}
