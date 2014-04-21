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
#include <stdarg.h>
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

unsigned int MAGIC = 0x0bee;

char ts[16];

/**
 * Returns a properly formatted timestamp
 */
char *timestamp() {
  time_t ltime;
  ltime=time(NULL);
  struct tm *tm;
  tm=localtime(&ltime);
  struct timeval tv1;
  gettimeofday(&tv1, NULL);

  sprintf(ts,"%02d:%02d:%02d %03d.%03d", tm->tm_hour, tm->tm_min, tm->tm_sec, (int) (tv1.tv_usec/1000), (int) (tv1.tv_usec % 1000));
  return ts;
}

/**
 * Logs debugging messages.  Works like printf(...)
 */
void mylog(char *fmt, ...) {
  va_list args;
  va_start(args,fmt);
  fprintf(stderr, "%s: ", timestamp());
  vfprintf(stderr, fmt,args);
  va_end(args);
}

/**
 * This function takes in a bunch of header fields and 
 * returns a brand new header.  The caller is responsible for
 * eventually free-ing the header.
 */
header *make_header(int sequence, int length, int eof, int ack) {
  header *myheader = (header *) malloc(sizeof(header));
  myheader->magic = MAGIC;
  myheader->eof = eof;
  myheader->sequence = sequence;
  myheader->length = length;
  myheader->ack = ack;
  // included to hold value of checksum in header
  myheader->checksum = 0;
  return myheader;
}

/**
 * This function takes a returned packet and returns a header pointer.  It
 * does not allocate any new memory, so no free is needed.
 */
header *get_header(void *data) {
  header *h = (header *) data;
  h->sequence = ntohl(h->sequence);
  h->length = ntohs(h->length);
  h->checksum = ntohs(h->checksum);

  return h;
}

/**
 * This function takes a returned packet and returns a pointer to the data.  It
 * does not allocate any new memory, so no free is needed.
 */
char *get_data(void *data) {
  return (char *) data + sizeof(header);
}

/**
 * This function will print a hex dump of the provided packet to the screen
 * to help facilitate debugging.  
 *
 * DO NOT MODIFY THIS FUNCTION
 *
 * data - The pointer to your packet buffer
 * size - The length of your packet
 */
void dump_packet(unsigned char *data, int size) {
    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((unsigned int)(intptr_t) p-(unsigned int)(intptr_t) data) );
        }
            
        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

/**
 * This function prints the header
 */
void print_header(header *h) {
  mylog("[header dump] Magic: %d Ack: %d EOF: %d Length %d Sequence %d Checksum %d\n", h->magic, h->ack, h->eof, h->length, h->sequence, h->checksum);
}

/**
 * This function takes in a packet and inserts it into a linked list (serving 
 * as a buffer)
 */
void insert_packet_in_list(packet_list_head *list, packet *p) {
  if (list->first == NULL) {
    // Add as the first item
    packet_list *new_head = (packet_list *) calloc(1, sizeof(packet_list));
    new_head->pack = *p;
    new_head->next = NULL;
    list->first = new_head;
    return;
  }

  packet_list *current = list->first;
  packet_list *next = list->first->next;
  unsigned int p_seq = p->head.sequence;
  while (current != NULL) {
    unsigned int c_seq = current->pack.head.sequence;

    if (c_seq == p_seq) {
      mylog("[duplicate packet - did not save]\n");
      return;
    }

    if (current == list->first && p_seq < c_seq) {
      // new first
      packet_list *new_next = (packet_list *) calloc(1, sizeof(packet_list));
      new_next->pack = *p;
      new_next->next = current->next;
      list->first = new_next;
      break;
    } else if (next == NULL) {
      // add to end of list
      packet_list *new_next = (packet_list *) calloc(1, sizeof(packet_list));
      new_next->pack = *p;
      new_next->next = NULL;
      current->next = new_next;
      break;
    } else if (next->pack.head.sequence > p_seq) {
      // insert it
      packet_list *new_next = (packet_list *) calloc(1, sizeof(packet_list));
      new_next->pack = *p;
      new_next->next = current->next;
      current->next = new_next;
      break;
    }

    current = current->next;
    next = current->next;
  }
}


/**
 * This function removes any packets from the list that are below the sequence number. 
 */
void remove_packets_from_list(packet_list_head *list, unsigned int seq) {
  if (list->first == NULL) {
    return;
  }

  packet_list *current = list->first;
  while (current != NULL) {
    if (current->pack.head.sequence < seq) {
      list->first = current->next;
      free(current);
    } else {
      break;
    }
    current = list->first;
  }
}

/**
 * This function determines the checksum of the packet for packet corruption
 */
unsigned short checksum(char *buf, unsigned short length) {
  unsigned long sum = 0;
  for (int i = 0; i < length; i++) {
    char c = buf[i];
    sum += c;
    if (sum & 0xFFFF0000) {
      // carry occurred, wrap around
      sum &= 0xFFFF;
      sum++;
    }
  }
  
  // This is probably bad
  if (sum == 0) {
    return 0;
  } else {
    return ~(sum & 0xFFFF);    
  }
} 

/**
 * This function takes the checksum of the packet header
 */
unsigned short checksum_header(header *h) {
  unsigned long sum = 0;
  sum += h->magic;
  if (sum & 0xFFFF0000) {
    // carry occurred, wrap around
    sum &= 0xFFFF;
    sum++;
  }

  sum += h->ack;
  if (sum & 0xFFFF0000) {
    // carry occurred, wrap around
    sum &= 0xFFFF;
    sum++;
  }

  sum += h->eof;
  if (sum & 0xFFFF0000) {
    // carry occurred, wrap around
    sum &= 0xFFFF;
    sum++;
  }

  sum += h->length;
  if (sum & 0xFFFF0000) {
    // carry occurred, wrap around
    sum &= 0xFFFF;
    sum++;
  }

  sum += h->sequence;
  if (sum & 0xFFFF0000) {
    // carry occurred, wrap around
    sum &= 0xFFFF;
    sum++;
  }

  return ~(sum & 0xFFFF);
}
