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
 * stringops.h
 *
 * Miscellaneous C-string functions
 *
 * Copyright (c) 1995 Next Computer, Inc.  All Rights Reserved.
 * Written by Marc Majka
 */

#define streq(A, B) (strcmp(A, B) == 0)
#define IndexNull (unsigned int)-1
char *copyString(char *);
char *concatString(char *, char *);
char **insertString(char *, char **, unsigned int);
char **appendString(char *, char **);
void freeList(char **);
void freeString(char *);
unsigned int listLength(char **);
unsigned int listIndex(char *,char **);
char *prefix(char *, char);
char *postfix(char *, char);
char *presuffix(char *, char);
char *suffix(char *, char);
char *lowerCase(char *);
char **explode(char *, char);
char *itoa(int);
