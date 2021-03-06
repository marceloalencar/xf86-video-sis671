/* $XFree86$ */
/* $XdotOrg$ */
/*
 * Mode setup and basic video bridge detection
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria.
 *
 * The SISInit() function for old series (except TV and FIFO calculation)
 * was previously based on code which was Copyright (C) 1998,1999 by Alan
 * Hourihane, Wigan, England. However, the code has been rewritten entirely
 * and is - it its current representation - not covered by this old copyright.
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
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sis.h"
#define SIS_NEED_inSISREG
#define SIS_NEED_outSISREG
#define SIS_NEED_inSISIDXREG
#define SIS_NEED_outSISIDXREG
#define SIS_NEED_orSISIDXREG
#define SIS_NEED_andSISIDXREG
#include "sis_regs.h"
#include "sis_dac.h"

extern void  SISSense30x(ScrnInfoPtr pScrn, Bool quiet);
extern void  SISSenseChrontel(ScrnInfoPtr pScrn, Bool quiet);

void RecalcScreenPitch(ScrnInfoPtr pScrn);

/* Our very own vgaHW functions */
void SiSVGASave(ScrnInfoPtr pScrn, SISRegPtr save, int flags);
void SiSVGARestore(ScrnInfoPtr pScrn, SISRegPtr restore, int flags);
void SiSVGASaveFonts(ScrnInfoPtr pScrn);
void SiSVGARestoreFonts(ScrnInfoPtr pScrn);
void SISVGALock(SISPtr pSiS);
void SiSVGAUnlock(SISPtr pSiS);
void SiSVGAProtect(ScrnInfoPtr pScrn, Bool on);
#ifdef SIS_PC_PLATFORM
Bool SiSVGAMapMem(ScrnInfoPtr pScrn);
void SiSVGAUnmapMem(ScrnInfoPtr pScrn);
#endif
Bool SiSVGASaveScreen(ScreenPtr pScreen, int mode);
static Bool SiSVGAInit(ScrnInfoPtr pScrn, DisplayModePtr mode, int fixsync);

static const CARD8 SiS6326TVRegs1[14] = {
	 0x00,0x01,0x02,0x03,0x04,0x11,0x12,0x13,0x21,0x26,0x27,0x3a,0x3c,0x43
};

static const CARD8 SiS6326TVRegs1_NTSC[6][14] = {
	{0x81,0x3f,0x49,0x1b,0xa9,0x03,0x00,0x09,0x08,0x7d,0x00,0x88,0x30,0x60},
	{0x81,0x3f,0x49,0x1d,0xa0,0x03,0x00,0x09,0x08,0x7d,0x00,0x88,0x30,0x60},
	{0x81,0x45,0x24,0x8e,0x26,0x0b,0x00,0x09,0x02,0xfe,0x00,0x09,0x51,0x60},
	{0x81,0x45,0x24,0x8e,0x26,0x07,0x00,0x29,0x04,0x30,0x10,0x3b,0x61,0x60},
	{0x81,0x3f,0x24,0x8e,0x26,0x09,0x00,0x09,0x02,0x30,0x10,0x3b,0x51,0x60},  /* 640x400, 640x480 */
	{0x83,0x5d,0x21,0xbe,0x75,0x03,0x00,0x09,0x08,0x42,0x10,0x4d,0x61,0x79}   /* 640x480u */
};

static const CARD8 SiS6326TVRegs2_NTSC[6][54] = {
	{0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
	 0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
	 0xFC, 0xDF, 0x94, 0x1F, 0x4A, 0x03, 0x71, 0x07, 0x97, 0x10, 0x40,
	 0x48, 0x00, 0x26, 0xB6, 0x10, 0x5C, 0xEC, 0x21, 0x2E, 0xBE, 0x10,
	 0x64, 0xF4, 0x21, 0x13, 0x75, 0x08, 0x31, 0x6A, 0x01, 0xA0},
	{0x11, 0x17, 0x03, 0x0A, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
	 0x0D, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
	 0xFF, 0xDF, 0x94, 0x1F, 0x4A, 0x03, 0x71, 0x07, 0x97, 0x10, 0x40,
	 0x48, 0x00, 0x26, 0xB6, 0x10, 0x5C, 0xEC, 0x21, 0x2E, 0xBE, 0x10,
	 0x64, 0xF4, 0x21, 0x13, 0x75, 0x08, 0x31, 0x6A, 0x01, 0xA0},
	{0x11, 0x17, 0x03, 0x0A, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
	 0x0D, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
	 0xFF, 0xDF, 0x94, 0x3F, 0x8C, 0x06, 0xCE, 0x07, 0x27, 0x30, 0x73,
	 0x7B, 0x00, 0x48, 0x68, 0x30, 0xB2, 0xD2, 0x52, 0x50, 0x70, 0x30,
	 0xBA, 0xDA, 0x52, 0xDC, 0x02, 0xD1, 0x53, 0xF7, 0x02, 0xA0},
	{0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
	 0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
	 0xDC, 0xDF, 0x94, 0x3F, 0x8C, 0x06, 0xCE, 0x07, 0x27, 0x30, 0x73,
	 0x7B, 0x00, 0x48, 0x68, 0x30, 0xB2, 0xD2, 0x52, 0x50, 0x70, 0x30,
	 0xBA, 0xDA, 0x52, 0x00, 0x02, 0xF5, 0x53, 0xF7, 0x02, 0xA0},
	{0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
	 0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
	 0xDC, 0xDF, 0x94, 0x3F, 0x8C, 0x06, 0xCE, 0x07, 0x27, 0x30, 0x73,
	 0x7B, 0x00, 0x48, 0x68, 0x30, 0xB2, 0xD2, 0x52, 0x50, 0x70, 0x30,
	 0xBA, 0xDA, 0x52, 0xDC, 0x02, 0xD1, 0x53, 0xF7, 0x02, 0xA0},
	{0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,  /* 640x480u */
	 0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
	 0xDC, 0xDF, 0x94, 0xAF, 0x95, 0x06, 0xDD, 0x07, 0x5F, 0x30, 0x7E,
	 0x86, 0x00, 0x4C, 0xA4, 0x30, 0xE3, 0x3B, 0x62, 0x54, 0xAC, 0x30,
	 0xEB, 0x43, 0x62, 0x48, 0x34, 0x3D, 0x63, 0x29, 0x03, 0xA0}
};

static const CARD8 SiS6326TVRegs1_PAL[6][14] = {
	{0x81,0x2d,0xc8,0x07,0xb2,0x0b,0x00,0x09,0x02,0xed,0x00,0xf8,0x30,0x40},
	{0x80,0x2d,0xa4,0x03,0xd9,0x0b,0x00,0x09,0x02,0xed,0x10,0xf8,0x71,0x40},
	{0x81,0x2d,0xa4,0x03,0xd9,0x0b,0x00,0x09,0x02,0xed,0x10,0xf8,0x71,0x40},  /* 640x480 */
	{0x81,0x2d,0xa4,0x03,0xd9,0x0b,0x00,0x09,0x02,0x8f,0x10,0x9a,0x71,0x40},  /* 800x600 */
	{0x83,0x63,0xa1,0x7a,0xa3,0x0a,0x00,0x09,0x02,0xb5,0x11,0xc0,0x81,0x59},  /* 800x600u */
	{0x81,0x63,0xa4,0x03,0xd9,0x01,0x00,0x09,0x10,0x9f,0x10,0xaa,0x71,0x59}   /* 720x540  */
};

