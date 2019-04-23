/* $XFree86$ */
/* $XdotOrg$ */
/*
 * Basic hardware and memory detection
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author:  	Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Ideas and methods for old series based on code by Can-Ru Yeou, SiS Inc.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sis.h"
#define SIS_NEED_inSISREGW
#define SIS_NEED_inSISREGL
#define SIS_NEED_outSISREGW
#define SIS_NEED_outSISREGL
#define SIS_NEED_inSISIDXREG
#define SIS_NEED_outSISIDXREG
#include "sis_regs.h"

extern int SiSMclk(SISPtr pSiS);

static const char* dramTypeStr[] = {
	"Fast Page DRAM",
	"2 cycle EDO RAM",
	"1 cycle EDO RAM",
	"SDRAM/SGRAM",
	"SDR SDRAM",
	"SGRAM",
	"ESDRAM",
	"DDR SDRAM",  /* for 550/650/etc */
	"DDR SDRAM",  /* for 550/650/etc */
	"VCM",	      /* for 630 */
	"DDR2 SDRAM", /* for 340 */
	""
};

/* MCLK tables for SiS6326 */
static const int SiS6326MCLKIndex[4][8] = {
	{ 10, 12, 14, 16, 17, 18, 19,  7 },  /* SGRAM */
	{  4,  6,  8, 10, 11, 12, 13,  3 },  /* Fast Page */
	{  9, 11, 12, 13, 15, 16,  5,  7 },  /* 2 cycle EDO */
	{ 10, 12, 14, 16, 17, 18, 19,  7 }   /* ? (Not 1 cycle EDO) */
};

static const struct _sis6326mclk {
	CARD16 mclk;
	UChar  sr13;
	UChar  sr28;
	UChar  sr29;
} SiS6326MCLK[] = {
	{  0, 0,    0,    0 },
	{  0, 0,    0,    0 },
	{  0, 0,    0,    0 },
	{ 45, 0, 0x2b, 0x26 },
	{ 53, 0, 0x49, 0xe4 },
	{ 55, 0, 0x7c, 0xe7 },
	{ 56, 0, 0x7c, 0xe7 },
	{ 60, 0, 0x42, 0xe3 },
	{ 61, 0, 0x21, 0xe1 },
	{ 65, 0, 0x5a, 0xe4 },
	{ 66, 0, 0x5a, 0xe4 },
	{ 70, 0, 0x61, 0xe4 },
	{ 75, 0, 0x3e, 0xe2 },
	{ 80, 0, 0x42, 0xe2 },
	{ 83, 0, 0xb3, 0xc5 },
	{ 85, 0, 0x5e, 0xe3 },
	{ 90, 0, 0xae, 0xc4 },
	{100, 0, 0x37, 0xe1 },
	{115, 0, 0x78, 0x0e },
	{134, 0, 0x4a, 0xa3 }
};

#include "xf86.h"


struct pci_device*
	sis_get_device(int device)
{
	struct pci_slot_match bridge_match = {
	0, 0, device, PCI_MATCH_ANY, 0
	};
	struct pci_device_iterator* slot_iterator;
	struct pci_device* bridge;

	slot_iterator = pci_slot_match_iterator_create(&bridge_match);
	bridge = pci_device_next(slot_iterator);
	pci_iterator_destroy(slot_iterator);
	return bridge;
}

unsigned int
sis_pci_read_device_u32(int device, int offset)
{
	struct pci_device* host_bridge = sis_get_device(device);
	unsigned int result;

	pci_device_cfg_read_u32(host_bridge, &result, offset);
	return result;
}

unsigned char
sis_pci_read_device_u8(int device, int offset)
{
	struct pci_device* host_bridge = sis_get_device(device);
	unsigned char result;

	pci_device_cfg_read_u8(host_bridge, &result, offset);
	return result;
}

void
sis_pci_write_host_bridge_u32(int offset, unsigned int value)
{
	struct pci_device* host_bridge = sis_get_device(0);
	pci_device_cfg_write_u32(host_bridge, value, offset);
}

void
sis_pci_write_host_bridge_u8(int offset, unsigned char value)
{
	struct pci_device* host_bridge = sis_get_device(0);
	pci_device_cfg_write_u8(host_bridge, value, offset);
}

