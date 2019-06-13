#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <signal.h>
#include <stdbool.h>
#include "../headers/raw.h"
#include "../headers/common.h"
#include "../headers/structures.h"

#define PROTO_UDP	17
#define DST_PORT	8000
#define MAXNICKS	50
#define MAXCHANNELS	50
#define MAXUSERS 500
#define SIZENICK 50
#define CHANNAME 30
#define SIZEMSG 300
#define INTERFACE_NAME "wlp3s0"

struct ifreq if_idx, if_mac, ifopts;
char ifName[] = INTERFACE_NAME;

char this_mac[6];
char bcast_mac[6] =	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char dst_mac[6] =	{0x00, 0x00, 0x00, 0x22, 0x22, 0x44};
char src_mac[6] =	{0x00, 0x00, 0x00, 0x33, 0x33, 0x44};
struct sockaddr_ll socket_address;


User users[MAXUSERS];
int user_number = 0;

Channel channels[MAXCHANNELS];
int channel_number = 0;

int sockfd;

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

// Envia a mensagem para um determinado IP
int sendMessage(char* message, uint8_t ip[]) {
	
	union eth_buffer buffer_u = (const union eth_buffer){0};

//	memset(&buffer_u, 0, sizeof buffer_u);

	/* Fill IP header data. Fill all fields and a zeroed CRC field, then update the CRC! */
	buffer_u.cooked_data.payload.ip.ver = 0x45;
	buffer_u.cooked_data.payload.ip.tos = 0x00;
	buffer_u.cooked_data.payload.ip.len = htons(sizeof(struct ip_hdr) + sizeof(struct udp_hdr) + strlen(message));
	buffer_u.cooked_data.payload.ip.id = htons(0x00);
	buffer_u.cooked_data.payload.ip.off = htons(0x00);
	buffer_u.cooked_data.payload.ip.ttl = 50;
	buffer_u.cooked_data.payload.ip.proto = PROTO_UDP;
	buffer_u.cooked_data.payload.ip.sum = htons(0x0000);
	
	printf("mensagem: %s\n", message);
//	printf("recv: %i\n", ip[0]);

	/* Fill source and destination IP */
	buffer_u.cooked_data.payload.ip.dst[0] = ip[0];
	buffer_u.cooked_data.payload.ip.dst[1] = ip[1];
	buffer_u.cooked_data.payload.ip.dst[2] = ip[2];
	buffer_u.cooked_data.payload.ip.dst[3] = ip[3];
	
	buffer_u.cooked_data.payload.ip.src[0] = 192;
	buffer_u.cooked_data.payload.ip.src[1] = 168;
	buffer_u.cooked_data.payload.ip.src[2] = 10;
	buffer_u.cooked_data.payload.ip.src[3] = 1;
	
	memcpy(buffer_u.cooked_data.ethernet.dst_addr, bcast_mac, 6);
	memcpy(buffer_u.cooked_data.ethernet.src_addr, src_mac, 6);
	buffer_u.cooked_data.ethernet.eth_type = htons(ETH_P_IP);
	

	/* calculate the IP checksum */
	buffer_u.cooked_data.payload.ip.sum = htons((~ipchksum((uint8_t *)&buffer_u.cooked_data.payload.ip) & 0xffff));

	/* fill payload data */
	uint8_t msg[strlen(message)];
	memcpy(msg, message, strlen(message)+1);

	/* Fill UDP header */
	buffer_u.cooked_data.payload.udp.udphdr.src_port = htons(555);
	buffer_u.cooked_data.payload.udp.udphdr.dst_port = htons(8000);
	buffer_u.cooked_data.payload.udp.udphdr.udp_len = htons(sizeof(struct udp_hdr) + strlen(msg));
	buffer_u.cooked_data.payload.udp.udphdr.udp_chksum = 0;

	/* Fill UDP payload */
	memcpy(buffer_u.raw_data + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr), msg, strlen(msg));

	/* Send it.. */
	memcpy(socket_address.sll_addr, dst_mac, 6);
	
	int i = sendto(sockfd, buffer_u.raw_data, 
					sizeof(struct eth_hdr) 
					+ sizeof(struct ip_hdr) 
					+ sizeof(struct udp_hdr) + strlen(msg), 0, 
					(struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll));

	memset(buffer_u.raw_data 
			+ sizeof(struct eth_hdr) 
			+ sizeof(struct ip_hdr) 
			+ sizeof(struct udp_hdr), 
			0, sizeof (char) * 99);
	
	if (i < 0) {
		printf("Send failed\n");
		printf("Error: %i\n", i);
	}

	//memset(&buffer_u, 0, sizeof buffer_u);
	__fpurge(stdin);
	return 0;
}

