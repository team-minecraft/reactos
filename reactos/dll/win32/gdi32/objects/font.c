/* $Id$
 *
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS system libraries
 * FILE:            lib/gdi32/object/font.c
 * PURPOSE:         
 * PROGRAMMER:
 *
 */

#include "precomp.h"

#define NDEBUG
#include <debug.h>

#define INITIAL_FAMILY_COUNT 64

/***********************************************************************
 *              TEXTMETRIC conversion functions.
 */
static void FONT_TextMetricWToA(const TEXTMETRICW *ptmW, LPTEXTMETRICA ptmA )
{
    ptmA->tmHeight = ptmW->tmHeight;
    ptmA->tmAscent = ptmW->tmAscent;
    ptmA->tmDescent = ptmW->tmDescent;
    ptmA->tmInternalLeading = ptmW->tmInternalLeading;
    ptmA->tmExternalLeading = ptmW->tmExternalLeading;
    ptmA->tmAveCharWidth = ptmW->tmAveCharWidth;
    ptmA->tmMaxCharWidth = ptmW->tmMaxCharWidth;
    ptmA->tmWeight = ptmW->tmWeight;
    ptmA->tmOverhang = ptmW->tmOverhang;
    ptmA->tmDigitizedAspectX = ptmW->tmDigitizedAspectX;
    ptmA->tmDigitizedAspectY = ptmW->tmDigitizedAspectY;
    ptmA->tmFirstChar = ptmW->tmFirstChar > 255 ? 255 : ptmW->tmFirstChar;
    ptmA->tmLastChar = ptmW->tmLastChar > 255 ? 255 : ptmW->tmLastChar;
    ptmA->tmDefaultChar = ptmW->tmDefaultChar > 255 ? 255 : ptmW->tmDefaultChar;
    ptmA->tmBreakChar = ptmW->tmBreakChar > 255 ? 255 : ptmW->tmBreakChar;
    ptmA->tmItalic = ptmW->tmItalic;
    ptmA->tmUnderlined = ptmW->tmUnderlined;
    ptmA->tmStruckOut = ptmW->tmStruckOut;
    ptmA->tmPitchAndFamily = ptmW->tmPitchAndFamily;
    ptmA->tmCharSet = ptmW->tmCharSet;
}

/***********************************************************************
 *           FONT_mbtowc
 *
 * Returns a Unicode translation of str. If count is -1 then str is
 * assumed to be '\0' terminated, otherwise it contains the number of
 * bytes to convert.  If plenW is non-NULL, on return it will point to
 * the number of WCHARs that have been written.  The caller should free
 * the returned LPWSTR from the process heap itself.
 */
static LPWSTR FONT_mbtowc(LPCSTR str, INT count, INT *plenW)
{
    UINT cp = CP_ACP;
    INT lenW;
    LPWSTR strW;

    if(count == -1) count = strlen(str);
    lenW = MultiByteToWideChar(cp, 0, str, count, NULL, 0);
    strW = HeapAlloc(GetProcessHeap(), 0, lenW*sizeof(WCHAR));
    MultiByteToWideChar(cp, 0, str, count, strW, lenW);
    DPRINT("mapped %s -> %s  \n", str, strW);
    if(plenW) *plenW = lenW;
    return strW;
}


static BOOL FASTCALL
MetricsCharConvert(WCHAR w, UCHAR *b)
  {
  UNICODE_STRING WString;
  WCHAR WBuf[2];
  ANSI_STRING AString;
  CHAR ABuf[2];
  NTSTATUS Status;

  if (L'\0' == w)
    {
      *b = '\0';
    }
  else
    {
      WBuf[0] = w;
      WBuf[1] = L'\0';
      RtlInitUnicodeString(&WString, WBuf);
      ABuf[0] = '*';
      ABuf[1] = L'\0';
      RtlInitAnsiString(&AString, ABuf);

      Status = RtlUnicodeStringToAnsiString(&AString, &WString, FALSE);
      if (! NT_SUCCESS(Status))
	{
	  SetLastError(RtlNtStatusToDosError(Status));
	  return FALSE;
	}
      *b = ABuf[0];
    }

  return TRUE;
}