static const CARD8 SiS6326TVRegs2_PAL[6][54] = {
	{0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,
	 0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
	 0xE5, 0xDF, 0x94, 0xEF, 0x5A, 0x03, 0x7F, 0x07, 0xFF, 0x10, 0x4E,
	 0x56, 0x00, 0x2B, 0x23, 0x20, 0xB4, 0xAC, 0x31, 0x33, 0x2B, 0x20,
	 0xBC, 0xB4, 0x31, 0x83, 0xE1, 0x78, 0x31, 0xD6, 0x01, 0xA0},
	{0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,
	 0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
	 0xE5, 0xDF, 0x94, 0xDF, 0xB2, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x90,
	 0x98, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
	 0x6A, 0x5A, 0x73, 0x03, 0xC1, 0xF8, 0x63, 0xB6, 0x03, 0xA0},
	{0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,  /* 640x480 */
	 0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
	 0xE5, 0xDF, 0x94, 0xDF, 0xB2, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x90,
	 0x98, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
	 0x6A, 0x5A, 0x73, 0x03, 0xC1, 0xF8, 0x63, 0xB6, 0x03, 0xA0},
	{0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,  /* 800x600 */
	 0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
	 0xE5, 0xDF, 0x94, 0xDF, 0xB2, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x90,
	 0x98, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
	 0x6A, 0x5A, 0x73, 0xA0, 0xC1, 0x95, 0x73, 0xB6, 0x03, 0xA0},
	{0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,  /* 800x600u */
	 0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
	 0xE5, 0xDF, 0x94, 0x7F, 0xBD, 0x08, 0x0E, 0x07, 0x47, 0x40, 0x9D,
	 0xA5, 0x00, 0x54, 0x94, 0x40, 0xA4, 0xE4, 0x73, 0x5C, 0x9C, 0x40,
	 0xAC, 0xEC, 0x73, 0x0B, 0x0E, 0x00, 0x84, 0x03, 0x04, 0xA0},
	{0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,  /* 720x540  */
	 0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
	 0xE5, 0xDF, 0x94, 0xDF, 0xB0, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x9D,
	 0xA5, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
	 0x6A, 0x5A, 0x73, 0xA0, 0xC1, 0x95, 0x73, 0xB6, 0x03, 0xA0}
};

static const CARD8 SiS6326CR[9][15] = {
	 {0x79,0x63,0x64,0x1d,0x6a,0x93,0x00,0x6f,0xf0,0x58,0x8a,0x57,0x57,0x70,0x20},  /* PAL 800x600   */
	 {0x79,0x4f,0x50,0x95,0x60,0x93,0x00,0x6f,0xba,0x14,0x86,0xdf,0xe0,0x30,0x00},  /* PAL 640x480   */
	 {0x5f,0x4f,0x50,0x82,0x53,0x9f,0x00,0x0b,0x3e,0xe9,0x8b,0xdf,0xe7,0x04,0x00},  /* NTSC 640x480  */
	 {0x5f,0x4f,0x50,0x82,0x53,0x9f,0x00,0x0b,0x3e,0xcb,0x8d,0x8f,0x96,0xe9,0x00},  /* NTSC 640x400  */
	 {0x83,0x63,0x64,0x1f,0x6d,0x9b,0x00,0x6f,0xf0,0x48,0x0a,0x23,0x57,0x70,0x20},  /* PAL 800x600u  */
	 {0x79,0x59,0x5b,0x1d,0x66,0x93,0x00,0x6f,0xf0,0x42,0x04,0x1b,0x40,0x70,0x20},  /* PAL 720x540   */
	 {0x66,0x4f,0x51,0x0a,0x57,0x89,0x00,0x0b,0x3e,0xd9,0x0b,0xb6,0xe7,0x04,0x00},  /* NTSC 640x480u */
	 {0xce,0x9f,0x9f,0x92,0xa4,0x16,0x00,0x28,0x5a,0x00,0x04,0xff,0xff,0x29,0x39},  /* 1280x1024-75  */
	 {0x09,0xc7,0xc7,0x0d,0xd2,0x0a,0x01,0xe0,0x10,0xb0,0x04,0xaf,0xaf,0xe1,0x1f}   /* 1600x1200-60  */
};

/* Reset screen pitch (for display hardware, not
 * for accelerator engines)
 * pSiS->scrnOffset is constant throughout
 * server runtime (except for when DGA is active)
 */

void
RecalcScreenPitch(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	pSiS->scrnPitch = pSiS->scrnOffset;
}

/* Init a mode for SiS 300 series and later.
 * This function is only used for setting up some
 * variables.
 */
static Bool
SISNewInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	SISPtr    pSiS = SISPTR(pScrn);
	SISRegPtr pReg = &pSiS->ModeReg;
	UShort    temp;

	/* Copy current register settings to structure */
	(*pSiS->SiSSave)(pScrn, pReg);

	/* accelerator framebuffer depth */
	switch (pSiS->CurrentLayout.bitsPerPixel) {
	case 8:
		pSiS->DstColor = 0x0000;
		pSiS->SiS310_AccelDepth = 0x00000000;
		break;
	case 16:
		pSiS->DstColor = (short)0x8000;
		pSiS->SiS310_AccelDepth = 0x00010000;
		break;
	case 32:
		pSiS->DstColor = (short)0xC000;
		pSiS->SiS310_AccelDepth = 0x00020000;
		break;
	default:
		return FALSE;
	}

	/* scrnOffset is for the accelerator */
	pSiS->scrnOffset = pSiS->CurrentLayout.displayWidth * (pSiS->CurrentLayout.bitsPerPixel >> 3);

	/* display pitch (used by init.c only) */
	RecalcScreenPitch(pScrn);

#ifdef UNLOCK_ALWAYS
	outSISIDXREG(SISSR, 0x05, 0x86);
#endif

	/* Enable PCI LINEAR ADDRESSING (0x80), MMIO (0x01), PCI_IO (0x20) */
	pReg->sisRegs3C4[0x20] = 0xA1;

	/* Now initialize TurboQueue. TB is always located at the very top of
	 * the videoRAM (notably NOT the x framebuffer memory, which can/should
	 * be limited by MaxXFbMem when using DRI). Also, enable the accelerators.
	 */
	if (!pSiS->NoAccel) {
		pReg->sisRegs3C4[0x1E] |= 0x42;  /* Enable 2D accelerator */
		pReg->sisRegs3C4[0x1E] |= 0x18;  /* Enable 3D accelerator */
#ifndef SISVRAMQ
		/* See comments in sis_driver.c */
		pReg->sisRegs3C4[0x27] = 0x1F;
		pReg->sisRegs3C4[0x26] = 0x22;
		pReg->sisMMIO85C0 = (pScrn->videoRam - 512) * 1024;
#endif
	}

	return TRUE;
}

static void
SiS6326TVDelay(ScrnInfoPtr pScrn, int delay)
{
	SISPtr  pSiS = SISPTR(pScrn);
	int i;
	UChar temp;

	for (i = 0; i < delay; i++) {
		inSISIDXREG(SISSR, 0x05, temp);
	}
	(void)temp;
}

