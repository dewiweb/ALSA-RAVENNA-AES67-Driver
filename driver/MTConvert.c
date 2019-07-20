/****************************************************************************
*
*  Module Name    : MTConvert.h
*  Version        :
*
*  Abstract       : FloatToInt / IntToFloat conversion functions using 
*                   SSE/SSE2 instructions
*
*  Written by     : Loic Andrieu
*  Date           : 14/06/2005
*  Modified by    : van Kempen Bertrand, Beguec Frederic, Baume Florian
*  Date           :
*  Modification   :
*  Known problems : None
*
* Copyright(C) 2017 Merging Technologies
*
* RAVENNA/AES67 ALSA LKM is free software; you can redistribute it and / or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* RAVENNA/AES67 ALSA LKM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with RAVAENNA ALSA LKM ; if not, see <http://www.gnu.org/licenses/>.
*
****************************************************************************/

#include "MTConvert.h"

#if !(defined(MTAL_LINUX) && defined(MTAL_KERNEL))
    #ifndef max
    #define max(a,b)            (((a) > (b)) ? (a) : (b))
    #endif

    #ifndef min
    #define min(a,b)            (((a) < (b)) ? (a) : (b))
    #endif
#endif

#define SWAP16(x) (((x >> 8) & 0x00ff) | (((x) << 8) & 0xff00))
#define SWAP32(x) ((((x) >> 24) & 0x000000ff) | (((x) >> 8 ) & 0x0000ff00) | (((x) << 8 ) & 0x00ff0000) | (((x) << 24) & 0xff000000))

static bool g_bInitialized = false;

static bool g_bMTConvert_ReversedLookupTableInitialized = false;
static uint8_t g_aui8MTConvert_ReversedBitLookupTable[256];



static inline bool Arch_is_big_endian(void)
{
    #ifdef BIG_ENDIAN
    static const bool ans = true;
    #else
    static const bool ans = false;
    #endif // BIG_ENDIAN
    return ans;
}




#if defined(WIN32) && !defined(NT_DRIVER)

#include <intrin.h>
#include <math.h>
#ifdef _DEBUG
#include <assert.h>
#endif // _DEBUG

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
void NotOptimized_ConvertFloatToInt8(float *pSrc, int8_t *pDest, uint32_t dwLength);
//void Optimized_ConvertFloatToInt8(float *pSrc, int8_t *pDest, uint32_t dwLength);
//void SSE_ConvertFloatToInt8(float *pSrc, int8_t *pDest, uint32_t dwLength);

void NotOptimized_ConvertFloatToInt16(float *pSrc, int16_t *pDest, uint32_t dwLength);
#ifndef _AMD64_
void SSE_ConvertFloatToInt16(float *pSrc, int16_t *pDest, uint32_t dwLength);
#endif

void NotOptimized_ConvertFloatToInt24(float *pSrc, TRIBYTE *pDest, uint32_t dwLength);
#ifndef _AMD64_
void SSE2_ConvertFloatToInt24(float *pSrc, TRIBYTE *pDest, uint32_t dwLength);
#endif

static void NotOptimized_ConvertFloatToInt32(float *pSrc, int32_t *pDest, uint32_t dwLength);
#ifndef _AMD64_
void SSE2_ConvertFloatToInt32(float *pSrc, int32_t *pDest, uint32_t dwLength);
#endif

void NotOptimized_ConvertInt16ToFloat(int16_t * pSrc, float * pDest, uint32_t dwLength);
#ifndef _AMD64_
void SSE_ConvertInt16ToFloat(int16_t * pSrc, float * pDest, uint32_t dwLength);
#endif

void NotOptimized_ConvertInt24ToFloat(TRIBYTE * pSrc, float * pDest, uint32_t dwLength);
#ifndef _AMD64_
void SSE2_ConvertInt24ToFloat(TRIBYTE * pSrc, float * pDest, uint32_t dwLength);
#endif

static void NotOptimized_ConvertInt32ToFloat(int32_t * pSrc, float * pDest, uint32_t dwLength);
#ifndef _AMD64_
void SSE2_ConvertInt32ToFloat(int32_t * pSrc, float * pDest, uint32_t dwLength);
#endif

typedef void (*FLOATTOINT8)(float *pSrc, int8_t *pDest, uint32_t dwLength);
typedef void (*FLOATTOINT16)(float *pSrc, int16_t *pDest, uint32_t dwLength);
typedef void (*FLOATTOINT24)(float *pSrc, TRIBYTE *pDest, uint32_t dwLength);
typedef void (*FLOATTOINT32)(float *pSrc, int32_t *pDest, uint32_t dwLength);

typedef void (*INT16TOFLOAT)(int16_t * pSrc, float * pDest, uint32_t dwLength);
typedef void (*INT24TOFLOAT)(TRIBYTE * pSrc, float * pDest, uint32_t dwLength);
typedef void (*INT32TOFLOAT)(int32_t * pSrc, float * pDest, uint32_t dwLength);

FLOATTOINT8 JmpTableFloatToInt8   = NotOptimized_ConvertFloatToInt8;
FLOATTOINT16 JmpTableFloatToInt16 = NotOptimized_ConvertFloatToInt16;
FLOATTOINT24 JmpTableFloatToInt24 = NotOptimized_ConvertFloatToInt24;
FLOATTOINT32 JmpTableFloatToInt32 = NotOptimized_ConvertFloatToInt32;

INT16TOFLOAT JmpTableInt16ToFloat = NotOptimized_ConvertInt16ToFloat;
INT24TOFLOAT JmpTableInt24ToFloat = NotOptimized_ConvertInt24ToFloat;
INT32TOFLOAT JmpTableInt32ToFloat = NotOptimized_ConvertInt32ToFloat;




///////////////////////////////////////////////////////////////////////////////
// Global variables

///////////////////////////////////////////////////////////////////////////////
static uint32_t GetFeatures()
{
    /*uint32_t dwFeature;
    _asm {
        push eax
        push ebx
        push ecx
        push edx

        // get the Standard bits
        mov eax, 1
        cpuid
        mov dwFeature, edx

        pop edx
        pop ecx
        pop ebx
        pop eax
    }
    return dwFeature;*/
    int CPUInfo[4];
    int InfoType = 1; // get the Standard bits
    __cpuid(CPUInfo, InfoType);

    return CPUInfo[3];
}

///////////////////////////////////////////////////////////////////////////////
#define _MMX_FEATURE_BIT 0x00800000
#define _SSE_FEATURE_BIT 0x02000000
#define _SSE2_FEATURE_BIT 0x04000000

#elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
    #include <asm/processor.h>
    #include <linux/thread_info.h>
    #include <linux/sched.h>
    #include <asm/uaccess.h>
#endif

void MTConvertInit() //Init_Converter()
{
    if(g_bInitialized)
    {
        return;
    }
    g_bInitialized = true;

#if defined(WIN32) && !defined(NT_DRIVER)
    #ifndef _AMD64_
        uint32_t dwFeature;

        ////////////////////////////////////////////////////////////////////////////////////////////
        /*BOOL bOptimization = FALSE;
        HKEY hKey = NULL;
        LONG lResult;
        if (lResult = RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Merging Technologies\\VS3\\Settings", &hKey) == NOERROR)
        {
            uint32_t dwType = REG_uint32_t;
            uint32_t dwSize = sizeof(BOOL);
            lResult = RegQueryValueEx(hKey, "SIMD", 0, &dwType, (LPBYTE)&bOptimization, &dwSize);
            RegCloseKey(hKey);
        }
        if(( lResult != ERROR_SUCCESS) || !bOptimization)
        {
            // use default functions (not SSE)
            return;
        }*/
        ////////////////////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////////////////
        //
        // First detect the presence of the new SIMD instructions
        //
        dwFeature = GetFeatures();
        if(/*FIXME(dwFeature & _SSE2_FEATURE_BIT)*/FALSE)
        {
            // SSE2 instructions are supported
    //      JmpTableFloatToInt8 = Optimized_ConvertFloatToInt8;
            JmpTableFloatToInt16 = SSE_ConvertFloatToInt16;
            JmpTableFloatToInt24 = SSE2_ConvertFloatToInt24;
    //      JmpTableFloatToInt32 = NotOptimized_ConvertFloatToInt32;
            JmpTableFloatToInt32 = SSE2_ConvertFloatToInt32;

            JmpTableInt16ToFloat = SSE_ConvertInt16ToFloat;
            JmpTableInt24ToFloat = SSE2_ConvertInt24ToFloat;
    //      JmpTableInt32ToFloat = NotOptimized_ConvertInt32ToFloat;
            JmpTableInt32ToFloat = SSE2_ConvertInt32ToFloat;

        }
    #endif
#endif

    // Init m_abyReversedLookupTable
    if(!g_bMTConvert_ReversedLookupTableInitialized)
    {
        uint16_t dw;
        for(dw = 0; dw < 256; dw++)
        {
            uint8_t c = (uint8_t)dw;
            c = (uint8_t)((c & 0x0F) << 4 | (c & 0xF0) >> 4);
            c = (uint8_t)((c & 0x33) << 2 | (c & 0xCC) >> 2);
            c = (uint8_t)((c & 0x55) << 1 | (c & 0xAA) >> 1);
            g_aui8MTConvert_ReversedBitLookupTable[dw] = c;
        }
        g_bMTConvert_ReversedLookupTableInitialized = true;
    }
}