BOOL FASTCALL
TextMetricW2A(TEXTMETRICA *tma, TEXTMETRICW *tmw)
{
  UNICODE_STRING WString;
  WCHAR WBuf[256];
  ANSI_STRING AString;
  CHAR ABuf[256];
  UINT Letter;
  NTSTATUS Status;

  tma->tmHeight = tmw->tmHeight;
  tma->tmAscent = tmw->tmAscent;
  tma->tmDescent = tmw->tmDescent;
  tma->tmInternalLeading = tmw->tmInternalLeading;
  tma->tmExternalLeading = tmw->tmExternalLeading;
  tma->tmAveCharWidth = tmw->tmAveCharWidth;
  tma->tmMaxCharWidth = tmw->tmMaxCharWidth;
  tma->tmWeight = tmw->tmWeight;
  tma->tmOverhang = tmw->tmOverhang;
  tma->tmDigitizedAspectX = tmw->tmDigitizedAspectX;
  tma->tmDigitizedAspectY = tmw->tmDigitizedAspectY;

  /* The Unicode FirstChar/LastChar need not correspond to the ANSI
     FirstChar/LastChar. For example, if the font contains glyphs for
     letters A-Z and an accented version of the letter e, the Unicode
     FirstChar would be A and the Unicode LastChar would be the accented
     e. If you just translate those to ANSI, the range would become
     letters A-E instead of A-Z.
     We translate all possible ANSI chars to Unicode and find the first
     and last translated character which fall into the Unicode FirstChar/
     LastChar range and return the corresponding ANSI char. */

  /* Setup an Ansi string containing all possible letters (note: skip '\0' at
     the beginning since that would be interpreted as end-of-string, handle
     '\0' special later */
  for (Letter = 1; Letter < 256; Letter++)
    {
    ABuf[Letter - 1] = (CHAR) Letter;
    WBuf[Letter - 1] = L' ';
    }
  ABuf[255] = '\0';
  WBuf[255] = L'\0';
  RtlInitAnsiString(&AString, ABuf);
  RtlInitUnicodeString(&WString, WBuf);

  /* Find the corresponding Unicode characters */
  Status = RtlAnsiStringToUnicodeString(&WString, &AString, FALSE);
  if (! NT_SUCCESS(Status))
    {
      SetLastError(RtlNtStatusToDosError(Status));
      return FALSE;
    }

  /* Scan for the first ANSI character which maps to an Unicode character
     in the range */
  tma->tmFirstChar = '\0';
  if (L'\0' != tmw->tmFirstChar)
    {
      for (Letter = 1; Letter < 256; Letter++)
	{
	  if (tmw->tmFirstChar <= WBuf[Letter - 1] &&
	      WBuf[Letter - 1] <= tmw->tmLastChar)
	    {
	      tma->tmFirstChar = (CHAR) Letter;
	      break;
	    }
	}
    }

  /* Scan for the last ANSI character which maps to an Unicode character
     in the range */
  tma->tmLastChar = '\0';
  if (L'\0' != tmw->tmLastChar)
    {
      for (Letter = 255; 0 < Letter; Letter--)
	{
	  if (tmw->tmFirstChar <= WBuf[Letter - 1] &&
	      WBuf[Letter - 1] <= tmw->tmLastChar)
	    {
	      tma->tmLastChar = (CHAR) Letter;
	      break;
	    }
	}
    }

  if (! MetricsCharConvert(tmw->tmDefaultChar, &tma->tmDefaultChar) ||
      ! MetricsCharConvert(tmw->tmBreakChar, &tma->tmBreakChar))
    {
      return FALSE;
    }

  tma->tmItalic = tmw->tmItalic;
  tma->tmUnderlined = tmw->tmUnderlined;
  tma->tmStruckOut = tmw->tmStruckOut;
  tma->tmPitchAndFamily = tmw->tmPitchAndFamily;
  tma->tmCharSet = tmw->tmCharSet;

  return TRUE;
}

BOOL FASTCALL
NewTextMetricW2A(NEWTEXTMETRICA *tma, NEWTEXTMETRICW *tmw)
{
  if (! TextMetricW2A((TEXTMETRICA *) tma, (TEXTMETRICW *) tmw))
    {
      return FALSE;
    }

  tma->ntmFlags = tmw->ntmFlags;
  tma->ntmSizeEM = tmw->ntmSizeEM;
  tma->ntmCellHeight = tmw->ntmCellHeight;
  tma->ntmAvgWidth = tmw->ntmAvgWidth;

  return TRUE;
}

BOOL FASTCALL
NewTextMetricExW2A(NEWTEXTMETRICEXA *tma, NEWTEXTMETRICEXW *tmw)
{
  if (! NewTextMetricW2A(&tma->ntmTm, &tmw->ntmTm))
    {
      return FALSE;
    }

  tma->ntmFontSig = tmw->ntmFontSig;

  return TRUE;
}