unsigned int
sis_pci_read_host_bridge_u32(int offset)
{
	return sis_pci_read_device_u32(0, offset);
}

unsigned char
sis_pci_read_host_bridge_u8(int offset)
{
	return sis_pci_read_device_u8(0, offset);
}

static int sisESSPresent(ScrnInfoPtr pScrn)
{
	int flags = 0;
	struct pci_id_match id_match = { 0x1274, PCI_MATCH_ANY,
					 PCI_MATCH_ANY, PCI_MATCH_ANY,
					 PCI_MATCH_ANY, PCI_MATCH_ANY,
					 0 };
	struct pci_device_iterator* id_iterator;
	struct pci_device* ess137x;

	id_iterator = pci_id_match_iterator_create(&id_match);

	ess137x = pci_device_next(id_iterator);
	while (ess137x) {
		if ((ess137x->device_id == 0x5000) ||
			((ess137x->device_id & 0xfff0) == 0x1370)) {
			flags |= ESS137xPRESENT;
		}
		ess137x = pci_device_next(id_iterator);
	}
	return flags;
}



/* For old chipsets, 5597, 6326, 530/620 */
static void
sisOldSetup(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);
	int    ramsize[8] = { 1,  2,  4, 0, 0,  2,  4,  8 };
	int    buswidth[8] = { 32, 64, 64, 0, 0, 32, 32, 64 };
	int    clockTable[4] = { 66, 75, 83, 100 };
	int    ramtype[4] = { 5, 0, 1, 3 };
	int    config, temp, i;
	UChar  sr23, sr33, sr37;
#if 0
	UChar  newsr13, newsr28, newsr29;
#endif


	if (pSiS->oldChipset <= OC_SIS6225) {
		inSISIDXREG(SISSR, 0x0F, temp);
		pScrn->videoRam = (1 << (temp & 0x03)) * 1024;
		if (pScrn->videoRam > 4096) pScrn->videoRam = 4096;
		pSiS->BusWidth = 32;
	}
	else {
		inSISIDXREG(SISSR, 0x0C, temp);
		config = ((temp & 0x10) >> 2) | ((temp & 0x06) >> 1);
		pScrn->videoRam = ramsize[config] * 1024;
		pSiS->BusWidth = buswidth[config];
	}

	pSiS->MemClock = SiSMclk(pSiS);

	pSiS->Flags &= ~(SYNCDRAM | RAMFLAG);
	if (pSiS->oldChipset >= OC_SIS82204) {
		inSISIDXREG(SISSR, 0x23, sr23);
		inSISIDXREG(SISSR, 0x33, sr33);
		if (pSiS->oldChipset >= OC_SIS530A) sr33 &= ~0x08;
		if (sr33 & 0x09) {				/* 5597: Sync DRAM timing | One cycle EDO ram;   */
			pSiS->Flags |= (sr33 & SYNCDRAM);	/* 6326: Enable SGRam timing | One cycle EDO ram */
			pSiS->Flags |= RAMFLAG;			/* 530:  Enable SGRAM timing | reserved (0)      */
		}
		else if ((pSiS->oldChipset < OC_SIS530A) && (sr23 & 0x20)) {
			pSiS->Flags |= SYNCDRAM;		/* 5597, 6326: EDO DRAM enabled */
		}						/* 530/620:    reserved (0)     */
	}

	pSiS->Flags &= ~(ESS137xPRESENT);

	pSiS->Flags &= ~(SECRETFLAG);
	if (pSiS->oldChipset >= OC_SIS5597) {
		inSISIDXREG(SISSR, 0x37, sr37);
		if (sr37 & 0x80) pSiS->Flags |= SECRETFLAG;
	}

	pSiS->Flags &= ~(A6326REVAB);

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Memory clock: %3.3f MHz\n",
		pSiS->MemClock / 1000.0);

	if (pSiS->oldChipset > OC_SIS6225) {
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"DRAM bus width: %d bit\n",
			pSiS->BusWidth);
	}

#ifdef TWDEBUG
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"oldChipset = %d, Flags %x\n", pSiS->oldChipset, pSiS->Flags);
#endif
}

