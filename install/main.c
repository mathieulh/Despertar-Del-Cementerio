#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspsuspend.h>
#include <psppower.h>
#include <pspreg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <libpsardumper.h>
#include <pspdecrypt.h>
#include <kubridge.h>
#include <vlf.h>

#include "ipl_01g.h"
#include "ipl_02g.h"
#include "iplsel.h"
#include "pspdecrypt_module.h"
#include "pspbtcnf_01g.h"
#include "pspbtcnf_02g.h"
#include "libpsardumper.h"
#include "resurrection.h"
#include "iplloader.h"
#include "tmctrl401.h"
#include "recovery.h"
#include "systemctrl_01g.h"
#include "systemctrl_02g.h"
#include "vshctrl.h"
#include "satelite.h"
#include "popcorn.h"
#include "march33.h"
#include "galaxy.h"
#include "idcanager.h"
#include "usbdevice.h"
#include "dcman.h"
#include "vlf.h"
#include "intraFont.h"
#include "iop.h"
#include "bg.h"
#include "ipl_update.h"
#include "lflash_fdisk.h"
#include "idsregeneration.h"

PSP_MODULE_INFO("VResurrection_Manager", 0x800, 2, 0);
PSP_MAIN_THREAD_ATTR(0);

#define printf    pspDebugScreenPrintf

#define PRX_SIZE_401				5214736
#define LFLASH_FATFMT_UPDATER_SIZE	0x1650	
#define NAND_UPDATER_SIZE			0x2510
#define LFATFS_UPDATER_SIZE			0x8F90

#define PSAR_SIZE_401	20415152

#define N_FILES	31

typedef struct
{
	char *path;
	u8 *buf;
	int size;
} M33File;

M33File m33files[N_FILES] =
{
	{ "ms0:/TM/DC7/kd/resurrection.prx", resurrection, sizeof(resurrection) },
	{ "ms0:/TM/DC7/kd/pspdecrypt.prx", pspdecrypt, sizeof(pspdecrypt) },
	{ "ms0:/TM/DC7/kd/libpsardumper.prx", libpsardumper, sizeof(libpsardumper) },
	{ "ms0:/TM/DC7/kd/pspbtjnf.bin", pspbtjnf_01g, sizeof(pspbtjnf_01g) },
	{ "ms0:/TM/DC7/kd/pspbtjnf_02g.bin", pspbtjnf_02g, sizeof(pspbtjnf_02g) },
	{ "ms0:/TM/DC7/kd/pspbtknf.bin", pspbtknf_01g, sizeof(pspbtknf_01g) },
	{ "ms0:/TM/DC7/kd/pspbtknf_02g.bin", pspbtknf_02g, sizeof(pspbtknf_02g) },
	{ "ms0:/TM/DC7/kd/pspbtlnf.bin", pspbtlnf_01g, sizeof(pspbtlnf_01g) },
	{ "ms0:/TM/DC7/kd/pspbtlnf_02g.bin", pspbtlnf_02g, sizeof(pspbtlnf_02g) },
	{ "ms0:/TM/DC7/kd/pspbtdnf.bin", pspbtdnf_01g, sizeof(pspbtdnf_01g) },
	{ "ms0:/TM/DC7/kd/pspbtdnf_02g.bin", pspbtdnf_02g, sizeof(pspbtdnf_02g) },
	{ "ms0:/TM/DC7/tmctrl401.prx", tmctrl401, sizeof(tmctrl401) },
	{ "ms0:/TM/DC7/vsh/module/recovery.prx", recovery, sizeof(recovery) },
	{ "ms0:/TM/DC7/kd/systemctrl_02g.prx", systemctrl_02g, sizeof(systemctrl_02g) },
	{ "ms0:/TM/DC7/kd/systemctrl.prx", systemctrl_01g, sizeof(systemctrl_01g) },
	{ "ms0:/TM/DC7/kd/vshctrl.prx", vshctrl, sizeof(vshctrl) },
	{ "ms0:/TM/DC7/vsh/module/satelite.prx", satelite, sizeof(satelite) },
	{ "ms0:/TM/DC7/kd/march33.prx", march33, sizeof(march33) },
	{ "ms0:/TM/DC7/kd/popcorn.prx", popcorn, sizeof(popcorn) },
	{ "ms0:/TM/DC7/kd/idcanager.prx", idcanager, sizeof(idcanager) },
	{ "ms0:/TM/DC7/kd/galaxy.prx", galaxy, sizeof(galaxy) },
	{ "ms0:/TM/DC7/kd/usbdevice.prx", usbdevice, sizeof(usbdevice) },
	{ "ms0:/TM/DC7/kd/dcman.prx", dcman, sizeof(dcman) },
	{ "ms0:/TM/DC7/kd/iop.prx", iop, sizeof(iop) },
	{ "ms0:/TM/DC7/kd/ipl_update.prx", ipl_update, sizeof(ipl_update) },
	{ "ms0:/TM/DC7/vsh/module/vlf.prx", vlf, sizeof(vlf) },
	{ "ms0:/TM/DC7/vsh/module/intraFont.prx", intraFont, sizeof(intraFont) },
	{ "ms0:/TM/DC7/vsh/resource/bg.bmp", bg, sizeof(bg) },
	{ "ms0:/TM/DC7/ipl.bin", iplsel, sizeof(iplsel) },
	{ "ms0:/TM/DC7/kd/lflash_fdisk.prx", lflash_fdisk, sizeof(lflash_fdisk) },
	{ "ms0:/TM/DC7/kd/idsregeneration.prx", idsregeneration, sizeof(idsregeneration) },
};