static int FASTCALL
IntEnumFontFamilies(HDC Dc, LPLOGFONTW LogFont, PVOID EnumProc, LPARAM lParam,
                    BOOL Unicode)
{
  int FontFamilyCount;
  int FontFamilySize;
  PFONTFAMILYINFO Info;
  int Ret = 0;
  int i;
  ENUMLOGFONTEXA EnumLogFontExA;
  NEWTEXTMETRICEXA NewTextMetricExA;

  Info = RtlAllocateHeap(GetProcessHeap(), 0,
                         INITIAL_FAMILY_COUNT * sizeof(FONTFAMILYINFO));
  if (NULL == Info)
    {
      return 0;
    }
  FontFamilyCount = NtGdiGetFontFamilyInfo(Dc, LogFont, Info, INITIAL_FAMILY_COUNT);
  if (FontFamilyCount < 0)
    {
      RtlFreeHeap(GetProcessHeap(), 0, Info);
      return 0;
    }
  if (INITIAL_FAMILY_COUNT < FontFamilyCount)
    {
      FontFamilySize = FontFamilyCount;
      RtlFreeHeap(GetProcessHeap(), 0, Info);
      Info = RtlAllocateHeap(GetProcessHeap(), 0,
                             FontFamilyCount * sizeof(FONTFAMILYINFO));
      if (NULL == Info)
        {
          return 0;
        }
      FontFamilyCount = NtGdiGetFontFamilyInfo(Dc, LogFont, Info, FontFamilySize);
      if (FontFamilyCount < 0 || FontFamilySize < FontFamilyCount)
        {
          RtlFreeHeap(GetProcessHeap(), 0, Info);
          return 0;
        }
    }

  for (i = 0; i < FontFamilyCount; i++)
    {
      if (Unicode)
        {
          Ret = ((FONTENUMPROCW) EnumProc)(
            &Info[i].EnumLogFontEx,
            &Info[i].NewTextMetricEx,
            Info[i].FontType, lParam);
        }
      else
        {
          LogFontW2A(&EnumLogFontExA.elfLogFont, &Info[i].EnumLogFontEx.elfLogFont);
          WideCharToMultiByte(CP_THREAD_ACP, 0, Info[i].EnumLogFontEx.elfFullName, -1,
                              (LPSTR)EnumLogFontExA.elfFullName, LF_FULLFACESIZE, NULL, NULL);
          WideCharToMultiByte(CP_THREAD_ACP, 0, Info[i].EnumLogFontEx.elfStyle, -1,
                              (LPSTR)EnumLogFontExA.elfStyle, LF_FACESIZE, NULL, NULL);
          WideCharToMultiByte(CP_THREAD_ACP, 0, Info[i].EnumLogFontEx.elfScript, -1,
                              (LPSTR)EnumLogFontExA.elfScript, LF_FACESIZE, NULL, NULL);
          NewTextMetricExW2A(&NewTextMetricExA,
                             &Info[i].NewTextMetricEx);
          Ret = ((FONTENUMPROCA) EnumProc)(
            &EnumLogFontExA,
            &NewTextMetricExA,
            Info[i].FontType, lParam);
        }
    }

  RtlFreeHeap(GetProcessHeap(), 0, Info);

  return Ret;
}

/*
 * @implemented
 */
int STDCALL
EnumFontFamiliesExW(HDC hdc, LPLOGFONTW lpLogfont, FONTENUMPROCW lpEnumFontFamExProc,
                    LPARAM lParam, DWORD dwFlags)
{
  return IntEnumFontFamilies(hdc, lpLogfont, lpEnumFontFamExProc, lParam, TRUE);
}


/*
 * @implemented
 */
int STDCALL
EnumFontFamiliesW(HDC hdc, LPCWSTR lpszFamily, FONTENUMPROCW lpEnumFontFamProc,
                  LPARAM lParam)
{
  LOGFONTW LogFont;

  ZeroMemory(&LogFont, sizeof(LOGFONTW));
  LogFont.lfCharSet = DEFAULT_CHARSET;
  if (NULL != lpszFamily)
    {
      lstrcpynW(LogFont.lfFaceName, lpszFamily, LF_FACESIZE);
    }

  return IntEnumFontFamilies(hdc, &LogFont, lpEnumFontFamProc, lParam, TRUE);
}


/*
 * @implemented
 */
int STDCALL
EnumFontFamiliesExA (HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpEnumFontFamExProc,
                     LPARAM lParam, DWORD dwFlags)
{
  LOGFONTW LogFontW;

  LogFontA2W(&LogFontW, lpLogfont);

  /* no need to convert LogFontW back to lpLogFont b/c it's an [in] parameter only */
  return IntEnumFontFamilies(hdc, &LogFontW, lpEnumFontFamExProc, lParam, FALSE);
}


/*
 * @implemented
 */
