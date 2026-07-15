#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <algorithm>

using namespace std;

static bool present[65536] = {false};
static uint8_t payload[65536][160] = {0};
static bool played[65536] = {false};

static bool has_fec[65536] = {false};
static uint8_t fec[65536][160] = {0};

int out_fd;
struct sockaddr_in player_addr;

void send_to_player(uint16_t seq, const uint8_t* data) {
    if (played[seq]) return;
    
    unsigned char out_buf[164];
    uint32_t seq32 = htonl((uint32_t)seq);
    memcpy(out_buf, &seq32, 4);
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

        if (!present[seq]) {
            memcpy(payload[seq], buf + 1, 160);
            present[seq] = true;
            send_to_player(seq, payload[seq]);
        }

        if (n == 321 && !has_fec[seq]) {
            memcpy(fec[seq], buf + 161, 160);
            has_fec[seq] = true;
        }

        bool progress = true;
        while (progress) {
            progress = false;
            int start = std::max(2, (int)latest_seq - 100);
            for (int i = start; i <= latest_seq; i++) {
                if (!has_fec[i]) continue;

                if (i == 1) {
                    if (!present[0]) {
                        memcpy(payload[0], fec[1], 160);
                        present[0] = true;
                        send_to_player(0, payload[0]);
                        progress = true;
                    }
                } else if (i >= 2) {
                    if (present[i-1] && !present[i-2]) {
                        for (int j = 0; j < 160; j++) {
                            payload[i-2][j] = fec[i][j] ^ payload[i-1][j];
                        }
                        present[i-2] = true;
                        send_to_player(i-2, payload[i-2]);
                        progress = true;
                    } else if (!present[i-1] && present[i-2]) {
                        for (int j = 0; j < 160; j++) {
                            payload[i-1][j] = fec[i][j] ^ payload[i-2][j];
                        }
                        present[i-1] = true;
                        send_to_player(i-1, payload[i-1]);
                        progress = true;
                    }
                }
            }
        }
    }
    return 0;
}