/* For 550, 65x, 740, 661, 741, 660, 760, 761, 670, 770 */
static void
sis550Setup(ScrnInfoPtr pScrn)
{
	SISPtr       pSiS = SISPTR(pScrn);
	unsigned int config, ramtype = 0, i;
	CARD8	 pciconfig, temp;
	Bool	 alldone = FALSE;
	Bool	 ddrtimes2 = TRUE;

	pSiS->IsAGPCard = TRUE;
	pSiS->IsPCIExpress = FALSE;
	pSiS->ChipFlags &= ~(SiSCF_760UMA | SiSCF_760LFB);

	pSiS->MemClock = SiSMclk(pSiS);

	if (pSiS->Chipset == PCI_CHIP_SIS671) {
		inSISIDXREG(SISCR, 0x79, config);
		/* DDR */
		ramtype = 8;

		/* Bus Data Width*/
		pSiS->BusWidth = (!(config & 0x0C)) ? 64 : 0;
		if (pSiS->BusWidth == 0) xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Bad Bus Width!\n");

		/* share mem (UMA) size */
		pScrn->videoRam = 0;
		unsigned int power = (config & 0xf0) >> 4;
		if (power) {
			for (pScrn->videoRam = 1; power > 0; power--)
				pScrn->videoRam *= 2;
		}
		pScrn->videoRam *= 1024; /* K */
		pSiS->UMAsize = pScrn->videoRam;
		pSiS->SiS76xUMASize = pScrn->videoRam * 1024; /* byte */
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"%dK shared video RAM (UMA)\n",
			pScrn->videoRam);



		pSiS->ChipFlags |= SiSCF_760UMA;
		pSiS->IsPCIExpress = TRUE;
		alldone = TRUE;



	}
	else {

		/* 550 */

		pciconfig = sis_pci_read_host_bridge_u8(0x63);
		if (pciconfig & 0x80) {
			pScrn->videoRam = (1 << (((pciconfig & 0x70) >> 4) + 21)) / 1024;
			pSiS->UMAsize = pScrn->videoRam;
			pSiS->BusWidth = 64;
			ramtype = sis_pci_read_host_bridge_u8(0x65);
			ramtype &= 0x01;
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
				"Shared Memory Area is on DIMM%d\n", ramtype);
			ramtype = 4;
			alldone = TRUE;
		}

	}

	/* Fall back to BIOS detection results in case of problems: */

	if (!alldone) {

		pSiS->SiS76xLFBSize = pSiS->SiS76xUMASize = 0;
		pSiS->UMAsize = pSiS->LFBsize = 0;

		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			"Shared Memory Area is disabled - awaiting doom\n");
		inSISIDXREG(SISSR, 0x14, config);
		pScrn->videoRam = (((config & 0x3F) + 1) * 4) * 1024;
		pSiS->UMAsize = pScrn->videoRam;
		ramtype = 4;
		pSiS->BusWidth = 64;
	}

	/* These (might) need special attention: Memory controller in CPU, hence
	 * - no DDR * 2 for bandwidth calculation,
	 * - overlay magic (bandwidth dependent one/two overlay stuff)
	 */
	switch (pSiS->ChipType) {
	case SIS_760:
#ifdef SIS761MEMFIX
	case SIS_761:
#endif
#ifdef SIS770MEMFIX
	case SIS_770:
#endif
		if (!(pSiS->ChipFlags & SiSCF_760LFB)) {
			ddrtimes2 = FALSE;
			pSiS->SiS_SD2_Flags |= SiS_SD2_SUPPORT760OO;
		}
	}

	/* DDR -> Mclk * 2 - needed for bandwidth calculation */
	if (ddrtimes2) {
		if (ramtype == 8) pSiS->MemClock *= 2;
		if (ramtype == 0xa) pSiS->MemClock *= 2;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"DRAM type: %s\n",
		dramTypeStr[ramtype]);

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Memory clock: %3.3f MHz\n",
		pSiS->MemClock / 1000.0);

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"DRAM bus width: %d bit\n",
		pSiS->BusWidth);
}

void
SiSSetup(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	pSiS->Flags = 0;
	pSiS->VBFlags = 0;
	pSiS->SiS76xLFBSize = pSiS->SiS76xUMASize = 0;
	pSiS->UMAsize = pSiS->LFBsize = 0;

	switch (pSiS->Chipset) {
	case PCI_CHIP_SIS671: /* 671, 771 */
		sis550Setup(pScrn);
		break;
	default:
		sisOldSetup(pScrn);
		break;
	}
}