char error_msg[256];
static int g_running = 0, g_cancel = 0;
static SceUID install_thid = -1;

////////////////////////////////////////////////////////////////////
// big buffers for data. Some system calls require 64 byte alignment

// big enough for the full PSAR file
static u8 *g_dataPSAR;
static u8 *g_dataOut;
static u8 *g_dataOut2;   

static int PSAR_BUFFER_SIZE;
static const int SMALL_BUFFER_SIZE = 2500000;

static char com_table[0x4000];
static int comtable_size;

static char _1g_table[0x4000];
static int _1gtable_size;

static char _2g_table[0x4000];
static int _2gtable_size;

u8 updater_prx_401_md5[16] = 
{
	0x90, 0x8E, 0xD7, 0x13, 0x71, 0x76, 0x46, 0x8A, 
	0x21, 0x57, 0x16, 0x41, 0x20, 0x51, 0xDA, 0x71
};


u8 psar_401_md5[16] = 
{
	0x82, 0x02, 0x61, 0x0E, 0x61, 0x0E, 0xF4, 0x75, 
	0x3E, 0x21, 0x78, 0x42, 0x8B, 0xF4, 0x6D, 0x4F
};
// Gui vars
int begin_install_text;
int install_status;
char st_text[256];
int progress_bar, progress_text;
char pg_text[20];

void ErrorExit(int milisecs, char *fmt, ...)
{
	va_list list;
	char msg[256];	

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (!g_running)
	{
		vlfGuiMessageDialog(msg, VLF_MD_TYPE_ERROR | VLF_MD_BUTTONS_NONE);
		sceKernelDelayThread(milisecs*1000);
		sceKernelExitGame();
	}
	else
	{
		strcpy(error_msg, msg);
		g_running = 0;
	}
	
	sceKernelSleepThread();
}

////////////////////////////////////////////////////////////////////
// File helpers

int ReadFile(char *file, int seek, void *buf, int size);
int WriteFile(char *file, void *buf, int size);
void *malloc64(int size);

static int FindTablePath(char *table, int table_size, char *number, char *szOut)
{
	int i, j, k;

	for (i = 0; i < table_size-5; i++)
	{
		if (strncmp(number, table+i, 5) == 0)
		{
			for (j = 0, k = 0; ; j++, k++)
			{
				if (table[i+j+6] < 0x20)
				{
					szOut[k] = 0;
					break;
				}

				if (!strncmp(table+i+6, "flash", 5) &&
					j == 6)
				{
					szOut[6] = ':';
					szOut[7] = '/';
					k++;
				}
				else if (!strncmp(table+i+6, "ipl", 3) &&
					j == 3)
				{
					szOut[3] = ':';
					szOut[4] = '/';
					k++;
				}
				else
				{				
					szOut[k] = table[i+j+6];
				}
			}

			return 1;
		}
	}

	return 0;
}

int get_registry_value(const char *dir, const char *name, int *val);


int LoadStartModule(char *module, int partition)
{
	SceUID mod = kuKernelLoadModule(module, 0, NULL);

	if (mod < 0)
		return mod;

	return sceKernelStartModule(mod, 0, NULL, NULL, NULL);
}

int last_percentage = -1;
int last_time;

int DoProgressUpdate(void *param)
{
	vlfGuiProgressBarSetProgress(progress_bar, last_percentage);
	vlfGuiSetText(progress_text, pg_text);
	
	return VLF_EV_RET_REMOVE_HANDLERS;
}