#if defined(WIN32) && !defined(NT_DRIVER)
///////////////////////////////////////////////////////////////////////////////
///////////////////////////// Reverse DSD //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertReverseDSD64_32(uint32_t const* pui32Src, uint32_t* pui32Dest, uint32_t ui32Length)
{
    BYTE const* pbSrc = (BYTE const*)pui32Src;
    for (unsigned int a = 0; a < ui32Length; a++)
    {
        pui32Dest[a] = g_aui8MTConvert_ReversedBitLookupTable[pbSrc[0]];
        pbSrc -= 4;
    }
}
///////////////////////////////////////////////////////////////////////////////
void MTConvertReverseDSD128_32(uint32_t const* pui32Src, uint32_t* pui32Dest, uint32_t ui32Length)
{
    BYTE const* pbSrc = (BYTE const*)pui32Src;
    for (unsigned int a = 0; a < ui32Length; a++)
    {
        pui32Dest[a] = (g_aui8MTConvert_ReversedBitLookupTable[pbSrc[0]] << 8) | g_aui8MTConvert_ReversedBitLookupTable[pbSrc[1]];
        pbSrc -= 4;
    }
}
///////////////////////////////////////////////////////////////////////////////
void MTConvertReverseDSD256_32(uint32_t const* pui32Src, uint32_t* pui32Dest, uint32_t ui32Length)
{
    BYTE const* pbSrc = (BYTE const*)pui32Src;
    for (unsigned int a = 0; a < ui32Length; a++)
    {
        pui32Dest[a] =  (g_aui8MTConvert_ReversedBitLookupTable[pbSrc[0]] << 24) |
                        (g_aui8MTConvert_ReversedBitLookupTable[pbSrc[1]] << 16) |
                        (g_aui8MTConvert_ReversedBitLookupTable[pbSrc[2]] << 8) |
                        g_aui8MTConvert_ReversedBitLookupTable[pbSrc[3]];
        pbSrc -= 4;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////// Float to Int ////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Float to int8
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertFloatToInt8(float *pSrc, int8_t *pDest, uint32_t dwLength)
{
    JmpTableFloatToInt8(pSrc, pDest, dwLength);
}
///////////////////////////////////////////////////////////////////////////////
void NotOptimized_ConvertFloatToInt8(float *pSrc, int8_t *pDest, uint32_t dwLength)
{
    float f0dB = (float)pow(2.0, 7.0);
    float fPositiveMax = 1.0f - (float)pow(2.0, -7.0);

    for (ULONG i = 0; i < dwLength; i++)
    {
        *pSrc = min( fPositiveMax, max(*pSrc, -1.f));
        *pDest++ = (int8_t)(*pSrc++ * f0dB);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Float to int16
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertFloatToInt16(float *pSrc, int16_t *pDest, uint32_t dwLength)
{
    JmpTableFloatToInt16(pSrc, pDest, dwLength);
}

#ifndef _AMD64_
///////////////////////////////////////////////////////////////////////////////
void SSE_ConvertFloatToInt16(float *pSrc, int16_t *pDest, uint32_t dwLength)
{
    __m128* pSrcFloat; // = (__m128*) pSrc;
    __m64* pDestInt16; // = (__m64*) pDest;

    uint32_t dwSamplesRemaining;
    uint32_t dwLoop;
    uint32_t i;

    float f32768 = 32768.0f;

    __m128 m128_0dB = _mm_load1_ps(&f32768);

    //Source pointer has to be 16byte aligned
    //We first check if we are 4byte(float) aligned
    if(((INT_PTR)pSrc & 0x3))
    {
        //we are not able to get 16byte alignment
        NotOptimized_ConvertFloatToInt16(pSrc, pDest, dwLength);
        return;
    }

    if((DWORD_PTR)pSrc & 0xF) // not aligned on a multiple of 0xF (= 4 * float)
    {
        uint32_t dwSrcSamplesNotAligned = (16 - ((DWORD_PTR)pSrc & 0xF)) >> 2;
        dwSrcSamplesNotAligned = min(dwSrcSamplesNotAligned, dwLength);

        NotOptimized_ConvertFloatToInt16(pSrc, pDest, dwSrcSamplesNotAligned);
        dwLength -= dwSrcSamplesNotAligned;
        pDest = (int16_t*)((INT_PTR)pDest + dwSrcSamplesNotAligned*sizeof(*pDest));
        pSrc = (float*)((INT_PTR)pSrc + dwSrcSamplesNotAligned*sizeof(*pSrc));
    }
    if((INT_PTR)pDest & 0xF)
    {
        // Src pointer is aligned but destination is not, so we cannot use SSE instructions
        NotOptimized_ConvertFloatToInt16(pSrc, pDest, dwLength);
        return;
    }

    dwSamplesRemaining = dwLength & 0x3; // modulo
    dwLoop = dwLength / 4;
    pSrcFloat = (__m128*) pSrc;
    pDestInt16 = (__m64*) pDest;
    for(i = 0; i < dwLoop; i++)
    {
        *pSrcFloat = _mm_mul_ps(*pSrcFloat,m128_0dB);
        *pDestInt16 = _mm_cvtps_pi16(*pSrcFloat);
        pDestInt16++;
        pSrcFloat++;
    }
    if (dwSamplesRemaining != 0)
    {
        NotOptimized_ConvertFloatToInt16((float*)pSrcFloat, (int16_t*)pDestInt16, dwSamplesRemaining);
    }
    _mm_empty();
}
#endif

///////////////////////////////////////////////////////////////////////////////
void NotOptimized_ConvertFloatToInt16(float *pSrc, int16_t *pDest, uint32_t dwLength)
{
    ULONG i;
    float f0dB = (float)pow(2.0, 15.0);
    float fPositiveMax = 1.0f - (float)pow(2.0, -15.0);

    for (i = 0; i < dwLength; i++)
    {
        float fTmp = min(fPositiveMax, max(*pSrc, -1.f));
        pSrc++;
        *pDest++ = (int16_t)(fTmp * f0dB);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Float to int24
///////////////////////////////////////////////////////////////////////////////
void MTConvertFloatToInt24(float *pSrc, TRIBYTE *pDest, uint32_t dwLength)
{
    JmpTableFloatToInt24(pSrc, pDest, dwLength);
}

#ifndef _AMD64_
///////////////////////////////////////////////////////////////////////////////
void SSE2_ConvertFloatToInt24(float *pSrc, TRIBYTE *pDest, uint32_t dwLength)
{
    uint32_t dwSamplesRemaining;
    uint32_t dwLoop;

    float f8388608 = 8388608.0f;

    __m128 m128_0dB = _mm_load1_ps(&f8388608);

    //Source pointer has to be 16byte aligned
    //We first check if we are 4byte(float) aligned
    if(((INT_PTR)pSrc & 0x3))
    {
        //we are not able to get 16byte alignment
        NotOptimized_ConvertFloatToInt24(pSrc, pDest, dwLength);
        return;
    }

    if((DWORD_PTR)pSrc & 0xF) // not aligned on a multiple of 0xF (= 4 * float)
    {
        uint32_t dwSrcSamplesNotAligned = (16 - ((DWORD_PTR)pSrc & 0xF)) >> 2;
        dwSrcSamplesNotAligned = min(dwSrcSamplesNotAligned, dwLength);

        NotOptimized_ConvertFloatToInt24(pSrc, pDest, dwSrcSamplesNotAligned);
        dwLength -= dwSrcSamplesNotAligned;
        pDest = (TRIBYTE*)((INT_PTR)pDest + dwSrcSamplesNotAligned*sizeof(*pDest));
        pSrc = (float*)((INT_PTR)pSrc + dwSrcSamplesNotAligned*sizeof(*pSrc));
    }
    if((INT_PTR)pDest & 0xF)
    {
        // Src pointer is aligned but destination is not, so we cannot use SSE instructions
        NotOptimized_ConvertFloatToInt24(pSrc, pDest, dwLength);
        return;
    }
    dwSamplesRemaining = dwLength & 0x3; // modulo 4
    dwLoop = dwLength / 4;

    if(dwLoop)
    {
        _asm
        {
            push        eax
            push        edx
            push        ecx
            push        esi
            push        edi

            mov         esi, pSrc
            mov         edi, pDest
            mov         ecx, dwLoop

            movaps      xmm1, m128_0dB

    ConvertFloatToInt24_Loop:
            movaps      xmm0, [esi]
            add         esi, 16
            prefetcht0  [esi]
            mulps       xmm0, xmm1
            cvtps2dq    xmm0, xmm0      ;=> X3X2X1X0

            movd        eax, xmm0
            mov         [edi], ax

            pshufd      xmm0, xmm0, 0E5h    ;=> X3X2X1X1
            movd        edx, xmm0

            shr         eax, 16
            and         eax, 0ffh
            shl         edx, 8
            or          eax, edx
            mov         [edi+2], eax

            pshufd      xmm0, xmm0, 0E6h    ; => X3X2X1X2
            movd        eax, xmm0
            mov         [edi+6], ax

            pshufd      xmm0, xmm0, 0E7h    ;=> X3X2X1X3
            movd        edx, xmm0

            shr         eax, 16
            and         eax, 0ffh
            shl         edx, 8
            or          eax, edx
            mov         [edi+8], eax
            add         edi, 12

            dec ecx
            jne         ConvertFloatToInt24_Loop

            pop         edi
            pop         esi
            pop         ecx
            pop         edx
            pop         eax
        }
    }
    if (dwSamplesRemaining != 0)
    {
        pSrc += (dwLength-dwSamplesRemaining);
        pDest += (dwLength-dwSamplesRemaining);
        NotOptimized_ConvertFloatToInt24(pSrc, pDest, dwSamplesRemaining);
    }
    _mm_empty();
}
#endif

///////////////////////////////////////////////////////////////////////////////
void NotOptimized_ConvertFloatToInt24(float *pSrc, TRIBYTE *pDest, uint32_t dwLength)
{
    float f0dB = (float)pow(2.0, 23.0);
    float fPositiveMax = 1.0f - (float)pow(2.0, -23.0);

    for (ULONG i = 0; i < dwLength; i++ )
    {
        float fTmp = min(fPositiveMax, max(*pSrc, -1.0f));
        long nTmp = (long)(fTmp * f0dB);

        TRIBYTE tbTmp;
        tbTmp.w = (uint16_t)(nTmp & 0xFFFF);
        tbTmp.b = (uint8_t)((nTmp & 0xFF0000) >> 16);

        pSrc++;
        *pDest++ = tbTmp;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Float to 32BitInt24
// 24 bits are in MSB.
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertFloatTo32BitInt24(float *pSrc, int32_t *pDest, uint32_t dwLength)
{
    MTConvertFloatToInt32(pSrc, pDest, dwLength);
    //TODO: put 0 in bits [7..0]
}

///////////////////////////////////////////////////////////////////////////////
// Float to Int32
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertFloatToInt32(float *pSrc, int32_t *pDest, uint32_t dwLength)
{
    //NotOptimized_ConvertFloatToInt32(pSrc, pDest, dwLength);
    JmpTableFloatToInt32(pSrc, pDest, dwLength);
}

#ifndef _AMD64_
///////////////////////////////////////////////////////////////////////////////
void SSE2_ConvertFloatToInt32(float *pSrc, int32_t *pDest, uint32_t dwLength)
{
    uint32_t dwSamplesRemaining;
    uint32_t dwLoop;

    float f2147483648 = 2147483648.0f;

    __m128 m128_0dB = _mm_load1_ps(&f2147483648);

    //Source pointer has to be 16byte aligned
    //We first check if we are 4byte(float) aligned
    if(((INT_PTR)pSrc & 0x3))
    {
        //we are not able to get 16byte alignment
        NotOptimized_ConvertFloatToInt32(pSrc, pDest, dwLength);
        return;
    }

    if((DWORD_PTR)pSrc & 0xF) // not aligned on a multiple of 0xF (= 4 * float)
    {
        uint32_t dwSrcSamplesNotAligned = (16 - ((DWORD_PTR)pSrc & 0xF)) >> 2;
        dwSrcSamplesNotAligned = min(dwSrcSamplesNotAligned, dwLength);

        NotOptimized_ConvertFloatToInt32(pSrc, pDest, dwSrcSamplesNotAligned);
        dwLength -= dwSrcSamplesNotAligned;
        pDest = (int32_t*)((INT_PTR)pDest + dwSrcSamplesNotAligned*sizeof(*pDest));
        pSrc = (float*)((INT_PTR)pSrc + dwSrcSamplesNotAligned*sizeof(*pSrc));
    }
    if((INT_PTR)pDest & 0xF)
    {
        // Src pointer is aligned but destination is not, so we cannot use SSE instructions
        NotOptimized_ConvertFloatToInt32(pSrc, pDest, dwLength);
        return;
    }
    dwSamplesRemaining = dwLength & 0x3; // modulo 4
    dwLoop = dwLength / 4;

    if(dwLoop)
    {
        _asm
        {
            push        eax
            push        ecx
            push        esi
            push        edi

            mov         esi, pSrc
            mov         edi, pDest
            mov         ecx, dwLoop

            movaps      xmm1, m128_0dB

    ConvertFloatToInt32_Loop:
            movaps      xmm0, [esi]
            add         esi, 16
            prefetcht0  [esi]
            mulps       xmm0, xmm1
            cvtps2dq    xmm0, xmm0      ;=> X3X2X1X0

            movd        eax, xmm0
            mov         [edi], eax
            pshufd      xmm0, xmm0, 0E5h    ;=> X3X2X1X1

            movd        eax, xmm0
            mov         [edi+4], eax
            pshufd      xmm0, xmm0, 0E6h    ;=> X3X2X1X1

            movd        eax, xmm0
            mov         [edi+8], eax
            pshufd      xmm0, xmm0, 0E7h    ;=> X3X2X1X1

            movd        eax, xmm0
            mov         [edi+0Ch], eax

            add         edi, 16

            dec ecx
            jne         ConvertFloatToInt32_Loop

            pop         edi
            pop         esi
            pop         ecx
            pop         eax
        }
    }
    if (dwSamplesRemaining != 0)
    {
        pSrc += (dwLength-dwSamplesRemaining);
        pDest += (dwLength-dwSamplesRemaining);
        NotOptimized_ConvertFloatToInt32(pSrc, pDest, dwSamplesRemaining);
    }
    _mm_empty();
}
#endif

///////////////////////////////////////////////////////////////////////////////
void NotOptimized_ConvertFloatToInt32(float *pSrc, int32_t *pDest, uint32_t dwLength)
{
    volatile float f0dB = 2147483648.0f;
    volatile float fPositiveMax;
     *(unsigned int*)&fPositiveMax = 0x3F7FFFFF; // hex representation of 1.0f - powf(2.0f, -31.0f);
                                                 // to avoid unwanted float rounding to nearest which (1.0f in this case)
                                                 // and would cause overflow to MININT32 when casting to int32 for input values
                                                 // over 1.0f, hence big glitch instead of normal limiting

    for (ULONG i = 0; i < dwLength; i++)
    {
        float fTmp = min(fPositiveMax, max(*pSrc, -1.f));
        pSrc++;
        *pDest++ = (int32_t)(fTmp * f0dB);
    }
}


///////////////////////////////////////////////////////////////////////////////
// Float to int32_x; x bits align to right
///////////////////////////////////////////////////////////////////////////////
void MTConvertFloatToInt32_24r(float *pSrc, int32_t *pDest, uint32_t dwLength)
{
    float f0dB = (float)pow(2.0, 23.0);
    float fPositiveMax = 1.0f - (float)pow(2.0, -23.0);

    for (ULONG i = 0; i < dwLength; i++)
    {
        float fTmp = min(fPositiveMax, max(*pSrc, -1.0f));

        pSrc++;
        *pDest++ = (int32_t)(fTmp * f0dB);
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////// Int to Float ////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Int16 to Float
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertInt16ToFloat(int16_t * pSrc, float * pDest, uint32_t dwLength)
{
    JmpTableInt16ToFloat(pSrc, pDest, dwLength);
}

#ifndef _AMD64_
void SSE_ConvertInt16ToFloat(int16_t *pSrc, float *pDest, uint32_t dwLength)
{
    __m128* pDestFloat; // = (__m128*) pDest;
    __m64* pSrcInt16; // = (__m64*) pSrc;

    uint32_t dwSamplesRemaining;
    uint32_t dwLoop;
    uint32_t i;

    float f1 = 1.0f;
    float f32768 = 32768.0f;

    __m128 m128_0dB = _mm_div_ps(_mm_load1_ps(&f1),_mm_load1_ps(&f32768));

    //Source pointer has to be 16byte aligned
    //We first check if we are 2byte(int16_t) aligned
    if((INT_PTR)pSrc & 0x1)
    {
        // we are not able to align to 16byte
        NotOptimized_ConvertInt16ToFloat(pSrc, pDest, dwLength);
        return;
    }

    if((DWORD_PTR)pSrc & 0xF) // not aligned on a multiple of 0xF (= 4 * float)
    {
        uint32_t dwSrcSamplesNotAligned = (16 - ((DWORD_PTR)pSrc & 0xF)) >> 2;
        dwSrcSamplesNotAligned = min(dwSrcSamplesNotAligned, dwLength);

        NotOptimized_ConvertInt16ToFloat(pSrc, pDest, dwSrcSamplesNotAligned);
        dwLength -= dwSrcSamplesNotAligned;
        pDest = (float*)((INT_PTR)pDest + dwSrcSamplesNotAligned*sizeof(*pDest));
        pSrc = (int16_t*)((INT_PTR)pSrc + dwSrcSamplesNotAligned*sizeof(*pSrc));
    }
    if((INT_PTR)pDest & 0xF)
    {
        // Src pointer is aligned but destination is not, so we cannot use SSE instructions
        NotOptimized_ConvertInt16ToFloat(pSrc, pDest, dwLength);
        return;
    }

    dwSamplesRemaining = dwLength & 0x3; // modulo
    dwLoop = dwLength / 4;

    pDestFloat = (__m128*) pDest;
    pSrcInt16 = (__m64*) pSrc;
    for(i = 0; i < dwLoop; i++)
    {
        *pDestFloat = _mm_cvtpi16_ps(*pSrcInt16);
        *pDestFloat = _mm_mul_ps(*pDestFloat,m128_0dB);
        pSrcInt16++;
        pDestFloat++;
    }
    if (dwSamplesRemaining != 0)
    {
        pDest = (float*)pDestFloat;
        pSrc = (int16_t*)pSrcInt16;
        NotOptimized_ConvertInt16ToFloat(pSrc, pDest, dwSamplesRemaining);
    }
    _mm_empty();
}
#endif

void NotOptimized_ConvertInt16ToFloat(int16_t * pSrc, float * pDest, uint32_t dwLength)
{
    ULONG i;
    float fInv0dB = 1.f / 32768.0f;

    for (i = 0; i < dwLength; i++)
    {
        *(pDest++) = ((float)*(pSrc++)) * fInv0dB;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Int24 to float
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertInt24ToFloat(TRIBYTE * pSrc, float * pDest, uint32_t dwLength)
{
    JmpTableInt24ToFloat(pSrc, pDest, dwLength);
}

#ifndef _AMD64_
void SSE2_ConvertInt24ToFloat(TRIBYTE* pSrc, float * pDest, uint32_t dwLength)
{
    uint32_t dwSamplesRemaining;
    uint32_t dwLoop;

    float f1 = 1.0f;
    float f8388608 = 8388608.0f;

    __m128 m128_0dB = _mm_div_ps(_mm_load1_ps(&f1),_mm_load1_ps(&f8388608));


    //Source pointer has to be 16byte aligned
    //We first check if we are 4byte(float) aligned
    if((DWORD_PTR)pSrc & 0xF) // not aligned on a multiple of 0xF (= 4 * float)
    {
        uint32_t dwSrcSamplesNotAligned = (16 - ((DWORD_PTR)pSrc & 0xF)) >> 2;
        dwSrcSamplesNotAligned = min(dwSrcSamplesNotAligned, dwLength);

        NotOptimized_ConvertInt24ToFloat(pSrc, pDest, dwSrcSamplesNotAligned);
        dwLength -= dwSrcSamplesNotAligned;
        pDest = (float*)((INT_PTR)pDest + dwSrcSamplesNotAligned*sizeof(*pDest));
        pSrc = (TRIBYTE*)((INT_PTR)pSrc + dwSrcSamplesNotAligned*sizeof(*pSrc));
    }
    if((INT_PTR)pDest & 0xF)
    {
        // Src pointer is aligned but destination is not, so we cannot use SSE instructions
        NotOptimized_ConvertInt24ToFloat(pSrc, pDest, dwLength);
        return;
    }
    dwSamplesRemaining = dwLength & 0x3; // modulo 4
    dwLoop = dwLength / 4;

        _asm
        {
            push        eax
            push        edx
            push        ecx
            push        esi
            push        edi

            mov         ecx,dwLoop
            mov         esi, pSrc           ; source pointer
            mov         edi, pDest      ; destination pointer
            movaps      xmm1,xmmword ptr [m128_0dB]

ConvertInt24ToFloat_Loop:

            pinsrw      xmm0, word ptr [esi], 0
            movsx edx,byte ptr [esi + 2]
            pinsrw      xmm0, edx, 1

            pinsrw      xmm0, word ptr [esi + 3], 2
            movsx edx,byte ptr [esi + 5]
            pinsrw      xmm0, edx, 3

            pinsrw      xmm0, word ptr [esi + 6], 4
            movsx edx,byte ptr [esi + 8]
            pinsrw      xmm0, edx, 5

            pinsrw      xmm0, word ptr [esi + 9], 6
            movsx edx,byte ptr [esi + 0Bh]
            pinsrw      xmm0, edx, 7

            add         esi,12
            prefetcht0  [esi]

            cvtdq2ps    xmm0,xmm0
            mulps       xmm0,xmm1
            movaps      xmmword ptr [edi],xmm0

            add         edi,16

            dec ecx
            jne         ConvertInt24ToFloat_Loop

            pop         edi
            pop         esi
            pop         ecx
            pop         edx
            pop         eax
        }
    if (dwSamplesRemaining != 0)
    {
        pSrc += (dwLength-dwSamplesRemaining);
        pDest += (dwLength-dwSamplesRemaining);
        NotOptimized_ConvertInt24ToFloat(pSrc, pDest, dwSamplesRemaining);
    }
    _mm_empty();
}
#endif

void NotOptimized_ConvertInt24ToFloat(TRIBYTE* pSrc, float * pDest, uint32_t dwLength)
{
    ULONG i;
    float fInv0dB = 1.f / 8388608.0f;

    for (i = 0; i < dwLength; i++)
    {
        uint16_t nLow = (uint16_t)pSrc->w;
        int16_t nHigh = (int8_t)pSrc->b; // has to be int8_t, to be signed (uint8_t is unsigned)
        int32_t nData = nLow | ((nHigh) << 16);
        pSrc++;

        *(pDest++) = ((float)nData) * fInv0dB;
    }
}

///////////////////////////////////////////////////////////////////////////////
// 32BitInt24 to Float
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvert32BitInt24ToFloat(int32_t *pSrc, float *pDest, uint32_t dwLength)
{
    MTConvertInt32ToFloat(pSrc, pDest, dwLength);
}

///////////////////////////////////////////////////////////////////////////////
// Int32 to float
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MTConvertInt32ToFloat(int32_t * pSrc, float * pDest, uint32_t dwLength)
{
    JmpTableInt32ToFloat(pSrc, pDest, dwLength);
}

#ifndef _AMD64_
///////////////////////////////////////////////////////////////////////////////
void SSE2_ConvertInt32ToFloat(int32_t* pSrc, float * pDest, uint32_t dwLength)
{
    uint32_t dwSamplesRemaining;
    uint32_t dwLoop;

    float f1 = 1.0f;
    float f2147483648 = 2147483648.0f;

    __m128 m128_0dB = _mm_div_ps(_mm_load1_ps(&f1),_mm_load1_ps(&f2147483648));


    //Source pointer has to be 16byte aligned
    //We first check if we are 4byte(int32_t) aligned
    if(((INT_PTR)pSrc & 0x3))
    {
        //we are not able to get 16byte alignment
        NotOptimized_ConvertInt32ToFloat(pSrc, pDest, dwLength);
        return;
    }

    if((DWORD_PTR)pSrc & 0xF) // not aligned on a multiple of 0xF (= 4 * float)
    {
        uint32_t dwSrcSamplesNotAligned = (16 - ((DWORD_PTR)pSrc & 0xF)) >> 2;
        dwSrcSamplesNotAligned = min(dwSrcSamplesNotAligned, dwLength);

        NotOptimized_ConvertInt32ToFloat(pSrc, pDest, dwSrcSamplesNotAligned);
        dwLength -= dwSrcSamplesNotAligned;
        pDest = (float*)((INT_PTR)pDest + dwSrcSamplesNotAligned*sizeof(*pDest));
        pSrc = (int32_t*)((INT_PTR)pSrc + dwSrcSamplesNotAligned*sizeof(*pSrc));
    }
    if((INT_PTR)pDest & 0xF)
    {
        // Src pointer is aligned but destination is not, so we cannot use SSE instructions
        NotOptimized_ConvertInt32ToFloat(pSrc, pDest, dwLength);
        return;
    }

    dwSamplesRemaining = dwLength & 0x3; // modulo 4
    dwLoop = dwLength / 4;

        _asm
        {
            push        eax
            push        ecx
            push        esi
            push        edi

            mov         ecx,dwLoop
            mov         esi, pSrc           ; source pointer
            mov         edi, pDest      ; destination pointer
            movaps      xmm1,xmmword ptr [m128_0dB]

ConvertInt32ToFloat_Loop:

            pinsrw      xmm0, word ptr [esi], 0
            pinsrw      xmm0, word ptr [esi + 2], 1

            pinsrw      xmm0, word ptr [esi + 4], 2
            pinsrw      xmm0, word ptr [esi + 6], 3

            pinsrw      xmm0, word ptr [esi + 8], 4
            pinsrw      xmm0, word ptr [esi + 0Ah], 5

            pinsrw      xmm0, word ptr [esi + 0Ch], 6
            pinsrw      xmm0, word ptr [esi + 0Eh], 7

            add         esi,16
            prefetcht0  [esi]

            cvtdq2ps    xmm0,xmm0
            mulps       xmm0,xmm1
            movaps      xmmword ptr [edi],xmm0

            add         edi,16

            dec ecx
            jne         ConvertInt32ToFloat_Loop

            pop         edi
            pop         esi
            pop         ecx
            pop         eax
        }
    if (dwSamplesRemaining != 0)
    {
        pSrc += (dwLength-dwSamplesRemaining);
        pDest += (dwLength-dwSamplesRemaining);
        NotOptimized_ConvertInt32ToFloat(pSrc, pDest, dwSamplesRemaining);
    }
    _mm_empty();
}
#endif

///////////////////////////////////////////////////////////////////////////////
void NotOptimized_ConvertInt32ToFloat(int32_t * pSrc, float * pDest, uint32_t dwLength)
{
    ULONG i;
    float fInv0dB = (float)(1. / pow(2.0, 31.0));

    for (i = 0; i < dwLength; i++)
    {
        *(pDest++) = ((float)*(pSrc++)) * fInv0dB;
    }
}

///////////////////////////////////////////////////////////////////////////////
void MTConvertInt32_24rToFloat(int32_t *pSrc, float *pDest, uint32_t dwLength)
{
    ULONG i;
    float fInv0dB = (float)(1. / pow(2.0, 31.0));

    for (i = 0; i < dwLength; i++)
    {
        *(pDest++) = ((float)(*(pSrc++) << 8)) * fInv0dB;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*///////////////////////////////////////////////////////////////////////////////
void MTConvertInterleave(float* pfUninterleave, float* pfInterleave, uint32_t dwNbOfChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    //ASSERT(pfUninterleave);
    //ASSERT(pfInterleave);

    for(dwSample = 0; dwSample < dwFrameSize; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            *pfInterleave++ = pfUninterleave[dwSample + dwChannel * dwFrameSize];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void MTConvertDeInterleave(float* pfInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    //ASSERT(pfUninterleave);
    //ASSERT(pfInterleave);

    for(dwSample = 0; dwSample < dwFrameSize; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            pfUninterleave[dwSample + dwChannel * dwFrameSize] = *pfInterleave++;
        }
    }
}*/

///////////////////////////////////////////////////////////////////////////////
void MTConvertInterleave(float* pfUninterleave, float* pfInterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    //ASSERT(pfUninterleave);
    //ASSERT(pfInterleave);

    for(dwSample = 0; dwSample < dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            *pfInterleave++ = pfUninterleave[dwSample + dwChannel * dwFrameSize];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void MTConvertDeInterleave(float* pfInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    //ASSERT(pfUninterleave);
    //ASSERT(pfInterleave);

    for(dwSample = 0; dwSample < dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            pfUninterleave[dwSample + dwChannel * dwFrameSize] = *pfInterleave++;
        }
    }
}
#endif //WIN32 && !NT_DRIVER

#if !defined(NT_DRIVER) && !defined(__KERNEL__)
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Contiguous Float block buffers
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Convert from contiguous un-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertFloatToBigEndianInt16Interleave(float* pfUninterleave, int16_t* pi16BigEndianInterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    static const float f0dB = 32768.0f;
    static const float fPositiveMax = 32768.0f - 1.0f;

    for(dwSample = 0; dwSample < dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            int16_t i16Sample = (int16_t)min(fPositiveMax, max(pfUninterleave[dwSample + dwChannel * dwFrameSize] * f0dB, -f0dB));

            *pi16BigEndianInterleave = SWAP16(i16Sample);
            pi16BigEndianInterleave++;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to contiguous un-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt16ToFloatDeInterleave(int16_t* pi16BigEndianInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    //ASSERT(pi16BigEndianInterleave);
    //ASSERT(pfUninterleave);

    static const float fInv0dB = 1.f / 32768.0f;

    for(dwSample = 0; dwSample < dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            pfUninterleave[dwSample + dwChannel * dwFrameSize] = ((float)(int16_t)SWAP16(*pi16BigEndianInterleave)) * fInv0dB;  // !!!SWAP16 is a MACRO so we mustn't increament pointer here
            pi16BigEndianInterleave++;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from contiguous un-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertFloatToBigEndianInt24Interleave(float* pfUninterleave, uint8_t* pbyBigEndianInterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    static const float f0dB = 8388608.0f;
    static const float fPositiveMax = 8388608.0f - 1.0f;

    for(dwSample = 0; dwSample < dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            int32_t nTmp = (int32_t)min(fPositiveMax, max(pfUninterleave[dwSample + dwChannel * dwFrameSize] * f0dB, -f0dB));

            // put it in big endian order
            pbyBigEndianInterleave[0] = (nTmp >> 16) & 0xFF;
            pbyBigEndianInterleave[1] = (nTmp >> 8) & 0xFF;
            pbyBigEndianInterleave[2] = nTmp & 0xFF;
            pbyBigEndianInterleave += 3;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to contiguous un-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt24ToFloatDeInterleave(uint8_t* pbyBigEndianInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize)
{
    uint32_t dwSample, dwChannel;
    static const float fInv0dB = 1.f / 8388608.0f;

    for(dwSample = 0; dwSample < dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            int16_t nHigh = (int8_t)pbyBigEndianInterleave[0]; // has to be int8_t, to be signed (uint8_t is unsigned)
            int32_t nData = (nHigh << 16) | pbyBigEndianInterleave[1] << 8 | pbyBigEndianInterleave[2];
            pbyBigEndianInterleave += 3;


            pfUninterleave[dwSample + dwChannel * dwFrameSize] = ((float)nData) * fInv0dB;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Mapped Float block buffers
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Float
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedFloatToBigEndianInt16Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    int16_t* pi16BigEndianInterleave = (int16_t*)pvBigEndianInterleave;
    static const float f0dB = 32768.f;
    static const float fPositiveMax = f0dB - 1.f;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                int16_t i16Sample = (int16_t)min(fPositiveMax, max(((float**)ppfUninterleave)[dwChannel][dwSample] * f0dB, -f0dB));
                *pi16BigEndianInterleave = SWAP16(i16Sample);   // !!!SWAP16 is a MACRO so we mustn't do any operation
                pi16BigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt16ToMappedFloatDeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    int16_t* pi16BigEndianInterleave = (int16_t*)pvBigEndianInterleave;
    static const float fInv0dB = 1.f / 32768.0f;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((float**)ppfUninterleave)[dwChannel][dwSample] = ((float)(int16_t)SWAP16(*pi16BigEndianInterleave)) * fInv0dB; // !!!SWAP16 is a MACRO so we mustn't increament pointer here
                pi16BigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedFloatToBigEndianInt24Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint8_t* pbyBigEndianInterleave = (uint8_t*)pvBigEndianInterleave;
    static const float f0dB = 8388608.0f;
    static const float fPositiveMax = 8388608.0f - 1.0f;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                int32_t nTmp = (int32_t)min(fPositiveMax, max(((float**)ppfUninterleave)[dwChannel][dwSample] * f0dB, -f0dB));

                // put it in big endian order
                pbyBigEndianInterleave[0] = (nTmp >> 16) & 0xFF;
                pbyBigEndianInterleave[1] = (nTmp >> 8) & 0xFF;
                pbyBigEndianInterleave[2] = nTmp & 0xFF;
                pbyBigEndianInterleave += 3;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt24ToMappedFloatDeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint8_t* pbyBigEndianInterleave = (uint8_t*)pvBigEndianInterleave;
    static const float fInv0dB = 1.f / 8388608.0f;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                int16_t nHigh = (int8_t)pbyBigEndianInterleave[0]; // has to be int8_t, to be signed (uint8_t is unsigned)
                int32_t nData = (nHigh << 16) | pbyBigEndianInterleave[1] << 8 | pbyBigEndianInterleave[2];
                pbyBigEndianInterleave += 3;

                ((float**)ppfUninterleave)[dwChannel][dwSample] = ((float)nData) * fInv0dB;
            }
        }
    }
}

#endif //!defined(NT_DRIVER)






int MTConvertMappedInt32ToInt16LEInterleave(void** input_buffer, const uint32_t offset_input_buf, 
    void* output_buffer, const uint32_t nb_channels, const uint32_t nb_samples_in, const bool from_kernel_to_user)
{
    int ret = 0;
    int ret_pu;
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 4;
    unsigned int in_pos = offset_input_buf * stride_in;

    if(Arch_is_big_endian())
    {
        if(from_kernel_to_user)
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[0], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[1];
                        out[1] = in[0];
                        out += 2;
                    #endif
                }
                in_pos += stride_in;
            }
        }
        else
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    out[0] = in[1];
                    out[1] = in[0];
                    out += 2;
                }
                in_pos += stride_in;
            }
        }
    }
    else
    {
        if(from_kernel_to_user)
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[3], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[2];
                        out[1] = in[3];
                        out += 2;
                    #endif
                }
                in_pos += stride_in;
            }
        }
        else
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    out[0] = in[2];
                    out[1] = in[3];
                    out += 2;
                }
                in_pos += stride_in;
            }
        }
    }
    return ret;
}

int MTConvertMappedInt32ToInt24LEInterleave(void** input_buffer, const uint32_t offset_input_buf,
    void* output_buffer, const uint32_t nb_channels, const uint32_t nb_samples_in, const bool from_kernel_to_user)
{
    int ret = 0;
    int ret_pu;
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 4;
    unsigned int in_pos = offset_input_buf * stride_in;

    if(Arch_is_big_endian())
    {
        if(from_kernel_to_user)
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[0], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[2];
                        out[1] = in[1];
                        out[2] = in[0];
                        out += 3;
                    #endif
                }
                in_pos += stride_in;
            }
        }
        else
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    out[0] = in[2];
                    out[1] = in[1];
                    out[2] = in[0];
                    out += 3;
                }
                in_pos += stride_in;
            }

        }
    }
    else
    {
        if(from_kernel_to_user)
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[3], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[1];
                        out[1] = in[2];
                        out[2] = in[3];
                        out += 3;
                    #endif
                }
                in_pos += stride_in;
            }
        }
        else
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    out[0] = in[1];
                    out[1] = in[2];
                    out[2] = in[3];
                    out += 3;
                }
                in_pos += stride_in;
            }
        }
    }
    return ret;
}

