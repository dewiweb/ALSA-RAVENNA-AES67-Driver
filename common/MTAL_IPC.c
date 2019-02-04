///////////////////////////////////////////////////////////////////////////////
// Copyright 1993-2018 Merging Technologies S.A., All Rights Reserved - CONFIDENTIAL -
///////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
	#pragma warning(disable : 4996)
#else
	#include <stdio.h>
	#include <stdlib.h>
	#include <stdbool.h>
	#include <string.h>
	//#include <sys/msg.h>
	//#include <sys/ipc.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>


	#include <errno.h>
	
	#include <pthread.h>
#endif

#include "rv_log.h"
#include "MTAL_IPC.h"

//#define USE_SYNCWORD 1

#define DEFAULT_DISPLAY_ELAPSEDTIME_THRESHOLD ~0 // means disabled
#define LOG_AFTER_N_RETRIES 1000

#ifdef __linux__ 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LINUX
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef MT_EMBEDDED
// send command
	#define REQUEST_FIFO_PREFIX "/tmp/fifo/MTAL_IPC_Request"
	// send command's answer
	#define ANSWER_FIFO_PREFIX "/tmp/fifo/MTAL_IPC_Answer"
#else
	// send command
	#define REQUEST_FIFO_PREFIX "/tmp/MTAL_IPC_Request"
	// send command's answer
	#define ANSWER_FIFO_PREFIX "/tmp/MTAL_IPC_Answer"
#endif

#define MSGBLOCK_SYNC_WORD	"Ab$5FGh&^HG&456\0"
#define MAX_CONTROLLER_BUFFER_SIZE	2048
typedef struct
{
#ifdef USE_SYNCWORD
	char strSyncWord[16];
#endif
	uint32_t ui32MsgSize;
	uint32_t ui32MsgSeqId;
	uint32_t ui32MsgId;

	int32_t	i32Result;

} MTAL_IPC_MsgBlockBase;

typedef struct
{	
	MTAL_IPC_MsgBlockBase	base;

	uint8_t					pui8Buffer[MAX_CONTROLLER_BUFFER_SIZE];

} MTAL_IPC_MsgBlock;


typedef struct
{
	int s_bInitialized;
	int s_bPTPv2d;

	uint32_t ui32LocalServerPrefix;
	uint32_t ui32PeerServerPrefix;

	MTAL_IPC_IOCTL_CALLBACK s_callback;
	void* s_callback_user;

	int s_server_fd; // it's the local server   RX
	int s_client_fd; // it's the client of the local server   TX

	int s_peer_server_fd; // it's the peer/remote server TX
	int s_peer_client_fd; // it's the client of the peer server -> in fact it's the local client   RX

	uint32_t s_ui32MsgSeqId;

	pthread_t s_pthread;
	pthread_mutex_t s_lock;

	// debug
	uint32_t ui32DisplayElapsedTimeThreshold;

} TMTAL_IPC_Instance;


static EMTAL_IPC_Error private_init(int bPTPMode, uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle);
static int create_thread(TMTAL_IPC_Instance* pTMTAL_IPC_Instance);
static void destroy_thread(TMTAL_IPC_Instance* pTMTAL_IPC_Instance);
static void* thread_proc(void *pParam);

////////////////////////////////////////////////////////////////
static void DumpMsgBlock(MTAL_IPC_MsgBlockBase* pMTAL_IPC_MsgBlockBase)
{
	if (!pMTAL_IPC_MsgBlockBase)
		return;
#ifdef USE_SYNCWORD
	rv_log(LOG_DEBUG, "MTAL_IPC_MsgBlockBase(%p): [%s] Id: %u, SeqId: %u, Size: %u, Result: %i\n",
		pMTAL_IPC_MsgBlockBase,
		pMTAL_IPC_MsgBlockBase->strSyncWord,
		pMTAL_IPC_MsgBlockBase->ui32MsgId,
		pMTAL_IPC_MsgBlockBase->ui32MsgSeqId,
		pMTAL_IPC_MsgBlockBase->ui32MsgSize,
		pMTAL_IPC_MsgBlockBase->i32Result);
#else
	rv_log(LOG_DEBUG, "MTAL_IPC_MsgBlockBase(%p): Id: %u, SeqId: %u, Size: %u, Result: %i\n",
		pMTAL_IPC_MsgBlockBase,
		pMTAL_IPC_MsgBlockBase->ui32MsgId,
		pMTAL_IPC_MsgBlockBase->ui32MsgSeqId,
		pMTAL_IPC_MsgBlockBase->ui32MsgSize,
		pMTAL_IPC_MsgBlockBase->i32Result);
#endif
}

