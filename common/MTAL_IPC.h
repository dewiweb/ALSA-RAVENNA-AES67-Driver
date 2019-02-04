/****************************************************************************
* 
*  Module Name    : MT_PTPv2d_IPC.h
*  
*  Abstract       : IPC for communication between Horus/zman and PTPv2d processes
* 
*  Written by     : van Kempen Bertrand
*  Date           : 22/03/2017
*  Modified by    : 
*  Date           : 
*  Modification   : 
*  Known problems : None
* 
*  Copyright 1993-2018 Merging Technologies S.A., All Rights Reserved - CONFIDENTIAL -
****************************************************************************/

#pragma once


#ifdef __cplusplus
	extern "C" {
#endif


#include <stdint.h>

#define MTAL_IPC_PTPV2D 1

/////////////////////////////////////////////////////////
// Error
typedef enum
{
	MIE_SUCCESS				= 0,
	MIE_FAIL				= -1,
	MIE_INVALID_ARG			= -2,
	MIE_INVALID_BUFFER_SIZE = -3,
	MIE_TIMEOUT				= -4,
	MIE_FIFO_ERROR			= -5,
	MIE_HANDLE_INVALID		= -6,
	MIE_NOT_IMPL			= -7
} EMTAL_IPC_Error;

typedef int32_t(*MTAL_IPC_IOCTL_CALLBACK)(void* cb_user, uint32_t  ui32MsgId, void const * pui8InBuffer, uint32_t ui32InBufferSize, void* pui8OutBuffer, uint32_t* pui32OutBufferSize);

////////////////////////////////////////////////////////////////
// inputs:
//	ui32LocalServerPrefix: if 0 means no server capability
//	ui32PeerServerPrefix: if 0 means no client capability
// outputs:
//	pptrHandle handle of the IPC which must be pass to MTAL_IPC_destroy(), MTAL_IPC_SendIOCTL()
// remark: 
//  if the same Local and Peer prefix are used by multiple process, callback works only with the latest process calling init
#ifdef WIN32
	EMTAL_IPC_Error MTAL_IPC_init_WIN32(int8_t bHost, uint32_t ui32Prefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle);
#else
	EMTAL_IPC_Error MTAL_IPC_init(uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle);
#endif
EMTAL_IPC_Error MTAL_IPC_destroy(uintptr_t ptrHandle);
// debug
EMTAL_IPC_Error MTAL_IPC_set_display_elapsedtime_threshold(uintptr_t ptrHandle, uint32_t ui32threshold); // [us]. ~0 means disabled

// PTPv2d only
#ifdef MTAL_IPC_PTPV2D
	EMTAL_IPC_Error MTAL_IPC_init_PTPV2D(uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle);
	EMTAL_IPC_Error MTAL_IPC_get_FIFO_fd(uintptr_t ptrHandle, int * piFd);
	EMTAL_IPC_Error MTAL_IPC_process_FIFO(uintptr_t ptrHandle, void* user, int * piRet);
#endif

////////////////////////////////////////////////////////////////
// inputs:
// ....
// outputs:
//	pui32OutBufferSize: input; size of pui8OutBuffer buffer [byte]
//	pui32OutBufferSize: output; number of bytes written to pui8OutBuffer 
//  pi32MsgErr: error return by the callback which process the message
EMTAL_IPC_Error MTAL_IPC_SendIOCTL(uintptr_t ptrHandle, uint32_t  ui32MsgId, void const * pui8InBuffer, uint32_t ui32InBufferSize, void* pui8OutBuffer, uint32_t* pui32OutBufferSize, int32_t *pi32MsgErr);

#ifdef __cplusplus
	}
#endif
