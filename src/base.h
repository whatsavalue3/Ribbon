#pragma once

typedef unsigned int dword;
typedef unsigned short word;
typedef unsigned char byte;


#define deref(a) (*(volatile dword*)(a))
#define attr __attribute__

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#ifndef __cplusplus
	typedef int bool;
	#define true 1
	#define false 0
#endif

#define ALWAYS_INLINE __attribute__((always_inline)) inline

extern void SendString(const char* str);
extern void SendDword(dword num);
extern void Wait(dword time);