static int
SIS6326DoSense(ScrnInfoPtr pScrn, int tempbh, int tempbl, int tempch, int tempcl)
{
	UChar temp;

	SiS6326SetTVReg(pScrn, 0x42, tempbl);
	temp = SiS6326GetTVReg(pScrn, 0x43);
	temp &= 0xfc;
	temp |= tempbh;
	SiS6326SetTVReg(pScrn, 0x43, temp);
	SiS6326TVDelay(pScrn, 0x1000);
	temp = SiS6326GetTVReg(pScrn, 0x43);
	temp |= 0x04;
	SiS6326SetTVReg(pScrn, 0x43, temp);
	SiS6326TVDelay(pScrn, 0x8000);
	temp = SiS6326GetTVReg(pScrn, 0x44);
	if (!(tempch & temp)) tempcl = 0;
	return tempcl;
}

static void
SISSense6326(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);
	UChar  temp;
	int    result;

	pSiS->SiS6326Flags &= (SIS6326_HASTV | SIS6326_TVPAL);
	temp = SiS6326GetTVReg(pScrn, 0x43);
	temp &= 0xfb;
	SiS6326SetTVReg(pScrn, 0x43, temp);
	result = SIS6326DoSense(pScrn, 0x01, 0xb0, 0x06, SIS6326_TVSVIDEO);  /* 0x02 */
	pSiS->SiS6326Flags |= result;
	result = SIS6326DoSense(pScrn, 0x01, 0xa0, 0x01, SIS6326_TVCVBS);    /* 0x04 */
	pSiS->SiS6326Flags |= result;
	temp = SiS6326GetTVReg(pScrn, 0x43);
	temp &= 0xfb;
	SiS6326SetTVReg(pScrn, 0x43, temp);
	if (pSiS->SiS6326Flags & (SIS6326_TVSVIDEO | SIS6326_TVCVBS)) {
		pSiS->SiS6326Flags |= SIS6326_TVDETECTED;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Detected TV connected to %s output\n",
			(((pSiS->SiS6326Flags & (SIS6326_TVSVIDEO | SIS6326_TVCVBS)) ==
			(SIS6326_TVSVIDEO | SIS6326_TVCVBS)) ?
				"both SVIDEO and COMPOSITE" :
				((pSiS->SiS6326Flags & SIS6326_TVSVIDEO) ?
					"SVIDEO" : "COMPOSITE")));
	}
	else {
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"No TV detected\n");
	}
}

static Bool
SISIsUMC(SISPtr pSiS)
{
	UShort p4_0f, p4_25, p4_27, temp;

	inSISIDXREG(SISPART4, 0x0f, p4_0f);
	inSISIDXREG(SISPART4, 0x25, p4_25);
	inSISIDXREG(SISPART4, 0x27, p4_27);
	andSISIDXREG(SISPART4, 0x0f, 0x7f);
	orSISIDXREG(SISPART4, 0x25, 0x08);
	andSISIDXREG(SISPART4, 0x27, 0xfd);
	inSISIDXREG(SISPART4, 0x26, temp);
	outSISIDXREG(SISPART4, 0x27, p4_27);
	outSISIDXREG(SISPART4, 0x25, p4_25);
	outSISIDXREG(SISPART4, 0x0f, p4_0f);
	return((temp & 0x08) ? TRUE : FALSE);
}

/* Detect video bridge and set VBFlags accordingly */
void SISVGAPreInit(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);
	int    temp, temp1, temp2, sistypeidx;
	int    upperlimitlvds, lowerlimitlvds;
	int    upperlimitch, lowerlimitch;
	int    chronteltype, chrontelidreg, upperlimitvb;
	static const char* detectvb = "Detected SiS%s video bridge (%s, ID %d; Rev 0x%x)\n";
#if 0
	UChar sr17 = 0;
