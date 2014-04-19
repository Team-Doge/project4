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

static int SLIDING_WINDOW_SIZE = 10;

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

  // construct the timeout
  struct timeval t;
  t.tv_sec = 30;
  t.tv_usec = 0;

  // our receive buffer
  packet* packet_buffer = (packet *) calloc(SLIDING_WINDOW_SIZE, sizeof(packet));
  unsigned int in_window = 0;
  unsigned int next_data = 0;

  // wait to receive, or for a timeout
  while (1) {
    FD_ZERO(&socks);
    FD_SET(sock, &socks);
    packet buf;

    if (select(sock + 1, &socks, NULL, NULL, &t)) {
      int received;
      if ((received = recvfrom(sock, &buf, sizeof(packet), 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
        perror("recvfrom");
        exit(1);
      }

      buf.head = *get_header(&buf);

      if (buf.head.magic == MAGIC) {
        mylog("[recv data] %d (%d) %s\n", buf.head.sequence, buf.head.length, "ACCEPTED (in-order)");
        // mylog("Current sequence number: %d. Received: %d.\n", next_data, buf.head.sequence);
        if (buf.head.sequence == next_data) {
          // Write it
          char *data = get_data(&buf);
          write(1, data, buf.head.length);
          next_data += buf.head.length;
          mylog("[send ack] %d\n", buf.head.sequence + buf.head.length);
          send_response_header(buf.head.sequence + buf.head.length, buf.head.eof, &in, sock);
          if (buf.head.eof) {
            mylog("[recv eof]\n");
            mylog("[completed]\n");
            exit(0);
          }
        } else if (buf.head.sequence > next_data) {
          // Save it for later
        } else {
          // Duplicate
        }
      } else {
        mylog("[recv corrupted packet]\n");
      }
    } else {
      mylog("[error] timeout occurred\n");
      exit(1);
    }
  }


  return 0;
}

void send_response_header(int offset, int eof, struct sockaddr_in* in, int sock) {
  header *responseheader = make_header(offset, 0, eof, 1);
  if (sendto(sock, responseheader, sizeof(header), 0, (struct sockaddr *) in, (socklen_t) sizeof(*in)) < 0) {
    perror("sendto");
    exit(1);
  }
}