int MTConvertMappedInt32ToInt24LE4ByteInterleave(void** input_buffer, const uint32_t offset_input_buf,
    void* output_buffer, const uint32_t nb_channels, const uint32_t nb_samples_in, const bool from_kernel_to_user)
{
    int ret = 0;
    int ret_pu;
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 4;
    unsigned int in_pos = offset_input_buf * stride_in;

    if(Arch_is_big_endian())
    {
        if(from_kernel_to_user)
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        char zero = 0x00;
                        __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[0], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, zero, (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[2];
                        out[1] = in[1];
                        out[2] = in[0];
                        out[3] = 0x00;
                        out += 4;
                    #endif
                }
                in_pos += stride_in;
            }
        }
        else
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    out[0] = in[2];
                    out[1] = in[1];
                    out[2] = in[0];
                    out[3] = 0x00;
                    out += 4;
                }
                in_pos += stride_in;
            }

        }
    }
    else
    {
        if(from_kernel_to_user)
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        char zero = 0x00;
                        __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[3], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, zero, (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[1];
                        out[1] = in[2];
                        out[2] = in[3];
                        out[3] = 0x00;
                        out += 4;
                    #endif
                }
                in_pos += stride_in;
            }
        }
        else
        {
            for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
            {
                for(ch = 0; ch < nb_channels; ++ch)
                {
                    const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        char zero = 0x00;
                        __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, in[3], (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                        __put_user_x(1, zero, (unsigned long __user *)out, ret_pu);
                        ret |= ret_pu;
                        out++;
                    #else
                        out[0] = in[1];
                        out[1] = in[2];
                        out[2] = in[3];
                        out[3] = 0x00;
                        out += 4;
                    #endif
                }
                in_pos += stride_in;
            }
        }
    }
    return ret;
}

