#include "doip.h"
#include <stdio.h>

/*global variable*/

//uint8 TgtBufferPtr[1032] = {0};
//uint8 rxHdrBuf[84] = {0};

/*Function*/
uint8 IpBase_GetUint8(const uint8 *const BufferPtr, uint32_least Offset)
{
  return BufferPtr[Offset];
}

uint16 IpBase_GetUint16(const uint8 *const BufferPtr, uint32_least Offset)
{
  return (uint16)(((uint16)(BufferPtr[Offset]) << 8u) | BufferPtr[Offset + 1u]);
}

uint32 IpBase_GetUint32(const uint8 *const BufferPtr, uint32_least Offset)
{
  return (uint32)(((uint32)(BufferPtr[Offset]) << 24u)     | ((uint32)(BufferPtr[Offset + 1u]) << 16u) | 
                  ((uint32)(BufferPtr[Offset + 2u]) << 8u) | BufferPtr[Offset + 3u]);
}


void DoIP_Util_PutUInt16(uint8 *Buffer, uint32 Offset, uint16 Value)                                                  /* INTERNAL_NOTE_SBSW_DOIP_PARAM_UNDEFINED */
{
  /* ----- Implementation ----------------------------------------------- */
  /* #10 Put value into buffer. */
  (Buffer)[(Offset)     ] = (uint8)((uint16_least)(Value) >> 8u);                                                      /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
  (Buffer)[(Offset) + 1u] = (uint8)((uint16_least)(Value));                                                            /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
} /* DoIP_Util_PutUInt16() */

/*
Puts an uint32 into a byte buffer.

Parameters:
Buffer – Target buffer. [range: POINTER != NULL_PTR && (Offset + 4u) <= sizeof(POINTER)]
Offset – Buffer write offset. [range: undefined]
Value – Value to put into buffer. [range: undefined]
*/
void DoIP_Util_PutUInt32(uint8 *Buffer, uint32 Offset, uint32 Value)                                                                                                       /* INTERNAL_NOTE_SBSW_DOIP_PARAM_UNDEFINED */
{
  /* ----- Implementation ----------------------------------------------- */
  /* #10 Put value into buffer. */
  (Buffer)[(Offset)     ] = (uint8)((uint32_least)(Value) >> 24u);                                                     /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
  (Buffer)[(Offset) + 1u] = (uint8)((uint32_least)(Value) >> 16u);                                                     /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
  (Buffer)[(Offset) + 2u] = (uint8)((uint32_least)(Value) >> 8u);                                                      /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
  (Buffer)[(Offset) + 3u] = (uint8)((uint32_least)(Value));                                                            /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
} /* DoIP_Util_PutUInt32() */


void DoIP_Util_FillGenHdr(                                                                 /* INTERNAL_NOTE_SBSW_OK */
  uint16 HdrType,                                                                                                      /* INTERNAL_NOTE_SBSW_DOIP_PARAM_UNDEFINED */
  uint32 HdrLen,                                                                                                       /* INTERNAL_NOTE_SBSW_DOIP_PARAM_UNDEFINED */
  uint8* TgtBufferPtr)                                                                 /* INTERNAL_NOTE_SBSW_DOIP_PARAM_VALID_POINTER */
{
  /* ----- Implementation ----------------------------------------------- */
  /* #10 Write data to target buffer. */
  /* header version */
  TgtBufferPtr[0u] = DOIP_PROTOCOL_VERSION_2012;                                                                       /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
  /* inverse header version */
  TgtBufferPtr[1u] = (uint8)(~TgtBufferPtr[0u]);                                                                       /* SBSW_DOIP_PARAM_VALID_POINTER_WRITE */
  /* header type */
  DoIP_Util_PutUInt16(&TgtBufferPtr[2u], 0u, HdrType);                                                                 /* SBSW_DOIP_PARAM_VALID_POINTER_FORWARD */
  /* header length */
  DoIP_Util_PutUInt32(&TgtBufferPtr[4u], 0u, HdrLen);                                                                  /* SBSW_DOIP_PARAM_VALID_POINTER_FORWARD */
} /* DoIP_Util_FillGenHdr() */


void DoIP_RxTcp_HandleCopyRxDataCopyBody(uint8 *rxHdrBuf) 
{
 // uint8 *rxHdrBuf = DoIP_GetAddrRxHdrBuf(DoIP_GetRxHdrBufStartIdxOfConnectionTcp(ConnectionTcpIdx));
  uint8  hdrVer;
  uint8  hdrInvVer;
  uint16 hdrType;
  uint32 hdrLen;

  /* ----- Implementation ----------------------------------------------- */
  IPBASE_GET_UINT8(rxHdrBuf, 0u, hdrVer);                                                                              /* SBSW_DOIP_PARAM_CONST_POINTER_FORWARD */
  IPBASE_GET_UINT8(rxHdrBuf, 1u, hdrInvVer);      
  if ( ((uint8)(hdrVer) != DOIP_PROTOCOL_VERSION_2012) || ((uint8)(~hdrInvVer) != DOIP_PROTOCOL_VERSION_2012) )
  {
    printf("head version error!\n");
    //(void)DoIP_TxTcp_Transmit(ConnectionTcpIdx, DOIP_HDR_TYPE_GEN_NACK, DOIP_GEN_NACK_FORMAT, 0u, 0u, NULL_PTR, TRUE); /* SBSW_DOIP_PARAM_NULL_POINTER_FORWARD */
  }
  else
  {
    IPBASE_GET_UINT16(rxHdrBuf, 2u, hdrType);                                                                          /* SBSW_DOIP_PARAM_CONST_POINTER_FORWARD */
    IPBASE_GET_UINT32(rxHdrBuf, 4u, hdrLen);
  } 
  printf("hdrVer:%x,hdrInvVer:%x,hdrType:%x,hdrLen:%x\n",hdrVer,hdrInvVer,hdrType,hdrLen);                                                                       

}