/* $XFree86$ */
/* $XdotOrg$ */
/*
 * 2D Acceleration for SiS 530, 620, 300, 540, 630, 730.
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria
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
 * Authors:   Thomas Winischhofer <thomas@winischhofer.net>
 *	      Can-Ru Yeou, SiS Inc.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if 0
#define DEBUG
#endif

#include "sis.h"
#include "sis_regs.h"

#include "sis300_accel.h"

 /* This is the offset to the memory for each head */
#define HEADOFFSET 	(pSiS->dhmOffset)

#ifdef SIS_USE_EXA
extern void SiSScratchSave(ScreenPtr pScreen, ExaOffscreenArea* area);
extern Bool SiSUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h, char* src, int src_pitch);
extern Bool SiSUploadToScratch(PixmapPtr pSrc, PixmapPtr pDst);
extern Bool SiSDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
	char* dst, int dst_pitch);
#endif /* EXA */

extern UChar SiSGetCopyROP(int rop);
extern UChar SiSGetPatternROP(int rop);

static void
SiSInitializeAccelerator(ScrnInfoPtr pScrn)
{
}

static void
SiSSync(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	SiSIdle
}

static void
SiSSyncAccel(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	if (!pSiS->NoAccel) SiSSync(pScrn);
}

static void
SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
	int xdir, int ydir, int rop,
	unsigned int planemask, int trans_color)
{
	SISPtr  pSiS = SISPTR(pScrn);

	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupSRCPitch(pSiS->scrnOffset)
		SiSSetupDSTRect(pSiS->scrnOffset, -1)

		if (trans_color != -1) {
			SiSSetupROP(0x0A)
				SiSSetupSRCTrans(trans_color)
				SiSSetupCMDFlag(TRANSPARENT_BITBLT)
		}
		else {
			SiSSetupROP(SiSGetCopyROP(rop))
		}
	if (xdir > 0) {
		SiSSetupCMDFlag(X_INC)
	}
	if (ydir > 0) {
		SiSSetupCMDFlag(Y_INC)
	}
}

static void
SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
	int src_x, int src_y, int dst_x, int dst_y,
	int width, int height)
{
	SISPtr pSiS = SISPTR(pScrn);
	CARD32 srcbase, dstbase;

	srcbase = dstbase = 0;
	if (src_y >= 2048) {
		srcbase = pSiS->scrnOffset * src_y;
		src_y = 0;
	}
	if ((dst_y >= pScrn->virtualY) || (dst_y >= 2048)) {
		dstbase = pSiS->scrnOffset * dst_y;
		dst_y = 0;
	}
#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif
	SiSSetupSRCBase(srcbase);
	SiSSetupDSTBase(dstbase);

	if (!(pSiS->CommandReg & X_INC)) {
		src_x += width - 1;
		dst_x += width - 1;
	}
	if (!(pSiS->CommandReg & Y_INC)) {
		src_y += height - 1;
		dst_y += height - 1;
	}
	SiSSetupRect(width, height)
		SiSSetupSRCXY(src_x, src_y)
		SiSSetupDSTXY(dst_x, dst_y)

		SiSDoCMD
}

static void
SiSSetupForSolidFill(ScrnInfoPtr pScrn,
	int color, int rop, unsigned int planemask)
{
	SISPtr pSiS = SISPTR(pScrn);

	if (pSiS->disablecolorkeycurrent || pSiS->nocolorkey) {
		if ((CARD32)color == pSiS->colorKey) {
			rop = 5;  /* NOOP */
		}
	}
	SiSSetupPATFG(color)
		SiSSetupDSTRect(pSiS->scrnOffset, -1)
		SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(SiSGetPatternROP(rop))
		/* SiSSetupCMDFlag(PATFG) - is zero */
}

static void
SiSSubsequentSolidFillRect(ScrnInfoPtr pScrn,
	int x, int y, int w, int h)
{
	SISPtr pSiS = SISPTR(pScrn);
	CARD32 dstbase = 0;

	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)
		SiSSetupDSTXY(x, y)
		SiSSetupRect(w, h)
		/* Clear commandReg because Setup can be used for Rect and Trap */
		pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
			T_L_X_INC | T_L_Y_INC |
			T_R_X_INC | T_R_Y_INC |
			TRAPAZOID_FILL);
	SiSSetupCMDFlag(X_INC | Y_INC | BITBLT)

		SiSDoCMD
}

#ifdef SIS_USE_EXA  /* ---------------------------- EXA -------------------------- */

static const unsigned short dstcol[] = { 0x0000, 0x8000, 0xc000 };

static void
SiSEXASync(ScreenPtr pScreen, int marker)
{
	SISPtr pSiS = SISPTR(xf86Screens[pScreen->myNum]);

	SiSIdle
}