int MTConvertMappedInt32ToInt32LEInterleave(void** input_buffer, const uint32_t offset_input_buf,
    void* output_buffer, const uint32_t nb_channels, const uint32_t nb_samples_in, const bool from_kernel_to_user)
{
    int ret = 0;
    int ret_pu;
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 4;
    unsigned int in_pos = offset_input_buf * stride_in;
    
    {
        if(Arch_is_big_endian())
        {
            if(from_kernel_to_user)
            {
                for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
                {
                    for(ch = 0; ch < nb_channels; ++ch)
                    {
                        const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                            __put_user_x(1, in[3], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                            __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                            __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                            __put_user_x(1, in[0], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                        #else
                            out[0] = in[3];
                            out[1] = in[2];
                            out[2] = in[1];
                            out[3] = in[0];
                            out += 4;
                        #endif
                    }
                    in_pos += stride_in;
                }
            }
            else
            {
                for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
                {
                    for(ch = 0; ch < nb_channels; ++ch)
                    {
                        const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                        out[0] = in[3];
                        out[1] = in[2];
                        out[2] = in[1];
                        out[3] = in[0];
                        out += 4;
                    }
                    in_pos += stride_in;
                }
            }

        }
        else
        {
            if(from_kernel_to_user)
            {
                for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
                {
                    for(ch = 0; ch < nb_channels; ++ch)
                    {
                        const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                            __put_user_x(1, in[0], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                            __put_user_x(1, in[1], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                            __put_user_x(1, in[2], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                            __put_user_x(1, in[3], (unsigned long __user *)out, ret_pu);
                            ret |= ret_pu;
                            out++;
                        #else
                            out[0] = in[0];
                            out[1] = in[1];
                            out[2] = in[2];
                            out[3] = in[3];
                            out += 4;
                        #endif
                    }
                    in_pos += stride_in;
                }
            }
            else
            {
                for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
                {
                    for(ch = 0; ch < nb_channels; ++ch)
                    {
                        const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                        out[0] = in[0];
                        out[1] = in[1];
                        out[2] = in[2];
                        out[3] = in[3];
                        out += 4;
                    }
                    in_pos += stride_in;
                }
            }
        }
    }
    return ret;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// INT24
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(nb_samples_in-1)][B0..B(nb_samples_in-1)] - > [A0.B0.A1.B1...A(nb_samples_in-1).B(nb_samples_in-1)]
void MTConvertMappedInt24ToBigEndianInt16Interleave(void** input_buffer,
                                                    const uint32_t offset_input_buf,
                                                    void* output_buffer,
                                                    const uint32_t nb_channels,
                                                    const uint32_t nb_samples_in)
{
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 3;
    unsigned int in_pos = offset_input_buf * stride_in;
    for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
    {
        if(Arch_is_big_endian())
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[0];
                out[1] = in[1];
                out += 2;
            }
        }
        else
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[2];
                out[1] = in[1];
                out += 2;
            }
        }
        in_pos += stride_in;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt16ToMappedInt24DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint8_t* pbyBigEndianInterleave = (uint8_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((TRIBYTE**)ppfUninterleave)[dwChannel][dwSample].w = pbyBigEndianInterleave[1] << 8;
                ((TRIBYTE**)ppfUninterleave)[dwChannel][dwSample].b = pbyBigEndianInterleave[0];

                pbyBigEndianInterleave += 2;
            }
        }
    }
}






