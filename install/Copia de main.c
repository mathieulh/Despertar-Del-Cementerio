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

#include "ipl.h"
#include "pspdecrypt_module.h"
#include "pspbtcnf.h"
#include "libpsardumper.h"
#include "resurrection.h"
#include "flashemu.h"
#include "backup.h"
#include "hibari.h"
#include "iplloader.h"

PSP_MODULE_INFO("VResurrection_Manager", 0x800, 2, 0);
PSP_MAIN_THREAD_ATTR(0);

#define printf    pspDebugScreenPrintf

#define PRX_SIZE_390				5326560
#define LFLASH_FATFMT_UPDATER_SIZE	0x13B0	
#define NAND_UPDATER_SIZE			0x2190
#define LFATFS_UPDATER_SIZE			0x7860

#define PRX_SIZE_150	3737904
#define MSIPL_SIZE		241664
#define IPL150_SIZE		225280
#define IPL150_LIN		213488

#define PSAR_SIZE_150	10149440
#define PSAR_SIZE_340	16820272

#define N_FILES	9

typedef struct
{
	char *path;
	u8 *buf;
	int size;
} M33File;

M33File m33files[N_FILES] =
{
	{ "ms0:/TM/DC5/kd/resurrection.elf", resurrection, sizeof(resurrection) },
	{ "ms0:/TM/DC5/kd/hibari.prx", hibari, sizeof(hibari) },
	{ "ms0:/TM/DC5/kd/pspdecrypt.prx", pspdecrypt, sizeof(pspdecrypt) },
	{ "ms0:/TM/DC5/kd/libpsardumper.prx", libpsardumper, sizeof(libpsardumper) },
	{ "ms0:/TM/DC5/kd/pspbtcnf.txt", pspbtcnf, sizeof(pspbtcnf) },
	{ "ms0:/TM/DC5/kd/pspbtcnf_game.txt", pspbtcnf_game, sizeof(pspbtcnf_game) },
	{ "ms0:/TM/DC5/kd/pspbtcnf_updater.txt", pspbtcnf_updater, sizeof(pspbtcnf_updater) },
	{ "ms0:/TM/DC5/kd/flashemu.prx", flashemu, sizeof(flashemu) },
	{ "ms0:/TM/DC5/kd/backup.elf", backup, sizeof(backup) }
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

static u8 msipl[MSIPL_SIZE]; 

static char com_table[0x4000];
static int comtable_size;

static char _1g_table[0x4000];
static int _1gtable_size;

static char _2g_table[0x4000];
static int _2gtable_size;

u8 updater_prx_390_md5[16] = 
{
	0x0D, 0x78, 0x75, 0x6A, 0x3E, 0x44, 0x2E, 0x0E, 
	0x9E, 0xF8, 0xD4, 0x1F, 0x41, 0xF6, 0xFC, 0x09
};

u8 updater_prx_150_md5[16] = 
{
	0x9F, 0xB7, 0xC7, 0xE9, 0x96, 0x79, 0x7F, 0xF2,
	0xD7, 0xFF, 0x4D, 0x14, 0xDD, 0xF5, 0xD3, 0xF2
};

u8 psar_150_md5[16] = 
{
	0xF3, 0x4A, 0x9A, 0xC3, 0x02, 0x86, 0x2D, 0x16, 
	0xF1, 0xE3, 0x57, 0xCB, 0x9A, 0xEE, 0x32, 0x8E
};

u8 psar_340_md5[16] = 
{
	0xEF, 0xD6, 0x9D, 0x1F, 0xEF, 0x7E, 0xA0, 0x52, 
	0x6C, 0x4D, 0x75, 0x52, 0xF6, 0x5A, 0x81, 0x10
};

// Gui vars
int begin_install_text;
int install_status;
int progress_bar, progress_text;

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

#define NOWRITE_150	70

char *nowrite_150[NOWRITE_150] =
{
	"ata.prx",
	"audio.prx",
	"audiocodec.prx",
	"blkdev.prx",
	"chkreg.prx",
	"codec.prx",
	"ifhandle.prx",
	"impose.prx",
	"isofs.prx",
	"lflash_fatfmt.prx",
	"libatrac3plus.prx",
	"libhttp.prx",
	"libparse_http.prx",
	"libparse_uri.prx",
	"libupdown.prx",
	"me_for_vsh.prx",
	"me_wrapper.prx",
	"mebooter.prx",
	"mebooter_umdvideo.prx",
	"mediaman.prx",
	"mediasync.prx",
	"mgr.prx",
	"mpeg_vsh.prx",
	"mpegbase.prx",
	"msaudio.prx",
	"openpsid.prx",
	"peq.prx",
	"pspnet.prx",
	"pspnet_adhoc.prx",
	"pspnet_adhoc_auth.prx",
	"pspnet_adhoc_download.prx",
	"pspnet_adhoc_matching.prx",
	"pspnet_adhocctl.prx",
	"pspnet_ap_dialog_dummy.prx",
	"pspnet_apctl.prx",
	"pspnet_inet.prx",
	"pspnet_inet.prx",
	"reboot.prx",
	"semawm.prx",
	"sircs.prx",
	"uart4.prx",
	"umdman.prx",
	"umd9660.prx",
	"usb.prx",
	"usbstor.prx",
	"usbstorboot.prx",
	"usbstormgr.prx",
	"usbstorms.prx",
	"utility.prx",
	"vaudio.prx",
	"vaudio_game.prx",
	"videocodec.prx",
	"vshbridge.prx",
	"wlan.prx",
	"syscon.prx",
	"emc_sm.prx",
	"led.prx",
	"display.prx",
	"clockgen.prx",
	"ctrl.prx",
	"lcdc.prx",
	"gpio.prx",
	"i2c.prx",
	"sysreg.prx",
	"dmacman.prx",
	"dmacplus.prx",
	"pwm.prx",
	"emc_ddr.prx",
	"power.prx",
	"systimer.prx"
};

#define WRITE_340	16

char *write_340[WRITE_340] =
{
	"syscon.prx",
	"emc_sm.prx",
	"led.prx",
	"display.prx",
	"clockgen.prx",
	"ctrl.prx",
	"lcdc.prx",
	"gpio.prx",
	"i2c.prx",
	"sysreg.prx",
	"dmacman.prx",
	"dmacplus.prx",
	"pwm.prx",
	"emc_ddr.prx",
	"power.prx",
	"systimer.prx"
};

// display, clockgen, ctrl, lcdc, gpio, i2c, sysreg, dmacman, dmacplus, pwm, emc_ddr, power, systimer

static int WritePrx(char *name, int is150)
{
	int i;

	if (is150)
	{
		for (i = 0; i < NOWRITE_150; i++)
		{
			if (strcmp(name, nowrite_150[i]) == 0)
				return 0;
		}

		return 1;
	}

	for (i = 0; i < WRITE_340; i++)
	{
		if (strcmp(name, write_340[i]) == 0)
			return 1;
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

void SetProgress(int percentage, int force)
{
	int st =  sceKernelGetSystemTimeLow();
	
	if (force || (percentage > last_percentage && st >= (last_time+600000)))
	{	
		vlfGuiProgressBarSetProgress(progress_bar, percentage);
		vlfGuiSetTextF(progress_text, "%d%%", percentage);

		last_percentage = percentage;
		last_time = st;
	}
}

void SetPSARProgress(int is150, int value, int max, int force)
{
	int prog;

	prog = ((44 * value) / max);

	if (is150)
		prog += 8;

	else
		prog += 52;

	SetProgress(prog, force);
}

void ExtractPrxs(int cbFile, int is150, SceUID fd)
{
	int psar_pos = 0;
	
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
			
			if (strncmp(name, "flash0:/kd/", 11) == 0 &&
				   strncmp(name, "flash0:/kd/resource", 19) != 0)
			{
				if (WritePrx(szFileBase, is150))
				{				
					int lfatfs = 0;
					
					sprintf(szDataPath, "ms0:/TM/DC5/kd/%s", szFileBase);

					if (is150)
					{
					   if (WriteFile(szDataPath, g_dataOut2, cbExpanded) != cbExpanded)
					   {
							ErrorExit(1000, "Error writing %s.\n", szDataPath);
					   }

					   if (strcmp(name, "flash0:/kd/lfatfs.prx") == 0)
					   {
						   lfatfs = 1;
						   strcpy(szDataPath, "ms0:/TM/DC5/kd/lfatfs_patched.prx");
						}
					}					

					if (lfatfs || !is150)
					{
						cbExpanded = pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
						if (cbExpanded <= 0)
						{
							ErrorExit(1000, "Error decrypting %s.\n", szFileBase);
						}

						cbExpanded = pspDecompress(g_dataOut, g_dataOut2, SMALL_BUFFER_SIZE);

						if (cbExpanded <= 0)
						{
							ErrorExit(1000, "Error 0x%08X decompressing %s.\n", cbExpanded, szFileBase);
						}

						if (lfatfs)
						{
							_sw(0x3E00008, (u32)g_dataOut2+0x7AC); // jr ra
							_sw(0x1021, (u32)g_dataOut2+0x7B0); // mov v0, zero
						}

						if (WriteFile(szDataPath, g_dataOut2, cbExpanded) != cbExpanded)
						{
							ErrorExit(1000, "Error writing %s.\n", szDataPath);
						}
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

		SetPSARProgress(is150, pos, cbFile, 0);
		scePowerTick(0);
		error = 0;

		if (g_cancel)
			return;
	}

	SetPSARProgress(is150, 1, 1, 1);
}

static void Extract390Modules()
{
	int size, i;
	u8 md5[16];
	u8 pbp_header[0x28];

	vlfGuiSetText(install_status, "Extracting 390 updater modules...");
		
	if (ReadFile("ms0:/390.PBP", 0, pbp_header, sizeof(pbp_header)) != sizeof(pbp_header))
	{
		ErrorExit(1000, "Error reading 390.PBP at root.\n");
	}

	if (g_cancel)
		return;

	if (ReadFile("ms0:/390.PBP", *(u32 *)&pbp_header[0x20], g_dataPSAR, PRX_SIZE_390) != PRX_SIZE_390)
	{
		ErrorExit(1000, "Invalid 390.PBP.\n");
	}

	if (g_cancel)
		return;

	sceKernelUtilsMd5Digest(g_dataPSAR, PRX_SIZE_390, md5);

	if (memcmp(md5, updater_prx_390_md5, 16) != 0)
	{
		ErrorExit(1000, "Invalid 390.PBP (2).\n");
	}

	size = pspDecryptPRX(g_dataPSAR, g_dataPSAR, PRX_SIZE_390);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decrypting 390 updater.\n");
	}

	if (g_cancel)
		return;

	if (WriteFile("ms0:/TM/DC5/kd/lflash_fatfmt_updater.prx", g_dataPSAR+0x4A7700, LFLASH_FATFMT_UPDATER_SIZE) != LFLASH_FATFMT_UPDATER_SIZE)
	{
		ErrorExit(1000, "Error writing lflash_fatfmt_updater.prx.\n");
	}

	for (i = 0; i < LFATFS_UPDATER_SIZE; i++)
	{
		g_dataPSAR[i+0xD340] ^= 0xF1;
	}

	if (g_cancel)
		return;

	if (WriteFile("ms0:/TM/DC5/kd/lfatfs_updater.prx", g_dataPSAR+0xD340, LFATFS_UPDATER_SIZE) != LFATFS_UPDATER_SIZE)
	{
		ErrorExit(1000, "Error writing lfatfs_updater.prx.\n");
	}

	for (i = 0; i < NAND_UPDATER_SIZE; i++)
	{
		g_dataPSAR[i+0x14BC0] ^= 0xF1;
	}

	if (g_cancel)
		return;

	if (WriteFile("ms0:/TM/DC5/kd/nand_updater.prx", g_dataPSAR+0x14BC0, NAND_UPDATER_SIZE) != NAND_UPDATER_SIZE)
	{
		ErrorExit(1000, "Error writing nand_updater.prx.\n");
	}
}

static void BuildIpl()
{
	int size, i;
	u8  md5[16];

	vlfGuiSetText(install_status, "Building IPL...");
	
	if (ReadFile("ms0:/150.PBP", 0x3251, g_dataPSAR, PRX_SIZE_150) != PRX_SIZE_150)
	{
		ErrorExit(1000, "Incorrect or inexistant 150.PBP at root.\n");
	}	

	if (g_cancel)
		return;

	sceKernelUtilsMd5Digest(g_dataPSAR, PRX_SIZE_150, md5);

	if (memcmp(md5, updater_prx_150_md5, 16) != 0)
	{
		ErrorExit(1000, "Incorrect 150.PBP.\n");
	}

	size = pspDecryptPRX(g_dataPSAR, g_dataPSAR, PRX_SIZE_150);
	if (size <= 0)
	{
		ErrorExit(1000, "Error decrypting 150 updater.\n");
	}

	for (i = 0; i < size-20; i++)
	{
		if (memcmp(g_dataPSAR+i, "~PSP", 4) == 0)
		{
			if (strcmp((char *)g_dataPSAR+i+0x0A, "IplUpdater") == 0)
			{
				size = *(int *)&g_dataPSAR[i+0x2C];
				memcpy(g_dataOut, g_dataPSAR+i, size);
				break;
			}
		}
	}

	if (i == (size-20))
	{
		ErrorExit(1000, "Curious error.\n");
	}

	if (pspDecryptPRX(g_dataOut, g_dataOut2, size) < 0)
	{
		ErrorExit(1000, "Error decrypting ipl_update.prx.\n");
	}

	if (pspDecryptIPL1(g_dataOut2+0x980, g_dataOut, IPL150_SIZE) != IPL150_SIZE)
	{
		ErrorExit(1000, "Error decrypting 150 ipl.\n");
	}

	if (pspLinearizeIPL2(g_dataOut, g_dataOut2, IPL150_SIZE) != IPL150_LIN)
	{
		ErrorExit(1000, "Error in linearize.\n");		
	}

	u32 *code = (u32 *)(g_dataOut2);
	code[0xE0/4] = 0x09038000; // j 0x040e0000
	code[0xE4/4] = 0; // nop

	sceKernelDcacheWritebackAll();

	memcpy(msipl, custom_block, sizeof(custom_block));
	memcpy(msipl+sizeof(custom_block), g_dataOut2, IPL150_LIN);

	if (g_cancel)
		return;
	
	if (WriteFile("ms0:/TM/DC5/ipl.bin", msipl, IPL150_LIN+sizeof(custom_block)) != IPL150_LIN+sizeof(custom_block))
	{
		ErrorExit(1000, "Error creating ipl.\n");
	}
}

static void Extract150PSAR()
{
	u8 md5[16];
	SceUID fd;
		
	vlfGuiSetText(install_status, "Extracting 1.50 kernel subset... ");
	
	fd = sceIoOpen("ms0:/150.PBP", PSP_O_RDONLY, 0);
	if (fd < 0)
	{
		ErrorExit(1000, "Incorrect or inexistant 150.PBP at root.\n");
	}

	sceIoLseek32(fd, 0x393B81, PSP_SEEK_SET);
	sceIoRead(fd, g_dataPSAR, PSAR_BUFFER_SIZE);

	if (g_cancel)
	{
		sceIoClose(fd);
		return;
	}

	sceKernelUtilsMd5Digest(g_dataPSAR, 5*1024*1024, md5);

	if (memcmp(md5, psar_150_md5, 16) != 0)
	{
		ErrorExit(1000, "Incorrect or corrupted 150.PBP.\n");
	}

	if (g_cancel)
	{
		sceIoClose(fd);
		return;
	}

	ExtractPrxs(PSAR_SIZE_150, 1, fd);	
	sceIoClose(fd);
}

static void Extract340PSAR()
{
	u8 md5[16];
	SceUID fd;
	
	vlfGuiSetText(install_status, "Extracting 3.40 kernel subset... ");
	
	fd = sceIoOpen("ms0:/340.PBP", PSP_O_RDONLY, 0);
	if (fd < 0)
	{
		ErrorExit(1000, "Incorrect or inexistant 340.PBP at root.\n");
	}
	
	sceIoLseek32(fd, 0x4FC6C5, PSP_SEEK_SET);
	sceIoRead(fd, g_dataPSAR, PSAR_BUFFER_SIZE);	

	if (g_cancel)
	{
		sceIoClose(fd);
		return;
	}
    
	sceKernelUtilsMd5Digest(g_dataPSAR, 5*1024*1024, md5);

	if (memcmp(md5, psar_340_md5, 16) != 0)
	{
		ErrorExit(1000, "Incorrect or corrupted 340.PBP.\n");
	}

	if (g_cancel)
	{
		sceIoClose(fd);
		return;
	}

	ExtractPrxs(PSAR_SIZE_340, 0, fd);
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

	vlfGuiSetText(install_status, "Installing iplloader... ");

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
		return 0;

	res = WriteSector(0x10, iplloader, 32);
	if (res != 32)
	{
		ErrorExit(1000, "Error 0x%08X in WriteSector.\n", res);
	}

	char *default_config = "NOTHING = \"/TM/DC5/ipl.bin\";";

	SceIoStat stat;

	if (sceIoGetstat("ms0:/TM/config.txt", &stat) < 0)
	{
		WriteFile("ms0:/TM/config.txt", default_config, strlen(default_config));
		return 0;
	}
	
	return 1;
}

void OnProgramFinish()
{
	g_running = 0;
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
	sceIoMkdir("ms0:/TM/DC5", 0777);
	sceIoMkdir("ms0:/TM/DC5/kd", 0777);
	sceIoMkdir("ms0:/TM/DC5/registry", 0777);

	if (g_cancel)
		goto exit_thread;
	
	sceKernelDelayThread(800000);
	SetProgress(1, 1);	

	Extract390Modules();
	if (g_cancel)
		goto exit_thread;

	sceKernelDelayThread(250000);
	SetProgress(7, 1);

	BuildIpl();
	if (g_cancel)
		goto exit_thread;

	sceKernelDelayThread(250000);
	SetProgress(8, 1);

	Extract150PSAR();
	if (g_cancel)
		goto exit_thread;

	Extract340PSAR();
	if (g_cancel)
		goto exit_thread;

	sceKernelDelayThread(250000);
	SetProgress(97, 1);

	vlfGuiSetText(install_status, "Writing custom modules...");

	for (i = 0; i < N_FILES; i++)
	{
		if (g_cancel)
			goto exit_thread;

		if (WriteFile(m33files[i].path, m33files[i].buf, m33files[i].size) != m33files[i].size)
		{
			ErrorExit(1000, "Error writing %s.\n", m33files[i].path);
		}	
	}

	sceKernelDelayThread(250000);
	SetProgress(98, 1);

	vlfGuiSetText(install_status, "Copying registry... ");

	i = ReadFile("flash1:/registry/system.dreg", 0, g_dataOut, SMALL_BUFFER_SIZE);
	if (i <= 0)
	{
		ErrorExit(1000, "Error reading system.dreg.\n");
	}

	if (g_cancel)
		goto exit_thread;

	if (WriteFile("ms0:/TM/DC5/registry/system.dreg", g_dataOut, i) != i)
	{
		ErrorExit(1000, "Error writing system.dreg.\n");
	}

	i = ReadFile("flash1:/registry/system.ireg", 0, g_dataOut, SMALL_BUFFER_SIZE);
	if (i <= 0)
	{
		ErrorExit(1000, "Error reading system.ireg.\n");
	}

	if (g_cancel)
		goto exit_thread;
	
	if (WriteFile("ms0:/TM/DC5/registry/system.ireg", g_dataOut, i) != i)
	{
		ErrorExit(1000, "Error writing system.ireg.\n");
	}

	i = ReadFile("flash2:/act.dat", 0, g_dataOut, SMALL_BUFFER_SIZE);
	if (i > 0)
	{
		if (WriteFile("ms0:/TM/DC5/registry/act.dat", g_dataOut, i) != i)
		{
			ErrorExit(1000, "Error writing act.dat.\n");
		}
	}

	if (g_cancel)
		goto exit_thread;

	sceKernelDelayThread(250000);
	SetProgress(99, 1);

	int key_install = install_iplloader();
	if (g_cancel)
		goto exit_thread;

	sceKernelDelayThread(250000);
	SetProgress(100, 1);
	sceKernelDelayThread(1200000);

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

	char text[256];

	strcpy(text, "");

	vlfGuiCancelBottomDialog();

	if (key_install)
	{
		SceCtrlData pad;
		char buf[256];		
		
		strcpy(text, "Please keep pressed for some seconds\n"
			         "the key/s which you want to use to\n"
					 "boot DC6... ");
		
		vlfGuiSetText(install_status, text);
		
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
		sceKernelDelayThread(450000);
		vlfGuiSetText(install_status, text);

		strcat(buf, " = \"/TM/DC5/ipl.bin\";\r\n");
		memcpy(g_dataOut, buf, strlen(buf));

		int size = ReadFile("ms0:/TM/config.txt", 0, g_dataOut+strlen(buf), SMALL_BUFFER_SIZE);

		if (size >= 0)
		{
			WriteFile("ms0:/TM/config.txt", g_dataOut, size+strlen(buf));
		}
		
		strcat(text, "\n");
		sceKernelDelayThread(250000);
	}
	
	strcat(text, "\n");
	strcat(text, "Installation completed.\nPress * to exit.");
	sceKernelDelayThread(400000);
	vlfGuiSetText(install_status, text);
	sceKernelDelayThread(300000);
	vlfGuiAddEventHandler(PSP_CTRL_ENTER, 0, OnProgramFinish);

exit_thread:
		
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
				sceKernelWaitThreadEnd(install_thid, NULL);
			}
		
			g_running = 0;
			return 0;
		}
	}
	
	return 0;
}

void OnInstallBegin()
{
	install_thid = sceKernelCreateThread("install_thread", install_thread, 0x18, 0x10000, 0, NULL);
	if (install_thid >= 0)
	{
		sceKernelStartThread(install_thid, 0, 0);
	}

	vlfGuiRemoveEventHandler(OnInstallBegin);
}

int app_main()
{
	int theme;
	
	vlfGuiSystemSetup(1, 1);

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

	int tt = vlfGuiAddText(0, 0, "Despertar del Cementerio V6");
	//int tp = vlfGuiAddPictureResource("update_plugin", "tex_update_icon", 0, 0);
	int tp = vlfGuiAddPictureFile("m33.tga", 0, 0);
	vlfGuiChangeCharacterByButton('*', VLF_ENTER);
	begin_install_text = vlfGuiAddText(110, 118, "Press * to begin the installation");

	vlfGuiSetTitleBar(tt, tp, 1, 0);
	vlfGuiBottomDialog(VLF_DI_CANCEL, -1, 0, OnCancelInstall);
	vlfGuiAddEventHandler(PSP_CTRL_ENTER, 0, OnInstallBegin);

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


