/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Transmitter.m
 * - uses BPF to send a broadcast packet 
 *   and the socket interface to send a non-broadcast packet
 */

/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <arpa/inet.h>
#import <syslog.h>
#import "ts_log.h"

#import "Transmitter.h"
#import "bpflib.h"
#import "in_cksum.h"

static struct ether_addr ether_broadcast = { 
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} 
};

typedef struct {
    struct ip 		ip;
    struct udphdr 	udp;
} ip_udp_header_t;

typedef struct {
    struct in_addr	src_ip;
    struct in_addr	dest_ip;
    char		zero;
    char		proto;
    unsigned short	length;
} udp_pseudo_hdr_t;

static int 
get_bpf_fd(char * if_name)
{
    int bpf_fd;

    bpf_fd = bpf_new();
    if (bpf_fd < 0) {
	/* BPF transmit unavailable */
	ts_log(LOG_ERR, "Transmitter: bpf_fd() failed, %m");
    }
    else if (bpf_filter_receive_none(bpf_fd) < 0) {
	ts_log(LOG_ERR,  "Transmitter: failed to set filter, %m");
	bpf_dispose(bpf_fd);
	bpf_fd = -1;
    }
    else if (bpf_setif(bpf_fd, if_name) < 0) {
	ts_log(LOG_ERR, "Transmitter: bpf_setif(%s) failed: %m", if_name);
	bpf_dispose(bpf_fd);
	bpf_fd = -1;
    }

    return (bpf_fd);
}

/* 
 * Assumption: 
 * init/free single threaded
 * transmit:Packet:Length: multi-threaded
 */

@implementation Transmitter

- init
{
    [super init];

    sockfd = -1;
    ip_id = random();
    mutex_init(&lock);
    condition_init(&wakeup);
    return self;
}

- free
{
    mutex_clear(&lock);
    condition_clear(&wakeup);
    return [super free];
}