////////////////////////////////////////////////////////////////
static void DumpMemory(void* pBuffer, uint32_t ui32BufferSize)
{
	rv_log(LOG_INFO, "pBuffer = %p, size = %u\n", pBuffer, ui32BufferSize);
	uint32_t ui = 0;
	if (ui32BufferSize > 256) ui32BufferSize = 256;
	for (ui = 0; ui < ui32BufferSize; ui += 16)
	{
		char hexDump[16 * 4];
		memset(hexDump, 0, sizeof(hexDump));
		uint32_t ui2;
		for (ui2 = 0; ui2 < 16; ++ui2)
		{
			char hex[10] = { 0 };
			sprintf(hex, "%02x ", ((char*)pBuffer)[ui + ui2]);
			strcat(hexDump, hex);
		}

		char charDump[16 * 2];
		memset(charDump, 0, sizeof(charDump));		
		for (ui2 = 0; ui2 < 16; ++ui2)
		{
			char hex[2] = { 0 };
			char c = ((char*)pBuffer)[ui + ui2];
			if (c < 32) c = '-';
			sprintf(hex, "%c", c);
			strcat(charDump, hex);
		}

		rv_log(LOG_DEBUG, "0x%04x: %s  %s\n", ui, hexDump, charDump);
	}
}

////////////////////////////////////////////////////////////////
#ifdef USE_SYNCWORD
static int IsMsgBlockValid(MTAL_IPC_MsgBlockBase* pMTAL_IPC_MsgBlockBase)
{
	return memcmp(pMTAL_IPC_MsgBlockBase->strSyncWord, MSGBLOCK_SYNC_WORD, sizeof(pMTAL_IPC_MsgBlockBase->strSyncWord));
}
#endif
////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_init(uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle)
{
	return private_init(0, ui32LocalServerPrefix, ui32PeerServerPrefix, cb, cb_user, pptrHandle);
}

#ifdef MTAL_IPC_PTPV2D
//////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_init_PTPV2D(uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle)
{
	return private_init(1, ui32LocalServerPrefix, ui32PeerServerPrefix, cb, cb_user, pptrHandle);
}
#endif

