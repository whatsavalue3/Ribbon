#include "base.h"
#include "hardware.h"
#include "sd_proto.h"

#define SDEDM_WRITE_THRESHOLD_SHIFT 9
#define SDEDM_READ_THRESHOLD_SHIFT 14
#define SDEDM_THRESHOLD_MASK     0x1f

#define SAFE_READ_THRESHOLD     4
#define SAFE_WRITE_THRESHOLD    4

#define VOLTAGE_SUPPLY_RANGE 0x100
#define CHECK_PATTERN 0x55

#define SDHSTS_BUSY_IRPT                0x400
#define SDHSTS_BLOCK_IRPT               0x200
#define SDHSTS_SDIO_IRPT                0x100
#define SDHSTS_REW_TIME_OUT             0x80
#define SDHSTS_CMD_TIME_OUT             0x40
#define SDHSTS_CRC16_ERROR              0x20
#define SDHSTS_CRC7_ERROR               0x10
#define SDHSTS_FIFO_ERROR               0x08

#define SDEDM_FSM_MASK           0xf
#define SDEDM_FSM_IDENTMODE      0x0
#define SDEDM_FSM_DATAMODE       0x1
#define SDEDM_FSM_READDATA       0x2
#define SDEDM_FSM_WRITEDATA      0x3
#define SDEDM_FSM_READWAIT       0x4
#define SDEDM_FSM_READCRC        0x5
#define SDEDM_FSM_WRITECRC       0x6
#define SDEDM_FSM_WRITEWAIT1     0x7
#define SDEDM_FSM_POWERDOWN      0x8
#define SDEDM_FSM_POWERUP        0x9
#define SDEDM_FSM_WRITESTART1    0xa
#define SDEDM_FSM_WRITESTART2    0xb
#define SDEDM_FSM_GENPULSES      0xc
#define SDEDM_FSM_WRITEWAIT2     0xd
#define SDEDM_FSM_STARTPOWDOWN   0xf

#define SDHSTS_TRANSFER_ERROR_MASK      (SDHSTS_CRC7_ERROR|SDHSTS_CRC16_ERROR|SDHSTS_REW_TIME_OUT|SDHSTS_FIFO_ERROR)
#define SDHSTS_ERROR_MASK               (SDHSTS_CMD_TIME_OUT|SDHSTS_TRANSFER_ERROR_MASK)

void SD_Wait()
{
	dword timeout = 100000;
	while(SH_CMD & SH_CMD_NEW_FLAG_SET)
	{
		if((--timeout) == 0)
		{
			break;
		}
	}
}

void SD_WaitForFIFO()
{
	dword timeout = 100000;
	while((SH_HSTS & SH_HSTS_DATA_FLAG_SET) == 0)
	{
		if((--timeout) == 0)
		{
			break;
		}
	}
}

void SD_Send(dword command, dword arg)
{
	SD_Wait();
	dword sts = SH_HSTS;
	if(sts & SDHSTS_ERROR_MASK)
	{
		SH_HSTS = sts;
	}
	
	SH_ARG = arg;
	SH_CMD = command | SH_CMD_NEW_FLAG_SET;
}

void SD_SendCmd(dword command, dword arg)
{
	SD_Send(command&SH_CMD_COMMAND_SET,arg);
}

void SD_SendCmdNoResp(dword command, dword arg)
{
	SD_Send((command&SH_CMD_COMMAND_SET) | SH_CMD_NO_RESPONSE_SET,arg);
}

void SD_SendCmdLongResp(dword command, dword arg)
{
	SD_Send((command&SH_CMD_COMMAND_SET) | SH_CMD_LONG_RESPONSE_SET,arg);
}

void SD_ConfigurePinmux()
{
	GP_FSEL4 = 0x24000000;
	GP_FSEL5 = 0x924;
	
	GP_PUD = 2;
	Wait(1000);
	GP_PUD = 0;
	
	GP_PUDCLK1 = GP_PUDCLK1_PUDCLKn32_SET;
	GP_PUDCLK2 = GP_PUDCLK2_PUDCLKn64_SET;
	Wait(1000);
	
	GP_PUDCLK1 = 0;
	GP_PUDCLK2 = 0;
}

void SD_SetPower(bool on)
{
	SH_VDD = on ? SH_VDD_POWER_ON_SET : 0x0;
}

void SD_Reset()
{
	SD_SetPower(false);
	SH_CMD = 0;
	SH_ARG = 0;
	SH_TOUT = 0xf00000;
	SH_CDIV = 0;
	SH_HSTS = 0x7f8;
	SH_HCFG = 0;
	SH_HBCT = 0;
	SH_HBLC = 0;
	dword temp = SH_EDM;

	temp &= ~((SDEDM_THRESHOLD_MASK<<SDEDM_READ_THRESHOLD_SHIFT) |
			  (SDEDM_THRESHOLD_MASK<<SDEDM_WRITE_THRESHOLD_SHIFT));
	temp |= (SAFE_READ_THRESHOLD << SDEDM_READ_THRESHOLD_SHIFT) |
			(SAFE_WRITE_THRESHOLD << SDEDM_WRITE_THRESHOLD_SHIFT);

	SH_EDM = temp;
	Wait(600);
	SD_SetPower(true);
	Wait(600);
}