#endif
	static const char* ChrontelTypeStr[] = {
	"7004",
	"7005",
	"7007",
	"7006",
	"7008",
	"7013",
	"7019",
	"7020",
	"(unknown)"
	};
	static const char* SiSVBTypeStr[] = {
	"301",		/* 0 */
	"301B",		/* 1 */
	"301B-DH",	/* 2 */
	"301LV",	/* 3 */
	"302LV",	/* 4 */
	"301C",		/* 5 */
	"302ELV",	/* 6 */
	"302B",		/* 7 */
	"307T",		/* 8 */
	"307LV"		/* 9 */
	};

	pSiS->ModeInit = SISNewInit;

	pSiS->VBFlags = pSiS->VBFlags2 = pSiS->VBFlags3 = pSiS->VBFlags4 = 0;
	pSiS->SiS_Pr->SiS_UseLCDA = FALSE;
	pSiS->SiS_Pr->Backup = FALSE;

	inSISIDXREG(SISPART4, 0x00, temp);
	temp &= 0x0F;
	if (temp == 1) {

		inSISIDXREG(SISPART4, 0x01, temp1);
		temp1 &= 0xff;

		if (temp1 >= 0xC0) {
			if (SISIsUMC(pSiS)) pSiS->VBFlags2 |= VB2_SISUMC;
		}

		if (temp1 >= 0xE0) {
			inSISIDXREG(SISPART4, 0x39, temp2);
			if (temp2 == 0xff) {
				pSiS->VBFlags2 |= VB2_302LV;
				sistypeidx = 4;
			}
			else {
				pSiS->VBFlags2 |= VB2_301C;	/* VB_302ELV; */
				sistypeidx = 5; 		/* 6; */
			}
		}
		else if (temp1 >= 0xD0) {
			pSiS->VBFlags2 |= VB2_301LV;
			sistypeidx = 3;
		}
		else if (temp1 >= 0xC0) {
			pSiS->VBFlags2 |= VB2_301C;
			sistypeidx = 5;
		}
		else if (temp1 >= 0xB0) {
			pSiS->VBFlags2 |= VB2_301B;
			sistypeidx = 1;
			inSISIDXREG(SISPART4, 0x23, temp2);
			if (!(temp2 & 0x02)) {
				pSiS->VBFlags2 |= VB2_30xBDH;
				sistypeidx = 2;
			}
		}
		else {
			pSiS->VBFlags2 |= VB2_301;
			sistypeidx = 0;
		}

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, detectvb, SiSVBTypeStr[sistypeidx],
			(pSiS->VBFlags2 & VB2_SISUMC) ? "UMC-0" : "Charter/UMC-1", 1, temp1);

		SISSense30x(pScrn, FALSE);

	}
	else if (temp == 2) {

		inSISIDXREG(SISPART4, 0x01, temp1);
		temp1 &= 0xff;

		if (temp1 >= 0xC0) {
			if (SISIsUMC(pSiS)) pSiS->VBFlags2 |= VB2_SISUMC;
		}

		if (temp1 >= 0xE0) {
			pSiS->VBFlags2 |= VB2_302LV;
			sistypeidx = 4;
		}
		else if (temp1 >= 0xD0) {
			pSiS->VBFlags2 |= VB2_301LV;
			sistypeidx = 3;
		}
		else {
			pSiS->VBFlags2 |= VB2_302B;
			sistypeidx = 7;
		}

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, detectvb, SiSVBTypeStr[sistypeidx],
			(pSiS->VBFlags2 & VB2_SISUMC) ? "UMC-0" : "Charter/UMC-1", 2, temp1);

		SISSense30x(pScrn, FALSE);

	}
	else if (temp == 3) {

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, detectvb, "303", "unsupported, unknown", temp, 0);

	}
	else if (temp == 7) {

		inSISIDXREG(SISPART4, 0x01, temp1);
		temp1 &= 0xff;
		inSISIDXREG(SISPART4, 0x39, temp2);
		if (temp1 != 0xE0 && temp1 != 0xE1)
		{
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED, detectvb, "307", "unsupported, unknown", 7, temp1);
		}
		else
		{
			switch (temp2)
			{
			case 0xE0:
				pSiS->VBFlags2 |= VB2_307T;
				sistypeidx = 8;
				break;
			case 0xFF:
				pSiS->VBFlags2 |= VB2_307LV;
				sistypeidx = 9;
				break;
			}
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED, detectvb, SiSVBTypeStr[sistypeidx],
				(pSiS->VBFlags2 & VB2_SISUMC) ? "UMC-0" : "Charter/UMC-1", 7, temp1);
			SISSense30x(pScrn, FALSE);
		}
	}
	else {

		if (pSiS->NewCRLayout) {
			inSISIDXREG(SISCR, 0x38, temp);
			temp = (temp >> 5) & 0x07;
		}
		else {
			inSISIDXREG(SISCR, 0x37, temp);
			temp = (temp >> 1) & 0x07;
		}
		lowerlimitlvds = 2; upperlimitlvds = 3;
		lowerlimitch = 3; upperlimitch = 3;
		chronteltype = 2;   chrontelidreg = 0x4b;
		upperlimitvb = upperlimitlvds;
		if (pSiS->NewCRLayout) {
			upperlimitvb = 4;
		}

		if ((temp >= lowerlimitlvds) && (temp <= upperlimitlvds)) {
			pSiS->VBFlags2 |= VB2_LVDS;
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
				"Detected LVDS transmitter (External chip ID %d)\n", temp);
		}
		if ((temp >= lowerlimitch) && (temp <= upperlimitch)) {
			/* Set global for init301.c */
			pSiS->SiS_Pr->SiS_IF_DEF_CH70xx = chronteltype;

			if (chronteltype == 1) {
				/* Set general purpose IO for Chrontel communication */
				SiS_SetChrontelGPIO(pSiS->SiS_Pr, 0x9c);
			}

			/* Read Chrontel version number */
			temp1 = SiS_GetCH70xx(pSiS->SiS_Pr, chrontelidreg);
			if (chronteltype == 1) {
				/* See Chrontel TB31 for explanation */
				temp2 = SiS_GetCH700x(pSiS->SiS_Pr, 0x0e);
				if (((temp2 & 0x07) == 0x01) || (temp2 & 0x04)) {
					SiS_SetCH700x(pSiS->SiS_Pr, 0x0e, 0x0b);
					SiS_DDC2Delay(pSiS->SiS_Pr, 300);
				}
				temp2 = SiS_GetCH70xx(pSiS->SiS_Pr, chrontelidreg);
				if (temp2 != temp1) temp1 = temp2;
			}
			if (temp1 == 0xFFFF) {	/* 0xFFFF = error reading DDC port */
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"Detected Chrontel 70xx, but encountered error reading I2C port\n");
				andSISIDXREG(SISCR, 0x32, ~0x07);
				pSiS->postVBCR32 &= ~0x07;
			}
			else if ((temp1 >= 0x19) && (temp1 <= 200)) {
				/* We only support device ids 0x19-200; other values may indicate DDC problems */
				pSiS->VBFlags2 |= VB2_CHRONTEL;
				switch (temp1) {
				case 0x32: temp2 = 0; pSiS->ChrontelType = CHRONTEL_700x; break;
				case 0x3A: temp2 = 1; pSiS->ChrontelType = CHRONTEL_700x; break;
				case 0x50: temp2 = 2; pSiS->ChrontelType = CHRONTEL_700x; break;
				case 0x2A: temp2 = 3; pSiS->ChrontelType = CHRONTEL_700x; break;
				case 0x40: temp2 = 4; pSiS->ChrontelType = CHRONTEL_700x; break;
				case 0x22: temp2 = 5; pSiS->ChrontelType = CHRONTEL_700x; break;
				case 0x19: temp2 = 6; pSiS->ChrontelType = CHRONTEL_701x; break;
				case 0x20: temp2 = 7; pSiS->ChrontelType = CHRONTEL_701x; break;  /* ID for 7020? */
				default:   temp2 = 8; pSiS->ChrontelType = CHRONTEL_701x; break;
				}
				xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
					"Detected Chrontel %s TV encoder (ID 0x%02x; chip ID %d)\n",
					ChrontelTypeStr[temp2], temp1, temp);

				/* Sense connected TV's */
				SISSenseChrontel(pScrn, FALSE);

			}
			else if (temp1 == 0) {
				/* This indicates a communication problem, but it only occures if there
				 * is no TV attached. So we don't use TV in this case.
				 */
				xf86DrvMsg(pScrn->scrnIndex, X_INFO,
					"Detected Chrontel TV encoder in promiscuous state (DDC/I2C mix-up)\n");
				andSISIDXREG(SISCR, 0x32, ~0x07);
				pSiS->postVBCR32 &= ~0x07;
			}
			else {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"Chrontel: Unsupported device id (%d) detected\n", temp1);
				andSISIDXREG(SISCR, 0x32, ~0x07);
				pSiS->postVBCR32 &= ~0x07;
			}
			if (chronteltype == 1) {
				/* Set general purpose IO for Chrontel communication */
				SiS_SetChrontelGPIO(pSiS->SiS_Pr, 0x00);
			}
		}
		if ((pSiS->NewCRLayout) && (temp == 4)) {
			pSiS->VBFlags2 |= VB2_CONEXANT;
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
				"Detected Conexant video bridge - UNSUPPORTED\n");
		}
		if (temp > upperlimitvb) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Detected unknown bridge type (%d)\n", temp);
		}
	}

	/* Old BIOSes store the detected CRT2 type in SR17, 16 and 13
	 * instead of CR32. However, since our detection routines
	 * store their results to CR32, we now copy the
	 * remaining bits (for LCD and VGA) to CR32 for unified usage.
	 * SR17[0] CRT1     [1] LCD       [2] TV    [3] VGA2
	 *     [4] AVIDEO   [5] SVIDEO
	 * SR13[0] SCART    [1] HiVision
	 * SR16[5] PAL/NTSC [6] LCD-SCALE [7] OVERSCAN
	 */

#if 0
	inSISIDXREG(SISSR, 0x17, sr17);
