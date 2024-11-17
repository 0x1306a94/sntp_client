
#ifndef _sntp_types_h_
#define _sntp_types_h_

#include <stdint.h>

#pragma pack(1)
struct li_vn_mode {
    unsigned int mode : 3;
    unsigned int vn : 3;
    unsigned int li : 2;
};

struct ntp_timestamp {
    uint32_t seconds;
    uint32_t fraction;
};

struct sntp_packet {
    struct li_vn_mode lvm;
    uint8_t stratum;
    uint8_t poll;
    int8_t precision;
    int16_t root_delay_int;
    uint16_t root_delay_fraction;
    int16_t root_dispersion_int;
    uint16_t root_dispersion_fraction;
    uint8_t ref_id[4];
    struct ntp_timestamp ref_time;
    struct ntp_timestamp ori_time;
    struct ntp_timestamp recv_time;
    struct ntp_timestamp tran_time;
};
#pragma pack()

#endif /* _sntp_types_h_ */