static dword r[4];

static bool SD_IsSDHC = false;

void SD_GetResponse()
{
	r[0] = SH_RSP0;
	r[1] = SH_RSP1;
	r[2] = SH_RSP2;
	r[3] = SH_RSP3;
}

void SD_WaitForResponse()
{
	SD_Wait();
	
	SD_GetResponse();
}

void SD_QueryVoltageAndType()
{
	SD_SendCmd(SD_SEND_IF_COND,0x1aa);
	SD_WaitForResponse();
	
	dword t = MMC_OCR_3_3V_3_4V;
	if(r[0] == 0x1aa)
	{
		t |= MMC_OCR_HCS;
		SD_IsSDHC = true;
	}
	
	while(1)
	{
		SD_SendCmd(MMC_APP_CMD,0);
		SD_SendCmdNoResp(SD_APP_OP_COND,t);
		SD_WaitForResponse();
		if(r[0] & MMC_OCR_MEM_READY)
		{
			break;
		}
		
		Wait(100);
	}
}

static dword rca;
static dword cid[4];
static dword csd[4];

static dword block_length;
static qword capacity_bytes;
static dword clock_div;

#define SD_CopyResponse(a) a[0] = r[0]; a[1] = r[1]; a[2] = r[2]; a[3] = r[3]

void SD_IndentifyCard()
{
	SD_SendCmdLongResp(MMC_ALL_SEND_CID,0);
	SD_WaitForResponse();
	
	SD_SendCmd(MMC_SET_RELATIVE_ADDR,0);
	rca = SD_R6_RCA(r);
	
	SD_SendCmdLongResp(MMC_SEND_CID,MMC_ARG_RCA(rca));
	SD_WaitForResponse();
	
	SD_CopyResponse(cid);
	
	SD_SendCmdLongResp(MMC_SEND_CSD,MMC_ARG_RCA(rca));
	SD_WaitForResponse();
	
	SD_CopyResponse(csd);
}

void SD_DrainFIFO()
{
	SD_Wait();
	
	while(SH_HSTS & SH_HSTS_DATA_FLAG_SET)
	{
		SH_DATA;
	}
}

void SD_ReadBlock(dword sector, dword* buf)
{
	if(!SD_IsSDHC)
		sector <<= 9;
	
	SD_DrainFIFO();
	
	SD_Send(MMC_READ_BLOCK_MULTIPLE | SH_CMD_READ_CMD_SET, sector);
	
	for(dword i = 0; i < 128; i++)
	{
		SD_WaitForFIFO();
		
		volatile dword data = SH_DATA;
		
		if(buf)
		{
			*(buf++) = data;
		}
	}
	
	SD_Send(MMC_STOP_TRANSMISSION | SH_CMD_BUSY_CMD_SET, 0);
}

void SD_InitCard()
{
	char pnm[8];
	
	SD_SendCmdNoResp(MMC_GO_IDLE_STATE,0);
	
	SD_QueryVoltageAndType();
	
	SD_IndentifyCard();
	
	SD_CID_PNM_CPY(cid,pnm);
	
	SendString("SD Card : ");
	SendString(pnm);
	SendString("\n");
	
	if(SD_CSD_CSDVER(csd) == SD_CSD_CSDVER_2_0)
	{
		block_length = 1 << SD_CSD_V2_BL_LEN;
		
		capacity_bytes = (SD_CSD_V2_CAPACITY(csd) * block_length);
		
		clock_div = 5;
	}
	else
	{
		block_length = 1 << SD_CSD_READ_BL_LEN(csd);
		
		capacity_bytes = (SD_CSD_CAPACITY(csd) * block_length);
		
		clock_div = 10;
	}
	
	SendString("Block Length : ");
	SendDword(block_length);
	SendString("\n");
	
	SD_SendCmd(MMC_SELECT_CARD,MMC_ARG_RCA(rca));
	SD_Wait();
	
	if(SD_CSD_CSDVER(csd) == SD_CSD_CSDVER_1_0)
	{
		SD_SendCmd(MMC_SET_BLOCKLEN,512);
		SD_Wait();
	}
	
	SH_CDIV = clock_div - 2;
}

#define kIdentSafeClockRate 0x148

void SD_Init()
{
	SD_ConfigurePinmux();
	SD_Reset();
	
	SH_HCFG &= ~SH_HCFG_WIDE_EXT_BUS_SET;
	SH_HCFG = SH_HCFG_SLOW_CARD_SET | SH_HCFG_WIDE_INT_BUS_SET;
	SH_CDIV = kIdentSafeClockRate;
	
	Wait(600);
	
	SD_InitCard();
	
	// some silicon bug
	SD_ReadBlock(0,0);
	SD_ReadBlock(0,0);
	SD_ReadBlock(0,0);
}