// Processa a mensagem recebida pelo servidor.
void processMessage(uint8_t* ip, char* message) {
	
	//GAMBIARRA POIS NÃO ESTÁ LIMPANDO A MEMÓRIA DO BUFFER
	uint8_t fake_ip[4] = {0,0,0,0};

	sendMessage("                                                             ", fake_ip);


	bool user_exists = false;
	
	// Percorre a lista de usuários comparando o IP, para verificar se o usuário/IP já existe
	for (int i = 0; i < user_number; i++) {
		if (memcmp(users[i].ip, ip, sizeof ip) == 0) {
			user_exists = true;
		}
	}
	
	// se não existe, cria um nick randomico (Random_762328), atribui o IP e inicia sem estar cadastrado em nenhum canal;
	if (!user_exists) {

		char nick[SIZENICK];
		sprintf(nick, "Random_%d", rand());
		
		bool user_available = false;
		
		while(!user_available) {
			
			for (int i = 0; i < user_number; i++) {
				if (strcmp(users[i].nick, nick) == 0) {
					user_exists = true;
				}
			}
			
			if (!user_exists) {
				user_available = true;

			}
		}

		memcpy(users[user_number].ip, ip, sizeof users[user_number].ip);
		memcpy(users[user_number].nick, nick, sizeof nick);
		memset(users[user_number].channel, 0, sizeof users[user_number].channel);
		user_number++;
	}
	
	// se existe, user_index recebe o index(posição) do usuário no array users.
	int user_index;
	for (int i = 0; i < user_number; i++) {
		if (memcmp(users[i].ip, ip, sizeof users[i].ip) == 0) {
			user_index = i;
			break;		
		}
	}

	// se a mensagem inicia por "/"
	if (strncmp(message, "/", 1) == 0) {
		// se for o comando /nick
		if (strncmp(message, "/nick ", 5) == 0) {
			char nick[SIZENICK];
			char *ret = strchr(&message[6], ' ');
			if(ret != NULL){
				sendMessage("Nick não pode conter espaço em branco.", ip);
				return;
			} else {
				memcpy(nick, &message[6], strlen(message));
				memcpy(users[user_index].nick, nick, sizeof nick);
				
				if (strncmp(users[user_index].channel, "#", 1) == 0) {
					int channel_index = -1;

					for (int i = 0; i < channel_number; i++) {
						if (strcmp(users[user_index].channel, channels[i].name) == 0) {
							channel_index = i;
							break;
						}
					}

					for (int i = 0; i < channels[channel_index].user_number; i++) {
						if (memcmp(channels[channel_index].members[i].ip,
									ip, sizeof channels[channel_index].members[i].ip) == 0) {
							strcpy(channels[channel_index].members[i].nick, nick);
							break;
						}
					}
				}
				
				char message[SIZEMSG] = "Você alterou o nickname para ";
				strcat(message, nick);
				strcat(message, ".");
				sendMessage(message, ip);
			}
		// se for o comando /create
		} else if (strncmp(message, "/create ", 8) == 0) {
			char name[CHANNAME];
			memcpy(name, &message[8], strlen(message));
			char *ret = strchr(&message[8], ' ');
			if(ret != NULL){
				sendMessage("O nome do canal não pode conter espaço em branco.", ip);
				return;
			} else if (strncmp(name, "#", 1) != 0) {
				sendMessage("Nome do canal deve iniciar com #", ip);
				return;
			}

			bool channel_exists = false;
			
			for (int i = 0; i < channel_number; i++) {
				if (strcmp(name, channels[i].name) == 0) {
					channel_exists = true;
				}
			}
			
			if (channel_exists) {
				sendMessage("Canal já existe!", ip);
			} else {
				memcpy(&channels[channel_number].admin, 
						&users[user_index], 
						sizeof(users[user_index]));
				memcpy(&channels[channel_number].members[0], 
						&users[user_index], 
						sizeof(users[user_index]));
				memcpy(channels[channel_number].name, name, sizeof name);
				channel_number++;
				sendMessage("Canal criado!", ip);
			}
		// se for o comando /remove
		} else if (strncmp(message, "/remove ", 8) == 0) {
			char name[CHANNAME];
			memcpy(name, &message[8], strlen(message));
			
			int channel_index = -1;
			
			for (int i = 0; i < channel_number; i++) {
				if (strcmp(name, channels[i].name) == 0) {
					channel_index = i;
					break;
				}
			}
			
			if (channel_index == -1) {
				sendMessage("Canal inexistente!", ip);
			} else {
				if (memcmp(channels[channel_index].admin.ip, ip, sizeof channels[channel_index].admin.ip) == 0) {
				
					for (int i = 0; i < channels[channel_index].user_number; i++) {
						for (int j = 0; j < user_number; j++) {
							if (memcmp(users[j].ip,
										channels[channel_index].members[i].ip, 
										sizeof users[j].ip) == 0) {
								strcpy(users[j].channel, "");
							}
						} 
					}
				
					if (channel_index != channel_number -1) {
						channels[channel_index] = channels[channel_number - 1];
					} 
					channel_number--;
					sendMessage("Canal removido!", ip);
				} else {
					sendMessage("Apenas o administrador pode remover o canal!", ip);
				}
			}
		// se for o comando /list
		} else if (strncmp(message, "/list", 5) == 0) {	
			if (channel_number == 0) {
				sendMessage("Não existem canais registrados!", ip);
			} else {
				sendMessage("Lista de canais no servidor:", ip);
				for (int i = 0; i < channel_number; i++) {
					if(strcmp(channels[i].name, "Null") != 0){
						sendMessage(channels[i].name, ip);
					}
				}
			}
		// se for o comando /join
		} else if (strncmp(message, "/join ", 6) == 0) {
			if (strncmp(users[user_index].channel, "#", 1) != 0) {
				char name[CHANNAME];
				memcpy(name, &message[6], strlen(message));
				
				int channel_index = -1;
				
				for (int i = 0; i < channel_number; i++) {
					if (strcmp(name, channels[i].name) == 0) {
						channel_index = i;
						break;
					}
				}
				
				if (channel_index == -1) {
					sendMessage("Canal inexistente!", ip);
				} else {
					memcpy(&channels[channel_index].members[channels[channel_index].user_number],
							&users[user_index], sizeof users[user_index]);
					
					strcpy(users[user_index].channel, 
							channels[channel_index].name 
							);
					
					channels[channel_index].user_number++;		
					
					char message[99] = "Você entrou no canal ";
					strcat(message, channels[channel_index].name);
					strcat(message, ".");
					sendMessage(message, ip);
				}	
				
			} else {
				sendMessage("Você precisa sair do canal atual para entrar em outro!", ip);
			}
		// se for o comando /part
		} else if (strncmp(message, "/part", 5) == 0) {
			if (strncmp(users[user_index].channel, "#", 1) != 0) {
				sendMessage("Você não está em nenhum canal!", ip);
			} else {

				int channel_index = -1;
				
				for (int i = 0; i < channel_number; i++) {
					if (strcmp(users[user_index].channel, channels[i].name) == 0) {
						channel_index = i;
						break;
					}
				}
				
				for (int i = 0; i < channels[channel_index].user_number; i++) {
					if (memcmp(users[user_index].ip,
									channels[channel_index].members[i].ip, 
									sizeof users[user_index].ip) == 0) {
						if (i != channels[channel_index].user_number -1) {
							channels[channel_index].members[i] 
								= channels[channel_index].members[channel_number - 1];
						}
						channels[channel_index].user_number--;
						break;
					}
				}
				
				strcpy(users[user_index].channel, "");
				char message[99] = "Voce saiu do canal ";
				strcat(message, channels[channel_index].name);
				strcat(message, ".");
				sendMessage(message, ip);
			}
		// se for o comando /names
		} else if (strncmp(message, "/names", 6) == 0) {	
			if (strncmp(users[user_index].channel, "#", 1) != 0) {
				sendMessage("Você não está em nenhum canal!", ip);
			} else {

				int channel_index = -1;
				
				for (int i = 0; i < channel_number; i++) {
					if (strcmp(users[user_index].channel, channels[i].name) == 0) {
						channel_index = i;
						break;
					}
				}
				char message[100] = "Lista de usuários no canal ";
				strcat(message, channels[channel_index].name);
				strcat(message, ":");
				sendMessage(message, ip);
				
				for (int i = 0; i < channels[channel_index].user_number; i++) {
					sendMessage(channels[channel_index].members[i].nick, ip);
				}
			}
		// se for o comando /kick
		} else if (strncmp(message, "/kick ", 6) == 0) {
						
			char command[90];
			memcpy(command, &message[6], strlen(message));
			
			char *channel_name = strtok(command, " ");
			int channel_index = -1;
				
			for (int i = 0; i < channel_number; i++) {
				if (strcmp(channel_name, channels[i].name) == 0) {
					channel_index = i;
					break;
				}
			}

			if (channel_index == -1) {
				sendMessage("Canal inexistente!", ip);
			} else {
				if (memcmp(channels[channel_index].admin.ip, 
					ip, 
					sizeof channels[channel_index].admin.ip) == 0) {
				
					char *user_name = strtok(NULL, " ");
					
					int user_index = -1;
					
					for (int i = 0; i < channels[channel_index].user_number; i++) {
						if (strcmp(channels[channel_index].members[i].nick, user_name) == 0) {
							user_index = i;
							break;
						}
					}
					
					if (user_index == -1) {
						sendMessage("Usuário não está no canal!", ip);
					} else {
						sendMessage("Você foi kickado do canal!", 
									channels[channel_index].members[user_index].ip);
						
						if (user_index != channels[channel_index].user_number - 1) {
							channels[channel_index].members[user_index]
								= channels[channel_index].members[channels[channel_index].user_number - 1];
						}
											
						channels[channel_index].user_number--;
						
						for (int i = 0; i < user_number; i++) {
							if (strcmp(users[i].nick, user_name) == 0) {
								strcpy(users[i].channel, "");
								break;
							}
						}
						
						sendMessage("Você kickou o usuário do canal!", ip);
					}
					
				} else {
					sendMessage("Apenas o administrador pode remover o canal!", ip);
				}
			}
		// se for o comando /msg
		} else if (strncmp(message, "/msg ", 5) == 0) { 
			char command[90];
			memcpy(command, &message[5], strlen(message));
			
			char *user_name = strtok(command, " ");
			int user_to_send_index = -1;
				
			for (int i = 0; i < user_number; i++) {
				if (strcmp(user_name, users[i].nick) == 0) {
					user_to_send_index = i;
					break;
				}
			}
			
			if (user_to_send_index == -1) {
				sendMessage("Usuário não existe!", ip);
			} else {
				char *message = strtok(NULL, " ");
				
				char message_to_send[99];
				strcpy(message_to_send, "<");
				strcat(message_to_send, users[user_index].nick);
				strcat(message_to_send, "> ");
				strcat(message_to_send, message); 
				sendMessage(message_to_send, ip); 
				sendMessage(message_to_send, users[user_to_send_index].ip);
			}
		// se for o comando /quit
		} else if (strncmp(message, "/quit", 5) == 0) {
			if (strncmp(users[user_index].channel, "#", 1) == 0) {

				int channel_index = -1;
				
				for (int i = 0; i < channel_number; i++) {
					if (strcmp(users[user_index].channel, channels[i].name) == 0) {
						channel_index = i;
						break;
					}
				}
				
				for (int i = 0; i < channels[channel_index].user_number; i++) {
					if (memcmp(users[user_index].ip,
									channels[channel_index].members[i].ip, 
									sizeof users[user_index].ip) == 0) {
						if (i != channels[channel_index].user_number -1) {
							channels[channel_index].members[i] 
								= channels[channel_index].members[channel_number - 1];
						}
						channels[channel_index].user_number--;
						break;
					}
				}
				
				if (user_index != user_number - 1) {
					users[user_index] = users[user_number - 1];
				}
				
				user_number--;
			}
		// se não for nenhum dos comandos válidos
		} else {
			sendMessage("Comando inválido!", ip);
		}
		// se a mensagem não inicia por "/"
	} else {
		// se o usuário não está em nenhum canal, envia mensagem de volta ao usuário informando que não está
		if (strncmp(users[user_index].channel, "#", 1) != 0) {
				sendMessage("Você não está em nenhum canal!", ip);
		} else {
			// se o usuário está em algum canal, envia mensagem para todos os usuários do canal.
			int channel_index = -1;
				
			for (int i = 0; i < channel_number; i++) {
				if (strcmp(users[user_index].channel, channels[i].name) == 0) {
					channel_index = i;
					break;
				}
			}
			
			char message_to_send[99];
			strcpy(message_to_send, "<");
			strcat(message_to_send, users[user_index].nick);
			strcat(message_to_send, "> ");
			strcat(message_to_send, message);
			for (int i = 0; i < channels[channel_index].user_number; i++) {
				sendMessage(message_to_send, channels[channel_index].members[i].ip);
			}
		}
	}	
}