#endif

	/* Try to find out if the BIOS uses LCDA for low resolution and
	 * text modes. If sisfb saved this for us, use it. Otherwise,
	 * check if we are running on a low mode on LCD and read the
	 * relevant registers ourselves.
	 */
	if (pSiS->VGAEngine == SIS_315_VGA) {

		if (pSiS->VBFlags2 & VB2_SISLCDABRIDGE) {
			if (pSiS->sisfblcda != 0xff) {
				if ((pSiS->sisfblcda & 0x03) == 0x03) {
					pSiS->SiS_Pr->SiS_UseLCDA = TRUE;
				}
			}
			else {
				inSISIDXREG(SISCR, 0x34, temp);
				if (temp <= 0x13) {
					inSISIDXREG(SISCR, 0x38, temp);
					if ((temp & 0x03) == 0x03) {
						pSiS->SiS_Pr->SiS_UseLCDA = TRUE;
						pSiS->SiS_Pr->Backup = TRUE;
					}
					else {
						orSISIDXREG(SISPART1, 0x2f, 0x01);  /* Unlock CRT2 */
						inSISIDXREG(SISPART1, 0x13, temp);
						if (temp & 0x04) {
							pSiS->SiS_Pr->SiS_UseLCDA = TRUE;
							pSiS->SiS_Pr->Backup = TRUE;
						}
					}
				}
			}
			if (pSiS->SiS_Pr->SiS_UseLCDA) {
				xf86DrvMsgVerb(pScrn->scrnIndex, X_PROBED, 4,
					"BIOS uses LCDA for low resolution and text modes\n");
				if (pSiS->SiS_Pr->Backup == TRUE) {
					inSISIDXREG(SISCR, 0x34, pSiS->SiS_Pr->Backup_Mode);
					inSISIDXREG(SISPART1, 0x14, pSiS->SiS_Pr->Backup_14);
					inSISIDXREG(SISPART1, 0x15, pSiS->SiS_Pr->Backup_15);
					inSISIDXREG(SISPART1, 0x16, pSiS->SiS_Pr->Backup_16);
					inSISIDXREG(SISPART1, 0x17, pSiS->SiS_Pr->Backup_17);
					inSISIDXREG(SISPART1, 0x18, pSiS->SiS_Pr->Backup_18);
					inSISIDXREG(SISPART1, 0x19, pSiS->SiS_Pr->Backup_19);
					inSISIDXREG(SISPART1, 0x1a, pSiS->SiS_Pr->Backup_1a);
					inSISIDXREG(SISPART1, 0x1b, pSiS->SiS_Pr->Backup_1b);
					inSISIDXREG(SISPART1, 0x1c, pSiS->SiS_Pr->Backup_1c);
					inSISIDXREG(SISPART1, 0x1d, pSiS->SiS_Pr->Backup_1d);
				}
			}
		}
	}
}

static void
SiS_WriteAttr(SISPtr pSiS, int index, int value)
{
	(void)inSISREG(SISINPSTAT);
	index |= 0x20;
	outSISREG(SISAR, index);
	outSISREG(SISAR, value);
}

static int
SiS_ReadAttr(SISPtr pSiS, int index)
{
	(void)inSISREG(SISINPSTAT);
	index |= 0x20;
	outSISREG(SISAR, index);
	return(inSISREG(SISARR));
}

static void
SiS_EnablePalette(SISPtr pSiS)
{
	(void)inSISREG(SISINPSTAT);
	outSISREG(SISAR, 0x00);
	pSiS->VGAPaletteEnabled = TRUE;
}

static void
SiS_DisablePalette(SISPtr pSiS)
{
	(void)inSISREG(SISINPSTAT);
	outSISREG(SISAR, 0x20);
	pSiS->VGAPaletteEnabled = FALSE;
}

void
SISVGALock(SISPtr pSiS)
{
	orSISIDXREG(SISCR, 0x11, 0x80);  	/* Protect CRTC[0-7] */
}

void
SiSVGAUnlock(SISPtr pSiS)
{
	andSISIDXREG(SISCR, 0x11, 0x7f);	/* Unprotect CRTC[0-7] */
}

#define SIS_FONTS_SIZE (8 * 8192)

void
SiSVGASaveFonts(ScrnInfoPtr pScrn)
{
#ifdef SIS_PC_PLATFORM
	SISPtr pSiS = SISPTR(pScrn);
	pointer vgaMemBase = pSiS->VGAMemBase;
	UChar miscOut, attr10, gr4, gr5, gr6, seq2, seq4, scrn;

	if ((pSiS->fonts) || (vgaMemBase == NULL)) return;

	/* If in graphics mode, don't save anything */
	attr10 = SiS_ReadAttr(pSiS, 0x10);
	if (attr10 & 0x01) return;

	if (!(pSiS->fonts = malloc(SIS_FONTS_SIZE * 2))) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Could not save console fonts, mem allocation failed\n");
		return;
	}

	/* save the registers that are needed here */
	miscOut = inSISREG(SISMISCR);
	inSISIDXREG(SISGR, 0x04, gr4);
	inSISIDXREG(SISGR, 0x05, gr5);
	inSISIDXREG(SISGR, 0x06, gr6);
	inSISIDXREG(SISSR, 0x02, seq2);
	inSISIDXREG(SISSR, 0x04, seq4);

	/* Force into color mode */
	outSISREG(SISMISCW, miscOut | 0x01);

	inSISIDXREG(SISSR, 0x01, scrn);
	outSISIDXREG(SISSR, 0x00, 0x01);
	outSISIDXREG(SISSR, 0x01, scrn | 0x20);
	outSISIDXREG(SISSR, 0x00, 0x03);

	SiS_WriteAttr(pSiS, 0x10, 0x01);  /* graphics mode */

	/*font1 */
	outSISIDXREG(SISSR, 0x02, 0x04);  /* write to plane 2 */
	outSISIDXREG(SISSR, 0x04, 0x06);  /* enable plane graphics */
	outSISIDXREG(SISGR, 0x04, 0x02);  /* read plane 2 */
	outSISIDXREG(SISGR, 0x05, 0x00);  /* write mode 0, read mode 0 */
	outSISIDXREG(SISGR, 0x06, 0x05);  /* set graphics */
	slowbcopy_frombus(vgaMemBase, pSiS->fonts, SIS_FONTS_SIZE);

	/* font2 */
	outSISIDXREG(SISSR, 0x02, 0x08);  /* write to plane 3 */
	outSISIDXREG(SISSR, 0x04, 0x06);  /* enable plane graphics */
	outSISIDXREG(SISGR, 0x04, 0x03);  /* read plane 3 */
	outSISIDXREG(SISGR, 0x05, 0x00);  /* write mode 0, read mode 0 */
	outSISIDXREG(SISGR, 0x06, 0x05);  /* set graphics */
	slowbcopy_frombus(vgaMemBase, pSiS->fonts + SIS_FONTS_SIZE, SIS_FONTS_SIZE);

	inSISIDXREG(SISSR, 0x01, scrn);
	outSISIDXREG(SISSR, 0x00, 0x01);
	outSISIDXREG(SISSR, 0x01, scrn & ~0x20);
	outSISIDXREG(SISSR, 0x00, 0x03);

	/* Restore clobbered registers */
	SiS_WriteAttr(pSiS, 0x10, attr10);
	outSISIDXREG(SISSR, 0x02, seq2);
	outSISIDXREG(SISSR, 0x04, seq4);
	outSISIDXREG(SISGR, 0x04, gr4);
	outSISIDXREG(SISGR, 0x05, gr5);
	outSISIDXREG(SISGR, 0x06, gr6);
	outSISREG(SISMISCW, miscOut);
#endif
}

