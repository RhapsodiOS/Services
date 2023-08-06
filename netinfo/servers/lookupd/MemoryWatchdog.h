/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * MemoryWatchdog.h
 *
 * Keeps an array of object pointers.  Used to keep track of all
 * allocated lookupd objects.  Useful for finding leaks.
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"
#import "LUDictionary.h"
#import "LUArray.h"
#import "CacheAgent.h"
#import <stdio.h>

@interface MemoryWatchdog : Root
{
	Lock *listLock;
	LUArray *list;
	CacheAgent *cacheAgent;
	LUDictionary *stats;
}

- (void)checkObjects;
- (void)checkObjects:(FILE *)f;
- (void)printObject:(int)where file:(FILE *)f;
- (LUDictionary *)statistics;
- (void)addObject:(id)anObject;
- (void)removeObject:(id)anObject;

@end
