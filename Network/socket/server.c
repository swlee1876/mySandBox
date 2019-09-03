#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

int main(int argc, char **argv) {
	int handle;
	int opt;
	struct sockaddr_in this_sin;

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
