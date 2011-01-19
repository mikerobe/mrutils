#include "mr_prog.h"
#include "mr_sockets.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#ifdef __APPLE__
#include <net/if_types.h>
#include <net/if_dl.h>
#endif

namespace mrutils {

    #ifdef __APPLE__
        bool getMACAddress(char * out, const char * interface) {
            struct ifaddrs * ifaddrs_ptr;
            int status = getifaddrs(&ifaddrs_ptr);
            if (status != 0) return false;

            status = -1;

            for(struct ifaddrs * ptr = ifaddrs_ptr;;) {
                if (0 == strcmp(ptr->ifa_name,interface)) {
                    if (ptr->ifa_addr->sa_family == AF_LINK) {
                        struct sockaddr_dl * sdl
                            = (struct sockaddr_dl *) ptr->ifa_addr;

                        if (sdl->sdl_type == IFT_ETHER) {
                            const char * mac_address = (LLADDR(sdl));

                            char * p = out;

                            int mac_addr_offset;
                            for (mac_addr_offset = 0; mac_addr_offset < 6; mac_addr_offset++) {
                                sprintf (p,"%02x", (unsigned char) mac_address[mac_addr_offset]);
                                p += 2;
                            }

                            status = 0;
                        }
                    }
                } 

                if (ptr->ifa_next == NULL) break;
                ptr = ptr->ifa_next;
            }

            freeifaddrs(ifaddrs_ptr);
            return (status==0);
        }
    #elif defined(__linux__) || defined(__CYGWIN__)
        bool getMACAddress(char * out, const char * interface) {

            int fd = socket(PF_INET, SOCK_STREAM, 0); /* open socket */

            struct ifreq ifr;
            strcpy(ifr.ifr_name, interface);
            ioctl(fd, SIOCGIFHWADDR, &ifr); /* retrieve MAC address */

            uint8_t * p = (uint8_t *) ifr.ifr_hwaddr.sa_data;

            sprintf(out,"%02x%02x%02x%02x%02x%02x"
                ,(int)p[0]
                ,(int)p[1]
                ,(int)p[2]
                ,(int)p[3]
                ,(int)p[4]
                ,(int)p[5]
                );

            close(fd);
            return true;
        }

    #endif

}
