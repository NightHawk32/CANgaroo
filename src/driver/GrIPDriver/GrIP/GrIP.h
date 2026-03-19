#ifndef GRIP_H_INCLUDED
#define GRIP_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <QByteArray>

// Current protocol version
#define GRIP_VERSION        4u

// Transmit/Receive buffer size - Do not exceed (GRIP_BUFFER_SIZE - 10)
#define GRIP_BUFFER_SIZE    128u

typedef enum
{
    PROT_GrIP = 0, PROT_BoOTA
} GrIP_ProtocolType_e;

/**
  * Available message types.
  */
typedef enum
{
    MSG_SYSTEM_CMD          = 0u,
    MSG_REALTIME_CMD        = 1u,
    MSG_DATA                = 2u,
    MSG_DATA_NO_RESPONSE    = 3u,
    MSG_NOTIFICATION        = 4u,
    MSG_RESPONSE            = 5u,
    MSG_ERROR               = 6u,
    MSG_SYNC                = 7u,
    MSG_MAX_NUM             = 8u
} GrIP_MessageType_e;

/**
  * Return Types.
  */
typedef enum
{
    RET_OK              = 0,
    RET_NOK             = 1,
    RET_WRONG_VERSION   = 2,
    RET_WRONG_CRC       = 3,
    RET_WRONG_MAGIC     = 4,
    RET_WRONG_PARAM     = 5,
    RET_WRONG_TYPE      = 6,
    RET_WRONG_LEN       = 7,
} GrIP_ReturnType_e;

/**
  * GrIP Packet Header
  * Size: 8 bytes
  */
typedef struct __attribute__((packed))
{
    uint8_t  Version;
    uint8_t  Protocol;
    uint8_t  MsgType;
    uint8_t  ReturnCode;
    uint16_t Length;
    uint8_t  CRC_Header;
    uint8_t  CRC_Data;
} GrIP_PacketHeader_t;

/**
  * GrIP Receive Packet
  */
typedef struct
{
    GrIP_PacketHeader_t RX_Header;
    uint8_t             Data[GRIP_BUFFER_SIZE];
} GrIP_Packet_t;

/**
  * Data struct.
  * Data: Pointer to data.
  * Length: Length of data in bytes.
  */
typedef struct
{
    const uint8_t  *Data;
    uint16_t  Length;
} GrIP_Pdu_t;

typedef struct
{
    uint8_t  LastError;
    uint16_t CRC_Error;
    uint16_t Len_Error;
    uint16_t Param_Error;
} GrIP_ErrorFlags_t;

/**
  * Initialize the module
  */
void GrIP_Init();

/**
  * Transmit a message over GrIP
  */
[[nodiscard]] uint8_t GrIP_TransmitArray(GrIP_ProtocolType_e ProtType, GrIP_MessageType_e MsgType, GrIP_ReturnType_e ReturnCode, const uint8_t *data, uint16_t len);

/**
  * Transmit a message over GrIP
  */
[[nodiscard]] uint8_t GrIP_Transmit(GrIP_ProtocolType_e ProtType, GrIP_MessageType_e MsgType, GrIP_ReturnType_e ReturnCode, const GrIP_Pdu_t *pdu);

/**
  * Sync protocol flow
  */
[[nodiscard]] uint8_t GrIP_SendSync(void);

/**
  * Continuously call this function to process RX messages
  */
void GrIP_Update(void);

/**
  * Get error flags
  */
void GrIP_GetError(GrIP_ErrorFlags_t *ef);

[[nodiscard]] bool GrIP_Receive(GrIP_Packet_t *p);

int GrIP_GetLastResponse(void);

void       GrIP_RxCallback(const QByteArray &data);
QByteArray GrIP_GetTxData();

#endif // GRIP_H_INCLUDED
