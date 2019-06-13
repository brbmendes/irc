#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <poll.h>
#include <ncurses.h>
#include <pthread.h>
#include "../headers/raw.h"
#include "../headers/common.h"


#define PROTO_UDP	17
#define DST_PORT	8000
#define INTERFACE_NAME "wlp3s0"

char this_mac[6];
char bcast_mac[6] =	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char dst_mac[6] =	{0x00, 0x00, 0x00, 0x22, 0x22, 0x22};
char src_mac[6] =	{0x00, 0x00, 0x00, 0x33, 0x33, 0x33};
uint8_t this_ip[4];

union eth_buffer buffer_u;
union eth_buffer buffer_r;

char messages[50][99] = {"Bem-vindo ao IRC!"};


/* Toda estrutura entre @@@@@@@ deve sair daqui e ser dividida entre common.h e common.c */

// @@@@@@@
struct ifreq if_idx, if_mac, ifopts;
char ifName[] = INTERFACE_NAME;
struct sockaddr_ll socket_address;
int sockfd, numbytes;

int pidChild;
int pidFather;

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

// Não está sendo utilizado
// converte o tipo uint8_t para o tipo uint32_t
static inline uint32_t stream2int(const uint8_t *stream) {
 
    return (((uint32_t) stream[0]) << 24 |
            ((uint32_t) stream[1]) << 16 |
            ((uint32_t) stream[2]) <<  8 |
            ((uint32_t) stream[3]) <<  0);
}

// Não está sendo utilizado
/* Calcula o checksum do pacote UDP */
uint16_t udp_checksum	(const void * buff, size_t len, in_addr_t src_addr, in_addr_t dest_addr){
	const uint16_t *buf=buff;
	uint16_t *ip_src=(void *)&src_addr, *ip_dst=(void *)&dest_addr;
	uint32_t sum;
	size_t length=len;

	// Calculate the sum                                            //
	sum = 0;
	while (len > 1)
	{
			sum += *buf++;
			if (sum & 0x80000000)
					sum = (sum & 0xFFFF) + (sum >> 16);
			len -= 2;
	}

	if ( len & 1 )
			// Add the padding if the packet lenght is odd          //
			sum += *((uint8_t *)buf);

	// Add the pseudo-header                                        //
	sum += *(ip_src++);
	sum += *ip_src;

	sum += *(ip_dst++);
	sum += *ip_dst;

	sum += htons(IPPROTO_UDP);
	sum += htons(length);

	// Add the carries                                              //
	while (sum >> 16)
			sum = (sum & 0xFFFF) + (sum >> 16);

	// Return the one's complement of sum                           //
	return ( (uint16_t)(~sum)  );
}
// @@@@@@@

int sendMessage(char* message) {

	/* Fill the Ethernet frame header */
	memcpy(buffer_u.cooked_data.ethernet.dst_addr, bcast_mac, 6);
	memcpy(buffer_u.cooked_data.ethernet.src_addr, src_mac, 6);
	buffer_u.cooked_data.ethernet.eth_type = htons(ETH_P_IP);

	
	/* Fill IP header data. Fill all fields and a zeroed CRC field, then update the CRC! */
	buffer_u.cooked_data.payload.ip.ver = 0x45;
	buffer_u.cooked_data.payload.ip.tos = 0x00;
	buffer_u.cooked_data.payload.ip.len = htons(sizeof(struct ip_hdr) + sizeof(struct udp_hdr) + strlen(message));
	buffer_u.cooked_data.payload.ip.id = htons(0x00);
	buffer_u.cooked_data.payload.ip.off = htons(0x00);
	buffer_u.cooked_data.payload.ip.ttl = 50;
	buffer_u.cooked_data.payload.ip.proto = PROTO_UDP;
	buffer_u.cooked_data.payload.ip.sum = htons(0x0000);

	buffer_u.cooked_data.payload.ip.dst[0] = 192;
	buffer_u.cooked_data.payload.ip.dst[1] = 168;
	buffer_u.cooked_data.payload.ip.dst[2] = 5;
	buffer_u.cooked_data.payload.ip.dst[3] = 10;
	
	buffer_u.cooked_data.payload.ip.src[0] = this_ip[0];
	buffer_u.cooked_data.payload.ip.src[1] = this_ip[1];
	buffer_u.cooked_data.payload.ip.src[2] = this_ip[2];
	buffer_u.cooked_data.payload.ip.src[3] = this_ip[3];
	
	/* calculate the IP checksum */
	buffer_u.cooked_data.payload.ip.sum = htons((~ipchksum((uint8_t *)&buffer_u.cooked_data.payload.ip) & 0xffff));

	/* fill payload data */
	uint8_t msg[strlen(message)];
	
	memcpy(msg, message, strlen(message)+1);

	/* Fill UDP payload */
	memcpy(buffer_u.raw_data + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr), msg, strlen(msg));

	/* Fill UDP header */
	buffer_u.cooked_data.payload.udp.udphdr.src_port = htons(555);
	buffer_u.cooked_data.payload.udp.udphdr.dst_port = htons(8000);
	buffer_u.cooked_data.payload.udp.udphdr.udp_len = htons(sizeof(struct udp_hdr) + strlen(msg));
	buffer_u.cooked_data.payload.udp.udphdr.udp_chksum = 0;
	//buffer_u.cooked_data.payload.udp.udphdr.udp_chksum = udp_checksum(&buffer_u.cooked_data.payload.udp,sizeof buffer_u.cooked_data.payload.udp.udphdr.udp_len, stream2int(buffer_u.cooked_data.payload.ip.src), stream2int(buffer_u.cooked_data.payload.ip.dst));

	/* Send it.. */
	memcpy(socket_address.sll_addr, dst_mac, 6);
	if (sendto(sockfd, buffer_u.raw_data, sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr) + strlen(msg), 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
		printf("Send failed\n");

	memset(buffer_u.raw_data + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr), 0, strlen(msg));
	return 0;
}