- (int) transmit:(char *) if_name 
      DestHWType:(int)hwtype
      DestHWAddr:(void *)hwaddr
       DestHWLen:(int)hwlen
	  DestIP:(struct in_addr) dest_ip
	SourceIP:(struct in_addr) src_ip
	DestPort:(u_short) dest_port 
      SourcePort:(u_short) src_port
	    Data:(void *) data 
	  Length:(int) len
{
    int				bpf_fd = -1;
    int 			status = 0;

    mutex_lock(&lock);
    while (busy)
	condition_wait(&wakeup, &lock);
    busy = TRUE;
    mutex_unlock(&lock);

    if (hwtype == ARPHRD_ETHER
	&& (ntohl(dest_ip.s_addr) == INADDR_BROADCAST
	    || hwaddr != NULL)) {
	bpf_fd = get_bpf_fd(if_name);
	if (bpf_fd < 0)
	    status = -1;
	else {
	    struct ether_header * 	eh_p;
	    ip_udp_header_t *		ip_udp;
	    udp_pseudo_hdr_t *		udp_pseudo;
	    char *			payload;
	    
	    eh_p = (struct ether_header *)sendbuf;
	    ip_udp = (ip_udp_header_t *)(sendbuf + sizeof(*eh_p));
	    udp_pseudo = (udp_pseudo_hdr_t *)(((char *)&ip_udp->udp)
					      - sizeof(*udp_pseudo));
	    payload = sendbuf + sizeof(*eh_p) + sizeof(*ip_udp);
	    /* copy the data */
	    bcopy(data, payload, len);

	    /* fill in udp pseudo header */
	    udp_pseudo->src_ip = src_ip;
	    udp_pseudo->dest_ip = dest_ip;
	    udp_pseudo->zero = 0;
	    udp_pseudo->proto = IPPROTO_UDP;
	    udp_pseudo->length = htons(sizeof(ip_udp->udp) + len);

	    /* fill in UDP header */
	    ip_udp->udp.uh_sport = htons(src_port);
	    ip_udp->udp.uh_dport = htons(dest_port);
	    ip_udp->udp.uh_ulen = htons(sizeof(ip_udp->udp) + len);
	    ip_udp->udp.uh_sum = 0;
	    ip_udp->udp.uh_sum = in_cksum(udp_pseudo, sizeof(*udp_pseudo) + 
					  sizeof(ip_udp->udp) + len);

	    /* fill in IP header */
	    bzero(ip_udp, sizeof(ip_udp->ip));
	    ip_udp->ip.ip_v = IPVERSION;
	    ip_udp->ip.ip_hl = sizeof(struct ip) >> 2;
	    ip_udp->ip.ip_ttl = MAXTTL;
	    ip_udp->ip.ip_p = IPPROTO_UDP;
	    ip_udp->ip.ip_src = src_ip;
	    ip_udp->ip.ip_dst = dest_ip;
	    ip_udp->ip.ip_len = htons(sizeof(*ip_udp) + len);
	    ip_udp->ip.ip_id = htons(ip_id++);
	    /* compute the IP checksum */
	    ip_udp->ip.ip_sum = 0; /* needs to be zero for checksum */
	    ip_udp->ip.ip_sum = in_cksum(&ip_udp->ip, sizeof(ip_udp->ip));
	    
	    
	    /* set ethernet dest and type, source is inserted automatically */
	    if (ntohl(dest_ip.s_addr) == INADDR_BROADCAST) {
		bcopy(&ether_broadcast, eh_p->ether_dhost,
		      sizeof(ether_broadcast));
	    }
	    else {
		bcopy(hwaddr, eh_p->ether_dhost,
		      sizeof(eh_p->ether_dhost));
	    }
	    eh_p->ether_type = htons(ETHERTYPE_IP);
	    
	    status = bpf_write(bpf_fd, sendbuf, 
			       sizeof(*eh_p) + sizeof(*ip_udp) + len);
	    
	    if (status < 0) {
		ts_log(LOG_ERR, "Transmitter: bpf_write(%s) failed: %m",
		       if_name);
	    }
	}
    }
    else if (sockfd >= 0) { /* send using socket */
	struct sockaddr_in 	dst;
	bzero(&dst, sizeof(dst));
	dst.sin_len = sizeof(struct sockaddr_in);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(dest_port);
	dst.sin_addr = dest_ip;
	status = sendto(sockfd, data, len, 0,
		        (struct sockaddr *)&dst, 
			sizeof(struct sockaddr_in));
    }
    else
	ts_log(LOG_ERR, "Transmitter: neither bpf nor socket send available");

    if (bpf_fd >= 0)
	bpf_dispose(bpf_fd);

    mutex_lock(&lock);
    busy = FALSE;
    condition_signal(&wakeup);
    mutex_unlock(&lock);
	
    return (status);
}

- (int) transmit:(char *) if_name
	    Type:(short) type
	    Data:(void *) data 
	  Length:(int) len
{
    int				bpf_fd = -1;
    struct ether_header * 	eh_p;
    char *			payload;
    int 			status = 0;

    eh_p = (struct ether_header *)sendbuf;
    payload = sendbuf + sizeof(*eh_p);

    mutex_lock(&lock);
    while (busy)
	condition_wait(&wakeup, &lock);
    busy = TRUE;
    mutex_unlock(&lock);

    bpf_fd = get_bpf_fd(if_name);
    if (bpf_fd < 0)
	goto done;

    bcopy(&ether_broadcast, eh_p->ether_dhost,
	  sizeof(eh_p->ether_dhost));
    eh_p->ether_type = htons(type);
    bcopy(data, payload, len);
    status = bpf_write(bpf_fd, sendbuf, sizeof(*eh_p) + len);
    if (status < 0) {
	ts_log(LOG_ERR, "Transmitter transmit: bpf_write(%s) failed: %m",
		if_name);
    }

 done:
    if (bpf_fd >= 0)
	bpf_dispose(bpf_fd);

    mutex_lock(&lock);
    busy = FALSE;
    condition_signal(&wakeup);
    mutex_unlock(&lock);
	
    return (status);
}

- setSocket:(int)fd
{
    sockfd = fd;
    return self;
}
@end