int STDCALL
EnumFontFamiliesA(HDC hdc, LPCSTR lpszFamily, FONTENUMPROCA lpEnumFontFamProc,
                  LPARAM lParam)
{
  LOGFONTW LogFont;

  ZeroMemory(&LogFont, sizeof(LOGFONTW));
  LogFont.lfCharSet = DEFAULT_CHARSET;
  if (NULL != lpszFamily)
    {
      MultiByteToWideChar(CP_THREAD_ACP, 0, lpszFamily, -1, LogFont.lfFaceName, LF_FACESIZE);
    }

  return IntEnumFontFamilies(hdc, &LogFont, lpEnumFontFamProc, lParam, FALSE);
}


/*
 * @implemented
 */
BOOL
STDCALL
GetCharWidthA (
	HDC	hdc,
	UINT	iFirstChar,
	UINT	iLastChar,
	LPINT	lpBuffer
	)
{
DPRINT("GCWA iFirstChar %x\n",iFirstChar);

  return GetCharWidth32A ( hdc, iFirstChar, iLastChar, lpBuffer );
}


/*
 * @implemented
 */
BOOL
STDCALL
GetCharWidth32A(
	HDC	hdc,
	UINT	iFirstChar,
	UINT	iLastChar,
	LPINT	lpBuffer
	)
{
    INT i, wlen, count = (INT)(iLastChar - iFirstChar + 1);
    LPSTR str;
    LPWSTR wstr;
    BOOL ret = TRUE;
DPRINT("GCW32A iFirstChar %x\n",iFirstChar);

    if(count <= 0) return FALSE;

    str = HeapAlloc(GetProcessHeap(), 0, count);
    for(i = 0; i < count; i++)
	str[i] = (BYTE)(iFirstChar + i);

    wstr = FONT_mbtowc(str, count, &wlen);

    for(i = 0; i < wlen; i++)
    {
        /* FIXME should be NtGdiGetCharWidthW */
	if(!NtGdiGetCharWidth32 (hdc, wstr[i], wstr[i], lpBuffer))
	{
	    ret = FALSE;
	    break;
	}
	lpBuffer++;
    }

    HeapFree(GetProcessHeap(), 0, str);
    HeapFree(GetProcessHeap(), 0, wstr);

    return ret;
}


/*
 * @implemented
 */
BOOL
STDCALL
GetCharWidthW (
	HDC	hdc,
	UINT	iFirstChar,
	UINT	iLastChar,
	LPINT	lpBuffer
	)
{
DPRINT("GCW32w uFirstChar %x\n",iFirstChar);

  /* FIXME should be NtGdiGetCharWidthW */
  return NtGdiGetCharWidth32 ( hdc, iFirstChar, iLastChar, lpBuffer );
}


/*
 * @implemented
 */
DWORD
STDCALL
GetCharacterPlacementW(
	HDC hdc,
	LPCWSTR lpString,
	INT uCount,
	INT nMaxExtent,
	GCP_RESULTSW *lpResults,
	DWORD dwFlags
	)
{
  DWORD ret=0;
  SIZE size;
  UINT i, nSet;

  if(dwFlags&(~GCP_REORDER)) DPRINT("flags 0x%08lx ignored\n", dwFlags);
  if(lpResults->lpClass) DPRINT("classes not implemented\n");
  if (lpResults->lpCaretPos && (dwFlags & GCP_REORDER))
    DPRINT("Caret positions for complex scripts not implemented\n");

  nSet = (UINT)uCount;
  if(nSet > lpResults->nGlyphs)
    nSet = lpResults->nGlyphs;

  /* return number of initialized fields */
  lpResults->nGlyphs = nSet;

/*if((dwFlags&GCP_REORDER)==0 || !BidiAvail)
  {*/
    /* Treat the case where no special handling was requested in a fastpath way */
    /* copy will do if the GCP_REORDER flag is not set */
    if(lpResults->lpOutString)
      lstrcpynW( lpResults->lpOutString, lpString, nSet );

    if(lpResults->lpGlyphs)
      lstrcpynW( lpResults->lpGlyphs, lpString, nSet );

    if(lpResults->lpOrder)
    {
      for(i = 0; i < nSet; i++)
      lpResults->lpOrder[i] = i;
    }
/*} else
  {
      BIDI_Reorder( lpString, uCount, dwFlags, WINE_GCPW_FORCE_LTR, lpResults->lpOutString,
                    nSet, lpResults->lpOrder );
  }*/

  /* FIXME: Will use the placement chars */
  if (lpResults->lpDx)
  {
    int c;
    for (i = 0; i < nSet; i++)
    {
      if (NtGdiGetCharWidth32(hdc, lpString[i], lpString[i], &c))
        lpResults->lpDx[i]= c;
    }
  }

  if (lpResults->lpCaretPos && !(dwFlags & GCP_REORDER))
  {
    int pos = 0;
       
    lpResults->lpCaretPos[0] = 0;
    for (i = 1; i < nSet; i++)
      if (GetTextExtentPoint32W(hdc, &(lpString[i - 1]), 1, &size))
        lpResults->lpCaretPos[i] = (pos += size.cx);
  }
   
  /*if(lpResults->lpGlyphs)
    GetGlyphIndicesW(hdc, lpString, nSet, lpResults->lpGlyphs, 0);*/

  if (GetTextExtentPoint32W(hdc, lpString, uCount, &size))
    ret = MAKELONG(size.cx, size.cy);

  return ret;
}


