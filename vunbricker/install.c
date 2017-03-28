#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspsuspend.h>
#include <psppower.h>
#include <pspreg.h>
#include <psprtc.h>
#include <psputils.h>
#include <systemctrl.h>
#include <kubridge.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <libpsardumper.h>
#include <pspdecrypt.h>
#include <pspipl_update.h>
#include <vlf.h>

#include "dcman.h"
#include "main.h"
#include "install.h"

#include "ipl_01g.h"
#include "ipl_02g.h"
#include "es_recovery.h"
#include "galaxy.h"
#include "idcanager.h"
#include "march33.h"
#include "popcorn.h"
#include "pspbtcnf_01g.h"
#include "pspbtcnf_02g.h"
#include "recovery.h"
#include "satelite.h"
#include "systemctrl_01g.h"
#include "systemctrl_02g.h"
#include "usbdevice.h"
#include "vshctrl.h"

extern u8 *big_buffer;
extern u8 *sm_buffer1, *sm_buffer2;
extern int status, progress_text, progress_bar;

typedef struct
{
	char *path;
	u8 *buf;
	int size;
} M33File;

#define N_FILES		12

M33File m33_01g[N_FILES] =
{
	{ "flach0:/kd/pspbtjnf.bin", pspbtjnf_01g, sizeof(pspbtjnf_01g) },
	{ "flach0:/kd/pspbtknf.bin", pspbtknf_01g, sizeof(pspbtknf_01g) },
	{ "flach0:/kd/pspbtlnf.bin", pspbtlnf_01g, sizeof(pspbtlnf_01g) },
	{ "flach0:/kd/systemctrl.prx", systemctrl_01g, sizeof(systemctrl_01g) },
	{ "flach0:/kd/vshctrl.prx", vshctrl, sizeof(vshctrl) },
	{ "flach0:/kd/usbdevice.prx", usbdevice, sizeof(usbdevice) },
	{ "flach0:/vsh/module/satelite.prx", satelite, sizeof(satelite) },
	{ "flach0:/vsh/module/recovery.prx", recovery, sizeof(recovery) },
	{ "flach0:/kd/popcorn.prx", popcorn, sizeof(popcorn) },
	{ "flach0:/kd/march33.prx", march33, sizeof(march33) },
	{ "flach0:/kd/idcanager.prx", idcanager, sizeof(idcanager) },
	{ "flach0:/kd/galaxy.prx", galaxy, sizeof(galaxy) }
};

M33File m33_02g[N_FILES] =
{
	{ "flach0:/kd/pspbtjnf_02g.bin", pspbtjnf_02g, sizeof(pspbtjnf_02g) },
	{ "flach0:/kd/pspbtknf_02g.bin", pspbtknf_02g, sizeof(pspbtknf_02g) },
	{ "flach0:/kd/pspbtlnf_02g.bin", pspbtlnf_02g, sizeof(pspbtlnf_02g) },
	{ "flach0:/kd/systemctrl_02g.prx", systemctrl_02g, sizeof(systemctrl_02g) },
	{ "flach0:/kd/vshctrl.prx", vshctrl, sizeof(vshctrl) },
	{ "flach0:/kd/usbdevice.prx", usbdevice, sizeof(usbdevice) },
	{ "flach0:/vsh/module/satelite.prx", satelite, sizeof(satelite) },
	{ "flach0:/vsh/module/recovery.prx", recovery, sizeof(recovery) },
	{ "flach0:/kd/popcorn.prx", popcorn, sizeof(popcorn) },
	{ "flach0:/kd/march33.prx", march33, sizeof(march33) },
	{ "flach0:/kd/idcanager.prx", idcanager, sizeof(idcanager) },
	{ "flach0:/kd/galaxy.prx", galaxy, sizeof(galaxy) }
};

