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

#pragma once

#include "MTAL_stdint.h"

/////////////////////////////////////////////////////////////////////////////
// This structure is used for 24 bits wordlength sample (3 bytes/sample)
#pragma pack(push, 1)
typedef struct
{
	uint16_t	w; // LSB word
	uint8_t		b; // MSB byte
} TRIBYTE;
#pragma pack(pop)

#if defined WIN32 && !defined NT_DRIVER

#include <windows.h>

#pragma warning(disable : 4799)
#include <mmintrin.h>	// MMX instructions
#include <xmmintrin.h>	// SSE instructions
#include <emmintrin.h>	// SSE2 instructions
#pragma warning(default : 4799)
//////////////////////////////////////////////////////////////////////////////

#endif

void MTConvertInit(void);

#if defined WIN32 && !defined NT_DRIVER

void MTConvertReverseDSD64_32(uint32_t const* pui32Src, uint32_t* pui32Dest, uint32_t ui32Length);
void MTConvertReverseDSD128_32(uint32_t const* pui32Src, uint32_t* pui32Dest, uint32_t ui32Length);
void MTConvertReverseDSD256_32(uint32_t const* pui32Src, uint32_t* pui32Dest, uint32_t ui32Length);

void MTConvertFloatToInt8(float *pSrc, int8_t *pDest, uint32_t dwLength);
void MTConvertFloatToInt16(float *pSrc, int16_t *pDest, uint32_t dwLength);
void MTConvertFloatToInt24(float *pSrc, TRIBYTE *pDest, uint32_t dwLength);
void MTConvertFloatTo32BitInt24(float *pSrc, int32_t *pDest, uint32_t dwLength);
void MTConvertFloatToInt32(float *pSrc, int32_t *pDest, uint32_t dwLength);
void MTConvertFloatToInt32_24r(float *pSrc, int32_t *pDest, uint32_t dwLength);

void MTConvertInt16ToFloat(int16_t *pSrc, float *pDest, uint32_t dwLength);
void MTConvertInt24ToFloat(TRIBYTE *pSrc, float *pDest, uint32_t dwLength);
void MTConvert32BitInt24ToFloat(int32_t *pSrc, float *pDest, uint32_t dwLength);
void MTConvertInt32ToFloat(int32_t * pSrc, float * pDest, uint32_t dwLength);
void MTConvertInt32_24rToFloat(int32_t *pSrc, float *pDest, uint32_t dwLength);




/*void MTConvertInterleave(float* pfUninterleave, float* pfInterleave, uint32_t dwNbOfChannels, uint32_t dwFrameSize);
void MTConvertDeInterleave(float* pfInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwFrameSize);*/
void MTConvertInterleave(float* pfUninterleave, float* pfInterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize);
void MTConvertDeInterleave(float* pfInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize);
#endif //defined WIN32 && !defined NT_DRIVER

#if !defined(NT_DRIVER) && !defined(__KERNEL__)
// Contiguous Float block buffers
void MTConvertFloatToBigEndianInt16Interleave(float* pfUninterleave, int16_t* pi16BigEndianInterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize);
void MTConvertBigEndianInt16ToFloatDeInterleave(int16_t* pi16BigEndianInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize);

void MTConvertFloatToBigEndianInt24Interleave(float* pfUninterleave, uint8_t* pbyBigEndianInterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize);
void MTConvertBigEndianInt24ToFloatDeInterleave(uint8_t* pbyBigEndianInterleave, float* pfUninterleave, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels, uint32_t dwFrameSize);
#endif //!defined(NT_DRIVER)

