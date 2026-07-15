#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <algorithm>

static bool present[65536] = {false};
static uint8_t payload[65536][160] = {};
static bool played[65536] = {false};

int out_fd;
struct sockaddr_in player_addr;

void send_to_player(uint32_t seq32, const uint8_t* data) {
    uint16_t seq = (uint16_t)seq32;
    if (played[seq]) return;
    
    unsigned char out_buf[164];
    uint32_t net_seq32 = htonl(seq32);
    memcpy(out_buf, &net_seq32, 4);
    memcpy(out_buf + 4, data, 160);
    sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player_addr, sizeof player_addr);
    played[seq] = true;
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (::bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    player_addr = {0};
    player_addr.sin_family = AF_INET;
    player_addr.sin_port = htons(47020);
    player_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    uint16_t latest_seq = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < 161) continue;

        uint8_t wire_seq = buf[0];
        
        uint16_t seq = (latest_seq & 0xFF00) | wire_seq;
        if (seq < latest_seq && latest_seq - seq > 128) seq += 256;
        else if (seq > latest_seq && seq - latest_seq > 128) seq -= 256;
        
        if (seq > latest_seq && seq - latest_seq < 30000) {
            latest_seq = seq;
        }

        uint32_t seq32 = (uint32_t)seq;

        if (!present[seq]) {
            memcpy(payload[seq], buf + 1, 160);
            present[seq] = true;
            send_to_player(seq32, payload[seq]);
        }

        if (n == 321 && seq32 >= 1) {
            uint32_t prev_seq32 = seq32 - 1;
            uint16_t prev_idx = (uint16_t)prev_seq32;
            if (!present[prev_idx]) {
                memcpy(payload[prev_idx], buf + 161, 160);
                present[prev_idx] = true;
                send_to_player(prev_seq32, payload[prev_idx]);
            }
        }
    }
    return 0;
}