/*
 * @unimplemented
 */
BOOL
APIENTRY
GetCharWidthFloatA(
	HDC	hdc,
	UINT	iFirstChar,
	UINT	iLastChar,
	PFLOAT	pxBuffer
	)
{
  /* FIXME what to do with iFirstChar and iLastChar ??? */
  return NtGdiGetCharWidthFloat ( hdc, iFirstChar, iLastChar, pxBuffer );
}


/*
 * @implemented
 */
BOOL
APIENTRY
GetCharABCWidthsA(
	HDC	hdc,
	UINT	uFirstChar,
	UINT	uLastChar,
	LPABC	lpabc
	)
{
DPRINT("GCABCWA uFirstChar %x\n",uFirstChar);

return NtGdiGetCharABCWidths(hdc, uFirstChar, uLastChar, lpabc);
}


/*
 * @unimplemented
 */
BOOL
APIENTRY
GetCharABCWidthsFloatA(
	HDC		hdc,
	UINT		iFirstChar,
	UINT		iLastChar,
	LPABCFLOAT	lpABCF
	)
{
DPRINT("GCABCWFA iFirstChar %x\n",iFirstChar);

  /* FIXME what to do with iFirstChar and iLastChar ??? */
  return NtGdiGetCharABCWidthsFloat ( hdc, iFirstChar, iLastChar, lpABCF );
}


/*
 * @implemented
 */
DWORD
STDCALL
GetGlyphOutlineA(
	HDC		hdc,
	UINT		uChar,
	UINT		uFormat,
	LPGLYPHMETRICS	lpgm,
	DWORD		cbBuffer,
	LPVOID		lpvBuffer,
	CONST MAT2	*lpmat2
	)
{

    LPWSTR p = NULL;
    DWORD ret;
    UINT c;
    DPRINT("GetGlyphOutlineA  uChar %x\n", uChar);
    if(!(uFormat & GGO_GLYPH_INDEX)) {
        int len;
        char mbchs[2];
        if(uChar > 0xff) { /* but, 2 bytes character only */
            len = 2;
            mbchs[0] = (uChar & 0xff00) >> 8;
            mbchs[1] = (uChar & 0xff);
        } else {
            len = 1;
            mbchs[0] = (uChar & 0xff);
        }
        p = FONT_mbtowc(mbchs, len, NULL);
	c = p[0];
    } else
        c = uChar;
    ret = NtGdiGetGlyphOutline(hdc, c, uFormat, lpgm, cbBuffer, lpvBuffer, (CONST LPMAT2)lpmat2, TRUE);
    HeapFree(GetProcessHeap(), 0, p);
    return ret;
}


/*
 * @implemented
 */
DWORD
STDCALL
GetGlyphOutlineW(
	HDC		hdc,
	UINT		uChar,
	UINT		uFormat,
	LPGLYPHMETRICS	lpgm,
	DWORD		cbBuffer,
	LPVOID		lpvBuffer,
	CONST MAT2	*lpmat2
	)
{
  DPRINT("GetGlyphOutlineW  uChar %x\n", uChar);
  return NtGdiGetGlyphOutline ( hdc, uChar, uFormat, lpgm, cbBuffer, lpvBuffer, (CONST LPMAT2)lpmat2, TRUE);
}


/*
 * @implemented
 */
