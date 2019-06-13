#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include "../headers/raw.h"

// #define PROTO_UDP	17
// #define DST_PORT	8000
#define INTERFACE_NAME "enp0s3"


/* Open RAW socket */
int openRawSocket(){
	int lSockFD;
	if ((lSockFD = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1){
		perror("socket");
	}
	return lSockFD;
}

/* Set interface to promiscuous mode */
void setIntPromMode(int pSockFD){
	struct ifreq ifopts;
	char ifName[] = INTERFACE_NAME;

	strncpy(ifopts.ifr_name, ifName, IFNAMSIZ-1);
	ioctl(pSockFD, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(pSockFD, SIOCSIFFLAGS, &ifopts);
}