static Bool
SiSPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	SISPtr pSiS = SISPTR(pScrn);
	CARD16 pitch;

	/* Planemask not supported */
	if ((planemask & ((1 << pPixmap->drawable.depth) - 1)) !=
		(1 << pPixmap->drawable.depth) - 1) {
		return FALSE;
	}

	/* Since the 530/620 have no "dest color depth" register, I
	 * assume that the 2D engine reads the current color depth
	 * from the DAC.... FIXME ? */
	if ((pPixmap->drawable.bitsPerPixel != 8) &&
		(pPixmap->drawable.bitsPerPixel != 16) &&
		(pPixmap->drawable.bitsPerPixel != 32))
		return FALSE;

	/* Check that the pitch matches the hardware's requirements. Should
	 * never be a problem due to pixmapPitchAlign and fbScreenInit.
	 */
	if ((pitch = exaGetPixmapPitch(pPixmap)) & 3)
		return FALSE;

	if (pSiS->disablecolorkeycurrent || pSiS->nocolorkey) {
		if ((CARD32)fg == pSiS->colorKey) {
			/* NOOP - does not work: Pixmap is not neccessarily in the frontbuffer */
			/* alu = 5; */ /* NOOP */
			/* Fill it black; better than blue anyway */
			fg = 0;
		}
	}

	SiSSetupPATFG(fg)
		SiSSetupDSTRect(pitch, -1)
		SiSSetupDSTColorDepth(dstcol[pPixmap->drawable.bitsPerPixel >> 4]);
	SiSSetupROP(SiSGetPatternROP(alu))
		SiSSetupDSTBase(((CARD32)exaGetPixmapOffset(pPixmap) + HEADOFFSET))
		/* SiSSetupCMDFlag(PATFG) - is zero */

		return TRUE;
}

static void
SiSSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	SISPtr pSiS = SISPTR(pScrn);

	SiSSetupDSTXY(x1, y1)
		SiSSetupRect(x2 - x1, y2 - y1)

		SiSSetupCMDFlag(X_INC | Y_INC | BITBLT)

		SiSDoCMD
}

static void
SiSDoneSolid(PixmapPtr pPixmap)
{
}

static Bool
SiSPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int xdir, int ydir,
	int alu, Pixel planemask)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	SISPtr pSiS = SISPTR(pScrn);
	CARD16 srcpitch, dstpitch;

	/* Planemask not supported */
	if ((planemask & ((1 << pSrcPixmap->drawable.depth) - 1)) !=
		(1 << pSrcPixmap->drawable.depth) - 1) {
		return FALSE;
	}

	/* Since the 530/620 have no "dest color depth" register, I
	 * assume that the 2D engine reads the current color depth
	 * from the DAC.... FIXME ? */
	if ((pDstPixmap->drawable.bitsPerPixel != 8) &&
		(pDstPixmap->drawable.bitsPerPixel != 16) &&
		(pDstPixmap->drawable.bitsPerPixel != 32))
		return FALSE;

	/* Check that the pitch matches the hardware's requirements. Should
	 * never be a problem due to pixmapPitchAlign and fbScreenInit.
	 */
	if ((srcpitch = exaGetPixmapPitch(pSrcPixmap)) & 3)
		return FALSE;
	if ((dstpitch = exaGetPixmapPitch(pDstPixmap)) & 3)
		return FALSE;

	SiSSetupDSTColorDepth(dstcol[pDstPixmap->drawable.bitsPerPixel >> 4]);
	SiSSetupSRCPitch(srcpitch)
		SiSSetupDSTRect(dstpitch, -1)

		SiSSetupROP(SiSGetCopyROP(alu))

		if (xdir >= 0) {
			SiSSetupCMDFlag(X_INC)
		}
	if (ydir >= 0) {
		SiSSetupCMDFlag(Y_INC)
	}

	SiSSetupSRCBase(((CARD32)exaGetPixmapOffset(pSrcPixmap) + HEADOFFSET));
	SiSSetupDSTBase(((CARD32)exaGetPixmapOffset(pDstPixmap) + HEADOFFSET));

	return TRUE;
}

static void
SiSCopy(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY, int width, int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	SISPtr pSiS = SISPTR(pScrn);

	if (!(pSiS->CommandReg & X_INC)) {
		srcX += width - 1;
		dstX += width - 1;
	}
	if (!(pSiS->CommandReg & Y_INC)) {
		srcY += height - 1;
		dstY += height - 1;
	}
	SiSSetupRect(width, height)
		SiSSetupSRCXY(srcX, srcY)
		SiSSetupDSTXY(dstX, dstY)

		SiSDoCMD
}

static void
SiSDoneCopy(PixmapPtr pDstPixmap)
{
}

#endif /* EXA */


/* For DGA usage */

static void
SiSDGAFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h, int color)
{
	SiSSetupForSolidFill(pScrn, color, GXcopy, ~0);
	SiSSubsequentSolidFillRect(pScrn, x, y, w, h);
}

static void
SiSDGABlitRect(ScrnInfoPtr pScrn, int srcx, int srcy, int dstx, int dsty, int w, int h, int color)
{
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	SiSSetupForScreenToScreenCopy(pScrn, xdir, ydir, GXcopy, (CARD32)~0, color);
	SiSSubsequentScreenToScreenCopy(pScrn, srcx, srcy, dstx, dsty, w, h);
}
