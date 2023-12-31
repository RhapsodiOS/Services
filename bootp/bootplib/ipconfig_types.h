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

#include <sys/types.h>

#define IPCONFIG_IF_ANY		""
#define MAX_IF_NAMELEN 		32
#define MAX_INLINE_DATA 	4096

#ifdef MOSX
#ifndef port_t
#define port_t		mach_port_t
#endif port_t
#endif MOSX

typedef char if_name[MAX_IF_NAMELEN];
typedef char inline_data[MAX_INLINE_DATA];
typedef char *ooline_data;
typedef u_int32_t ip_address_t;