void MTConvertMappedInt32ToBigEndianInt16Interleave(void** input_buffer,
                                                    const uint32_t offset_input_buf,
                                                    void* output_buffer,
                                                    const uint32_t nb_channels,
                                                    const uint32_t nb_samples_in)
{
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 4;
    unsigned int in_pos = offset_input_buf * stride_in;
    for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
    {
        if(Arch_is_big_endian())
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[0];
                out[1] = in[1];
                out += 2;
            }
        }
        else
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[3];
                out[1] = in[2];
                out += 2;
            }
        }
        in_pos += stride_in;
    }
}


///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt16ToMappedInt32DeInterleave(  void* input_buffer,
                                                        void** output_buffer,
                                                        uint32_t offset_output_buf,
                                                        uint32_t nb_channels,
                                                        uint32_t nb_samples)
{
    uint32_t i, ch;
    const unsigned int stride_in = 2 * nb_channels, stride_out = 4;
    const unsigned int out_pos = offset_output_buf * stride_out;
    for(ch = 0; ch < nb_channels; ++ch)
    {
        uint8_t* in = (uint8_t*)input_buffer + 2 * ch;
        uint8_t* out = (uint8_t*)output_buffer[ch] + out_pos;

        if(Arch_is_big_endian())
        {
            for(i = 0; i < nb_samples; ++i)
            {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = 0;
                out[3] = 0;

                in += stride_in;
                out += stride_out;
            }
        }
        else
        {
            for(i = 0; i < nb_samples; ++i)
            {
                out[0] = 0;
                out[1] = 0;
                out[2] = in[1];
                out[3] = in[0];

                in += stride_in;
                out += stride_out;
            }
        }
    }
}




