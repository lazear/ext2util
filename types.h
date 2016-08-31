/*
types.h
*/

#ifndef __baremetal_types__
#define __baremetal_types__

typedef unsigned char uint8_t;			// 1 byte
typedef unsigned short uint16_t;		// 2 bytes
typedef unsigned long uint32_t;			// 4 bytes
typedef unsigned long long uint64_t;	// 8 bytes

typedef unsigned long size_t;			// 4 bytes




typedef int bool;
#define true	1
#define false	0

#define NULL	((void*) 0)
#define panic(a)	(printf("PANIC: %s (%s:%d:%s)\n", a, __FILE__, __LINE__, __func__))

#endif