//////////////////////////////////////////////////////////////////
EMTAL_IPC_Error private_init(int bPTPv2dMode, uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle)
{
	// static init
	{
		struct timespec clock_res;
		clock_getres(CLOCK_MONOTONIC, &clock_res);
		rv_log(LOG_DEBUG, "clock_res = %u [s] %u [ns]\n", (unsigned int)clock_res.tv_sec, (unsigned int)clock_res.tv_nsec);
	}


	// arguments validation
	if (!pptrHandle)
	{
		return MIE_INVALID_ARG;
	}
	if (!cb)
	{
		//rv_log(LOG_ERR, "error: in server mode, callback function is mandatory\n");
		return MIE_INVALID_ARG;
	}
	*pptrHandle = 0;


	TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)malloc(sizeof(TMTAL_IPC_Instance));
	if (!pTMTAL_IPC_Instance)
	{
		return MIE_FAIL;
	}

	// reset
	memset(pTMTAL_IPC_Instance, 0, sizeof(TMTAL_IPC_Instance));	
	pTMTAL_IPC_Instance->s_server_fd = -1;
	pTMTAL_IPC_Instance->s_client_fd = -1;

	pTMTAL_IPC_Instance->s_peer_client_fd = -1;
	pTMTAL_IPC_Instance->s_peer_server_fd = -1;
	


	// initialization
	pTMTAL_IPC_Instance->ui32DisplayElapsedTimeThreshold = DEFAULT_DISPLAY_ELAPSEDTIME_THRESHOLD;
	pTMTAL_IPC_Instance->ui32LocalServerPrefix = ui32LocalServerPrefix;
	pTMTAL_IPC_Instance->ui32PeerServerPrefix = ui32PeerServerPrefix;
	pTMTAL_IPC_Instance->s_bPTPv2d = bPTPv2dMode;
	pTMTAL_IPC_Instance->s_callback = cb;
	pTMTAL_IPC_Instance->s_callback_user = cb_user;
	
	char szFIFO_Name[256];
	

	////////////////////////////////////////////
	// local server FIFOs
	////////////////////////////////////////////
	// name of the fifo which is the server for this instance
	{		
		sprintf(szFIFO_Name, REQUEST_FIFO_PREFIX "_%u", ui32LocalServerPrefix);
		// create fifo if not exist
		if (access(szFIFO_Name, F_OK) == -1)
		{
			if ((mkfifo(szFIFO_Name, 0666) == -1) && (errno != EEXIST)) {
				rv_log(LOG_ERR, "Unable to create fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
			// change permission because umask removes o:w
			if (chmod(szFIFO_Name, 0666) == -1)
			{
				rv_log(LOG_ERR, "Unable to change permission to fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
		}

		// open fifo
		if ((pTMTAL_IPC_Instance->s_server_fd = open(szFIFO_Name, O_RDWR | (bPTPv2dMode ? O_NONBLOCK : 0))) == -1)
		{
			rv_log(LOG_ERR, "\n open file(%s) failed; errno = %i\n", szFIFO_Name, errno);
			return MIE_FIFO_ERROR;
		}
	}

	////////////////////////////////////////////	
	// name of the fifo which is the client to server for this instance
	{
		sprintf(szFIFO_Name, ANSWER_FIFO_PREFIX "_%u", ui32LocalServerPrefix);
		// create fifo if not exist
		if (access(szFIFO_Name, R_OK | W_OK) == -1)
		{
			if ((mkfifo(szFIFO_Name, 0666) == -1) && (errno != EEXIST)) {
				rv_log(LOG_ERR, "Unable to create fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
			if (chmod(szFIFO_Name, 0666) == -1)
			{
				rv_log(LOG_ERR, "Unable to change permission to fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
		}

		// open fifo
		if ((pTMTAL_IPC_Instance->s_client_fd = open(szFIFO_Name, O_RDWR)) == -1)
		{
			rv_log(LOG_ERR, "\n open file(%s) failed; errno = %i\n", szFIFO_Name, errno);
			return MIE_FIFO_ERROR;
		}
	}



	////////////////////////////////////////////
	// peer/remote server FIFOs
	////////////////////////////////////////////
	// name of the fifo which is the server for this instance
	{
		sprintf(szFIFO_Name, REQUEST_FIFO_PREFIX "_%u", ui32PeerServerPrefix);
		// create fifo if not exist
		if (access(szFIFO_Name, R_OK | W_OK) == -1)
		{
			if ((mkfifo(szFIFO_Name, 0666) == -1) && (errno != EEXIST)) {
				rv_log(LOG_ERR, "Unable to create fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
			if (chmod(szFIFO_Name, 0666) == -1)
			{
				rv_log(LOG_ERR, "Unable to change permission to fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
		}
		// open fifo
		if ((pTMTAL_IPC_Instance->s_peer_server_fd = open(szFIFO_Name, O_RDWR)) == -1)
		{
			rv_log(LOG_ERR, "\n open file(%s) failed; errno = %i\n", szFIFO_Name, errno);
			return MIE_FIFO_ERROR;
		}
	}
	
	// name of the fifo which is the client to server for this instance
	{
		sprintf(szFIFO_Name, ANSWER_FIFO_PREFIX "_%u", ui32PeerServerPrefix);
		// create fifo if not exist
		if (access(szFIFO_Name, R_OK | W_OK) == -1)
		{
			if ((mkfifo(szFIFO_Name, 0666) == -1) && (errno != EEXIST)) {
				rv_log(LOG_ERR, "Unable to create fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
			if (chmod(szFIFO_Name, 0666) == -1)
			{
				rv_log(LOG_ERR, "Unable to change permission to fifo %s", szFIFO_Name);
				return MIE_FIFO_ERROR;
			}
		}

		// open fifo
		if ((pTMTAL_IPC_Instance->s_peer_client_fd = open(szFIFO_Name, O_RDWR)) == -1)
		{
			rv_log(LOG_ERR, "\n open file(%s) failed; errno = %i\n", szFIFO_Name, errno);
			return MIE_FIFO_ERROR;
		}
	}
	

	// init mutex
	if (pthread_mutex_init(&pTMTAL_IPC_Instance->s_lock, NULL) != 0)
	{
		rv_log(LOG_ERR, "\n mutex init failed\n");
		return MIE_FAIL;
	}

	// create thread if needed
	if (!bPTPv2dMode)
	{
		if (create_thread(pTMTAL_IPC_Instance))
		{
			rv_log(LOG_ERR, "Failed to create thread\n");
			return MIE_FAIL;
		}
	}

	pTMTAL_IPC_Instance->s_bInitialized = 1;
	
	*pptrHandle = (uintptr_t)pTMTAL_IPC_Instance;
	return MIE_SUCCESS;
}



////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_destroy(uintptr_t ptrHandle)
{
	if (!ptrHandle)
	{
		// message
		return MIE_HANDLE_INVALID;
	}

	TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)ptrHandle;

	if (!pTMTAL_IPC_Instance->s_bPTPv2d)
	{
		destroy_thread(pTMTAL_IPC_Instance);
	}

	if (pTMTAL_IPC_Instance->s_server_fd >= 0)
	{
		close(pTMTAL_IPC_Instance->s_server_fd);
		pTMTAL_IPC_Instance->s_server_fd = -1;
	}
	if (pTMTAL_IPC_Instance->s_client_fd >= 0)
	{
		close(pTMTAL_IPC_Instance->s_client_fd);
		pTMTAL_IPC_Instance->s_client_fd = -1;
	}

	if (pTMTAL_IPC_Instance->s_peer_server_fd >= 0)
	{
		close(pTMTAL_IPC_Instance->s_peer_server_fd);
		pTMTAL_IPC_Instance->s_peer_server_fd = -1;
	}
	if (pTMTAL_IPC_Instance->s_peer_client_fd >= 0)
	{
		close(pTMTAL_IPC_Instance->s_peer_client_fd);
		pTMTAL_IPC_Instance->s_peer_client_fd = -1;
	}

	pthread_mutex_destroy(&pTMTAL_IPC_Instance->s_lock);

	pTMTAL_IPC_Instance->s_bInitialized = 0;

	free(pTMTAL_IPC_Instance);

	return MIE_SUCCESS;
}

////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_set_display_elapsedtime_threshold(uintptr_t ptrHandle, uint32_t ui32threshold) // [us]. ~0 means disabled
{
	if (!ptrHandle)
	{
		// message
		return MIE_HANDLE_INVALID;
	}

	TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)ptrHandle;
	pTMTAL_IPC_Instance->ui32DisplayElapsedTimeThreshold = ui32threshold;

	return MIE_SUCCESS;
}

static EMTAL_IPC_Error send_buffer_to_fifo(int fd, uint8_t* pBuffer, uint32_t ui32BufferSize);
static int process_message_from_fifo(TMTAL_IPC_Instance* pTMTAL_IPC_Instance, int fd);


static uint32_t get_current_time(); // [us]
static uint32_t get_elapse_time(uint32_t ui32startTime); // [us]
static void display_elapse_time(char* pcText, uint32_t ui32startTime);


static EMTAL_IPC_Error read_answer_from_fifo(int fd_client, uint32_t ui32SeqId, void* pvOutBuffer, uint32_t* pui32OutBufferSize, int32_t* pi32MsgErr);


#ifdef MTAL_IPC_PTPV2D
	//////////////////////////////////////////////////////////////////
	EMTAL_IPC_Error MTAL_IPC_get_FIFO_fd(uintptr_t ptrHandle, int * piFd)
	{
		if (!ptrHandle || !piFd)
		{
			return MIE_HANDLE_INVALID;
		}
		TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)ptrHandle;
		*piFd = pTMTAL_IPC_Instance->s_server_fd;
		return MIE_SUCCESS;
	}

	//////////////////////////////////////////////////////////////////
	EMTAL_IPC_Error MTAL_IPC_process_FIFO(uintptr_t ptrHandle, void* user, int* piRet)
	{
		if (!ptrHandle || !piRet)
		{
			return MIE_HANDLE_INVALID;
		}
		TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)ptrHandle;
		pTMTAL_IPC_Instance->s_callback_user = user;
		*piRet = process_message_from_fifo(pTMTAL_IPC_Instance, pTMTAL_IPC_Instance->s_server_fd);
		return MIE_SUCCESS;
	}
#endif

////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_SendIOCTL(uintptr_t ptrHandle, uint32_t ui32MsgId, void const * pvInBuffer, uint32_t ui32InBufferSize, void* pvOutBuffer, uint32_t* pui32OutBufferSize, int32_t *pi32MsgErr)
{
	if (ui32InBufferSize > MAX_CONTROLLER_BUFFER_SIZE || (pui32OutBufferSize && *pui32OutBufferSize > MAX_CONTROLLER_BUFFER_SIZE))
	{
		rv_log(LOG_ERR, "error: buffer size error\n");
		return MIE_INVALID_BUFFER_SIZE;
	}

    TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)ptrHandle;
	uint32_t ui32StartTime = get_current_time(); // [us]
	if (pTMTAL_IPC_Instance->ui32DisplayElapsedTimeThreshold != (unsigned)~0)
	{
		ui32StartTime = get_current_time();
	}

	EMTAL_IPC_Error iRet = MIE_FAIL;
	pthread_mutex_lock(&pTMTAL_IPC_Instance->s_lock);
	{
		// send message
		{
			MTAL_IPC_MsgBlock MTAL_IPC_MsgBlock_tmp;

			MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize = sizeof(MTAL_IPC_MsgBlockBase) + ui32InBufferSize;
			#ifdef USE_SYNCWORD
				memcpy(MTAL_IPC_MsgBlock_tmp.base.strSyncWord, MSGBLOCK_SYNC_WORD, sizeof(MTAL_IPC_MsgBlock_tmp.base.strSyncWord));
			#endif
			MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId = ++pTMTAL_IPC_Instance->s_ui32MsgSeqId;
			MTAL_IPC_MsgBlock_tmp.base.ui32MsgId = ui32MsgId;
			MTAL_IPC_MsgBlock_tmp.base.i32Result = 0;
			if (pvInBuffer && ui32InBufferSize > 0)
			{
				memcpy(&MTAL_IPC_MsgBlock_tmp.pui8Buffer, pvInBuffer, ui32InBufferSize);
			}


			ssize_t bytes_to_write = MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize;
			iRet = send_buffer_to_fifo(pTMTAL_IPC_Instance->s_peer_server_fd, (uint8_t*)&MTAL_IPC_MsgBlock_tmp, bytes_to_write);
		}

		if (iRet == 0)
		{
			// wait answer
			iRet = read_answer_from_fifo(pTMTAL_IPC_Instance->s_peer_client_fd, pTMTAL_IPC_Instance->s_ui32MsgSeqId, pvOutBuffer, pui32OutBufferSize, pi32MsgErr);
		}
	}
	pthread_mutex_unlock(&pTMTAL_IPC_Instance->s_lock);

	if (pTMTAL_IPC_Instance->ui32DisplayElapsedTimeThreshold != (unsigned)~0 && get_elapse_time(ui32StartTime) > pTMTAL_IPC_Instance->ui32DisplayElapsedTimeThreshold)
	{
		char display[256];
		sprintf(display, "[%i]MTAL_IPC_SendIOCTL", pTMTAL_IPC_Instance->ui32LocalServerPrefix);
		display_elapse_time(display, ui32StartTime);
	}

	if (iRet < MIE_SUCCESS)
	{
		rv_log(LOG_ERR, "MTAL_IPC_SendIOCTL(%u) failed\n", ui32MsgId);
	}
	return iRet;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////
// ui32Timeout in [s]
static int netSelect(int fd, uint32_t ui32Timeout_sec, uint32_t ui32Timeout_usec)
{
	int    ret;
	int nfds;
	fd_set readfds;
	struct timeval tv, *tv_ptr;


	/* Setup fd_set structure for select function */

	FD_ZERO(&readfds);

	FD_SET(fd, &readfds);

	/* Set time to wait if any, else setup NULL pointer */


	if (ui32Timeout_sec > 0 || ui32Timeout_usec > 0)
	{
		tv.tv_sec = ui32Timeout_sec; // [s]
		tv.tv_usec = ui32Timeout_usec;
		tv_ptr = &tv;
	}
	else
	{
		tv_ptr = 0;
	}

	/* Find highest Number Socket for select() function */
	nfds = fd;


	/* Call select function to check all receive sockets with optional timeout */
	ret = select(nfds + 1,  // nfds (highest socket number + 1)
		&readfds,  // readfds
		0,         // writefds
		0,         // exceptfds
		tv_ptr     // timeout structure pointer or NULL
	);

	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EINTR)
		{
			rv_log(LOG_ERR, "netSelect: errno EAGAIN or EINTR (%d)\n", errno);
			ret = 0;
		}
		else
		{
			rv_log(LOG_ERR, "netSelect: unexpected errno: %d, select returns %d\n", errno, ret);
		}
	}
	else if (ret == 0)
	{
		//rv_log(LOG_DEBUG, "netSelect: errno: %d, select returns %d\n", errno, ret);
	}
	else if(!FD_ISSET(nfds, &readfds))
	{
		rv_log(LOG_ERR, "netSelect: errno: %d, select returns %d but fd not part of rfds!?!?!?\n", errno, ret);
	}
	else
	{
		//rv_log(LOG_DEBUG, "netSelect:select returns %d; timeout is %u [s] %u [us]\n", ret, tv.tv_sec, tv.tv_usec);
	}
	//rv_log(LOG_DEBUG, "netSelect: return: %d\n", ret);
	return ret;
}

////////////////////////////////////////////////////////////////
EMTAL_IPC_Error send_buffer_to_fifo(int fd, uint8_t* pBuffer, uint32_t ui32BufferSize)
{
	EMTAL_IPC_Error iRet = MIE_SUCCESS;
	ssize_t bytes_written = write(fd, pBuffer, ui32BufferSize);
	if (bytes_written != ui32BufferSize) 
	{
		rv_log(LOG_ERR, "send_answer:write(%i) error: EBADF = %u, %u;  written: %lu should be %u\n", fd, EBADF, errno, bytes_written, ui32BufferSize);
		iRet = MIE_FAIL;
	}
	return iRet;
}

////////////////////////////////////////////////////////////////
// return latest message in the fifo.
EMTAL_IPC_Error read_answer_from_fifo(int fd_client, uint32_t ui32SeqId, void* pvOutBuffer, uint32_t* pui32OutBufferSize, int32_t* pi32MsgErr)
{
	uint32_t ui32MaxRetryDuration = 500000; // [us]
	uint32_t ui32StartTime = get_current_time(); // [us]

	int iMsgCounter = 0;
	int iRetryCounter = 0;
	do
	{
		if (netSelect(fd_client, 0, 100000) > 0)
		{
			do
			{
				MTAL_IPC_MsgBlock MTAL_IPC_MsgBlock_tmp;
				ssize_t bytes_read;

				// get the message base
				bytes_read = read(fd_client, &MTAL_IPC_MsgBlock_tmp, sizeof(MTAL_IPC_MsgBlockBase));
				if ((bytes_read == -1 && (errno == EINTR || errno == EAGAIN))
					|| bytes_read == 0)
				{
					break;
				}
				else if (bytes_read < 0)
				{
					rv_log(LOG_ERR, "read error: errno = %i\n", errno);
					return MIE_FAIL;
				}
				else if ((unsigned)bytes_read < sizeof(MTAL_IPC_MsgBlockBase))
				{
					// wrong message
					rv_log(LOG_ERR, "wrong message size: %u expected >= %lu", (unsigned int)bytes_read, sizeof(MTAL_IPC_MsgBlockBase));
					return MIE_FAIL;
				}


				// get the message buffer
				if (MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize > sizeof(MTAL_IPC_MsgBlockBase))
				{
					ssize_t bytes_read2 = read(fd_client, &MTAL_IPC_MsgBlock_tmp.pui8Buffer, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase));
					if (bytes_read2 < 0)
					{
						rv_log(LOG_ERR, "2. read error: errno = %i\n", errno);
						return MIE_FAIL;
					}
					else if ((unsigned)bytes_read2 < MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase))
					{
						// wrong message
						rv_log(LOG_ERR, "2. wrong message size: %u expected >= %lu", (unsigned int)bytes_read2, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase));
						return MIE_FAIL;
					}

					bytes_read += bytes_read2;
				}


				uint32_t ui32MsgSize = MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize;

				if (iRetryCounter > LOG_AFTER_N_RETRIES)
				{
					rv_log(LOG_DEBUG, "[%i] bytes_read = %zd\n", iRetryCounter, bytes_read);
					rv_log(LOG_NOTICE, "[%i] SeqId = answer.%u, request.%u; ui32MsgId = %u, ui32MsgSize = %u\n", iMsgCounter, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId, ui32SeqId, MTAL_IPC_MsgBlock_tmp.base.ui32MsgId, ui32MsgSize);
				}
				
				#ifdef USE_SYNCWORD
					if (IsMsgBlockValid(&MTAL_IPC_MsgBlock_tmp.base) != 0)
					{
						rv_log(LOG_ERR, "not a MsgBlock\n");
						rv_log(LOG_ERR, "SeqId = %u, %u; ui32MsgId = %u, ui32MsgSize = %u\n", MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId, ui32SeqId, MTAL_IPC_MsgBlock_tmp.base.ui32MsgId, ui32MsgSize);
						DumpMsgBlock(&MTAL_IPC_MsgBlock_tmp.base);
						DumpMemory(&MTAL_IPC_MsgBlock_tmp.base, ui32MsgSize);
						exit(-5);
					}
				#endif
				if (MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId != ui32SeqId)
				{
					rv_log(LOG_ERR, "Answer doesn't match our message SeqId = %u, %u; ui32MsgId = %u, ui32MsgSize = %u\n", MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId, ui32SeqId, MTAL_IPC_MsgBlock_tmp.base.ui32MsgId, ui32MsgSize);
					DumpMsgBlock(&MTAL_IPC_MsgBlock_tmp.base);
				}
				else
				{
					if (pvOutBuffer && pui32OutBufferSize)
					{
						uint32_t ui32BufferSize = ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase);
						if (ui32BufferSize != *pui32OutBufferSize)
						{
							rv_log(LOG_ERR, "output buffer size doesn't match: msg buffer size = %u when ui32OutBufferSize = %u\n", ui32BufferSize, *pui32OutBufferSize);
							return MIE_INVALID_BUFFER_SIZE;
						}						
						memcpy(pvOutBuffer, MTAL_IPC_MsgBlock_tmp.pui8Buffer, *pui32OutBufferSize);
					}

					/*if (MTAL_IPC_MsgBlock_tmp.base.i32Result < 0)
					{
						rv_log(LOG_WARNING, "ui32MsgId = %u, MTAL_IPC_MsgBlock_tmp.base.iResult = %i\n", MTAL_IPC_MsgBlock_tmp.base.ui32MsgId, MTAL_IPC_MsgBlock_tmp.base.i32Result);
					}*/
					//display_elapse_time("read_answer_from_fifo", ui32StartTime);
					if (pi32MsgErr)
					{
						*pi32MsgErr = MTAL_IPC_MsgBlock_tmp.base.i32Result;
					}
					return MIE_SUCCESS;
				}

				iMsgCounter++;
			} while (1);
		}
		else
		{
			if (iRetryCounter > LOG_AFTER_N_RETRIES)
			{
				rv_log(LOG_DEBUG, "[%i] netSelect timeout\n", iRetryCounter);
			}			
		}

		uint32_t ui32ElapseTime = get_elapse_time(ui32StartTime); // [us];
		if (ui32ElapseTime > ui32MaxRetryDuration)
		{
			rv_log(LOG_ERR, "read_answer_from_fifo error: maximum retry time reached. ui32ElapseTime = %u > ui32MaxRetryDuration = %u\n", ui32ElapseTime, ui32MaxRetryDuration);
			break;
		}	

		++iRetryCounter;
		rv_log(LOG_WARNING, "retry %u, %u [us]\n", iRetryCounter, ui32ElapseTime);
	} while (1);
	return MIE_TIMEOUT;
}

////////////////////////////////////////////////////////////////
// return: 
//		-1: fail
//		0: success
//		1: means we received the END message (thread must died)
static int process_message_from_fifo(TMTAL_IPC_Instance* pTMTAL_IPC_Instance, int fd)
{
	do
	{
		MTAL_IPC_MsgBlock MTAL_IPC_MsgBlock_tmp;
		ssize_t bytes_read;

		//display_elapse_time("fifo read", ui32StartTime);

		// get the message base
		bytes_read = read(fd, &MTAL_IPC_MsgBlock_tmp, sizeof(MTAL_IPC_MsgBlockBase));
		if ((bytes_read == -1 && (errno == EINTR || errno == EAGAIN))
			|| bytes_read == 0)
		{
			//display_elapse_time("fifo read", ui32StartTime);
			return 0;
		}
		else if (bytes_read < 0)
		{
			rv_log(LOG_ERR, "read error: errno = %i\n", errno);
			return -1;
		}
		else if (bytes_read == 1)
		{ // it is time to die
			rv_log(LOG_INFO, "it is time to die\n");
			return 1;
		}
		else if ((unsigned)bytes_read < sizeof(MTAL_IPC_MsgBlockBase))
		{
			// wrong message
			rv_log(LOG_ERR, "wrong message size: %u expected >= %lu", (unsigned int)bytes_read, sizeof(MTAL_IPC_MsgBlockBase));
			return -1;
		}

		// get the message input buffer
		if (MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize > sizeof(MTAL_IPC_MsgBlockBase))
		{
			ssize_t bytes_read2 = read(fd, &MTAL_IPC_MsgBlock_tmp.pui8Buffer, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase));
			if (bytes_read2 < 0)
			{
				rv_log(LOG_ERR, "2. read error: errno = %i\n", errno);
				return -1;
			}			
			else if ((unsigned)bytes_read2 <  MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase))
			{
				// wrong message
				rv_log(LOG_ERR, "2. wrong message size: %u expected >= %lu", (unsigned int)bytes_read2, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase));
				return -1;
			}

			bytes_read += bytes_read2;
		}

		//rv_log(LOG_DEBUG, "SeqId = %u, ui32MsgSize = %u, bytes_read=%u, sizeof(MTAL_IPC_MsgBlockBase)=%u\n", MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize, bytes_read, sizeof(MTAL_IPC_MsgBlockBase));

		//rv_log(LOG_DEBUG, "SeqId = %u\n", MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId);
		// call message_proc()	
		uint8_t pui8OutBuffer[MAX_CONTROLLER_BUFFER_SIZE];
		uint32_t ui32OutBufferSize = sizeof(pui8OutBuffer);
		MTAL_IPC_MsgBlock_tmp.base.i32Result = pTMTAL_IPC_Instance->s_callback(pTMTAL_IPC_Instance->s_callback_user,
																				MTAL_IPC_MsgBlock_tmp.base.ui32MsgId,
																				MTAL_IPC_MsgBlock_tmp.pui8Buffer, MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize - sizeof(MTAL_IPC_MsgBlockBase),
																				pui8OutBuffer, &ui32OutBufferSize);
		MTAL_IPC_MsgBlock_tmp.base.ui32MsgSize = sizeof(MTAL_IPC_MsgBlockBase) + ui32OutBufferSize;
		memcpy(MTAL_IPC_MsgBlock_tmp.pui8Buffer, pui8OutBuffer, ui32OutBufferSize);

		// send answer
		uint32_t ui32Seq = MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId;
		MTAL_IPC_MsgBlock_tmp.base.ui32MsgSeqId = ui32Seq;
		send_buffer_to_fifo(pTMTAL_IPC_Instance->s_client_fd, (uint8_t*)&MTAL_IPC_MsgBlock_tmp, sizeof(MTAL_IPC_MsgBlockBase) + ui32OutBufferSize);
	}
	while (true);

	return 0;	
}

