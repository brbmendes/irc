/*
 * structures.h
 *
 *  Created on: 17 de abr de 2019
 *      Author: root
 */

#include <stdint.h>

#define NICK_SIZE 50
#define CHANNEL_SIZE 50
#define CHANNAME 30

#ifndef HEADERS_STRUCTURES_H_
#define HEADERS_STRUCTURES_H_


typedef struct {
	char nick[NICK_SIZE];
	uint8_t ip[4];
	char channel[CHANNAME];
} User;

typedef struct {
	User admin;
	User members[CHANNEL_SIZE];
	char name[CHANNAME];
	int user_number;
} Channel;

typedef struct {
	char name[NICK_SIZE];
} Nick;

#endif /* HEADERS_STRUCTURES_H_ */

