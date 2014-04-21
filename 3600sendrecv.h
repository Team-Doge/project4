/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600SENDRECV_H__
#define __3600SENDRECV_H__

#include <stdio.h>
#include <stdarg.h>

#define DATA_SIZE 1460

typedef struct header_t {
  unsigned int magic:14;
  unsigned int ack:1;
  unsigned int eof:1;
  unsigned short length;
  unsigned int sequence;
  unsigned short checksum;
} header;

typedef struct packet_t {
	header head;
	char data[DATA_SIZE];
} packet;

typedef struct packet_list_t {
    packet pack;
    struct packet_list_t *next;
} packet_list;

typedef struct packet_list_head_t {
	packet_list *first;
} packet_list_head;

unsigned int MAGIC;

void dump_packet(unsigned char *data, int size);
header *make_header(int sequence, int length, int eof, int ack);
header *get_header(void *data);
char *get_data(void *data);
char *timestamp();
void mylog(char *fmt, ...);
void send_response_header(int offset, int eof, struct sockaddr_in* in, int sock);

void print_header(header *h);

void send_ack(int data_read, int eof, struct sockaddr_in *in, int sock);
void insert_packet_in_list(packet_list_head *list, packet *p);
void write_packets_from_list(packet_list_head *list, unsigned int *data_read);
int send_packet(int sock, struct sockaddr_in out, packet* p);
int send_packet_window(unsigned int amount, unsigned int window_top, int sock, struct sockaddr_in out, packet_list_head *p_list);
void remove_packets_from_list(packet_list_head *list, unsigned int seq);
unsigned short checksum(char *buf, unsigned short length);
unsigned short checksum_header(header *header);
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
#endif

