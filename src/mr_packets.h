#ifndef _MR_CPPLIB_PACKETS_H
#define _MR_CPPLIB_PACKETS_H

#include "mr_sockets.h"

namespace mrutils {
    namespace packets {

        template <class T> 
        inline char * baseconv(T num, int base) {
            static char retbuf[65];
            char *p;

            if(base < 2 || base > 16)
                return NULL;

            p = &retbuf[8*sizeof(num)];
            *p = '\0';

            do {
                *--p = "0123456789abcdef"[num % base];
                num /= base;
            } while(num != 0);

            switch (base) {
                case 2:
                    while (p != retbuf) *--p = '0';
                    break;
                case 16:
                    *--p = 'x';
                    *--p = '0';
                    break;
            }

            return p;
        }

        /* first, at the top of the file */
        typedef struct pcap_hdr_s {
                uint32_t magic_number;   /* magic number */
                uint16_t version_major;  /* major version number */
                uint16_t version_minor;  /* minor version number */
                uint32_t thiszone;       /* GMT to local correction */
                uint32_t sigfigs;        /* accuracy of timestamps */
                uint32_t snaplen;        /* max length of captured packets, in octets */
                uint32_t network;        /* data link type */
        } pcap_hdr_t;

        /* then, at the top of each packet */
        typedef struct pcaprec_hdr_s {
                uint32_t ts_sec;         /* timestamp seconds */
                uint32_t ts_usec;        /* timestamp microseconds */
                uint32_t incl_len;       /* number of octets of packet saved in file */
                uint32_t orig_len;       /* actual length of packet */
        } pcaprec_hdr_t;

        /* first, in the content of each packet */
        typedef struct ethernet_hdr_s {
                uint8_t  dst[6];
                uint8_t  src[6];
                uint16_t typ;
        } ethernet_hdr_t;

        static const int inet_prot_hdr_start = 0x0E;
        typedef struct inet_prot_hdr_s {
                uint8_t  hdr_len;
                uint8_t  dsfield;
                uint16_t len;
                uint16_t id;
                uint16_t flags;
                uint8_t  timetolive;
                uint8_t  protocol;
                uint16_t checksum;
                uint8_t  sourceIP[4];
                uint8_t  destIP[4];
        } inet_prot_hdr_t;

        static const int udp_hdr_start = 0x22;
        typedef struct udp_hdr_s {
                uint16_t sourcePort;
                uint16_t destPort;
                uint16_t len;
                uint16_t checksum;
        } udp_hdr_t;

        static const int data_start = 0x2A;

        inline uint16_t checksum_inet(const inet_prot_hdr_t * inet_hdr) {
            const uint8_t * p = (const uint8_t*) inet_hdr;
            uint16_t len = 20;
            unsigned sum = 0;

            // build the sum of 16bit words
            while(len>1) {
                sum += 0xFFFF & (*p<<8|*(p+1));
                p+=2;
                len-=2;
            }
            // if there is a byte left then add it (padded with zero)
            if (len) {
                sum += (0xFF & *p)<<8;
            }

            // now calculate the sum over the bytes in the sum
            // until the result is only 16bit long
            while (sum>>16) {
                sum = (sum & 0xFFFF)+(sum >> 16);
            }

            // build 1's complement:
            return htons( (uint16_t) sum ^ 0xFFFF);
        }

        inline uint16_t checksum_udp(const uint16_t * udp_hdr, size_t len, uint8_t* src_addr, uint8_t* dest_addr) {
            uint16_t *ip_src=(uint16_t *)src_addr, *ip_dst=(uint16_t *)dest_addr;
            uint32_t sum;
            size_t length=len;

            sum = 0;
            while (len > 1) {
                sum += *udp_hdr++;
                if (sum & 0x80000000)
                    sum = (sum & 0xFFFF) + (sum >> 16);
                len -= 2;
            }

            if ( len & 1 )
                sum += *((uint8_t *)udp_hdr);

            sum += *(ip_src++);
            sum += *ip_src;

            sum += *(ip_dst++);
            sum += *ip_dst;

            sum += htons(0x11);
            sum += htons(length);

            while (sum >> 16)
                sum = (sum & 0xFFFF) + (sum >> 16);

            return ( (uint16_t)(~sum)  );
        }


    }
}

#endif