void *printThread() {
	while (1){
			
		numbytes = recvfrom(sockfd, buffer_r.raw_data, ETH_LEN, 0, NULL, NULL);
		
		if (buffer_r.cooked_data.ethernet.eth_type == ntohs(ETH_P_IP)
			&& buffer_r.cooked_data.payload.ip.proto == PROTO_UDP 
			&& buffer_r.cooked_data.payload.udp.udphdr.dst_port == htons(DST_PORT)
			&& memcmp(buffer_r.cooked_data.payload.ip.dst, this_ip, sizeof this_ip) == 0) {
				
			for (int i = 49; i >= 0; i--) {
				strcpy(messages[i + 1], messages[i]);
			}
			
			memset(messages[0], 0, sizeof messages[0]);
			strcpy(messages[0], 
					(char *)&buffer_r.cooked_data.payload.udp.udphdr + sizeof(struct udp_hdr));
		
			memset(buffer_r.raw_data + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr), 0, strlen(messages[0]));
		}
	}
}

void *inputThread() {
	
	int n = 0;
	
	char input[99];
	while (1) {
		int x, y;
		char b[10];
		getmaxyx(stdscr, y, x);
		
		for (int i = 0; i < y - 2; i++) {
			move(y - 2 - i, 0);
			clrtoeol();
			mvprintw(y - 2 - i, 0, messages[i]);
		}
			
		move(y - 1, 0);
		clrtoeol();
		mvprintw(y - 1, 0, input);
		
		char a = getch();
		
		if (a != ERR) {
			if (a == '\n') {
				
				sendMessage(input);
				
				if (strcmp(input, "/quit") == 0) {
					break;
				} else {
					n = 0;
					memset(input, 0, sizeof input);
					move(y - 1, 0);
					clrtoeol();				
				}
				
			} else if (a == 127) {
				if (n > 0) {
					input[--n] = 0;
				}	
			} else {
				input[n++] = a;
			}
		}
	}
}

int main (int argc, char *argv[])
{
	this_ip[0] = atoi(argv[1]);
	this_ip[1] = atoi(argv[2]);
	this_ip[2] = atoi(argv[3]);
	this_ip[3] = atoi(argv[4]);
	
	initscr();
	noecho();
	nodelay(stdscr, TRUE);

	/* Get interface name */
	//strcpy(ifName, DEFAULT_IF);
	//strcpy(ifName, "enp0s3");
	strcpy(ifName, argv[5]);

	/* Open RAW socket */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
		perror("socket");

	/* Set interface to promiscuous mode */
	strncpy(ifopts.ifr_name, ifName, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

	/* Get the index of the interface */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0)
		perror("SIOCGIFINDEX");
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	socket_address.sll_halen = ETH_ALEN;

	/* Get the MAC address of the interface */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0)
		perror("SIOCGIFHWADDR");
	memcpy(this_mac, if_mac.ifr_hwaddr.sa_data, 6);

	pthread_t thread_print_id;
	pthread_t thread_input_id;
	pthread_create(&thread_input_id, NULL, inputThread, NULL);
	pthread_create(&thread_print_id, NULL, printThread, NULL);

	pthread_join(thread_input_id, NULL);

	endwin();

	printf("Desconectado.\n");
    return 0;
}
