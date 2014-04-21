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

int main() {
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

  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the local port
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(0);
  out.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *) &out, sizeof(out))) {
    perror("bind");
    exit(1);
  }

  struct sockaddr_in tmp;
  int len = sizeof(tmp);
  if (getsockname(sock, (struct sockaddr *) &tmp, (socklen_t *) &len)) {
    perror("getsockname");
    exit(1);
  }

  mylog("[bound] %d\n", ntohs(tmp.sin_port));

  // wait for incoming packets
  struct sockaddr_in in;
  socklen_t in_len = sizeof(in);

  // construct the socket set
  fd_set socks;

  // our receive buffer
  unsigned int data_read = 0;
  packet_list_head list;
  list.first = NULL;
  unsigned int retry_count = 0;
  unsigned int MAX_RETRY = 10;

  // construct the timeout
  struct timeval t;
  t.tv_sec = 30;
  t.tv_usec = 0;
  FD_ZERO(&socks);
  FD_SET(sock, &socks);

  // wait to receive, or for a timeout
  while (1) {
    packet buf;
    struct timeval start;
    gettimeofday(&start, NULL);
    if (select(sock + 1, &socks, NULL, NULL, &t)) {
      int received;
      if ((received = recvfrom(sock, &buf, sizeof(packet), 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
        perror("recvfrom");
        exit(1);
      }
      struct timeval end;
      gettimeofday(&end, NULL);
      buf.head = *get_header(&buf);

      if (buf.head.magic == MAGIC) {
        retry_count = 0;
        if (buf.head.sequence == data_read) {
         if (buf.head.eof) {
            mylog("[recv eof]\n");
          } else {
            mylog("[recv data] %d (%d)\n", buf.head.sequence, buf.head.length);          
          }
          // Write it
          char *data = buf.data;
          write(1, data, buf.head.length);
          data_read += buf.head.length;
          write_packets_from_list(&list, &data_read);
          send_ack(data_read, buf.head.eof, &in, sock);
          if (buf.head.eof) {
            mylog("[completed]\n");
            exit(0);
          }
        } else if (buf.head.sequence > data_read) {
          insert_packet_in_list(&list, &buf);
          mylog("[recv data] %d (%d)\n", buf.head.sequence, buf.head.length);          
        } else {
          // Duplicate
          mylog("[recv duplicate] %d (%d)\n", buf.head.sequence, buf.head.length);
          send_ack(data_read, buf.head.eof, &in, sock);
        }
      } else {
        mylog("[recv corrupted packet]\n");
      }
    } else {
      mylog("[timeout]\n");
      t.tv_sec = 5;
      if (retry_count >= MAX_RETRY) {
        mylog("[max retries]\n");
        exit(1);
      } else {
        send_ack(data_read, buf.head.eof, &in, sock);
        retry_count++;
      }
    }
  }


  return 0;
}

void send_ack(int data_read, int eof, struct sockaddr_in *in, int sock) {
  send_response_header(data_read, eof, in, sock);
  if (eof) {
    mylog("[send eof ack]\n");
  } else {
    mylog("[send ack] %d\n", data_read);    
  }
}

void send_response_header(int offset, int eof, struct sockaddr_in* in, int sock) {
  header *responseheader = make_header(offset, 0, eof, 1);
  responseheader->sequence = htonl(responseheader->sequence);
  responseheader->length = htons(responseheader->length);
  if (sendto(sock, responseheader, sizeof(header), 0, (struct sockaddr *) in, (socklen_t) sizeof(*in)) < 0) {
    perror("sendto");
    exit(1);
  }
  free(responseheader);
}

void write_packets_from_list(packet_list_head *list, unsigned int *data_read) {
  if (list->first != NULL) {
    packet_list *current = list->first;
    
    while (current != NULL) {
      unsigned int seq_num = current->pack.head.sequence;
      if (seq_num == *data_read) {
        // Write
        write(1, current->pack.data, current->pack.head.length);
        *data_read += current->pack.head.length;
        list->first = current->next;
        packet_list *next = current->next;
        free(current);
        current = next;
      } else {
        break;
      }
    }
  }
}