UINT
APIENTRY
GetOutlineTextMetricsA(
	HDC			hdc,
	UINT			cbData,
	LPOUTLINETEXTMETRICA	lpOTM
	)
{
    char buf[512], *ptr;
    UINT ret, needed;
    OUTLINETEXTMETRICW *lpOTMW = (OUTLINETEXTMETRICW *)buf;
    OUTLINETEXTMETRICA *output = lpOTM;
    INT left, len;

    if((ret = GetOutlineTextMetricsW(hdc, 0, NULL)) == 0)
        return 0;
    if(ret > sizeof(buf))
	lpOTMW = HeapAlloc(GetProcessHeap(), 0, ret);
    GetOutlineTextMetricsW(hdc, ret, lpOTMW);

    needed = sizeof(OUTLINETEXTMETRICA);
    if(lpOTMW->otmpFamilyName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpFamilyName), -1,
				      NULL, 0, NULL, NULL);
    if(lpOTMW->otmpFaceName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpFaceName), -1,
				      NULL, 0, NULL, NULL);
    if(lpOTMW->otmpStyleName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpStyleName), -1,
				      NULL, 0, NULL, NULL);
    if(lpOTMW->otmpFullName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpFullName), -1,
				      NULL, 0, NULL, NULL);

    if(!lpOTM) {
        ret = needed;
	goto end;
    }

    DPRINT("needed = %d\n", needed);
    if(needed > cbData)
        /* Since the supplied buffer isn't big enough, we'll alloc one
           that is and memcpy the first cbData bytes into the lpOTM at
           the end. */
        output = HeapAlloc(GetProcessHeap(), 0, needed);

    ret = output->otmSize = min(needed, cbData);
    FONT_TextMetricWToA( &lpOTMW->otmTextMetrics, &output->otmTextMetrics );
    output->otmFiller = 0;
    output->otmPanoseNumber = lpOTMW->otmPanoseNumber;
    output->otmfsSelection = lpOTMW->otmfsSelection;
    output->otmfsType = lpOTMW->otmfsType;
    output->otmsCharSlopeRise = lpOTMW->otmsCharSlopeRise;
    output->otmsCharSlopeRun = lpOTMW->otmsCharSlopeRun;
    output->otmItalicAngle = lpOTMW->otmItalicAngle;
    output->otmEMSquare = lpOTMW->otmEMSquare;
    output->otmAscent = lpOTMW->otmAscent;
    output->otmDescent = lpOTMW->otmDescent;
    output->otmLineGap = lpOTMW->otmLineGap;
    output->otmsCapEmHeight = lpOTMW->otmsCapEmHeight;
    output->otmsXHeight = lpOTMW->otmsXHeight;
    output->otmrcFontBox = lpOTMW->otmrcFontBox;
    output->otmMacAscent = lpOTMW->otmMacAscent;
    output->otmMacDescent = lpOTMW->otmMacDescent;
    output->otmMacLineGap = lpOTMW->otmMacLineGap;
    output->otmusMinimumPPEM = lpOTMW->otmusMinimumPPEM;
    output->otmptSubscriptSize = lpOTMW->otmptSubscriptSize;
    output->otmptSubscriptOffset = lpOTMW->otmptSubscriptOffset;
    output->otmptSuperscriptSize = lpOTMW->otmptSuperscriptSize;
    output->otmptSuperscriptOffset = lpOTMW->otmptSuperscriptOffset;
    output->otmsStrikeoutSize = lpOTMW->otmsStrikeoutSize;
    output->otmsStrikeoutPosition = lpOTMW->otmsStrikeoutPosition;
    output->otmsUnderscoreSize = lpOTMW->otmsUnderscoreSize;
    output->otmsUnderscorePosition = lpOTMW->otmsUnderscorePosition;


    ptr = (char*)(output + 1);
    left = needed - sizeof(*output);

    if(lpOTMW->otmpFamilyName) {
        output->otmpFamilyName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpFamilyName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
	ptr += len;
    } else
        output->otmpFamilyName = 0;

    if(lpOTMW->otmpFaceName) {
        output->otmpFaceName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpFaceName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
	ptr += len;
    } else
        output->otmpFaceName = 0;

    if(lpOTMW->otmpStyleName) {
        output->otmpStyleName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpStyleName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
	ptr += len;
    } else
        output->otmpStyleName = 0;

    if(lpOTMW->otmpFullName) {
        output->otmpFullName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (int)lpOTMW->otmpFullName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
    } else
        output->otmpFullName = 0;

    assert(left == 0);

    if(output != lpOTM) {
        memcpy(lpOTM, output, cbData);
        HeapFree(GetProcessHeap(), 0, output);

        /* check if the string offsets really fit into the provided size */
        /* FIXME: should we check string length as well? */
        if ((UINT_PTR)lpOTM->otmpFamilyName >= lpOTM->otmSize)
            lpOTM->otmpFamilyName = 0; /* doesn't fit */

        if ((UINT_PTR)lpOTM->otmpFaceName >= lpOTM->otmSize)
            lpOTM->otmpFaceName = 0; /* doesn't fit */

        if ((UINT_PTR)lpOTM->otmpStyleName >= lpOTM->otmSize)
            lpOTM->otmpStyleName = 0; /* doesn't fit */

        if ((UINT_PTR)lpOTM->otmpFullName >= lpOTM->otmSize)
            lpOTM->otmpFullName = 0; /* doesn't fit */
    }

end:
    if(lpOTMW != (OUTLINETEXTMETRICW *)buf)
        HeapFree(GetProcessHeap(), 0, lpOTMW);

    return ret;
}


/*
 * @implemented
 */
UINT
APIENTRY
GetOutlineTextMetricsW(
	HDC			hdc,
	UINT			cbData,
	LPOUTLINETEXTMETRICW	lpOTM
	)
{
  return NtGdiGetOutlineTextMetrics(hdc, cbData, lpOTM);
}