void ClearProgress()
{
	last_time = 0;
	last_percentage = -1;
}

void SetProgress(int percentage, int force)
{
	int st =  sceKernelGetSystemTimeLow();
	
	if (force || (percentage > last_percentage && st >= (last_time+520000)))
	{		
		sprintf(pg_text, "%d%%", percentage);
		last_percentage = percentage;
		last_time = st;
		vlfGuiAddEventHandler(0, -2, DoProgressUpdate, NULL);
	}
}

int DoStatusUpdate(void *param)
{
	vlfGuiSetText(install_status, st_text);
	return VLF_EV_RET_REMOVE_HANDLERS;
}

void SetStatus(char *status)
{
	strcpy(st_text, status);
	vlfGuiAddEventHandler(0, -2, DoStatusUpdate, NULL);
}

void SetPSARProgress(int value, int max, int force)
{
	u32 prog;

	prog = ((88 * value) / max) + 7;

	SetProgress(prog, force);
}

void CancelInstall()
{
	install_thid = -1;
	g_running = 0;
	sceKernelExitDeleteThread(0);
}

void ExtractPrxs(int cbFile, SceUID fd)
{
	int psar_pos = 0;
	int index_dat = 0;
	
	if (pspPSARInit(g_dataPSAR, g_dataOut, g_dataOut2) < 0)
	{
		ErrorExit(1000, "pspPSARInit failed!.\n");
	}   

	int error = 0;

    while (1)
	{
		char name[128];
		int cbExpanded;
		int pos;
		int signcheck;

		int res = pspPSARGetNextFile(g_dataPSAR, cbFile, g_dataOut, g_dataOut2, name, &cbExpanded, &pos, &signcheck);

		if (res < 0)
		{
			if (error)			
				ErrorExit(1000, "PSAR decode error, pos=0x%08X.\n", pos);

			int dpos = pos-psar_pos;
			psar_pos = pos;
			
			error = 1;
			memmove(g_dataPSAR, g_dataPSAR+dpos, PSAR_BUFFER_SIZE-dpos);

			if (sceIoRead(fd, g_dataPSAR+(PSAR_BUFFER_SIZE-dpos), dpos) <= 0)
			{
				ErrorExit(1000, "Error reading PBP.\n");
			}

			pspPSARSetBufferPosition(psar_pos);

			continue;
		}
		else if (res == 0) /* no more files */
		{
			break;
		}
		
		if (!strncmp(name, "com:", 4) && comtable_size > 0)
		{
			if (!FindTablePath(com_table, comtable_size, name+4, name))
			{
				ErrorExit(1000, "Error: cannot find path of %s.\n", name);
			}
		}

		else if (!strncmp(name, "01g:", 4) && _1gtable_size > 0)
		{
			if (!FindTablePath(_1g_table, _1gtable_size, name+4, name))
			{
				ErrorExit(1000, "Error: cannot find path of %s.\n", name);
			}
		}

		else if (!strncmp(name, "02g:", 4) && _2gtable_size > 0)
		{
			if (!FindTablePath(_2g_table, _2gtable_size, name+4, name))
			{
				ErrorExit(1000, "Error: cannot find path of %s.\n", name);
			}
		}

       	char* szFileBase = strrchr(name, '/');
		
		if (szFileBase != NULL)
			szFileBase++;  // after slash
		else
			szFileBase = "err.err";

		if (cbExpanded > 0)
		{
			char szDataPath[128];
			
			if (strstr(name, "flash0:/") == name)
			{
				sprintf(szDataPath, "ms0:/TM/DC7/%s", name+8);
				
				char *p = strstr(szDataPath, "index.dat");
					
				if (p)
				{
					if (!index_dat)
					{
						index_dat = 1;
					}
					else
					{
						*p = 'y';
					}
				}
				
				if (WriteFile(szDataPath, g_dataOut2, cbExpanded) != cbExpanded)
				{
					ErrorExit(1000, "Error writing %s.\n", szDataPath);
				}					  
			}

			else if (strstr(name, "ipl") == name)
			{
				int is2g = (strstr(name, "02h") != NULL);

				if (is2g)
				{
					cbExpanded = pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
					if (cbExpanded <= 0)
					{
						ErrorExit(1000, "Cannot pre-decrypt 2000 IPL\n");
					}
					else
					{
						memcpy(g_dataOut2, g_dataOut, cbExpanded);
					}	
				}

				int cb1 = pspDecryptIPL1(g_dataOut2, g_dataOut, cbExpanded);
				if (cb1 < 0)
				{
					ErrorExit(1000, "Error in IPL decryption.\n");
				}

				int cb2 = pspLinearizeIPL2(g_dataOut, g_dataOut2+0x3000, cb1);
				if (cb2 < 0)
				{
					ErrorExit(1000, "Error in IPL Linearize.\n");
				}

				*(u32 *)(g_dataOut2+0x3000+0x22C) = 0x09038000;
				*(u32 *)(g_dataOut2+0x3000+0x230) = 0;

				sceKernelDcacheWritebackAll();

				if (is2g)
				{
					memcpy(g_dataOut2, ipl_02g, 0x3000);
					if (WriteFile("ms0:/TM/DC7/ipl_02g.bin", g_dataOut2, cb2+0x3000) != (cb2+0x3000))
					{
						ErrorExit(1000, "Error writing 02g ipl.\n");
					}
				}
				else
				{
					memcpy(g_dataOut2, ipl_01g, 0x3000);
					if (WriteFile("ms0:/TM/DC7/ipl_01g.bin", g_dataOut2, cb2+0x3000) != (cb2+0x3000))
					{
						ErrorExit(1000, "Error writing 01g ipl.\n");
					}
				}
			}

			else if (!strcmp(name, "com:00000"))
			{
				comtable_size = pspDecryptTable(g_dataOut2, g_dataOut, cbExpanded, 2);
							
				if (comtable_size <= 0)
				{
					ErrorExit(1000, "Cannot decrypt common table.\n");
				}

				if (comtable_size > sizeof(com_table))
				{
					ErrorExit(1000, "Com table buffer too small. Recompile with bigger buffer.\n");
				}

				memcpy(com_table, g_dataOut2, comtable_size);						
			}
					
			else if (!strcmp(name, "01g:00000"))
			{
				_1gtable_size = pspDecryptTable(g_dataOut2, g_dataOut, cbExpanded, 2);
							
				if (_1gtable_size <= 0)
				{
					ErrorExit(1000, "Cannot decrypt 1g table.\n");
				}

				if (_1gtable_size > sizeof(_1g_table))
				{
					ErrorExit(1000, "1g table buffer too small. Recompile with bigger buffer.\n");
				}

				memcpy(_1g_table, g_dataOut2, _1gtable_size);						
			}
					
			else if (!strcmp(name, "02g:00000"))
			{
				_2gtable_size = pspDecryptTable(g_dataOut2, g_dataOut, cbExpanded, 2);
							
				if (_2gtable_size <= 0)
				{
					ErrorExit(1000, "Cannot decrypt 2g table %08X.\n", _2gtable_size);
				}

				if (_2gtable_size > sizeof(_2g_table))
				{
					ErrorExit(1000, "2g table buffer too small. Recompile with bigger buffer.\n");
				}

				memcpy(_2g_table, g_dataOut2, _2gtable_size);						
			}			
		}

		SetPSARProgress(pos, cbFile, 0);
		scePowerTick(0);
		error = 0;

		if (g_cancel)
		{
			sceIoClose(fd);
			CancelInstall();
			return;
		}
	}

	SetPSARProgress(1, 1, 1);
}