////////////////////////////////////////////////////////////////
static int create_thread(TMTAL_IPC_Instance* pTMTAL_IPC_Instance)
{
	int iRet = 0;
	//set up thread attributes
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	//increase the stack size (because the threads in ravenna daemon use more than the default amount)
	//pthread_attr_setstacksize(&attr, 1024 * 1024 * 16);

	//create and start the thread
	if (pthread_create(&pTMTAL_IPC_Instance->s_pthread, &attr, thread_proc, pTMTAL_IPC_Instance))
	{
		rv_log(LOG_ERR, "pthread_create failed\n");
		
		iRet = -1;
	}
	pthread_attr_destroy(&attr);
	return iRet;
}

////////////////////////////////////////////////////////////////
static void destroy_thread(TMTAL_IPC_Instance* pTMTAL_IPC_Instance)
{
	if (pTMTAL_IPC_Instance->s_pthread)
	{
		rv_log(LOG_DEBUG, "waiting on thread exit...\n");

		// send short message to himself to force thread to exit
		uint8_t buf[1];
		send_buffer_to_fifo(pTMTAL_IPC_Instance->s_server_fd, buf, sizeof(buf));


		pthread_join(pTMTAL_IPC_Instance->s_pthread, 0);
		pTMTAL_IPC_Instance->s_pthread = 0;
	}	
}

