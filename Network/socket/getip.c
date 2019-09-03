#include <unistd.h>  
#include <stdio.h>  
#include <string.h>  
#include <sys/socket.h>  
#include <sys/ioctl.h>  
#include <sys/stat.h>  
#include <netinet/in.h>  
#include <net/if.h>  
#include <arpa/inet.h>  
#include <ifaddrs.h>
  
int s_getIpAddress (const char * ifr, unsigned char * out) {  
    int sockfd;  
    struct ifreq ifrq;  
    struct sockaddr_in * sin;  
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  
    strcpy(ifrq.ifr_name, ifr);  
    if (ioctl(sockfd, SIOCGIFADDR, &ifrq) < 0) {  
        perror( "ioctl() SIOCGIFADDR error");  
        return -1;  
    }  
    sin = (struct sockaddr_in *)&ifrq.ifr_addr;  
    memcpy (out, (void*)&sin->sin_addr, sizeof(sin->sin_addr));  
  
    close(sockfd);  
  
    return 4;  
}  
  
int main(int argc, char * args[]) {  
  
    unsigned char addr[4] = {0,};  
	struct ifaddrs *addrs, *tmp;

	getifaddrs(&addrs);
	tmp = addrs;

	while (tmp) {
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {
			if (s_getIpAddress(tmp->ifa_name, addr) > 0) {  
				printf("ip addr:=%d.%d.%d.%d\n", (int)addr[0], (int)addr[1], (int)addr[2], (int)addr[3]);  
			}  
		}
		tmp = tmp->ifa_next;
	}
  
    return 0;  
}  
