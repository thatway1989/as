#ifndef _DOIP_H_
#define _DOIP_H_

#define uint16 unsigned short int
#define uint32 unsigned int
#define uint8 unsigned char
typedef unsigned long uint16_least;
typedef unsigned long uint32_least;  

/* DoIP header types */
# define DOIP_HDR_TYPE_GEN_NACK                    0x0000u
# define DOIP_HDR_TYPE_VEHICLE_IDENT_REQ           0x0001u
# define DOIP_HDR_TYPE_VEHICLE_IDENT_REQ_EID       0x0002u
# define DOIP_HDR_TYPE_VEHICLE_IDENT_REQ_VIN       0x0003u
# define DOIP_HDR_TYPE_VEHICLE_ANNOUNCEMENT        0x0004u
# define DOIP_HDR_TYPE_ROUT_ACTIV_REQ              0x0005u
# define DOIP_HDR_TYPE_ROUT_ACTIV_RES              0x0006u
# define DOIP_HDR_TYPE_ALIVE_CHECK_REQ             0x0007u
# define DOIP_HDR_TYPE_ALIVE_CHECK_RES             0x0008u
# define DOIP_HDR_TYPE_ENTITY_STATE_REQ            0x4001u
# define DOIP_HDR_TYPE_ENTITY_STATE_RES            0x4002u
# define DOIP_HDR_TYPE_DIAG_POWER_MODE_INFO_REQ    0x4003u
# define DOIP_HDR_TYPE_DIAG_POWER_MODE_INFO_RES    0x4004u
# define DOIP_HDR_TYPE_DIAG_REQ                    0x8001u
# define DOIP_HDR_TYPE_DIAG_ACK                    0x8002u
# define DOIP_HDR_TYPE_DIAG_NACK                   0x8003u
# define DOIP_HDR_TYPE_OEM_SPECIFIC                0xEEEEu
# define DOIP_HDR_TYPE_INVALID                     0xEFFFu

/* DoIP protocol versions */
# define DOIP_PROTOCOL_VERSION_2012                0x02u
# define DOIP_PROTOCOL_VERSION_DEFAULT             0xFFu


# define IPBASE_GET_UINT8(Buffer, Offset, Value)    (Value) = IpBase_GetUint8((Buffer), (Offset))
# define IPBASE_GET_UINT16(Buffer, Offset, Value)   (Value) = IpBase_GetUint16((Buffer), (Offset))
# define IPBASE_GET_UINT32(Buffer, Offset, Value)   (Value) = IpBase_GetUint32((Buffer), (Offset))


uint8 IpBase_GetUint8(const uint8 *const BufferPtr, uint32_least Offset);

uint16 IpBase_GetUint16(const uint8 *const BufferPtr, uint32_least Offset);

uint32 IpBase_GetUint32(const uint8 *const BufferPtr, uint32_least Offset);

typedef struct {
	uint8 protoclVersion;
	uint8 inverseVersion;
	uint16 dataType;
	uint32 dataLength;
	uint8 *data[0];
} Doip_Message;

void DoIP_Util_FillGenHdr(      /* INTERNAL_NOTE_SBSW_OK */
  uint16 HdrType,               /* INTERNAL_NOTE_SBSW_DOIP_PARAM_UNDEFINED */
  uint32 HdrLen,                /* INTERNAL_NOTE_SBSW_DOIP_PARAM_UNDEFINED */
  Doip_Message* p_doip);

void DoIP_RxTcp_HandleCopyRxDataCopyBody(uint8 *rxHdrBuf) ;
//extern uint8 TgtBufferPtr[1032];
#endif