////////////////////////////////////////////////////////////////
static void* thread_proc(void *pParam)
{
	TMTAL_IPC_Instance* pTMTAL_IPC_Instance = (TMTAL_IPC_Instance*)pParam;

	rv_log(LOG_DEBUG, "thread start...\n");
	
	while (1)
	{
		if (netSelect(pTMTAL_IPC_Instance->s_server_fd, 10, 0))
		{
			if (process_message_from_fifo(pTMTAL_IPC_Instance, pTMTAL_IPC_Instance->s_server_fd) == 1)
			{ // it is time to die
				break;
			}
		}
	}

	rv_log(LOG_DEBUG, "thread end...\n");
	return NULL;
}

////////////////////////////////////////////////////////////////
static uint32_t get_current_time() // [us]
{
	struct timespec clock;
	clock_gettime(CLOCK_MONOTONIC, &clock);
	return clock.tv_sec * 1000000 + clock.tv_nsec / 1000;
}

////////////////////////////////////////////////////////////////
static uint32_t get_elapse_time(uint32_t ui32StartTime) // [us]
{
	return get_current_time() - ui32StartTime; // [us];
}

////////////////////////////////////////////////////////////////
static void display_elapse_time(char* pcText, uint32_t ui32StartTime) // [us]
{
	if (!pcText)
	{
		rv_log(LOG_ERR, "Error: pcText is NULL\n");
		return;
	}
	rv_log(LOG_DEBUG, "Duration %s: %u [us]\n", pcText, get_elapse_time(ui32StartTime));
}
#elif !defined(WIN32)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// OSX must be implemented
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_init(uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle)
{
	return MIE_SUCCESS;
}

