#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <iostream>

using namespace std;

static uint8_t payload_history[65536][160];

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
        
        uint16_t seq = (uint16_t)seq32;
        memcpy(payload_history[seq], harness_buf + 4, 160);

        bytes_allowed += 316;

        unsigned char wire_buf[400];
        uint8_t net_seq = (uint8_t)(seq & 0xFF);
        wire_buf[0] = net_seq;
        memcpy(wire_buf + 1, payload_history[seq], 160);

        if (seq == 0) {
            sendto(out_fd, wire_buf, 161, 0, (struct sockaddr *)&relay, sizeof relay);
            bytes_sent += 161;
        } else if (seq == 1) {
            if (bytes_sent + 321 <= bytes_allowed) {
                memcpy(wire_buf + 161, payload_history[0], 160);
                sendto(out_fd, wire_buf, 321, 0, (struct sockaddr *)&relay, sizeof relay);
                bytes_sent += 321;
            } else {
                sendto(out_fd, wire_buf, 161, 0, (struct sockaddr *)&relay, sizeof relay);
                bytes_sent += 161;
            }
        } else {
            if (bytes_sent + 321 <= bytes_allowed) {
                for (int i = 0; i < 160; i++) {
                    wire_buf[161 + i] = payload_history[seq - 1][i] ^ payload_history[seq - 2][i];
                }
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