void MTConvertMappedInt24ToBigEndianInt24Interleave(void** input_buffer,
                                                    const uint32_t offset_input_buf,
                                                    void* output_buffer,
                                                    const uint32_t nb_channels,
                                                    const uint32_t nb_samples_in)
{
    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 3;
    unsigned int in_pos = offset_input_buf * stride_in;
    for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
    {
        if(Arch_is_big_endian())
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
                out += 3;
            }
        }
        else
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[2];
                out[1] = in[1];
                out[2] = in[0];
                out += 3;
            }
        }
        in_pos += stride_in;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt24ToMappedInt24DeInterleave(  void* input_buffer,
                                                        void** output_buffer,
                                                        uint32_t offset_output_buf,
                                                        uint32_t nb_channels,
                                                        uint32_t nb_samples)
{
    uint32_t i, ch;
    const unsigned int stride_in = 3 * nb_channels, stride_out = 3;
    const unsigned int out_pos = offset_output_buf * stride_out;
    for(ch = 0; ch < nb_channels; ++ch)
    {
        uint8_t* in = (uint8_t*)input_buffer + 3 * ch;
        uint8_t* out = (uint8_t*)output_buffer[ch] + out_pos;

        if(Arch_is_big_endian())
        {
            for(i = 0; i < nb_samples; ++i)
            {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];

                in += stride_in;
                out += stride_out;
            }
        }
        else
        {
            for(i = 0; i < nb_samples; ++i)
            {
                out[0] = in[2];
                out[1] = in[1];
                out[2] = in[0];

                in += stride_in;
                out += stride_out;
            }
        }
    }
}