static void Extract401Modules()
{
	int size, i;
	u8 pbp_header[0x28];

	SetStatus("Extracting 4.01 updater modules...");
		
	if (ReadFile("ms0:/401.PBP", 0, pbp_header, sizeof(pbp_header)) != sizeof(pbp_header))
	{
		ErrorExit(1000, "Error reading 401.PBP at root.\n");
	}

	if (g_cancel)
	{
		CancelInstall();
	}

	if (ReadFile("ms0:/401.PBP", *(u32 *)&pbp_header[0x20], g_dataPSAR, PRX_SIZE_401) != PRX_SIZE_401)
	{
		ErrorExit(1000, "Invalid 401.PBP.\n");
	}

	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(10000);

	size = pspDecryptPRX(g_dataPSAR, g_dataPSAR, PRX_SIZE_401);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decrypting 401 updater.\n");
	}

	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(10000);

	size = pspDecryptPRX(g_dataPSAR+0x48C200, g_dataOut, LFLASH_FATFMT_UPDATER_SIZE);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decoding lflash_fatfmt_updater.prx\n");
	}

	size = pspDecompress(g_dataOut, g_dataOut2, SMALL_BUFFER_SIZE);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decompressing lflash_fatfmt_updater.\n");
	}

	if (WriteFile("ms0:/TM/DC7/kd/lflash_fatfmt_updater.prx", g_dataOut2, size) != size)
	{
		ErrorExit(1000, "Error writing lflash_fatfmt_updater.prx.\n");
	}

	for (i = 0; i < LFATFS_UPDATER_SIZE; i++)
	{
		g_dataPSAR[i+0xCD80] ^= 0xF1;
	}

	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(10000);

	size = pspDecryptPRX(g_dataPSAR+0xCD80, g_dataOut, LFATFS_UPDATER_SIZE);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decoding lfatfs_updater.prx\n");
	}

	size = pspDecompress(g_dataOut, g_dataOut2, SMALL_BUFFER_SIZE);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decompressing lfatfs_updater.\n");
	}

	if (WriteFile("ms0:/TM/DC7/kd/lfatfs_updater.prx", g_dataOut2, size) != size)
	{
		ErrorExit(1000, "Error writing lfatfs_updater.prx.\n");
	}

	for (i = 0; i < NAND_UPDATER_SIZE; i++)
	{
		g_dataPSAR[i+0x15D40] ^= 0xF1;
	}

	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(10000);

	size = pspDecryptPRX(g_dataPSAR+0x15D40, g_dataOut, NAND_UPDATER_SIZE);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decoding emc_sm_updater.prx\n");
	}

	size = pspDecompress(g_dataOut, g_dataOut2, SMALL_BUFFER_SIZE);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decompressing emc_sm_updater.\n");
	}
	
	if (WriteFile("ms0:/TM/DC7/kd/emc_sm_updater.prx", g_dataOut2, size) != size)
	{
		ErrorExit(1000, "Error writing emc_sm_updater.prx.\n");
	}
}