int LoadUpdaterModules(int ofw)
{
	SceUID mod = sceKernelLoadModule("flash0:/kd/emc_sm_updater.prx", 0, NULL);
	if (mod < 0 && mod != SCE_KERNEL_ERROR_EXCLUSIVE_LOAD)
		return mod;

	if (mod >= 0)
	{
		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
		if (mod < 0)
			return mod;
	}

	if (!ofw && kuKernelGetModel() == 0)
		dcPatchModule("sceNAND_Updater_Driver", 1, 0x0D7E, 0xAC60);
	else
		dcPatchModule("sceNAND_Updater_Driver", 1, 0x0D7E, 0xAC64);

	mod = sceKernelLoadModule("flash0:/kd/lfatfs_updater.prx", 0, NULL);
	if (mod < 0 && mod != SCE_KERNEL_ERROR_EXCLUSIVE_LOAD)
		return mod;

	dcPatchModuleString("sceLFatFs_Updater_Driver", "flash", "flach");

	if (mod >= 0)
	{
		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
		if (mod < 0)
			return mod;
	}

	mod = sceKernelLoadModule("flash0:/kd/lflash_fatfmt_updater.prx", 0, NULL);
	if (mod < 0 && mod != SCE_KERNEL_ERROR_EXCLUSIVE_LOAD)
		return mod;

	sceKernelDelayThread(10000);

	if (mod >= 0)
	{
		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
		if (mod < 0)
			return mod;
	}

	mod = sceKernelLoadModule("flash0:/kd/ipl_update.prx", 0, NULL);
	if (mod < 0 && mod != SCE_KERNEL_ERROR_EXCLUSIVE_LOAD)
		return mod;

	if (mod >= 0)
	{
		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
		if (mod < 0)
			return mod;
	}

	mod = sceKernelLoadModule("flash0:/kd/libpsardumper.prx", 0, NULL);
	if (mod < 0 && mod != SCE_KERNEL_ERROR_EXCLUSIVE_LOAD)
		return mod;

	if (mod >= 0)
	{
		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
		if (mod < 0)
			return mod;
	}

	mod = sceKernelLoadModule("flash0:/kd/pspdecrypt.prx", 0, NULL);
	if (mod < 0 && mod != SCE_KERNEL_ERROR_EXCLUSIVE_LOAD)
		return mod;

	if (mod >= 0)
	{
		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
		if (mod < 0)
			return mod;
	}

	return 0;
}

void SetInstallProgress(int value, int max, int force, int ofw)
{
	u32 prog;
	
	prog = (((ofw ? 95 : 93) * value) / max) + 4;

	SetProgress(prog, force);
}

char upd_error_msg[256];

int OnInstallError(void *param)
{
	int ofw = *(int *)param;
	
	vlfGuiMessageDialog(upd_error_msg, VLF_MD_TYPE_ERROR | VLF_MD_BUTTONS_NONE);

	vlfGuiRemoveText(status);
	vlfGuiRemoveText(progress_text);
	vlfGuiRemoveProgressBar(progress_bar);

	progress_bar = -1;
	progress_text = -1;
	status = -1;

	dcSetCancelMode(0);
	MainMenu(ofw);

	return VLF_EV_RET_REMOVE_HANDLERS;
}

void InstallError(int ofw, char *fmt, ...)
{
	va_list list;
	
	va_start(list, fmt);
	vsprintf(upd_error_msg, fmt, list);
	va_end(list);

	vlfGuiAddEventHandler(0, -1, OnInstallError, &ofw);
	sceKernelExitDeleteThread(0);
}

int OnInstallComplete(void *param)
{
	int ofw = *(int *)param;

	vlfGuiRemoveProgressBar(progress_bar);
	vlfGuiRemoveText(progress_text);

	dcSetCancelMode(0);

	if (ofw && kuKernelGetModel() == 1)
	{
		SetStatus("Install is complete.\n"
			      "A shutdown is required. A normal battery is\n"
				  "required to boot this firmware on this PSP.");
		 AddShutdownRebootBD(1);
	}
	else
	{
		SetStatus("Install is complete.\nA shutdown or a reboot is required.");
		AddShutdownRebootBD(0);
	}

	progress_bar = -1;
	progress_text = -1;	

	return VLF_EV_RET_REMOVE_HANDLERS;
}