// inicia o servidor e começa a receber/monitorar os pacotes
int ServerMonitor(){
    int l_numbytes; // calcular quantos bytes estão sendo recebidos.
	 // socket file descriptor. retorna o descritor, ou -1 se der erro
	char *p; // terminador da mensagem enviada

	/* Start configuration of socket and network*/
	sockfd = openRawSocket();

	setIntPromMode(sockfd);
	
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
	
	/* End of configuration. Now we can receive data using raw sockets. */
    while(1){
		
union eth_buffer g_buffer_u;

	memcpy(g_buffer_u.cooked_data.ethernet.dst_addr, bcast_mac, 6);
	memcpy(g_buffer_u.cooked_data.ethernet.src_addr, src_mac, 6);
	g_buffer_u.cooked_data.ethernet.eth_type = htons(ETH_P_IP);
	
            l_numbytes = recvfrom(sockfd, g_buffer_u.raw_data, ETH_LEN, 0, NULL, NULL);
            if (g_buffer_u.cooked_data.ethernet.eth_type == ntohs(ETH_P_IP)
                && g_buffer_u.cooked_data.payload.ip.proto == PROTO_UDP 
					&& g_buffer_u.cooked_data.payload.udp.udphdr.dst_port == htons(DST_PORT)){
                    p = (char *)&g_buffer_u.cooked_data.payload.udp.udphdr + ntohs(g_buffer_u.cooked_data.payload.udp.udphdr.udp_len);
                    *p = '\0';
                    printf("src ip: %d.%d.%d.%d dst ip: %d.%d.%d.%d size: %d msg: %s\n",
                    g_buffer_u.cooked_data.payload.ip.src[0],
					g_buffer_u.cooked_data.payload.ip.src[1],
					g_buffer_u.cooked_data.payload.ip.src[2],
					g_buffer_u.cooked_data.payload.ip.src[3],
					g_buffer_u.cooked_data.payload.ip.dst[0],
					g_buffer_u.cooked_data.payload.ip.dst[1],
					g_buffer_u.cooked_data.payload.ip.dst[2],
					g_buffer_u.cooked_data.payload.ip.dst[3],
                    ntohs(g_buffer_u.cooked_data.payload.udp.udphdr.udp_len),
					(char *)&g_buffer_u.cooked_data.payload.udp.udphdr + sizeof(struct udp_hdr)
                    );
                    uint8_t ip[4];
                    memcpy(ip, g_buffer_u.cooked_data.payload.ip.src, sizeof ip);
                    processMessage(ip, 
									(char *)&g_buffer_u.cooked_data.payload.udp.udphdr + sizeof(struct udp_hdr));
                
					memset(g_buffer_u.raw_data + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) 
					+ sizeof(struct udp_hdr), 0, strlen((char *)&g_buffer_u.cooked_data.payload.udp.udphdr + sizeof(struct udp_hdr)));
                
                }
                
            printf("got a packet, %d bytes\n", l_numbytes);
            }
}

int main(int argc, char *argv[]) {
	srand(1);
	strcpy(ifName, argv[1]);

	printf("Starting server\n\n");

	ServerMonitor();

    return 0;
}