static void Extract401PSAR()
{
	SceUID fd;
	
	SetStatus("Extracting 4.01 modules... ");
	
	fd = sceIoOpen("ms0:/401.PBP", PSP_O_RDONLY, 0);
	if (fd < 0)
	{
		ErrorExit(1000, "Incorrect or inexistant 401.PBP at root.\n");
	}
	
	sceIoLseek32(fd, 0x4FC655, PSP_SEEK_SET);
	sceIoRead(fd, g_dataPSAR, PSAR_BUFFER_SIZE);	

	if (g_cancel)
	{
		sceIoClose(fd);
		CancelInstall();
	}

	sceKernelDelayThread(10000);

	if (g_cancel)
	{
		sceIoClose(fd);
		CancelInstall();
	}

	ExtractPrxs(PSAR_SIZE_401, fd);
	sceIoClose(fd);
}

int ReadSector(int sector, void *buf, int count)
{
	SceUID fd = sceIoOpen("msstor0:", PSP_O_RDONLY, 0);
	if (fd < 0)
		return fd;

	sceIoLseek(fd, sector*512, PSP_SEEK_SET);
	int read = sceIoRead(fd, buf, count*512);

	if (read > 0)
		read /= 512;

	sceIoClose(fd);
	return read;
}

int WriteSector(int sector, void *buf, int count)
{
	SceUID fd = sceIoOpen("msstor0:", PSP_O_WRONLY, 0);
	if (fd < 0)
		return fd;

	sceIoLseek(fd, sector*512, PSP_SEEK_SET);
	int written = sceIoWrite(fd, buf, count*512);

	if (written > 0)
		written /= 512;

	sceIoClose(fd);
	return written;
}

char *GetKeyName(u32 key)
{
	if (key == PSP_CTRL_SELECT)
		return "SELECT";

	if (key == PSP_CTRL_START)
		return "START";

	if (key == PSP_CTRL_UP)
		return "UP";

	if (key == PSP_CTRL_DOWN)
		return "DOWN";

	if (key == PSP_CTRL_LEFT)
		return "LEFT";

	if (key == PSP_CTRL_RIGHT)
		return "RIGHT";

	if (key == PSP_CTRL_SQUARE)
		return "SQUARE";

	if (key == PSP_CTRL_CIRCLE)
		return "CIRCLE";

	if (key == PSP_CTRL_CROSS)
		return "CROSS";

	if (key == PSP_CTRL_LTRIGGER)
		return "L";

	if (key == PSP_CTRL_TRIANGLE)
		return "TRIANGLE";

	if (key == PSP_CTRL_RTRIGGER)
		return "R";

	return NULL;
}