void MTConvertMappedInt32ToBigEndianInt24Interleave(void** input_buffer,
                                                    const uint32_t offset_input_buf,
                                                    void* output_buffer,
                                                    const uint32_t nb_channels,
                                                    const uint32_t nb_samples_in)
{

    uint32_t i, ch;
    uint8_t* out = (uint8_t*)output_buffer;
    const unsigned int stride_in = 4;
    unsigned int in_pos = offset_input_buf * stride_in;
    for(i = offset_input_buf; i < offset_input_buf + nb_samples_in; ++i)
    {
        if(Arch_is_big_endian())
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
                out += 3;
            }
        }
        else
        {
            for(ch = 0; ch < nb_channels; ++ch)
            {
                const uint8_t* in = (uint8_t*)input_buffer[ch] + in_pos;
                out[0] = in[3];
                out[1] = in[2];
                out[2] = in[1];
                out += 3;
            }
        }
        in_pos += stride_in;
    }
}



///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianInt24ToMappedInt32DeInterleave(  void* input_buffer,
                                                        void** output_buffer,
                                                        uint32_t offset_output_buf,
                                                        uint32_t nb_channels,
                                                        uint32_t nb_samples)
{
    uint32_t i, ch;
    const unsigned int stride_in = 3 * nb_channels, stride_out = 4;
    const unsigned int out_pos = offset_output_buf * stride_out;
    for(ch = 0; ch < nb_channels; ++ch)
    {
        uint8_t* in = (uint8_t*)input_buffer + 3 * ch;
        uint8_t* out = (uint8_t*)output_buffer[ch] + out_pos;

        if(Arch_is_big_endian())
        {
            for(i = 0; i < nb_samples; ++i)
            {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
                out[3] = 0;

                in += stride_in;
                out += stride_out;
            }
        }
        else
        {
            for(i = 0; i < nb_samples; ++i)
            {
                out[0] = 0;
                out[1] = in[2];
                out[2] = in[1];
                out[3] = in[0];

                in += stride_in;
                out += stride_out;
            }
        }
    }
}


/*void MTConvertBigEndianInt24ToMappedInt32DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint8_t* pbyBigEndianInterleave = (uint8_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((int32_t**)ppfUninterleave)[dwChannel][dwSample] = pbyBigEndianInterleave[2] << 24;
                ((int32_t**)ppfUninterleave)[dwChannel][dwSample] |= pbyBigEndianInterleave[1] << 16;
                ((int32_t**)ppfUninterleave)[dwChannel][dwSample] |= pbyBigEndianInterleave[0] << 8;
                pbyBigEndianInterleave += 3;
            }
        }
    }
}*/



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Int16
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Float -> DSD and DSD -> float
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedFloatToBigEndianDSD64Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint8_t* pbyBigEndianInterleave = (uint8_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                *pbyBigEndianInterleave = ((uint8_t*)&((float**)ppfUninterleave)[dwChannel][dwSample])[0];
                pbyBigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianDSD64ToMappedFloatDeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint8_t* pi8BigEndianInterleave = (uint8_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((uint32_t**)ppfUninterleave)[dwChannel][dwSample] = *pi8BigEndianInterleave;
                pi8BigEndianInterleave++;
            }
        }
    }
}