static void
SiSVGASaveMode(ScrnInfoPtr pScrn, SISRegPtr save)
{
	SISPtr pSiS = SISPTR(pScrn);
	int i;

	save->sisRegMiscOut = inSISREG(SISMISCR);

	for (i = 0; i < 25; i++) {
		inSISIDXREG(SISCR, i, save->sisRegs3D4[i]);
	}
	inSISIDXREG(SISCR, 0x7d, save->sisRegs3D4[0x7d]);

	SiS_EnablePalette(pSiS);
	for (i = 0; i < 21; i++) {
		save->sisRegsATTR[i] = SiS_ReadAttr(pSiS, i);
	}
	SiS_DisablePalette(pSiS);

	for (i = 0; i < 9; i++) {
		inSISIDXREG(SISGR, i, save->sisRegsGR[i]);
	}

	for (i = 1; i < 5; i++) {
		inSISIDXREG(SISSR, i, save->sisRegs3C4[i]);
	}
}

static void
SiSVGASaveColormap(ScrnInfoPtr pScrn, SISRegPtr save)
{
	SISPtr pSiS = SISPTR(pScrn);
	int i;

	if (pSiS->VGACMapSaved) return;

	outSISREG(SISPEL, 0xff);

	outSISREG(SISCOLIDXR, 0x00);
	for (i = 0; i < 768; i++) {
		save->sisDAC[i] = inSISREG(SISCOLDATA);
		(void)inSISREG(SISINPSTAT);
		(void)inSISREG(SISINPSTAT);
	}

	SiS_DisablePalette(pSiS);
	pSiS->VGACMapSaved = TRUE;
}

void
SiSVGASave(ScrnInfoPtr pScrn, SISRegPtr save, int flags)
{
	if (save == NULL) return;

	if (flags & SISVGA_SR_CMAP)  SiSVGASaveColormap(pScrn, save);
	if (flags & SISVGA_SR_MODE)  SiSVGASaveMode(pScrn, save);
	if (flags & SISVGA_SR_FONTS) SiSVGASaveFonts(pScrn);
}

void
SiSVGARestoreFonts(ScrnInfoPtr pScrn)
{
#ifdef SIS_PC_PLATFORM
	SISPtr pSiS = SISPTR(pScrn);
	pointer vgaMemBase = pSiS->VGAMemBase;
	UChar miscOut, attr10, gr1, gr3, gr4, gr5, gr6, gr8, seq2, seq4, scrn;

	if ((!pSiS->fonts) || (vgaMemBase == NULL)) return;

	/* save the registers that are needed here */
	miscOut = inSISREG(SISMISCR);
	attr10 = SiS_ReadAttr(pSiS, 0x10);
	inSISIDXREG(SISGR, 0x01, gr1);
	inSISIDXREG(SISGR, 0x03, gr3);
	inSISIDXREG(SISGR, 0x04, gr4);
	inSISIDXREG(SISGR, 0x05, gr5);
	inSISIDXREG(SISGR, 0x06, gr6);
	inSISIDXREG(SISGR, 0x08, gr8);
	inSISIDXREG(SISSR, 0x02, seq2);
	inSISIDXREG(SISSR, 0x04, seq4);

	/* Force into color mode */
	outSISREG(SISMISCW, miscOut | 0x01);
	inSISIDXREG(SISSR, 0x01, scrn);
	outSISIDXREG(SISSR, 0x00, 0x01);
	outSISIDXREG(SISSR, 0x01, scrn | 0x20);
	outSISIDXREG(SISSR, 0x00, 0x03);

	SiS_WriteAttr(pSiS, 0x10, 0x01);	  /* graphics mode */
	if (pScrn->depth == 4) {
		outSISIDXREG(SISGR, 0x03, 0x00);  /* don't rotate, write unmodified */
		outSISIDXREG(SISGR, 0x08, 0xFF);  /* write all bits in a byte */
		outSISIDXREG(SISGR, 0x01, 0x00);  /* all planes come from CPU */
	}

	outSISIDXREG(SISSR, 0x02, 0x04); /* write to plane 2 */
	outSISIDXREG(SISSR, 0x04, 0x06); /* enable plane graphics */
	outSISIDXREG(SISGR, 0x04, 0x02); /* read plane 2 */
	outSISIDXREG(SISGR, 0x05, 0x00); /* write mode 0, read mode 0 */
	outSISIDXREG(SISGR, 0x06, 0x05); /* set graphics */
	slowbcopy_tobus(pSiS->fonts, vgaMemBase, SIS_FONTS_SIZE);

	outSISIDXREG(SISSR, 0x02, 0x08); /* write to plane 3 */
	outSISIDXREG(SISSR, 0x04, 0x06); /* enable plane graphics */
	outSISIDXREG(SISGR, 0x04, 0x03); /* read plane 3 */
	outSISIDXREG(SISGR, 0x05, 0x00); /* write mode 0, read mode 0 */
	outSISIDXREG(SISGR, 0x06, 0x05); /* set graphics */
	slowbcopy_tobus(pSiS->fonts + SIS_FONTS_SIZE, vgaMemBase, SIS_FONTS_SIZE);

	inSISIDXREG(SISSR, 0x01, scrn);
	outSISIDXREG(SISSR, 0x00, 0x01);
	outSISIDXREG(SISSR, 0x01, scrn & ~0x20);
	outSISIDXREG(SISSR, 0x00, 0x03);

	/* restore the registers that were changed */
	outSISREG(SISMISCW, miscOut);
	SiS_WriteAttr(pSiS, 0x10, attr10);
	outSISIDXREG(SISGR, 0x01, gr1);
	outSISIDXREG(SISGR, 0x03, gr3);
	outSISIDXREG(SISGR, 0x04, gr4);
	outSISIDXREG(SISGR, 0x05, gr5);
	outSISIDXREG(SISGR, 0x06, gr6);
	outSISIDXREG(SISGR, 0x08, gr8);
	outSISIDXREG(SISSR, 0x02, seq2);
	outSISIDXREG(SISSR, 0x04, seq4);
#endif
}

static void
SiSVGARestoreMode(ScrnInfoPtr pScrn, SISRegPtr restore)
{
	SISPtr pSiS = SISPTR(pScrn);
	int i;

	outSISREG(SISMISCW, restore->sisRegMiscOut);

	for (i = 1; i < 5; i++) {
		outSISIDXREG(SISSR, i, restore->sisRegs3C4[i]);
	}

	outSISIDXREG(SISCR, 17, restore->sisRegs3D4[17] & ~0x80);

	for (i = 0; i < 25; i++) {
		outSISIDXREG(SISCR, i, restore->sisRegs3D4[i]);
	}
	outSISIDXREG(SISCR, 0x7d, restore->sisRegs3D4[0x7d]);


	for (i = 0; i < 9; i++) {
		outSISIDXREG(SISGR, i, restore->sisRegsGR[i]);
	}

	SiS_EnablePalette(pSiS);
	for (i = 0; i < 21; i++) {
		SiS_WriteAttr(pSiS, i, restore->sisRegsATTR[i]);
	}
	SiS_DisablePalette(pSiS);
}


