#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include "../headers/raw.h"
#include "../headers/common.h"


#define PROTO_UDP	17
#define DST_PORT	8000
#define INTERFACE_NAME "wlp3s0"

char this_mac[6];
char bcast_mac[6] =	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char dst_mac[6] =	{0x00, 0x00, 0x00, 0x22, 0x22, 0x22};
char src_mac[6] =	{0x00, 0x00, 0x00, 0x33, 0x33, 0x33};

union eth_buffer buffer_u;

/* Toda estrutura entre @@@@@@@ deve sair daqui e ser dividida entre common.h e common.c */

// @@@@@@@
struct ifreq if_idx, if_mac, ifopts;
char ifName[] = INTERFACE_NAME;
struct sockaddr_ll socket_address;
int sockFD, numbytes;


/* Get the index of the interface */
void getInterfIndex(int sockFD, char *ifName){
    //struct ifreq if_idx;
    memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockFD, SIOCGIFINDEX, &if_idx) < 0){
        perror("SIOCGIFINDEX");
    }
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	socket_address.sll_halen = ETH_ALEN;
}

/* Get the MAC address of the interface */
void getMacAddrInterf(int sockFD, char *ifName){
    memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockFD, SIOCGIFHWADDR, &if_mac) < 0){
        perror("SIOCGIFHWADDR");
    }
		
	memcpy(this_mac, if_mac.ifr_hwaddr.sa_data, 6);
}

/* Calcule ip checksum */
uint32_t ipchksum(uint8_t *packet)
{
	uint32_t sum=0;
	uint16_t i;

	for(i = 0; i < 20; i += 2)
		sum += ((uint32_t)packet[i] << 8) | (uint32_t)packet[i + 1];
	while (sum & 0xffff0000)
		sum = (sum & 0xffff) + (sum >> 16);
	return sum;
}
// @@@@@@@

int sendMessage(char *message){
    

	uint8_t msg[strlen(message)];
	memcpy(msg, message, strlen(message)+1); // para incluir o terminador \0 da string

	/* Start configuration of socket and network*/

	/* Open RAW socket */
	sockFD = openRawSocket();

	/* Set interface to promiscuous mode */
	setIntPromMode(sockFD);

	/* Get the index of the interface */
	getInterfIndex(sockFD, ifName);

    /* Get the MAC address of the interface */
    getMacAddrInterf(sockFD, ifName);

	/* End of configuration. Now we can send data using raw sockets. */

	/* Fill the Ethernet frame header */
	memcpy(buffer_u.cooked_data.ethernet.dst_addr, bcast_mac, 6);
	memcpy(buffer_u.cooked_data.ethernet.src_addr, src_mac, 6);
	buffer_u.cooked_data.ethernet.eth_type = htons(ETH_P_IP);

	/* Fill IP header data. Fill all fields and a zeroed CRC field, then update the CRC! */
	buffer_u.cooked_data.payload.ip.ver = 0x45;
	buffer_u.cooked_data.payload.ip.tos = 0x00;
	buffer_u.cooked_data.payload.ip.len = htons(sizeof(struct ip_hdr) + sizeof(struct udp_hdr) + strlen(msg));
	buffer_u.cooked_data.payload.ip.id = htons(0x00);
	buffer_u.cooked_data.payload.ip.off = htons(0x00);
	buffer_u.cooked_data.payload.ip.ttl = 50;
	buffer_u.cooked_data.payload.ip.proto = 17; //0xff;
	buffer_u.cooked_data.payload.ip.sum = htons(0x0000);

	buffer_u.cooked_data.payload.ip.src[0] = 192;
	buffer_u.cooked_data.payload.ip.src[1] = 168;
	buffer_u.cooked_data.payload.ip.src[2] = 5;
	buffer_u.cooked_data.payload.ip.src[3] = 25;
	buffer_u.cooked_data.payload.ip.dst[0] = 192;
	buffer_u.cooked_data.payload.ip.dst[1] = 168;
	buffer_u.cooked_data.payload.ip.dst[2] = 6;
	buffer_u.cooked_data.payload.ip.dst[3] = 6;
	buffer_u.cooked_data.payload.ip.sum = htons((~ipchksum((uint8_t *)&buffer_u.cooked_data.payload.ip) & 0xffff));

	/* Fill UDP header */
	buffer_u.cooked_data.payload.udp.udphdr.src_port = htons(555);
	buffer_u.cooked_data.payload.udp.udphdr.dst_port = 16415;
	buffer_u.cooked_data.payload.udp.udphdr.udp_len = htons(sizeof(struct udp_hdr) + strlen(msg));
	buffer_u.cooked_data.payload.udp.udphdr.udp_chksum = 0;

	/* Fill UDP payload */
	memcpy(buffer_u.raw_data + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr), msg, strlen(msg));

	/* Send it.. */
	memcpy(socket_address.sll_addr, dst_mac, 6);
	if (sendto(sockFD, buffer_u.raw_data, sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr) + strlen(msg), 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
		printf("Send failed\n");
}
