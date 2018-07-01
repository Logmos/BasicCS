#pragma once

enum {
    DO_SUM = 0,
    HEARTBEAT = 1,
    RET_SUM = 2, // for client to recv
    UNKNOWN = 3, // packet is incomplete 
    OVERFLOW = 4 // sum is overflow
};

struct packet_t {
    int id;
    int first_num;
    int second_num;
    unsigned short checksum;
};
