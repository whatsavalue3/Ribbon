#include "base.h"
#include "hardware.h"
#include "broadcom/hardware_vc4.h"
#include "broadcom/bcm2708_chip/fpga_peripheral.h"





void Wait(dword time)
{
	for(volatile register dword i asm("r1") = time; --i;)
	{
		
	}
}

void SendChar(char chr)
{
	deref(0x7e20001c) = 1<<20;
	Wait(0x800);
	deref(0x7e200028) = 1<<20;
	Wait(0x8000);
	char tmpchar = chr;
	for(dword i = 4; i--;)
	{
		deref(0x7e20001c) = 1<<20;
		if((tmpchar&0x80) != 0)
		{
			Wait(0x800);
		}
		else
		{
			Wait(0x8000);
		}
		tmpchar <<= 1;
		deref(0x7e200028) = 1<<20;
		if((tmpchar&0x80) != 0)
		{
			Wait(0x800);
		}
		else
		{
			Wait(0x8000);
		}
		tmpchar <<= 1;
	}
	deref(0x7e20001c) = 1<<20;
	Wait(0x800);
	deref(0x7e200028) = 1<<20;
	Wait(0x800);
}

void SendString(const char* str)
{
	while(*str)
	{
		SendChar(*(str++));
	}
}

static char printdwordbuf[11];

void SendDword(dword num)
{
	printdwordbuf[10] = '\x00';
	printdwordbuf[9] = ' ';
	byte* out = &printdwordbuf[8];
	for(int luup = 0; luup < 7; luup++)
	{
		if((num&0xf) > 0x9)
		{
			*out = 'a'+(num&0xf)-0xa;
		}
		else
		{
			*out = '0'+(num&0xf);
		}
		num = num>>4;
		if(!num)
		{
			SendString(out);
			return;
		}
		out--;
	}
	if((num&0xf) > 0x9)
	{
		*out = 'a'+(num&0xf)-0xa;
	}
	else
	{
		*out = '0'+(num&0xf);
	}
	SendString(out);
}


extern void sdram_init();
extern void SD_Init();
void Main()
{
	deref(0x7e200008) = (deref(0x7e200008)&(~(7<<0)))|(1<<0);
	deref(0x7e200010) = (deref(0x7e200010)&(~(7<<21)))|(1<<21);
	deref(0x7e20002c) = 1<<15;

	sdram_init();
	
	SD_Init();
	
	
	while(1)
	{
		//Wait(0x50000);
		//deref(0x7e20002c) = 1<<15;
		//SendString("Hello from the C-side...");
		//deref(0x7e200020) = 1<<15;
	}
}