#include "../headers/structures.h"

/* Open RAW socket */
int openRawSocket();

/* Set interface to promiscuous mode */
void setIntPromMode(int pSockFD);

/* Initialize unused nicks */
void InitNicks(Nick* p_nickNames);

/* Initialize randomic nicks */
void InitRndNicks(Nick* p_rndNicks);

/* Initialize channels */
void InitChannels(Channel* p_channels);
