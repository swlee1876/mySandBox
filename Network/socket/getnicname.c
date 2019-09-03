#include <sys/types.h>
#include <ifaddrs.h>
#include <stdio.h>
 
int main(void){
    struct ifaddrs *addrs,*tmp;
 
    getifaddrs(&addrs);
    tmp = addrs;
 
    while (tmp)
    {   
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET)
                        printf("%s\n", tmp->ifa_name);
 
                tmp = tmp->ifa_next;
    }   
 
    freeifaddrs(addrs);
 
    return 0;
}
