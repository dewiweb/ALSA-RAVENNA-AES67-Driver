/****************************************************************************
*
*  Module Name    : PTP_defs.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67
*
*  Written by     : van Kempen Bertrand
*  Date           : 26/09/2014
*  Modified by    : 
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


#pragma pack(push, 1)

typedef struct {
  uint8_t			byClockIdentity[8];
  uint16_t			ui16PortNumber;
} TPortIdentity;

typedef struct 
{
  uint8_t			bySeconds[6];
  int32_t			i32Nanoseconds;  
} TV2TimeRepresentation;

typedef struct {
	uint8_t				ui8ClockClass;
	uint8_t				ui8ClockAccuracy;
	uint16_t			ui16OffsetScaledLogVariance; // GrandmasterClockVariance;
} TClockQuality;

/* 1588 Version 2 common Message header */
typedef struct 
{																// Offset  Length (bytes)
	uint8_t				byTransportSpecificAndMessageType;		// 00       1 (2 4-bit fields)
	uint8_t				byReserved1AndVersionPTP;               // 01       1 (2 4-bit fields)
	uint16_t			wMessageLength;                         // 02       2
	uint8_t				byDomainNumber;                         // 04       1
	uint8_t				byReserved2;                            // 05       1
	uint16_t			wFlags;									// 06       2
	int64_t				i64CorrectionField;                     // 08       8
	uint32_t			ui32Reserved3;                          // 16       4
	TPortIdentity		SourcePortId;                           // 20      10
	uint16_t			wSequenceId;                            // 30       2
	uint8_t				byControl;                              // 32       1
	uint8_t				byLogMeanMessageInterval;               // 33       1
} TV2MsgHeader;

#define IS_PTP_TWO_STEP(flags) ((flags & 0x02) == 0x02) // note: flags is in big-endian and we assume the machine's architecture is little-endian so bytes order is inverted


// Version 2 Follow Up message
typedef struct {
  TV2TimeRepresentation	OriginTimestamp;						// 34       10
  uint16_t				OriginCurrentUTCOffset;					// 44		2
  uint8_t				Reserved;								// 46		1
  uint8_t				Priority1;								// 47		1
  TClockQuality			GrandmasterClockQuality;				// 48		4
  uint8_t				Priority2;								// 52		1
  uint8_t				byGrandmasterClockIdentity[8];			// 53		8
  uint16_t				ui16StepsRemoved;						// 61		2
  uint8_t				ui8TimeSource;							// 63		1
} TV2MsgAnnounce;

// Version 2 Sync or Delay_Req message
typedef struct
{																// Offset  Length (bytes)
  TV2TimeRepresentation	OriginTimestamp;						// 34       10
} TV2MsgSync;

// Version 2 Follow Up message
typedef struct {
  TV2TimeRepresentation	PreciseOriginTimestamp;
} TV2MsgFollowUp;





/*
// Version 2 Announce message
typedef struct {
                                                      // Offset  Length (bytes)
  V2TimeRepresentation originTimestamp;               // 34       10
  Integer16            currentUTCOffset;              // 44        2
  uint8_t            reserved;                      // 46        1
  uint8_t            grandmasterPriority1;          // 47        1
  ClockQuality         grandmasterClockQuality;       // 48        4
  uint8_t            grandmasterPriority2;          // 52        1
  Octet                grandmasterIdentity[8];        // 53        8
  WORD           stepsRemoved;                  // 61*       2
  Enumeration8         timeSource;                    // 63        1

// Note: stepsRemoved is a 16 bit field, but it is not 16 bit aligned in the announce message
} TMsgAnnounce;

*/

// Version 2 Delay Resp message

typedef struct {
  TV2TimeRepresentation receiveTimestamp;
  TPortIdentity         requestingPortId;
} TV2MsgDelayResp;


typedef enum
{
	PTP_SYNC_MESSAGE				= 0,
	PTP_DELAY_REQ_MESSAGE			= 1,
	PTP_PATH_DELAY_REQ_MESSAGE		= 2,
	PTP_PATH_DELAY_RESP_MESSAGE		= 3,
	// reserved
	PTP_FOLLOWUP_MESSAGE			= 8,
	PTP_DELAY_RESP_MESSAGE			= 9,
	PTP_PATH_DELAY_FOLLOWUP_MESSAGE	= 10,
	PTP_ANNOUNCE_MESSAGE			= 11,
	PTP_SIGNALLING_MESSAGE			= 12,
	PTP_MANAGEMENT_MESSAGE			= 13
	//reserved
} EV2MessageType;


