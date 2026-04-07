/*

  Copyright (c) 2016 Hubert Denkmair <hubert@denkmair.de>

  This file is part of the candle windows API.
  
  This library is free software: you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.
 
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <stdint.h>
#include <windows.h>
#include <winbase.h>
#include <winusb.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>

#undef __CRT__NO_INLINE
#include <strsafe.h>
#define __CRT__NO_INLINE

#include "candle.h"

#define CANDLE_MAX_DEVICES 32
#define CANDLE_URB_COUNT 30
#define CANDLE_ELMUE_MAX_PACKET_SIZE 72
#define CANDLE_RX_FIFO_SIZE 128

enum {
    CANDLE_PROTOCOL_LEGACY = 0,
    CANDLE_PROTOCOL_ELMUE = 1
};

enum {
    CANDLE_ELMUE_MSG_TX_FRAME = 10,
    CANDLE_ELMUE_MSG_TX_ECHO = 11,
    CANDLE_ELMUE_MSG_RX_FRAME = 12,
    CANDLE_ELMUE_MSG_ERROR = 13,
    CANDLE_ELMUE_MSG_STRING = 14,
    CANDLE_ELMUE_MSG_BUSLOAD = 15
};

enum {
    CANDLE_USB_REQ_HOST_FORMAT = 0,
    CANDLE_USB_REQ_BITTIMING = 1,
    CANDLE_USB_REQ_MODE = 2,
    CANDLE_USB_REQ_BERR = 3,
    CANDLE_USB_REQ_BT_CONST = 4,
    CANDLE_USB_REQ_DEVICE_CONFIG = 5,
    CANDLE_USB_REQ_TIMESTAMP_GET = 6,
    CANDLE_USB_REQ_IDENTIFY = 7,
    CANDLE_USB_REQ_GET_USER_ID = 8,
    CANDLE_USB_REQ_SET_USER_ID = 9,
    CANDLE_USB_REQ_BITTIMING_FD = 10,
    CANDLE_USB_REQ_BT_CONST_FD = 11,
    CANDLE_USB_REQ_TERMINATION_SET = 12,
    CANDLE_USB_REQ_TERMINATION_GET = 13,
    CANDLE_USB_REQ_GET_STATE = 14,
    CANDLE_USB_REQ_GET_BOARD_INFO = 20,
    CANDLE_USB_REQ_SET_FILTER = 21,
    CANDLE_USB_REQ_GET_LAST_ERROR = 22,
    CANDLE_USB_REQ_SET_BUSLOAD_REPORT = 23,
    CANDLE_USB_REQ_SET_PIN_STATUS = 24,
    CANDLE_USB_REQ_GET_PIN_STATUS = 25
};

#pragma pack(push,1)

typedef struct {
    uint32_t byte_order;
} candle_host_config_t;

typedef struct {
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint8_t icount;
    uint32_t sw_version;
    uint32_t hw_version;
} candle_device_config_t;

typedef struct {
    uint32_t mode;
    uint32_t flags;
} candle_device_mode_t;

typedef struct {
    uint8_t feedback;
    uint8_t reserved[9];
} candle_feedback_t;

#pragma pack(pop)

typedef struct {
    OVERLAPPED ovl;
    uint8_t buf[128];
} canlde_rx_urb;

typedef struct {
    candle_frame_any_t frames[CANDLE_RX_FIFO_SIZE];
    uint32_t read_index;
    uint32_t write_index;
    uint32_t count;
    CONDITION_VARIABLE not_empty;
    SRWLOCK lock;
} candle_rx_fifo_t;

typedef struct {
    wchar_t path[256];
    candle_devstate_t state;
    candle_err_t last_error;

    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE winUSBHandle;
    UCHAR interfaceNumber;
    UCHAR bulkInPipe;
    UCHAR bulkOutPipe;

    uint8_t protocol;
    uint8_t channel_index;
    bool supports_fd;
    bool started;
    uint32_t start_flags;

    HANDLE rx_thread;
    bool rx_thread_stop_req;
    HANDLE rx_thread_event;
    candle_rx_fifo_t rx_fifo;

    candle_device_config_t dconf;
    candle_capability_t bt_const;
    candle_capability_fd_t bt_const_fd;
    canlde_rx_urb rxurbs[CANDLE_URB_COUNT];
    HANDLE rxevents[CANDLE_URB_COUNT];
} candle_device_t;

typedef struct {
    uint8_t num_devices;
    candle_err_t last_error;
    candle_device_t dev[CANDLE_MAX_DEVICES];
} candle_list_t;
