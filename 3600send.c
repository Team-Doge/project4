/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "3600sendrecv.h"

static int MAX_RETRY = 20;
static int MAX_WINDOW_SIZE = 10;

void usage() {
  printf("Usage: 3600send host:port\n");
  exit(1);
}

/**
 * Reads the next block of data from stdin
 */
int get_next_data(char *data, int size) {
  return read(0, data, size);
}

/**
 * Builds and returns the next packet, or NULL
 * if no more data is available.
 */
void *get_next_packet(int sequence, int *len) {
  char *data = malloc(DATA_SIZE);
   int data_len = get_next_data(data, DATA_SIZE);

  if (data_len == 0) {
    free(data);
    return NULL;
  }

  header *myheader = make_header(sequence, data_len, 0, 0);
  void *packet = malloc(sizeof(header) + data_len);
  memcpy(packet, myheader, sizeof(header));
  memcpy(((char *) packet) +sizeof(header), data, data_len);

  free(data);
  free(myheader);

  *len = sizeof(header) + data_len;

  return packet;
}

int send_packet(int sock, struct sockaddr_in out, packet* p) {
  mylog("[send data] %d (%d)\n", p->head.sequence, p->head.length);
  unsigned short data_check = checksum(p->data, p->head.length);
  unsigned short header_check = checksum_header(&p->head);
  p->head.sequence = htonl(p->head.sequence);
  p->head.checksum = htons(data_check + header_check);
  p->head.length = htons(p->head.length);
  if (sendto(sock, p, p->head.length + sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
    perror("sendto");
    exit(1);
  }
  p->head.checksum = ntohs(p->head.checksum);
  p->head.sequence = ntohl(p->head.sequence);
  p->head.length = ntohs(p->head.length);
  return p->head.length;

}

void send_final_packet(int window_top, int sock, struct sockaddr_in out) {
  header *myheader = make_header(window_top, 0, 1, 0);
  mylog("[send eof]\n");
  myheader->checksum = htons(checksum_header(myheader));
  myheader->sequence = htonl(myheader->sequence);
  myheader->length = htons(myheader->length);
  if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
    perror("sendto");
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a UDP socket in C, 
   * binding to a empheral port, printing out the port number.
   * 
   * I've also included a very simple transport protocol that simply
   * acknowledges every received packet.  It has a header, but does
   * not do any error handling (i.e., it does not have sequence 
   * numbers, timeouts, retries, a "window"). You will
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */

  // extract the host IP and port
  if ((argc != 2) || (strstr(argv[1], ":") == NULL)) {
    usage();
  }

  char *tmp = (char *) malloc(strlen(argv[1])+1);
  strcpy(tmp, argv[1]);

  char *ip_s = strtok(tmp, ":");
  char *port_s = strtok(NULL, ":");
 
  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the local port
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(atoi(port_s));
  out.sin_addr.s_addr = inet_addr(ip_s);

  // socket for received packets
  struct sockaddr_in in;
  socklen_t in_len = sizeof(in);

  // construct the socket set
  fd_set socks;

  int window_size = 10;
  unsigned int window_bottom = 0;
  unsigned int window_top = 0;
  unsigned int eof_sent = 0;
  packet_list_head p_list;
  p_list.first = NULL;

  while (1) {
    // If everything has been acknowledged, send EOF
    if (window_bottom == window_top && window_top != 0 && eof_sent == 0) {
      send_final_packet(window_top, sock, out);
      eof_sent = 1;
    } else {
      // Send the sliding window
      window_top = send_packet_window(window_size, window_top, sock, out, &p_list);
    }

    // Prepare for an ack to come back
    int received_ack = 0;
    int retry_count = 0;
    while (!received_ack) {
      FD_ZERO(&socks);
      FD_SET(sock, &socks);
      // construct the timeout
      struct timeval t;
      t.tv_sec = 1;
      t.tv_usec = 0;


      if (retry_count > 0) {
        mylog("[retry %d]\n", retry_count);
        if (window_bottom == window_top) {
          send_final_packet(window_top, sock, out);
        } else {
          // Send the bottom-most packet
          mylog("[resend data]\n");
          send_packet(sock, out, &p_list.first->pack);
        }
      }
      // Wait for the ack
      if (select(sock + 1, &socks, NULL, NULL, &t)) {
        packet buf;
        int received;
        if ((received = recvfrom(sock, &buf, sizeof(packet), 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
          perror("recvfrom");
          exit(1);
        }
        buf.head = *get_header(&buf);
        if (buf.head.magic == MAGIC && buf.head.ack == 1 && window_bottom <= buf.head.sequence) {
          received_ack = 1;
          window_size = MAX_WINDOW_SIZE - ((window_top - buf.head.sequence) / DATA_SIZE);
          window_bottom = buf.head.sequence;
          remove_packets_from_list(&p_list, window_bottom);
          if (buf.head.eof == 1) {
            mylog("[recv eof ack]\n");
            mylog("[completed]\n");
            return 0;
          } else {
            mylog("[recv ack] %d\n", buf.head.sequence);
          }
        } else {
          mylog("[recv corrupted/duplicate ack] %x %d\n", MAGIC, buf.head.sequence);
        }
      } else {
        // timeout occured
        mylog("[timeout]\n");
        if (retry_count >= MAX_RETRY) {
          mylog("[max retries]\n");
          exit(1);
        } else {
          retry_count++;
        }
      }
    }
  }

  return 0;
}

int send_packet_window(unsigned int amount, unsigned int window_top, int sock, struct sockaddr_in out, packet_list_head *p_list) {
  while (amount > 0) {
    int p_len = 0;
    packet *p = get_next_packet(window_top, &p_len);
    if (p != NULL) {
      int data_sent = send_packet(sock, out, p);
      insert_packet_in_list(p_list, p);
      window_top += data_sent;
      amount--;
    } else {
      break;
    }
  }
  return window_top;
}
