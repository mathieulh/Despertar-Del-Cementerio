	.set noreorder

#include "pspstub.s"

	STUB_START "DcManager_driver",0x00090000,0x00100005
	STUB_FUNC  0xA22190CE,dcGetHardwareInfo
	STUB_FUNC  0x3A3EA355,dcPatchModule
	STUB_FUNC  0xD298BA53,dcPatchModuleString
	STUB_FUNC  0x9C87A1C7,dcGetCancelMode
	STUB_FUNC  0xF5161120,dcSetCancelMode
	STUB_FUNC  0x39D91E83,dcLflashStartFatfmt
	STUB_FUNC  0xA899D713,dcLflashStartFDisk
	STUB_FUNC  0x741FFCFD,dcGetNandInfo
	STUB_FUNC  0xEA3F4DE5,dcLockNand
	STUB_FUNC  0x4A5FCC2F,dcUnlockNand
	STUB_FUNC  0x7EB5614A,dcReadNandBlock
	STUB_FUNC  0x504C82B4,dcWriteNandBlock
	STUB_FUNC  0x7E9128B0,dcEraseNandBlock
	STUB_FUNC  0xAD3509E5,dcRegisterPhysicalFormatCallback
	STUB_FUNC  0x1A718513,dcUnregisterPhysicalFormatCallback
	STUB_FUNC  0x48DDEA84,dcQueryRealMacAddress
	STUB_END
