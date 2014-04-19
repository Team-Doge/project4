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
} header;

typedef struct packet_t {
	header head;
	char data[DATA_SIZE];
} packet;

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
void store_packet(packet to_store, packet *packet_buffer);
void write_next_packet_from_buffer(packet *packet_buffer, unsigned int *data_read);	

#endif