///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedFloatToBigEndianDSD128Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint16_t* pui16BigEndianInterleave = (uint16_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                *pui16BigEndianInterleave = ((uint16_t*)&((float**)ppfUninterleave)[dwChannel][dwSample])[0];
                pui16BigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianDSD128ToMappedFloatDeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint16_t* pui16BigEndianInterleave = (uint16_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((uint32_t**)ppfUninterleave)[dwChannel][dwSample] = *pui16BigEndianInterleave;
                pui16BigEndianInterleave++;
            }
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedFloatToBigEndianDSD256Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint32_t* pui32BigEndianInterleave = (uint32_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                *pui32BigEndianInterleave = ((uint32_t**)ppfUninterleave)[dwChannel][dwSample];
                pui32BigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianDSD256ToMappedFloatDeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint32_t* pui32BigEndianInterleave = (uint32_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((uint32_t**)ppfUninterleave)[dwChannel][dwSample] = *pui32BigEndianInterleave;
                pui32BigEndianInterleave++;
            }
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedInt8ToBigEndianDSD64_32Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint32_t* pui32BigEndianInterleave = (uint32_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                //((uint8_t*)pui32BigEndianInterleave)[0] = ((uint8_t**)ppfUninterleave)[dwChannel][dwSample]; // fill only the byte[0]; three remaining bytes are untouch
                *pui32BigEndianInterleave = ((uint8_t**)ppfUninterleave)[dwChannel][dwSample]; // DSD byte -> byte[0] and three remaing bytes to zero
                pui32BigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianDSD64_32ToMappedInt8DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint32_t* pui32BigEndianInterleave = (uint32_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((uint8_t**)ppfUninterleave)[dwChannel][dwSample] = ((uint8_t*)pui32BigEndianInterleave)[0];
                pui32BigEndianInterleave++;
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
// Convert from N non-interleave buffers to an interleave buffer
// i.e. [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)] - > [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)]
void MTConvertMappedInt16ToBigEndianDSD128_32Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint32_t* pui32BigEndianInterleave = (uint32_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                //((uint16_t*)pui32BigEndianInterleave)[0] = ((uint16_t**)ppfUninterleave)[dwChannel][dwSample]; // fill only byte[1-0]; two remaining bytes are untouch
                *pui32BigEndianInterleave = ((uint16_t**)ppfUninterleave)[dwChannel][dwSample]; // DSD bytse -> byte[1-0] and two remaing bytes to zero
                pui32BigEndianInterleave++;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertBigEndianDSD128_32ToMappedInt16DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels)
{
    uint32_t dwSample, dwChannel;
    uint32_t* pui32BigEndianInterleave = (uint32_t*)pvBigEndianInterleave;

    for(dwSample = dwOffsetInUninterleaveBuffer; dwSample < dwOffsetInUninterleaveBuffer + dwNbOfSamplesByChannels; dwSample++)
    {
        for(dwChannel = 0; dwChannel < dwNbOfChannels; dwChannel++)
        {
            if(ppfUninterleave[dwChannel])
            {
                ((uint16_t**)ppfUninterleave)[dwChannel][dwSample] = ((uint16_t*)pui32BigEndianInterleave)[0];
                pui32BigEndianInterleave++;
            }
        }
    }
}







#if !defined(NT_DRIVER) && !defined(__KERNEL__)
#include "string.h"
//////////////////////////////////////////////////////////////////////////////
void MTMute16(uint8_t ui8MuteSample, uint16_t* pui16Destination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t uiChannelIdx;
    for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
    {
        uint16_t* pui16DestTmp = &pui16Destination[uiChannelIdx * ui32DestinationFrameSize];
        memset(pui16DestTmp, ui8MuteSample, sizeof(uint16_t) * ui32NbOfSamplesToProcess);
    }
}

//////////////////////////////////////////////////////////////////////////////
void MTMute32(uint8_t ui8MuteSample, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t uiChannelIdx;
    for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
    {
        float* pfDestTmp = &pfDestination[uiChannelIdx * ui32DestinationFrameSize];
        memset(pfDestTmp, ui8MuteSample, sizeof(float) * ui32NbOfSamplesToProcess);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// CoreAudio DoP
//////////////////////////////////////////////////////////////////////////////
void MTConvertDoPDSD256_705_6ToDSD256FloatDeInterleave(float const * pfSource, uint16_t* pui16Destination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    uint32_t ui32SourceIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
        {
            uint32_t ui32DoPSample = (uint32_t)(pfSource[ui32SourceIdx] * 0x800000);
            uint16_t ui16MergingDSDSample = 0x5555; // mute

            uint8_t ui8DoPMarker = (uint8_t)(ui32DoPSample >> 16);
            if(ui8DoPMarker == 0xFA || ui8DoPMarker == 0x5) // check markers
            {
                ui16MergingDSDSample = (uint16_t)((ui32DoPSample & 0xFF) << 8 ) | ((ui32DoPSample >> 8) & 0xFF); // reverse bytes
            }

            pui16Destination[uiChannelIdx * ui32DestinationFrameSize + ui32SampleIdx] = ui16MergingDSDSample;

            ui32SourceIdx++; // same sample for next channel
        }
    }
}

// NOTE: this method has never been tested !!!!!!!!!!!!
void MTConvertDSD256FloatToDoPDSD256_705_6Interleaved(uint8_t * pui8DoPMarker, uint16_t const * pui16Source, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    static const float fInv0dB = 1.f / 8388608.0f;
    uint32_t ui32DestinationIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
        {
            uint16_t ui16MergingDSDSample = pui16Source[uiChannelIdx * ui32SourceFrameSize + ui32SampleIdx];

            // Note: (int32_t)(int8_t)*pui8DoPMarker is used to extended the sign of the DoP Marker
            int32_t nData = ((int32_t)(int8_t)*pui8DoPMarker << 16) | (int32_t)((ui16MergingDSDSample & 0xFF) << 8) | (int32_t)((ui16MergingDSDSample >> 8) & 0xFF); // reverse bytes

            pfDestination[ui32DestinationIdx] = ((float)nData) * fInv0dB;

            ui32DestinationIdx++; // same sample for next channel
        }
        ui32DestinationIdx += ui32NbOfChannels; // jump to sample n + 2

        *pui8DoPMarker = *pui8DoPMarker == 0x5 ? 0xFA : 0x5;
    }
}



//////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// we need two channels to make one
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0B0..A(dwNbOfSamplesByChannels-1)B0..B(dwNbOfSamplesByChannels-1)]]
// Note: only ui32NbOfChannels/2 channels will be fill
// Note: ui32NbOfChannels must be even
void MTConvertDoPDSD256_352_8ToDSD256FloatDeInterleave(float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    uint32_t* pui32Destination = (uint32_t*)pfDestination;
    for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels / 2; uiChannelIdx++)
    {
        for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
        {
            uint32_t ui32SourceIdx = ui32SampleIdx * ui32NbOfChannels + (uiChannelIdx * 2);
            uint32_t ui32DoPSampleLow = (uint32_t)(pfSource[ui32SourceIdx] * 0x800000);
            uint32_t ui32DoPSampleHigh = (uint32_t)(pfSource[ui32SourceIdx + 1] * 0x800000);

            uint32_t ui32MergingDSDSampleLow = 0x5555; // mute
            uint32_t ui32MergingDSDSampleHigh = 0x5555; // mute

            uint8_t ui8DoPMarkerLow = (uint8_t)(ui32DoPSampleLow >> 16);
            if(ui8DoPMarkerLow == 0xF9 || ui8DoPMarkerLow == 0x6)
            {
                ui32MergingDSDSampleLow = ((ui32DoPSampleLow & 0xFF) << 8 ) | ((ui32DoPSampleLow >> 8) & 0xFF); // reverse bytes
            }
            uint8_t ui8DoPMarkerHigh = (uint8_t)(ui32DoPSampleHigh >> 16);
            if(ui8DoPMarkerHigh == 0xF9 || ui8DoPMarkerHigh == 0x6)
            {
                ui32MergingDSDSampleHigh = ((ui32DoPSampleHigh & 0xFF) << 8 ) | ((ui32DoPSampleHigh >> 8) & 0xFF); // reverse bytes
            }

            uint32_t ui32MergingDSDSample = (ui32MergingDSDSampleHigh << 16) | ui32MergingDSDSampleLow;
            pui32Destination[uiChannelIdx * ui32DestinationFrameSize + ui32SampleIdx] = ui32MergingDSDSample;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
void MTConvertDSD256FloatToDoPDSD256_352_8Interleaved(uint8_t * pui8DoPMarker, float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    static const float fInv0dB = 1.f / 8388608.0f;
    uint32_t* pui32Source = (uint32_t*)pfSource;
    uint32_t ui32DestinationIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels / 2; uiChannelIdx++)
        {
            uint32_t ui32MergingDSDSample = pui32Source[uiChannelIdx * ui32SourceFrameSize + ui32SampleIdx];

            // Low
                    // Note: (int32_t)(int8_t)*pui8DoPMarker is used to extended the sign of the DoP Marker
            int32_t nData = ((int32_t)(int8_t)*pui8DoPMarker << 16) | (int32_t)((ui32MergingDSDSample & 0xFF) << 8) | (int32_t)((ui32MergingDSDSample >> 8) & 0xFF); // reverse bytes

            pfDestination[ui32DestinationIdx++] = ((float)nData) * fInv0dB;

            // High
            // Note: (int32_t)(int8_t)*pui8DoPMarker is used to extended the sign of the DoP Marker
            nData = ((int32_t)(int8_t)*pui8DoPMarker << 16) | (int32_t)((ui32MergingDSDSample >> 16) << 8) | (int32_t)((ui32MergingDSDSample >> 24) & 0xFF); // reverse bytes

            pfDestination[ui32DestinationIdx++] = ((float)nData) * fInv0dB;
        }
        *pui8DoPMarker = *pui8DoPMarker == 0x6 ? 0xF9 : 0x6;
    }
}



//////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
// i.e. [A0.B0.A1.B1...A(dwNbOfSamplesByChannels-1).B(dwNbOfSamplesByChannels-1)] -> [A0..A(dwNbOfSamplesByChannels-1)][B0..B(dwNbOfSamplesByChannels-1)]
void MTConvertDoPDSD128_352_8ToDSD128FloatDeInterleave(float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    uint32_t* pui32Destination = (uint32_t*)pfDestination;
    uint32_t ui32SourceIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
        {
            uint32_t ui32DoPSample = (uint32_t)(pfSource[ui32SourceIdx] * 0x800000);
            uint32_t ui32MergingDSDSample = 0x5555; // mute

            uint8_t ui8DoPMarker = (uint8_t)(ui32DoPSample >> 16);
            if(ui8DoPMarker == 0xFA || ui8DoPMarker == 0x5)
            {
                ui32MergingDSDSample = ((ui32DoPSample & 0xFF) << 8 ) | ((ui32DoPSample >> 8) & 0xFF); // reverse bytes
            }

            pui32Destination[uiChannelIdx * ui32DestinationFrameSize + ui32SampleIdx] = ui32MergingDSDSample;

            ui32SourceIdx++;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
void MTConvertDSD128FloatToDoPDSD128_352_8Interleaved(uint8_t * pui8DoPMarker, float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    static const float fInv0dB = 1.f / 8388608.0f;
    uint32_t* pui32Source = (uint32_t*)pfSource;
    uint32_t ui32DestinationIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
        {
            uint32_t ui32MergingDSDSample = pui32Source[uiChannelIdx * ui32SourceFrameSize + ui32SampleIdx];

            // Note: (int32_t)(int8_t)*pui8DoPMarker is used to extended the sign of the DoP Marker
            int32_t nData = ((int32_t)(int8_t)*pui8DoPMarker << 16) | (int32_t)((ui32MergingDSDSample & 0xFF) << 8) | (int32_t)((ui32MergingDSDSample >> 8) & 0xFF); // reverse bytes

            pfDestination[ui32DestinationIdx++] = ((float)nData) * fInv0dB;
        }
        *pui8DoPMarker = *pui8DoPMarker == 0x5 ? 0xFA : 0x5;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Convert from an interleave buffer to N non-interleave buffers
void MTConvertDoPDSD64_176_4ToDSD128FloatDeInterleave(float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    uint32_t* pui32Destination = (uint32_t*)pfDestination;
    uint32_t ui32SourceIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
        {
            uint32_t ui32DoPSample = (uint32_t)(pfSource[ui32SourceIdx] * 0x800000); // two 8bits samples in one DoP 24bit sample

            uint32_t ui32MergingDSDSample = 0x5555; // mute

            uint8_t ui8DoPMarker = (uint8_t)(ui32DoPSample >> 16);
            if(ui8DoPMarker == 0xFA || ui8DoPMarker == 0x5)
            {
                ui32MergingDSDSample = ui32DoPSample;
            }

            pui32Destination[uiChannelIdx * ui32DestinationFrameSize + ui32SampleIdx * 2 + 1] = (ui32MergingDSDSample & 0xFF);
            pui32Destination[uiChannelIdx * ui32DestinationFrameSize + ui32SampleIdx * 2 + 0] = ((ui32MergingDSDSample >> 8) & 0xFF);

            ui32SourceIdx++;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
void MTConvertDSD64FloatToDoPDSD64_176_4Interleaved(uint8_t * pui8DoPMarker, float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess)
{
    uint32_t ui32SampleIdx, uiChannelIdx;
    static const float fInv0dB = 1.f / 8388608.0f;

    uint32_t* pui32Source = (uint32_t*)pfSource;
    uint32_t ui32DestinationIdx = 0;
    for(ui32SampleIdx = 0; ui32SampleIdx < ui32NbOfSamplesToProcess; ui32SampleIdx++)
    {
        for(uiChannelIdx = 0; uiChannelIdx < ui32NbOfChannels; uiChannelIdx++)
        {
            uint32_t ui32MergingDSDSample0 = pui32Source[uiChannelIdx * ui32SourceFrameSize + ui32SampleIdx * 2 + 0];
            uint32_t ui32MergingDSDSample1 = pui32Source[uiChannelIdx * ui32SourceFrameSize + ui32SampleIdx * 2 + 1];


            // Note: (int32_t)(int8_t)*pui8DoPMarker is used to extended the sign of the DoP Marker
            int32_t nData = ((int32_t)(int8_t)*pui8DoPMarker << 16) | (int32_t)((ui32MergingDSDSample0 & 0xFF) << 8) | (int32_t)(ui32MergingDSDSample1 & 0xFF); // reverse bytes

            pfDestination[ui32DestinationIdx++] = ((float)nData) * fInv0dB;
        }
        *pui8DoPMarker = *pui8DoPMarker == 0x5 ? 0xFA : 0x5;
    }
}

#endif