/*
 * @implemented
 */
HFONT
STDCALL
CreateFontIndirectA(
	CONST LOGFONTA		*lplf
	)
{
  LOGFONTW tlf;

  LogFontA2W(&tlf, lplf);

  return NtGdiCreateFontIndirect(&tlf);
}


/*
 * @implemented
 */
HFONT
STDCALL
CreateFontIndirectW(
	CONST LOGFONTW		*lplf
	)
{
	return NtGdiCreateFontIndirect((CONST LPLOGFONTW)lplf);
}


/*
 * @implemented
 */
HFONT
STDCALL
CreateFontA(
	int	nHeight,
	int	nWidth,
	int	nEscapement,
	int	nOrientation,
	int	fnWeight,
	DWORD	fdwItalic,
	DWORD	fdwUnderline,
	DWORD	fdwStrikeOut,
	DWORD	fdwCharSet,
	DWORD	fdwOutputPrecision,
	DWORD	fdwClipPrecision,
	DWORD	fdwQuality,
	DWORD	fdwPitchAndFamily,
	LPCSTR	lpszFace
	)
{
        ANSI_STRING StringA;
        UNICODE_STRING StringU;
	HFONT ret;

	RtlInitAnsiString(&StringA, (LPSTR)lpszFace);
	RtlAnsiStringToUnicodeString(&StringU, &StringA, TRUE);

        ret = CreateFontW(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                          fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, StringU.Buffer);

	RtlFreeUnicodeString(&StringU);

	return ret;
}


/*
 * @implemented
 */
HFONT
STDCALL
CreateFontW(
	int	nHeight,
	int	nWidth,
	int	nEscapement,
	int	nOrientation,
	int	nWeight,
	DWORD	fnItalic,
	DWORD	fdwUnderline,
	DWORD	fdwStrikeOut,
	DWORD	fdwCharSet,
	DWORD	fdwOutputPrecision,
	DWORD	fdwClipPrecision,
	DWORD	fdwQuality,
	DWORD	fdwPitchAndFamily,
	LPCWSTR	lpszFace
	)
{
  return NtGdiCreateFont(nHeight, nWidth, nEscapement, nOrientation, nWeight, fnItalic, fdwUnderline, fdwStrikeOut,
                         fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace);
}


/*
 * @implemented
 */
BOOL
STDCALL
CreateScalableFontResourceW(
	DWORD		fdwHidden,
	LPCWSTR		lpszFontRes,
	LPCWSTR		lpszFontFile,
	LPCWSTR		lpszCurrentPath
	)
{
  return NtGdiCreateScalableFontResource ( fdwHidden,
					  lpszFontRes,
					  lpszFontFile,
					  lpszCurrentPath );
}


/*
 * @implemented
 */
BOOL
STDCALL
CreateScalableFontResourceA(
	DWORD		fdwHidden,
	LPCSTR		lpszFontRes,
	LPCSTR		lpszFontFile,
	LPCSTR		lpszCurrentPath
	)
{
  NTSTATUS Status;
  LPWSTR lpszFontResW, lpszFontFileW, lpszCurrentPathW;
  BOOL rc = FALSE;

  Status = HEAP_strdupA2W ( &lpszFontResW, lpszFontRes );
  if (!NT_SUCCESS (Status))
    SetLastError (RtlNtStatusToDosError(Status));
  else
    {
      Status = HEAP_strdupA2W ( &lpszFontFileW, lpszFontFile );
      if (!NT_SUCCESS (Status))
	SetLastError (RtlNtStatusToDosError(Status));
      else
	{
	  Status = HEAP_strdupA2W ( &lpszCurrentPathW, lpszCurrentPath );
	  if (!NT_SUCCESS (Status))
	    SetLastError (RtlNtStatusToDosError(Status));
	  else
	    {
	      rc = NtGdiCreateScalableFontResource ( fdwHidden,
						    lpszFontResW,
						    lpszFontFileW,
						    lpszCurrentPathW );

	      HEAP_free ( lpszCurrentPathW );
	    }

	  HEAP_free ( lpszFontFileW );
	}

      HEAP_free ( lpszFontResW );
    }
  return rc;
}


/*
 * @implemented
 */
int
STDCALL
AddFontResourceExW ( LPCWSTR lpszFilename, DWORD fl, PVOID pvReserved )
{
  UNICODE_STRING Filename;

  /* FIXME handle fl parameter */
  RtlInitUnicodeString(&Filename, lpszFilename);
  return NtGdiAddFontResource ( &Filename, fl );
}


/*
 * @implemented
 */
