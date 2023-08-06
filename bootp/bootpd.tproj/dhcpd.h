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
 * dhcpd.h
 * - DHCP server definitions
 */

#import "dhcplib.h"

void
dhcp_request(dhcp_msgtype_t dhcp_msgtype, interface_t * intface,
	     u_char * rxpkt, int n, id rq_options, struct in_addr * dstaddr_p,
	     struct timeval * time_in_p);

#define DHCP_CLIENT_TYPE		"dhcp"

/* default time to leave an ip address pending before re-using it */
#define DHCP_PENDING_SECS		60

#define DHCP_MIN_LEASE	((dhcp_lease_t)60 * 60) /* one hour */
#define DHCP_MAX_LEASE	((dhcp_lease_t)60 * 60 * 24 * 30) /* one month */

#define HOSTPROP__DHCP_LEASE		"_dhcp_lease"
#define HOSTPROP__DHCP_RELEASED		"_dhcp_released"
#define HOSTPROP__DHCP_DECLINED		"_dhcp_declined"

#define SUBNETPROP_LEASE_MIN		"lease_min"
#define SUBNETPROP_LEASE_MAX		"lease_max"

#define TIME_DRIFT_PERCENT		0.99

static __inline__ u_long
lease_prorate(u_long lease_time)
{
    double d = lease_time * TIME_DRIFT_PERCENT;

    return ((u_long)d);
}
