#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <ifaddrs.h>

int  s_getIpAddress (unsigned char * out) {
	struct ifaddrs *addrs, *tmp;
    int sockfd;
    struct ifreq ifrq;
    struct sockaddr_in * sin;
	char *ifr = NULL;
	getifaddrs(&addrs);
    tmp = addrs;

	 while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {
			if (strcasecmp(tmp->ifa_name, "lo") != 0) {
				ifr = tmp->ifa_name;
				break;
			}
        }
        tmp = tmp->ifa_next;
    }


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


int main(int argc, char **argv) {
	int handle;
	int opt;
	struct sockaddr_in this_sin;
	char *pAddr;
	int nPort;

	if (argc != 3) {
		printf("bad command : %s [ip] [port]\n", argv[0]);
		return -1;
	}

	pAddr = argv[1];
	nPort = atoi(argv[2]);

	printf("start send a packet 5 times (%s:%d)\n", pAddr, nPort);

	handle = socket(PF_INET, SOCK_DGRAM, 0);
	if (handle <= 0) {
		printf("failed to open socket : %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		printf("failed to set socket option : %s\n", strerror(errno));
		return -1;
	}

	memset(&this_sin , 0x00, sizeof(struct sockaddr_in));

	this_sin.sin_family = AF_INET;
	this_sin.sin_port = 32134;
	this_sin.sin_addr.s_addr = inet_addr("10.80.48.100");
	//this_sin.sin_addr.s_addr = htonl(inet_addr("10.80.48.100"));
	//this_sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(handle, &this_sin, sizeof(this_sin)) < 0) {
		printf("failed to bind socket : %s\n" , strerror(errno));
		return -1;
	}

	close(handle);
	return 0;
}