/* Version 2 control field values */
typedef enum
{
	PTP_SYNC_CONTROL           		= 0,
	PTP_DELAY_REQ_CONTROL      		= 1,
	PTP_FOLLOWUP_CONTROL       		= 2,
	PTP_DELAY_RESP_CONTROL     		= 3,
	PTP_MANAGEMENT_CONTROL     		= 4,
	PTP_ALL_OTHERS_CONTROL     		= 5

	// Reserved 0x06 - 0xFF
} EV2ControlType;

#define LOGMEAN_UNICAST               0x7f  // For unicast SYNC, FOLLOWUP and DELAY_RESP
#define LOGMEAN_DELAY_REQ             0x7f
#define LOGMEAN_SIGNALING             0x7f
#define LOGMEAN_MANAGEMENT            0x7f
#define LOGMEAN_PDELAY_REQ            0x7f
#define LOGMEAN_PDELAY_RESP           0x7f
#define LOGMEAN_PDELAY_RESP_FOLLOWUP  0x7f


/////////////////////////////////////////////////////////////////
// PTPPacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPV4Header;
	TUDPHeader			UDPHeader;
	TV2MsgHeader		V2MsgHeader;
} TPTPPacketBase;

typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPV4Header;
	TUDPHeader			UDPHeader;
	TV2MsgHeader		V2MsgHeader;
	TV2MsgSync			V2MsgSync;
} TPTPV2MsgSyncPacket;

typedef TPTPV2MsgSyncPacket TPTPV2MsgDelayReqPacket;


typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPHeader;
	TUDPHeader			UDPHeader;
	TV2MsgHeader		V2MsgHeader;
	TV2MsgAnnounce		V2MsgAnnounce;
} TPTPV2MsgAnnouncePacket;


typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPHeader;
	TUDPHeader			UDPHeader;
	TV2MsgHeader		V2MsgHeader;
	TV2MsgFollowUp		V2MsgFollowUp;
} TPTPV2MsgFollowUpPacket;

typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPHeader;
	TUDPHeader			UDPHeader;
	TV2MsgHeader		V2MsgHeader;
	TV2MsgDelayResp		V2MsgDelayResp;
} TPTPV2MsgDelayRespPacket;

#pragma pack(pop)

/*
class CPTPTime
{
public:
	CPTPTime()
	{
		m_i64Seconds	= 0;
		m_i32NSeconds	= 0;
	}

	CPTPTime(uint64_t ui64NSeconds)
	{
		m_i64Seconds	= ui64NSeconds / 1000000000;
		m_i32NSeconds	= ui64NSeconds % 1000000000;		
	}

	CPTPTime(int64_t i64Seconds, uint32_t i32NSeconds)
	{
		m_i64Seconds	= i64Seconds;
		m_i32NSeconds	= i32NSeconds;
		normalizeTime();
	}

	CPTPTime operator-(const CPTPTime& value)
	{
		m_i64Seconds  -= value.m_i64Seconds;
		m_i32NSeconds -= value.m_i32NSeconds;

		normalizeTime();
	}

	CPTPTime& operator+(const CPTPTime& value)
	{
		m_i64Seconds  += value.m_i64Seconds;
		m_i32NSeconds += value.m_i32NSeconds;
	}
protected:
	// *
	// * Function to do fixup of time after based
	// * on calling other routines here in arith.c
	// * This function "noramlizes" the time by making
	// * sure that after time additions, subtractions
	// * etc. that the nanoseconds field if between
	// * -999,999,999 and +999,999,999 and as
	// * necessary, the seconds field is adjusted if
	// * the nanosecond time is outside of those
	// * boundaries.
	// *
	// *
	// * @param[in,out] result       
	// * Pointer to TimeInternal structure containing ptpv2d
	// * code internal time representation of signed
	// * seconds and signed nanoseconds for which the 
	// * time values are "normalized".
	// *
	// * 
	void normalizeTime()
	{
	  m_i64Seconds     += m_i32NSeconds / 1000000000;
	  m_i32NSeconds -= m_i32NSeconds / 1000000000 * 1000000000;
	  
	  if(m_i64Seconds > 0 && m_i32NSeconds < 0)
	  {
		m_i64Seconds     -= 1;
		m_i32NSeconds += 1000000000;
	  }
	  else if(m_i64Seconds < 0 && m_i32NSeconds > 0)
	  {
		m_i64Seconds     += 1;
		m_i32NSeconds -= 1000000000;
	  }
	}

protected:
	int64_t m_i64Seconds;
	int32_t m_i32NSeconds;
};
*/