int CreateDirs()
{
	int res = sceIoMkdir("flach0:/data", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/data/cert", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/dic", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/font", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/kd", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/vsh", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/kd/resource", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/vsh/etc", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/vsh/module", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach0:/vsh/resource", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	return 0;
}

int CreateFlash1Dirs()
{
	int res = sceIoMkdir("flach1:/dic", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach1:/gps", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach1:/net", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach1:/net/http", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach1:/registry", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach1:/vsh", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	res = sceIoMkdir("flach1:/vsh/theme", 0777);
	if (res < 0 && res != 0x80010011)
		return res;

	return 0;
}

void DisablePlugins()
{
	u8 conf[15];

	memset(conf, 0, 15);
	
	WriteFile("ms0:/seplugins/conf.bin", conf, 15);
}

static char com_table[0x4000];
static int comtable_size;

static char _1g_table[0x4000];
static int _1gtable_size;

static char _2g_table[0x4000];
static int _2gtable_size;

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

int install_thread(SceSize args, void *argp)
{
	int ofw = *(int *)argp;	
	char *argv[2];
	int res;
	SceUID fd;
	int size;
	u8 pbp_header[0x28];
	int error = 0, psar_pos = 0;
	int model = kuKernelGetModel();

	dcSetCancelMode(1);
	
	if (LoadUpdaterModules(ofw) < 0)
	{
		InstallError(ofw, "Error loading updater modules.");
	}

	// Unassign with error ignore (they might have been assigned in a failed attempt of install M33/OFW)
	sceIoUnassign("flach0:");
	sceIoUnassign("flach1:");
	sceIoUnassign("flach2:");
	sceIoUnassign("flach3:");

	sceKernelDelayThread(1200000);

	SetStatus("Formatting flash0... ");

	argv[0] = "fatfmt";
	argv[1] = "lflach0:0,0";

	res = dcLflashStartFatfmt(2, argv);
	if (res < 0)
	{
		InstallError(ofw, "Flash0 format failed: 0x%08X", res);
	}

	sceKernelDelayThread(1200000);
	SetProgress(1, 1);

	SetStatus("Formatting flash1... ");

	argv[1] = "lflach0:0,1";

	res = dcLflashStartFatfmt(2, argv);
	if (res < 0)
	{
		InstallError(ofw, "Flash1 format failed: 0x%08X", res);
	}

	sceKernelDelayThread(1200000);

	SetStatus("Formatting flash2... ");

	argv[1] = "lflach0:0,2";
	dcLflashStartFatfmt(2, argv);

	sceKernelDelayThread(1200000);
	SetProgress(2, 1);

	SetStatus("Assigning flashes...");

	res = sceIoAssign("flach0:", "lflach0:0,0", "flachfat0:", IOASSIGN_RDWR, NULL, 0);
	if (res < 0)
	{
		InstallError(ofw, "Flash0 assign failed: 0x%08X", res);  
	}

	res = sceIoAssign("flach1:", "lflach0:0,1", "flachfat1:", IOASSIGN_RDWR, NULL, 0);
	if (res < 0)
	{
		InstallError(ofw, "Flash1 assign failed: 0x%08X", res);  
	}

	sceIoAssign("flach2:", "lflach0:0,2", "flachfat2:", IOASSIGN_RDWR, NULL, 0);
	
	sceKernelDelayThread(1200000);
	SetProgress(3, 1);

	SetStatus("Creating directories...");

	if (CreateDirs() < 0)
	{
		InstallError(ofw, "Directories creation failed.");
	}

	sceKernelDelayThread(1200000);
	SetProgress(4, 1);

	SetStatus("Flashing files...");

	fd = sceIoOpen("ms0:/401.PBP", PSP_O_RDONLY, 0);
	if (fd < 0)
	{
		InstallError(ofw, "Error opening 401.PBP: 0x%08X", fd);
	}

	size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);

	sceIoRead(fd, pbp_header, sizeof(pbp_header));
	sceIoLseek32(fd, *(u32 *)&pbp_header[0x24], PSP_SEEK_SET);

	size = size - *(u32 *)&pbp_header[0x24];

	if (sceIoRead(fd, big_buffer, BIG_BUFFER_SIZE) <= 0)
	{
		InstallError(ofw, "Error reading 401.PBP");
	}

	sceKernelDelayThread(10000);

	if (pspPSARInit(big_buffer, sm_buffer1, sm_buffer2) < 0)
	{
		InstallError(ofw, "pspPSARInit failed!");
	}

	comtable_size = 0;
	_1gtable_size = 0;
	_2gtable_size = 0;

	while (1)
	{
		char name[128];
		int cbExpanded;
		int pos;
		int signcheck;

		int res = pspPSARGetNextFile(big_buffer, size, sm_buffer1, sm_buffer2, name, &cbExpanded, &pos, &signcheck);

		if (res < 0)
		{
			if (error)			
				InstallError(ofw, "PSAR decode error, pos=0x%08X", pos);

			int dpos = pos-psar_pos;
			psar_pos = pos;
			
			error = 1;
			memmove(big_buffer, big_buffer+dpos, BIG_BUFFER_SIZE-dpos);

			if (sceIoRead(fd, big_buffer+(BIG_BUFFER_SIZE-dpos), dpos) <= 0)
			{
				InstallError(ofw, "Error reading PBP.");
			}

			pspPSARSetBufferPosition(psar_pos);

			continue;
		}
		else if (res == 0) /* no more files */
		{
			break;
		}
		
		if (cbExpanded > 0)
		{		
			if (!strncmp(name, "com:", 4) && comtable_size > 0)
			{
				if (!FindTablePath(com_table, comtable_size, name+4, name))
				{
					InstallError(ofw,  "Error: cannot find path of %s.\n", name);
				}
			}

			else if (model == 0 && !strncmp(name, "01g:", 4) && _1gtable_size > 0)
			{
				if (!FindTablePath(_1g_table, _1gtable_size, name+4, name))
				{
					InstallError(ofw, "Error: cannot find path of %s.\n", name);
				}
			}

			else if (model == 1 && !strncmp(name, "02g:", 4) && _2gtable_size > 0)
			{
				if (!FindTablePath(_2g_table, _2gtable_size, name+4, name))
				{
					InstallError(ofw, "Error: cannot find path of %s.\n", name);
				}
			}

			if (!strncmp(name, "flash0:/", 8))
			{
				name[3] = 'c';
				
				if (signcheck)
				{
					pspSignCheck(sm_buffer2);
				}

				res = WriteFile(name, sm_buffer2, cbExpanded);
				if (res <= 0)
				{
					name[3] = 's';
					InstallError(ofw, "Error 0x%08X flashing %s\n", res, name);
				}
			}
			else if (!strncmp(name, "ipl:", 4))
			{
				u8 *ipl;
				int ipl_size;
				
				SetStatus("Flashing IPL... ");
				
				if (model == 1)
				{
					cbExpanded = pspDecryptPRX(sm_buffer2, sm_buffer1+16384, cbExpanded);
					if (cbExpanded <= 0)
					{
						InstallError(ofw, "Cannot decrypt PSP Slim IPL.\n");
					}
					
					memcpy(sm_buffer1, ipl_02g, 16384);
				}
				else
				{
					memcpy(sm_buffer1+16384, sm_buffer2, cbExpanded);
					memcpy(sm_buffer1, ipl_01g, 16384);
				}

				ipl = sm_buffer1;
				ipl_size = cbExpanded+16384;

				if (ofw)
				{
					ipl += 16384;
					ipl_size -= 16384;
				}

				dcPatchModuleString("IoPrivileged", "IoPrivileged", "IoPrivileged");

				if (pspIplUpdateClearIpl() < 0)
					InstallError(ofw, "Error in pspIplUpdateClearIpl");
				
				if (pspIplUpdateSetIpl(ipl, ipl_size) < 0)
				{
					InstallError(ofw, "Error in pspIplUpdateSetIpl");
				}	
				
				sceKernelDelayThread(1200000);
			}

			else if (!strcmp(name, "com:00000"))
			{
				comtable_size = pspDecryptTable(sm_buffer2, sm_buffer1, cbExpanded, 2);
							
				if (comtable_size <= 0)
				{
					InstallError(ofw, "Cannot decrypt common table.\n");
				}

				memcpy(com_table, sm_buffer2, comtable_size);						
			}
					
			else if (model == 0 && !strcmp(name, "01g:00000"))
			{
				_1gtable_size = pspDecryptTable(sm_buffer2, sm_buffer1, cbExpanded, 2);
							
				if (_1gtable_size <= 0)
				{
					InstallError(ofw, "Cannot decrypt 1g table.\n");
				}

				memcpy(_1g_table, sm_buffer2, _1gtable_size);						
			}
					
			else if (model == 1 && !strcmp(name, "02g:00000"))
			{
				_2gtable_size = pspDecryptTable(sm_buffer2, sm_buffer1, cbExpanded, 2);
							
				if (_2gtable_size <= 0)
				{
					InstallError(ofw, "Cannot decrypt 2g table %08X.\n", _2gtable_size);
				}

				memcpy(_2g_table, sm_buffer2, _2gtable_size);						
			}			
		}

		SetInstallProgress(pos, size, 0, ofw);
		scePowerTick(0);
		error = 0;
	}

	sceKernelDelayThread(200000);
	SetInstallProgress(1, 1, 1, ofw);

	SetStatus("Restoring registry...");
	sceKernelDelayThread(100000);

	if (CreateFlash1Dirs() < 0)
		InstallError(ofw, "Error creating flash1 directories.");

	size = ReadFile("flash1:/registry/system.dreg", 0, sm_buffer1, SMALL_BUFFER_SIZE);
	if (size <= 0)
	{
		InstallError(ofw, "Cannot read system.dreg");
	}

	if (WriteFile("flach1:/registry/system.dreg", sm_buffer1, size) < size)
	{
		InstallError(ofw, "Cannot write system.dreg\n");
	}

	size = ReadFile("flash1:/registry/system.ireg", 0, sm_buffer1, SMALL_BUFFER_SIZE);
	if (size <= 0)
	{
		InstallError(ofw, "Cannot read system.ireg\n");
	}

	if (WriteFile("flach1:/registry/system.ireg", sm_buffer1, size) < size)
	{
		InstallError(ofw, "Cannot write system.ireg\n");		
	}

	res = ReadFile("flash2:/registry/act.dat", 0, sm_buffer1, SMALL_BUFFER_SIZE);
	if (res > 0)
	{
		WriteFile("flach2:/act.dat", sm_buffer1, res);		
	}

	sceKernelDelayThread(900000);
	
	if (!ofw)
	{
		M33File *files;
		int i;
		
		SetProgress(98, 1);
		SetStatus("Flashing M33 files...");

		if (model == 1)
		{
			files = m33_02g;
		}
		else
		{
			files = m33_01g;
		}

		for (i = 0; i < N_FILES; i++)
		{
			res = WriteFile(files[i].path, files[i].buf, files[i].size);

			if (res < 0)
			{
				files[i].path[3] = 's';
				InstallError(ofw, "Error 0x%08X flashing %s\n", res, files[i].path);
			}	
			
			if (i == 6)
			{
				SetProgress(99, 1);
			}

			sceKernelDelayThread(50000);
		}

		WriteFile("ms0:/seplugins/es_recovery.txt", es_recovery, sizeof(es_recovery));
		DisablePlugins();

		res = ReadFile("flash1:/config.se", 0, sm_buffer1, SMALL_BUFFER_SIZE);
		if (res > 0)
		{
			WriteFile("flach1:/config.se", sm_buffer1, res);		
		}
	}

	SetProgress(100, 1);
	vlfGuiAddEventHandler(0, 600000, OnInstallComplete, &ofw);

	return sceKernelExitDeleteThread(0);
}

int Install(int ofw)
{
	SceIoStat stat;
	
	if (sceIoGetstat("ms0:/401.PBP", &stat) < 0)
	{
		vlfGuiMessageDialog("401.PBP not found at root.", VLF_MD_TYPE_ERROR | VLF_MD_BUTTONS_NONE);
		return VLF_EV_RET_NOTHING;
	}

	if (stat.st_size != 25643269)
	{
		vlfGuiMessageDialog("Invalid 401.PBP file.", VLF_MD_TYPE_ERROR | VLF_MD_BUTTONS_NONE);
		return VLF_EV_RET_NOTHING;
	}
	
	ClearProgress();
	status = vlfGuiAddText(80, 100, "Loading updater modules...");

	progress_bar = vlfGuiAddProgressBar(136);	
	progress_text = vlfGuiAddText(240, 148, "0%");
	vlfGuiSetTextAlignment(progress_text, VLF_ALIGNMENT_CENTER);

	SceUID install_thid = sceKernelCreateThread("install_thread", install_thread, 0x18, 0x10000, 0, NULL);
	if (install_thid >= 0)
	{
		sceKernelStartThread(install_thid, 4, &ofw);
	}

	return VLF_EV_RET_REMOVE_OBJECTS | VLF_EV_RET_REMOVE_HANDLERS;
}