static void
SiSVGARestoreColormap(ScrnInfoPtr pScrn, SISRegPtr restore)
{
	SISPtr pSiS = SISPTR(pScrn);
	int i;

	if (!pSiS->VGACMapSaved) return;

	outSISREG(SISPEL, 0xff);

	outSISREG(SISCOLIDX, 0x00);
	for (i = 0; i < 768; i++) {
		outSISREG(SISCOLDATA, restore->sisDAC[i]);
		(void)inSISREG(SISINPSTAT);
		(void)inSISREG(SISINPSTAT);
	}

	SiS_DisablePalette(pSiS);
}

void
SiSVGARestore(ScrnInfoPtr pScrn, SISRegPtr restore, int flags)
{
	if (restore == NULL) return;

	if (flags & SISVGA_SR_MODE)  SiSVGARestoreMode(pScrn, restore);
	if (flags & SISVGA_SR_FONTS) SiSVGARestoreFonts(pScrn);
	if (flags & SISVGA_SR_CMAP)  SiSVGARestoreColormap(pScrn, restore);
}

static void
SiS_SeqReset(SISPtr pSiS, Bool start)
{
	if (start) {
		outSISIDXREG(SISSR, 0x00, 0x01);	/* Synchronous Reset */
	}
	else {
		outSISIDXREG(SISSR, 0x00, 0x03);	/* End Reset */
	}
}

void
SiSVGAProtect(ScrnInfoPtr pScrn, Bool on)
{
	SISPtr pSiS = SISPTR(pScrn);
	UChar  tmp;

	if (!pScrn->vtSema) return;

	if (on) {
		inSISIDXREG(SISSR, 0x01, tmp);
		SiS_SeqReset(pSiS, TRUE);		/* start synchronous reset */
		outSISIDXREG(SISSR, 0x01, tmp | 0x20);	/* disable display */
		SiS_EnablePalette(pSiS);
	}
	else {
		andSISIDXREG(SISSR, 0x01, ~0x20);	/* enable display */
		SiS_SeqReset(pSiS, FALSE);		/* clear synchronous reset */
		SiS_DisablePalette(pSiS);
	}
}

#ifdef SIS_PC_PLATFORM
Bool
SiSVGAMapMem(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	/* Map only once */
	if (pSiS->VGAMemBase) return TRUE;

	if (pSiS->VGAMapSize == 0) pSiS->VGAMapSize = (64 * 1024);
	if (pSiS->VGAMapPhys == 0) pSiS->VGAMapPhys = 0xA0000;

	(void)pci_device_map_legacy(pSiS->PciInfo, pSiS->VGAMapPhys, pSiS->VGAMapSize,
		PCI_DEV_MAP_FLAG_WRITABLE, &pSiS->VGAMemBase);

	return(pSiS->VGAMemBase != NULL);
}

void
SiSVGAUnmapMem(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	if (pSiS->VGAMemBase == NULL) return;

	(void)pci_device_unmap_legacy(pSiS->PciInfo, pSiS->VGAMemBase, pSiS->VGAMapSize);

	pSiS->VGAMemBase = NULL;
}
#endif

#if 0
static CARD32
SiS_HBlankKGA(DisplayModePtr mode, SISRegPtr regp, int nBits, unsigned int Flags)
{
	int    nExtBits = (nBits < 6) ? 0 : nBits - 6;
	CARD32 ExtBits;
	CARD32 ExtBitMask = ((1 << nExtBits) - 1) << 6;

	regp->sisRegs3D4[3] = (regp->sisRegs3D4[3] & ~0x1F) |
		(((mode->CrtcHBlankEnd >> 3) - 1) & 0x1F);
	regp->sisRegs3D4[5] = (regp->sisRegs3D4[5] & ~0x80) |
		((((mode->CrtcHBlankEnd >> 3) - 1) & 0x20) << 2);
	ExtBits = ((mode->CrtcHBlankEnd >> 3) - 1) & ExtBitMask;

	if ((Flags & SISKGA_FIX_OVERSCAN) &&
		((mode->CrtcHBlankEnd >> 3) == (mode->CrtcHTotal >> 3))) {
		int i = (regp->sisRegs3D4[3] & 0x1F) |
			((regp->sisRegs3D4[5] & 0x80) >> 2) |
			ExtBits;
		if (Flags & SISKGA_ENABLE_ON_ZERO) {
			if ((i-- > (((mode->CrtcHBlankStart >> 3) - 1) & (0x3F | ExtBitMask))) &&
				(mode->CrtcHBlankEnd == mode->CrtcHTotal)) {
				i = 0;
			}
		}
		else if (Flags & SISKGA_BE_TOT_DEC) i--;
		regp->sisRegs3D4[3] = (regp->sisRegs3D4[3] & ~0x1F) | (i & 0x1F);
		regp->sisRegs3D4[5] = (regp->sisRegs3D4[5] & ~0x80) | ((i << 2) & 0x80);
		ExtBits = i & ExtBitMask;
	}
	return ExtBits >> 6;
}
#endif

static CARD32
SiS_VBlankKGA(DisplayModePtr mode, SISRegPtr regp, int nBits, unsigned int Flags)
{
	CARD32 nExtBits = (nBits < 8) ? 0 : (nBits - 8);
	CARD32 ExtBitMask = ((1 << nExtBits) - 1) << 8;
	CARD32 ExtBits = (mode->CrtcVBlankEnd - 1) & ExtBitMask;
	CARD32 BitMask = (nBits < 7) ? 0 : ((1 << nExtBits) - 1);
	int VBlankStart = (mode->CrtcVBlankStart - 1) & 0xFF;
	regp->sisRegs3D4[22] = (mode->CrtcVBlankEnd - 1) & 0xFF;

	if ((Flags & SISKGA_FIX_OVERSCAN) && (mode->CrtcVBlankEnd == mode->CrtcVTotal)) {
		int i = regp->sisRegs3D4[22] | ExtBits;
		if (Flags & SISKGA_ENABLE_ON_ZERO) {
			if (((BitMask && ((i & BitMask) > (VBlankStart & BitMask))) ||
				((i > VBlankStart) &&  		            /* 8-bit case */
				((i & 0x7F) > (VBlankStart & 0x7F)))) &&    /* 7-bit case */
					(!(regp->sisRegs3D4[9] & 0x9F))) {	    /* 1 scanline/row */
				i = 0;
			}
			else {
				i--;
			}
		}
		else if (Flags & SISKGA_BE_TOT_DEC) i--;

		regp->sisRegs3D4[22] = i & 0xFF;
		ExtBits = i & 0xFF00;
	}
	return (ExtBits >> 8);
}