int install_iplloader()
{
	int res;
	int abs_sec,ttl_sec;
	int signature;
	int type;
	int free_sectors;

	SetStatus("Installing iplloader... ");

	res = ReadSector(0, g_dataOut, 1);
	if (res != 1)
	{
		ErrorExit(1000, "Error 0x%08X in ReadSector.\n", res);
	}

	abs_sec   = g_dataOut[0x1c6]|(g_dataOut[0x1c7]<<8)|(g_dataOut[0x1c8]<<16)|(g_dataOut[0x1c9]<<24);
	ttl_sec   = g_dataOut[0x1ca]|(g_dataOut[0x1cb]<<8)|(g_dataOut[0x1cc]<<16)|(g_dataOut[0x1cd]<<24);
	signature = g_dataOut[0x1fe]|(g_dataOut[0x1ff]<<8);
	type      = g_dataOut[0x1c2];

	if (signature != 0xAA55)
	{
		ErrorExit(1000, "Invalid signature 0x%04X\n", signature);
	}

	free_sectors = (abs_sec-0x10);
	if (free_sectors < 32)
	{
		ErrorExit(1000, "The install failed.\n"
			            "Contact the Sony Computer Entertainment technical support line for assistance.\n"
						"(ffffffff)\n\n"
						"Just kidding. Use Team C+D mspformat pc tool and run the installer again.");
	}

	if (g_cancel)
		CancelInstall();

	res = WriteSector(0x10, iplloader, 32);
	if (res != 32)
	{
		ErrorExit(1000, "Error 0x%08X in WriteSector.\n", res);
	}

	char *default_config = "NOTHING = \"/TM/DC7/ipl.bin\";";

	SceIoStat stat;

	if (sceIoGetstat("ms0:/TM/config.txt", &stat) < 0)
	{
		WriteFile("ms0:/TM/config.txt", default_config, strlen(default_config));
		return 0;
	}
	
	return 1;
}

int OnProgramFinish(int enter)
{
	if (enter)
	{
		g_running = 0;
		return VLF_EV_RET_REMOVE_HANDLERS;
	}

	return VLF_EV_RET_NOTHING;
}

char text[256];

int OnInstallFinish(void *param)
{
	int key_install = *(int *)param;
	
	vlfGuiCancelBottomDialog();
	vlfGuiRemoveProgressBar(progress_bar);
	vlfGuiRemoveText(progress_text);
	vlfGuiRemoveText(install_status);

	if (key_install)
	{
		install_status = vlfGuiAddText(80, 90, "");
	}
	else
	{
		install_status = vlfGuiAddText(150, 90, "");
	}	
	
	
	vlfGuiSetText(install_status, text);
	vlfGuiCustomBottomDialog(NULL, "Exit", 1, 0, VLF_DEFAULT, OnProgramFinish);
	return VLF_EV_RET_REMOVE_HANDLERS;
}

int OnPaintListenKeys(void *param)
{
	vlfGuiCancelBottomDialog();
	vlfGuiRemoveProgressBar(progress_bar);
	vlfGuiRemoveText(progress_text);
	vlfGuiRemoveText(install_status);

	install_status = vlfGuiAddText(80, 90, "");
	vlfGuiSetText(install_status, text);

	progress_bar = -1;
	progress_text = -1;

	return VLF_EV_RET_REMOVE_HANDLERS;
}