////////////////////////////////////////////////////////////////
#ifdef MTAL_IPC_PTPV2D
	EMTAL_IPC_Error MTAL_IPC_init_PTPV2D(uint32_t ui32LocalServerPrefix, uint32_t ui32PeerServerPrefix, MTAL_IPC_IOCTL_CALLBACK cb, void* cb_user, uintptr_t* pptrHandle)
	{
		return MIE_SUCCESS;
	}

	////////////////////////////////////////////////////////////////
	EMTAL_IPC_Error MTAL_IPC_get_FIFO_fd(uintptr_t ptrHandle, int* piFd)
	{
		return MIE_SUCCESS;
	}

	////////////////////////////////////////////////////////////////
	EMTAL_IPC_Error MTAL_IPC_process_FIFO(uintptr_t ptrHandle, void* user, int * piRet)
	{
		return MIE_SUCCESS;
	}
#endif

////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_destroy(uintptr_t ptrHandle)
{
	return MIE_SUCCESS;
}

////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_set_display_elapsedtime_threshold(uintptr_t ptrHandle, uint32_t ui32threshold)
{
	return MIE_SUCCESS;
}

////////////////////////////////////////////////////////////////
EMTAL_IPC_Error MTAL_IPC_SendIOCTL(uintptr_t ptrHandle, uint32_t  ui32MsgId, void const * pui8InBuffer, uint32_t ui32InBufferSize, void* pui8OutBuffer, uint32_t* pui32OutBufferSize, int32_t *pi32MsgErr)
{
	return MIE_SUCCESS;
}

#endif