static Bool
SiSVGAInit(ScrnInfoPtr pScrn, DisplayModePtr mode, int fixsync)
{
	SISPtr pSiS = SISPTR(pScrn);
	SISRegPtr regp = &pSiS->ModeReg;
	int depth = pScrn->depth;
	unsigned int i;

	/* Sync */
	if ((mode->Flags & (V_PHSYNC | V_NHSYNC)) && (mode->Flags & (V_PVSYNC | V_NVSYNC))) {
		regp->sisRegMiscOut = 0x23;
		if (mode->Flags & V_NHSYNC) regp->sisRegMiscOut |= 0x40;
		if (mode->Flags & V_NVSYNC) regp->sisRegMiscOut |= 0x80;
	}
	else {
		int VDisplay = mode->VDisplay;

		if (mode->Flags & V_DBLSCAN) VDisplay *= 2;
		if (mode->VScan > 1)         VDisplay *= mode->VScan;

		if (VDisplay < 400)        regp->sisRegMiscOut = 0xA3;	/* +hsync -vsync */
		else if (VDisplay < 480)  regp->sisRegMiscOut = 0x63;	/* -hsync +vsync */
		else if (VDisplay < 768)  regp->sisRegMiscOut = 0xE3;	/* -hsync -vsync */
		else                      regp->sisRegMiscOut = 0x23;	/* +hsync +vsync */
	}

	regp->sisRegMiscOut |= (mode->ClockIndex & 0x03) << 2;

	/* Seq */
	if (depth == 4) regp->sisRegs3C4[0] = 0x02;
	else	   regp->sisRegs3C4[0] = 0x00;

	if (mode->Flags & V_CLKDIV2) regp->sisRegs3C4[1] = 0x09;
	else                        regp->sisRegs3C4[1] = 0x01;

	regp->sisRegs3C4[2] = 0x0F;

	regp->sisRegs3C4[3] = 0x00;

	if (depth < 8)  regp->sisRegs3C4[4] = 0x06;
	else           regp->sisRegs3C4[4] = 0x0E;

	/* CRTC */
	regp->sisRegs3D4[0] = (mode->CrtcHTotal >> 3) - 5;
	regp->sisRegs3D4[1] = (mode->CrtcHDisplay >> 3) - 1;
	regp->sisRegs3D4[2] = (mode->CrtcHBlankStart >> 3) - 1;
	regp->sisRegs3D4[3] = (((mode->CrtcHBlankEnd >> 3) - 1) & 0x1F) | 0x80;
	i = (((mode->CrtcHSkew << 2) + 0x10) & ~0x1F);
	if (i < 0x80)  regp->sisRegs3D4[3] |= i;
	regp->sisRegs3D4[4] = (mode->CrtcHSyncStart >> 3) - fixsync;
	regp->sisRegs3D4[5] = ((((mode->CrtcHBlankEnd >> 3) - 1) & 0x20) << 2) |
		(((mode->CrtcHSyncEnd >> 3) - fixsync) & 0x1F);
	regp->sisRegs3D4[6] = (mode->CrtcVTotal - 2) & 0xFF;
	regp->sisRegs3D4[7] = (((mode->CrtcVTotal - 2) & 0x100) >> 8) |
		(((mode->CrtcVDisplay - 1) & 0x100) >> 7) |
		(((mode->CrtcVSyncStart - fixsync) & 0x100) >> 6) |
		(((mode->CrtcVBlankStart - 1) & 0x100) >> 5) |
		0x10 |
		(((mode->CrtcVTotal - 2) & 0x200) >> 4) |
		(((mode->CrtcVDisplay - 1) & 0x200) >> 3) |
		(((mode->CrtcVSyncStart - fixsync) & 0x200) >> 2);
	regp->sisRegs3D4[8] = 0x00;
	regp->sisRegs3D4[9] = (((mode->CrtcVBlankStart - 1) & 0x200) >> 4) | 0x40;
	if (mode->Flags & V_DBLSCAN) regp->sisRegs3D4[9] |= 0x80;
	if (mode->VScan >= 32)     regp->sisRegs3D4[9] |= 0x1F;
	else if (mode->VScan > 1) regp->sisRegs3D4[9] |= mode->VScan - 1;
	regp->sisRegs3D4[10] = 0x00;
	regp->sisRegs3D4[11] = 0x00;
	regp->sisRegs3D4[12] = 0x00;
	regp->sisRegs3D4[13] = 0x00;
	regp->sisRegs3D4[14] = 0x00;
	regp->sisRegs3D4[15] = 0x00;
	regp->sisRegs3D4[16] = (mode->CrtcVSyncStart - fixsync) & 0xFF;
	regp->sisRegs3D4[17] = ((mode->CrtcVSyncEnd - fixsync) & 0x0F) | 0x20;
	regp->sisRegs3D4[18] = (mode->CrtcVDisplay - 1) & 0xFF;
	regp->sisRegs3D4[19] = pScrn->displayWidth >> 4;
	regp->sisRegs3D4[20] = 0x00;
	regp->sisRegs3D4[21] = (mode->CrtcVBlankStart - 1) & 0xFF;
	regp->sisRegs3D4[22] = (mode->CrtcVBlankEnd - 1) & 0xFF;
	if (depth < 8) regp->sisRegs3D4[23] = 0xE3;
	else	  regp->sisRegs3D4[23] = 0xC3;
	regp->sisRegs3D4[24] = 0xFF;

#if 0
	SiS_HBlankKGA(mode, regp, 0, SISKGA_FIX_OVERSCAN | SISKGA_ENABLE_ON_ZERO);
#endif
	SiS_VBlankKGA(mode, regp, 0, SISKGA_FIX_OVERSCAN | SISKGA_ENABLE_ON_ZERO);

	/* GR */
	regp->sisRegsGR[0] = 0x00;
	regp->sisRegsGR[1] = 0x00;
	regp->sisRegsGR[2] = 0x00;
	regp->sisRegsGR[3] = 0x00;
	regp->sisRegsGR[4] = 0x00;
	if (depth == 4) regp->sisRegsGR[5] = 0x02;
	else           regp->sisRegsGR[5] = 0x40;
	regp->sisRegsGR[6] = 0x05;   /* only map 64k VGA memory !!!! */
	regp->sisRegsGR[7] = 0x0F;
	regp->sisRegsGR[8] = 0xFF;

	/* Attr */
	for (i = 0; i <= 15; i++) {	/* standard colormap translation */
		regp->sisRegsATTR[i] = i;
	}
	if (depth == 4) regp->sisRegsATTR[16] = 0x81;
	else           regp->sisRegsATTR[16] = 0x41;
	if (depth >= 4) regp->sisRegsATTR[17] = 0xFF;
	else           regp->sisRegsATTR[17] = 0x01;
	regp->sisRegsATTR[18] = 0x0F;
	regp->sisRegsATTR[19] = 0x00;
	regp->sisRegsATTR[20] = 0x00;

	return TRUE;
}

static void
SISVGABlankScreen(ScrnInfoPtr pScrn, Bool on)
{
	SISPtr pSiS = SISPTR(pScrn);
	UChar  tmp, orig;

	inSISIDXREG(SISSR, 0x01, tmp);
	orig = tmp;
	if (on) tmp &= ~0x20;
	else   tmp |= 0x20;
	/* Only update the hardware if the state changes because the reset will
	 * disrupt the output requiring the screen to resync.
	 */
	if (orig == tmp)
		return;
	SiS_SeqReset(pSiS, TRUE);
	outSISIDXREG(SISSR, 0x01, tmp);
	SiS_SeqReset(pSiS, FALSE);
}

Bool
SiSVGASaveScreen(ScreenPtr pScreen, int mode)
{
	ScrnInfoPtr pScrn = NULL;
	Bool on = xf86IsUnblank(mode);

	if (pScreen == NULL) return FALSE;

	pScrn = xf86Screens[pScreen->myNum];

	if (pScrn->vtSema) {
		SISVGABlankScreen(pScrn, on);
	}
	return TRUE;
}

#undef SIS_FONTS_SIZE