int install_thread(SceSize args, void *argp)
{
	int i;
	
	vlfGuiRemoveText(begin_install_text);
	install_status = vlfGuiAddText(80, 100, "Creating directories...");

	progress_bar = vlfGuiAddProgressBar(136);	
	progress_text = vlfGuiAddText(240, 148, "0%");
	vlfGuiSetTextAlignment(progress_text, VLF_ALIGNMENT_CENTER);

	sceIoMkdir("ms0:/TM", 0777);
	sceIoMkdir("ms0:/TM/DC7", 0777);
	sceIoMkdir("ms0:/TM/DC7/data", 0777);
	sceIoMkdir("ms0:/TM/DC7/data/cert", 0777);
	sceIoMkdir("ms0:/TM/DC7/dic", 0777);
	sceIoMkdir("ms0:/TM/DC7/font", 0777);
	sceIoMkdir("ms0:/TM/DC7/gps", 0777);
	sceIoMkdir("ms0:/TM/DC7/kd", 0777);
	sceIoMkdir("ms0:/TM/DC7/kd/resource", 0777);
	sceIoMkdir("ms0:/TM/DC7/net", 0777);
	sceIoMkdir("ms0:/TM/DC7/net/http", 0777);
	sceIoMkdir("ms0:/TM/DC7/vsh", 0777);
	sceIoMkdir("ms0:/TM/DC7/vsh/etc", 0777);
	sceIoMkdir("ms0:/TM/DC7/vsh/module", 0777);
	sceIoMkdir("ms0:/TM/DC7/vsh/resource", 0777);
	sceIoMkdir("ms0:/TM/DC7/vsh/theme", 0777);
	sceIoMkdir("ms0:/TM/DC7/registry", 0777);

	if (g_cancel)
		CancelInstall();
	
	sceKernelDelayThread(800000);
	SetProgress(1, 1);	

	Extract401Modules();
	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(250000);
	SetProgress(7, 1);

	Extract401PSAR();
	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(250000);
	SetProgress(95, 1);

	SetStatus("Writing custom modules...");

	for (i = 0; i < N_FILES; i++)
	{
		if (g_cancel)
			CancelInstall();

		if (WriteFile(m33files[i].path, m33files[i].buf, m33files[i].size) != m33files[i].size)
		{
			ErrorExit(1000, "Error writing %s.\n", m33files[i].path);
		}

		if (i == 10)
		{
			SetProgress(96, 1);
		}
		else if (i == 20)
		{
			SetProgress(97, 1);
		}
		
		sceKernelDelayThread(10000);
	}

	sceKernelDelayThread(250000);
	SetProgress(98, 1);

	SetStatus("Copying registry... ");

	i = ReadFile("flash1:/registry/system.dreg", 0, g_dataOut, SMALL_BUFFER_SIZE);
	if (i <= 0)
	{
		ErrorExit(1000, "Error reading system.dreg.\n");
	}

	if (g_cancel)
		CancelInstall();

	if (WriteFile("ms0:/TM/DC7/registry/system.dreg", g_dataOut, i) != i)
	{
		ErrorExit(1000, "Error writing system.dreg.\n");
	}

	i = ReadFile("flash1:/registry/system.ireg", 0, g_dataOut, SMALL_BUFFER_SIZE);
	if (i <= 0)
	{
		ErrorExit(1000, "Error reading system.ireg.\n");
	}

	if (g_cancel)
		CancelInstall();
	
	if (WriteFile("ms0:/TM/DC7/registry/system.ireg", g_dataOut, i) != i)
	{
		ErrorExit(1000, "Error writing system.ireg.\n");
	}

	i = ReadFile("flash2:/act.dat", 0, g_dataOut, SMALL_BUFFER_SIZE);
	if (i > 0)
	{
		if (WriteFile("ms0:/TM/DC7/act.dat", g_dataOut, i) != i)
		{
			ErrorExit(1000, "Error writing act.dat.\n");
		}
	}

	if (g_cancel)
		CancelInstall();

	sceKernelDelayThread(380000);
	SetProgress(99, 1);

	int key_install = install_iplloader();
	if (g_cancel)
		CancelInstall();

	install_thid = -1;
	sceKernelDelayThread(380000);
	SetProgress(100, 1);
	sceKernelDelayThread(1200000);

	strcpy(text, "");	

	if (key_install)
	{
		SceCtrlData pad;
		char buf[256];		
		
		strcpy(text, "Please keep pressed for some seconds\n"
			         "the key/s which you want to use to\n"
					 "boot DC7... ");
		
		vlfGuiAddEventHandler(0, -1, OnPaintListenKeys, NULL);	
		
		while (1)
		{		
			sceKernelDelayThread(3000000);
			
			sceCtrlPeekBufferPositive(&pad, 1);
			
			if (pad.Buttons != 0)
				break;
		}

		strcpy(buf, "");

		int first = 1;

		for (i = 0; i < 32; i++)
		{
			if ((pad.Buttons & (1 << i)))
			{
				if (GetKeyName(1 << i))
				{
					if (!first)
						strcat(buf, "+");
					else
						first = 0;
					
					strcat(buf, GetKeyName(1 << i));
				}
			}
		}

		strcat(text, buf);
		SetStatus(text);
		sceKernelDelayThread(850000);

		strcat(buf, " = \"/TM/DC7/ipl.bin\";\r\n");
		memcpy(g_dataOut, buf, strlen(buf));

		int size = ReadFile("ms0:/TM/config.txt", 0, g_dataOut+strlen(buf), SMALL_BUFFER_SIZE);

		if (size >= 0)
		{
			WriteFile("ms0:/TM/config.txt", g_dataOut, size+strlen(buf));
		}
		
		strcat(text, "\n");
		sceKernelDelayThread(350000);
	}
	
	strcat(text, "\n");
	strcat(text, "Installation completed.");
	sceKernelDelayThread(100000);
	vlfGuiAddEventHandler(0, -1, OnInstallFinish, &key_install);
	sceKernelDelayThread(800000);

	install_thid = -1;
	return sceKernelExitDeleteThread(0);
}