int
STDCALL
AddFontResourceExA ( LPCSTR lpszFilename, DWORD fl, PVOID pvReserved )
{
  NTSTATUS Status;
  PWSTR FilenameW;
  int rc = 0;

  Status = HEAP_strdupA2W ( &FilenameW, lpszFilename );
  if ( !NT_SUCCESS (Status) )
    SetLastError (RtlNtStatusToDosError(Status));
  else
    {
      rc = AddFontResourceExW ( FilenameW, fl, pvReserved );

      HEAP_free ( FilenameW );
    }
  return rc;
}


/*
 * @implemented
 */
int
STDCALL
AddFontResourceA ( LPCSTR lpszFilename )
{
  return AddFontResourceExA ( lpszFilename, 0, 0 );
}


/*
 * @implemented
 */
int
STDCALL
AddFontResourceW ( LPCWSTR lpszFilename )
{
	return AddFontResourceExW ( lpszFilename, 0, 0 );
}


/*
 * @implemented
 */
BOOL
STDCALL
RemoveFontResourceW(
	LPCWSTR	lpFileName
	)
{
  return NtGdiRemoveFontResource ( lpFileName );
}


/*
 * @implemented
 */
BOOL
STDCALL
RemoveFontResourceA(
	LPCSTR	lpFileName
	)
{
  NTSTATUS Status;
  LPWSTR lpFileNameW;
  BOOL rc = 0;

  Status = HEAP_strdupA2W ( &lpFileNameW, lpFileName );
  if (!NT_SUCCESS (Status))
    SetLastError (RtlNtStatusToDosError(Status));
  else
    {
      rc = NtGdiRemoveFontResource ( lpFileNameW );

      HEAP_free ( lpFileNameW );
    }

  return rc;
}

/***********************************************************************
 *           GdiGetCharDimensions
 *
 * Gets the average width of the characters in the English alphabet.
 *
 * PARAMS
 *  hdc    [I] Handle to the device context to measure on.
 *  lptm   [O] Pointer to memory to store the text metrics into.
 *  height [O] On exit, the maximum height of characters in the English alphabet.
 *
 * RETURNS
 *  The average width of characters in the English alphabet.
 *
 * NOTES
 *  This function is used by the dialog manager to get the size of a dialog
 *  unit. It should also be used by other pieces of code that need to know
 *  the size of a dialog unit in logical units without having access to the
 *  window handle of the dialog.
 *  Windows caches the font metrics from this function, but we don't and
 *  there doesn't appear to be an immediate advantage to do so.
 *
 * SEE ALSO
 *  GetTextExtentPointW, GetTextMetricsW, MapDialogRect.
 *
 * Despite most of MSDN insisting that the horizontal base unit is
 * tmAveCharWidth it isn't.  Knowledge base article Q145994
 * "HOWTO: Calculate Dialog Units When Not Using the System Font",
 * says that we should take the average of the 52 English upper and lower
 * case characters.
 */
/*
 * @implemented
 */
DWORD
STDCALL
GdiGetCharDimensions(HDC hdc, LPTEXTMETRICW lptm, LONG *height)
{
    SIZE sz;
    static const WCHAR alphabet[] = {
        'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q',
        'r','s','t','u','v','w','x','y','z','A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',0};

    if(lptm && !GetTextMetricsW(hdc, lptm)) return 0;

    if(!GetTextExtentPointW(hdc, alphabet, 52, &sz)) return 0;

    if (height) *height = sz.cy;
    return (sz.cx / 26 + 1) / 2;
}


/*
 * @unimplemented
 */
int
STDCALL
EnumFontsW(
	HDC  hDC,
	LPCWSTR lpFaceName,
	FONTENUMPROCW  FontFunc,
	LPARAM  lParam
	)
{
#if 0
  return NtGdiEnumFonts ( hDC, lpFaceName, FontFunc, lParam );
#else
  return EnumFontFamiliesW( hDC, lpFaceName, FontFunc, lParam );
#endif
}

/*
 * @unimplemented
 */
int
STDCALL
EnumFontsA (
	HDC  hDC,
	LPCSTR lpFaceName,
	FONTENUMPROCA  FontFunc,
	LPARAM  lParam
	)
{
#if 0
  NTSTATUS Status;
  LPWSTR lpFaceNameW;
  int rc = 0;

  Status = HEAP_strdupA2W ( &lpFaceNameW, lpFaceName );
  if (!NT_SUCCESS (Status))
    SetLastError (RtlNtStatusToDosError(Status));
  else
    {
      rc = NtGdiEnumFonts ( hDC, lpFaceNameW, FontFunc, lParam );

      HEAP_free ( lpFaceNameW );
    }
  return rc;
#else
  return EnumFontFamiliesA( hDC, lpFaceName, FontFunc, lParam );
#endif
}