// Mapped Float Block buffers
typedef void (*MTCONVERT_MAPPED_TO_INTERLEAVE_PROTOTYPE)(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
typedef void (*MTCONVERT_INTERLEAVE__TO_MAPPED_PROTOTYPE)(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);

#if !defined(NT_DRIVER) && !defined(__KERNEL__)
void MTConvertMappedFloatToBigEndianInt16Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16BigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianInt16ToMappedFloatDeInterleave(void* pi16BigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedFloatToBigEndianInt24Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pbyBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianInt24ToMappedFloatDeInterleave(void* pbyBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
#endif //!defined(NT_DRIVER)

int MTConvertMappedInt32ToInt16LEInterleave(void** ppi32Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16LittleEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels, const bool bUserSpace);
int MTConvertMappedInt32ToInt24LEInterleave(void** ppi32Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16LittleEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels, const bool bUserSpace);
int MTConvertMappedInt32ToInt24LE4ByteInterleave(void** ppi32Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16LittleEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels, const bool bUserSpace);
int MTConvertMappedInt32ToInt32LEInterleave(void** ppi32Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16LittleEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels, const bool bUserSpace);


void MTConvertMappedInt24ToBigEndianInt16Interleave(void** ppi24Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16BigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianInt16ToMappedInt24DeInterleave(void* pi16BigEndianInterleave, void** ppi24Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedInt32ToBigEndianInt16Interleave(void** ppi32Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pi16BigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianInt16ToMappedInt32DeInterleave(void* pi16BigEndianInterleave, void** ppi32Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedInt24ToBigEndianInt24Interleave(void** ppi24Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pbyBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianInt24ToMappedInt24DeInterleave(void* pbyBigEndianInterleave, void** ppi24Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedInt32ToBigEndianInt24Interleave(void** ppi32Uninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pbyBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianInt24ToMappedInt32DeInterleave(void* pbyBigEndianInterleave, void** ppi32Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);

// DSD
void MTConvertMappedFloatToBigEndianDSD64Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianDSD64ToMappedFloatDeInterleave(void* pi8BigEndianInterleave, void** ppi32Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedFloatToBigEndianDSD128Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianDSD128ToMappedFloatDeInterleave(void* pi16BigEndianInterleave, void** ppi32Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedFloatToBigEndianDSD256Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianDSD256ToMappedFloatDeInterleave(void* pi32BigEndianInterleave, void** ppi32Uninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);

// ASIO DSD
void MTConvertMappedInt8ToBigEndianDSD64_32Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianDSD64_32ToMappedInt8DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedInt16ToBigEndianDSD128_32Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianDSD128_32ToMappedInt16DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);
void MTConvertMappedInt32ToBigEndianDSD256Interleave(void** ppfUninterleave, const uint32_t dwOffsetInUninterleaveBuffer, void* pvBigEndianInterleave, const uint32_t dwNbOfChannels, const uint32_t dwNbOfSamplesByChannels);
void MTConvertBigEndianDSD256ToMappedInt32DeInterleave(void* pvBigEndianInterleave, void** ppfUninterleave, uint32_t dwOffsetInUninterleaveBuffer, uint32_t dwNbOfChannels, uint32_t dwNbOfSamplesByChannels);

#if !defined(NT_DRIVER) && !defined(__KERNEL__)
void MTMute16(uint8_t ui8MuteSample, uint16_t* pui16Destination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess);
void MTMute32(uint8_t ui8MuteSample, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess);
// CoreAudio DoP DSD
void MTConvertDoPDSD256_705_6ToDSD256FloatDeInterleave(float const * pfSource, uint16_t* pui16Destination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess);
void MTConvertDSD256FloatToDoPDSD256_705_6Interleaved(uint8_t * pui8DoPMarker, uint16_t const * pui16Source, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess);


void MTConvertDoPDSD256_352_8ToDSD256FloatDeInterleave(float const * pfSource, float* pfDestination, uint32_t dwNbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess);
void MTConvertDSD256FloatToDoPDSD256_352_8Interleaved(uint8_t * pui8DoPMarker, float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess);

void MTConvertDoPDSD128_352_8ToDSD128FloatDeInterleave(float const * pfSource, float* pfDestination, uint32_t dwNbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess);
void MTConvertDSD128FloatToDoPDSD128_352_8Interleaved(uint8_t * pui8DoPMarker, float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess);

void MTConvertDoPDSD64_176_4ToDSD128FloatDeInterleave(float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32DestinationFrameSize, uint32_t ui32NbOfSamplesToProcess);
void MTConvertDSD64FloatToDoPDSD64_176_4Interleaved(uint8_t * pui8DoPMarker, float const * pfSource, float* pfDestination, uint32_t ui32NbOfChannels, uint32_t ui32SourceFrameSize, uint32_t ui32NbOfSamplesToProcess);
#endif