int OnCancelInstall(int enter)
{
	if (!enter)
	{
		if (vlfGuiMessageDialog("Are you sure you want to cancel?", VLF_MD_TYPE_NORMAL | VLF_MD_BUTTONS_YESNO | VLF_MD_INITIAL_CURSOR_NO) == VLF_MD_YES)
		{
			if (install_thid >= 0)
			{
				int is = install_status;

				install_status = -1;				
				vlfGuiSetText(is, "Cancelling...");
				g_cancel = 1;				
			}
			else
			{
				g_running = 0;
			}

			return VLF_EV_RET_REMOVE_HANDLERS;
		}
	}
	
	return VLF_EV_RET_NOTHING;
}

int OnInstallBegin(void *param)
{
	install_thid = sceKernelCreateThread("install_thread", install_thread, 0x18, 0x10000, 0, NULL);
	if (install_thid >= 0)
	{
		sceKernelStartThread(install_thid, 0, 0);
	}

	return VLF_EV_RET_REMOVE_HANDLERS;
}

int app_main()
{
	int theme;
	
	vlfGuiSystemSetup(1, 1, 1);

	if (sceKernelDevkitVersion() < 0x02070110)
	{
		ErrorExit(1000, "This program requires 2.71 or higher.\n",
			             "If you are in a cfw, please reexecute psardumper on the higher kernel.\n");
	}

	SceUID mod = LoadStartModule("libpsardumper.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (mod < 0)
	{
		ErrorExit(1000, "Error 0x%08X loading/starting libpsardumper.prx.\n", mod);
	}

	mod = LoadStartModule("pspdecrypt.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (mod < 0)
	{
		ErrorExit(1000, "Error 0x%08X loading/starting pspdecrypt.prx.\n", mod);
	}

	if (get_registry_value("/CONFIG/SYSTEM/XMB/THEME", "custom_theme_mode", &theme))
	{	
		if (theme != 0)
		{
			ErrorExit(1000, "Your psp has a custom theme set.\n"
							"Turn the theme off before running this program.\n");
		}
	}

	g_dataOut = malloc64(SMALL_BUFFER_SIZE);
	g_dataOut2 = malloc64(SMALL_BUFFER_SIZE);

	if (!g_dataOut || !g_dataOut2)
	{
		ErrorExit(1000, "There is not enough RAM memory.\n");
	}

	PSAR_BUFFER_SIZE = (28*1024*1024);
	g_dataPSAR = malloc64(PSAR_BUFFER_SIZE);

	if (!g_dataPSAR)
	{
		PSAR_BUFFER_SIZE = 6*1024*1024;
		g_dataPSAR = malloc64(PSAR_BUFFER_SIZE);
	}

	if (!g_dataPSAR)
	{
		ErrorExit(1000, "There is not enough RAM memory.\n");
	}

	int tt = vlfGuiAddText(0, 0, "Despertar del Cementerio");
	int tp = vlfGuiAddPictureResource("update_plugin", "tex_update_icon", 0, 0);
	//int tp = vlfGuiAddPictureFile("m33.tga", 0, 0);
	vlfGuiChangeCharacterByButton('*', VLF_ENTER);
	begin_install_text = vlfGuiAddText(110, 118, "Press * to begin the installation");

	vlfGuiSetTitleBar(tt, tp, 1, 0);
	vlfGuiBottomDialog(VLF_DI_CANCEL, -1, 1, 0, VLF_DEFAULT, OnCancelInstall);
	vlfGuiAddEventHandler(PSP_CTRL_ENTER, 0, OnInstallBegin, NULL);

	g_running = 1;

	while (g_running)
	{		
		vlfGuiDrawFrame();
	}

	if (error_msg[0])
	{
		if (install_thid >= 0)
		{
			sceKernelDeleteThread(install_thid);
			install_thid = -1;
		}
		
		vlfGuiMessageDialog(error_msg, VLF_MD_TYPE_ERROR | VLF_MD_BUTTONS_NONE);
		sceKernelDelayThread(1000*1000);
	}

	sceKernelExitGame();

   	return 0;
}


