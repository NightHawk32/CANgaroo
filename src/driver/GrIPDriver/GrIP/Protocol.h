#ifndef PROTOCOL_H
#define PROTOCOL_H


#include <stdint.h>


enum StatusLedState_e {
    LED_OFF = 0, LED_RED, LED_GREEN, LED_ORANGE
};


struct Protocol_LinCfg_t {
    uint16_t Baudrate;
    uint8_t Timebase;
    uint16_t Jitter;
    uint8_t Mode;
    uint8_t Protocol;
};


struct CAN_Msg_t {
    uint8_t Channel;
    // CAN ID
    uint32_t ID;
    // Len of message in bytes
    uint8_t DLC;
    // CAN message flags
    uint8_t Flags;
    // TX: Cycle time / RX: Receive time
    uint32_t Time;
    // Message data (max 8 bytes)
    uint8_t Data[8];
};


struct LIN_Frame_t {
    // LIN Channel
    uint8_t Channel;

    // Direction
    uint8_t Direction;
    // Time in ms until next frame
    uint16_t Delay;

    // Unique frame identifier
    uint8_t ID;
    // Unique frame identifier with parity bits
    uint8_t ID_Parity;
    // Length of data
    uint8_t Len;
    // Data array
    uint8_t Data[8];
};


#define SYSTEM_REPORT_INFO      0u
#define SYSTEM_SET_STATUS       1u

#define SYSTEM_SEND_CAN_CFG     10u
#define SYSTEM_SEND_LIN_CFG     11u
#define SYSTEM_START_CAN        12u
#define SYSTEM_START_LIN        13u
#define SYSTEM_ADD_CAN_FRAME    14u
#define SYSTEM_ADD_LIN_FRAME    15u
#define SYSTEM_SEND_CAN_FRAME    20u


void Protocol_RequestDeviceInfo();

void Protocol_SetStatusLED(StatusLedState_e state);

void Protocol_SendCANCfg(uint8_t channel, uint32_t can_baud);
void Protocol_SendLINCfg(Protocol_LinCfg_t *lin1, Protocol_LinCfg_t *lin2);

void Protocol_StartStopCAN(bool start_can1, bool start_can2);
void Protocol_StartStopLIN(bool start_lin1, bool start_lin2);

void Protocol_AddCANFrame(CAN_Msg_t *can);
void Protocol_AddLINFrame(LIN_Frame_t *lin);


#endif // PROTOCOL_H
