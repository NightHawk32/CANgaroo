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

#include "candle.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "candle_defs.h"
#include "candle_ctrl_req.h"
#include "ch_9.h"

static bool candle_dev_interal_open(candle_handle hdev);
static bool candle_detect_protocol(candle_device_t *dev);
static bool candle_read_feedback_ok(candle_device_t *dev, uint16_t value);
static uint8_t candle_dlc_to_len(uint8_t dlc);
static uint8_t candle_len_to_dlc(uint8_t len);
static bool candle_parse_elmue_packet(candle_device_t *dev, const uint8_t *buf, DWORD size, candle_frame_any_t *frame);
static bool candle_rx_fifo_push(candle_device_t *dev, const candle_frame_any_t *frame);
static bool candle_rx_fifo_pop(candle_device_t *dev, candle_frame_any_t *frame, uint32_t timeout_ms);
static DWORD WINAPI candle_rx_thread_proc(LPVOID param);
static bool candle_start_rx_thread(candle_device_t *dev);
static void candle_stop_rx_thread(candle_device_t *dev);

static bool candle_read_di(HDEVINFO hdi, SP_DEVICE_INTERFACE_DATA interfaceData, candle_device_t *dev)
{
    ULONG requiredLength = 0;
    SetupDiGetDeviceInterfaceDetail(hdi, &interfaceData, NULL, 0, &requiredLength, NULL);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        dev->last_error = CANDLE_ERR_SETUPDI_IF_DETAILS;
        return false;
    }

    PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data =
        (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LMEM_FIXED, requiredLength);

    if (detail_data != NULL) {
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    } else {
        dev->last_error = CANDLE_ERR_MALLOC;
        return false;
    }

    bool retval = true;
    ULONG length = requiredLength;
    if (!SetupDiGetDeviceInterfaceDetail(hdi, &interfaceData, detail_data, length, &requiredLength, NULL)) {
        dev->last_error = CANDLE_ERR_SETUPDI_IF_DETAILS2;
        retval = false;
    } else if (FAILED(StringCchCopy(dev->path, sizeof(dev->path), detail_data->DevicePath))) {
        dev->last_error = CANDLE_ERR_PATH_LEN;
        retval = false;
    }

    LocalFree(detail_data);

    if (!retval) {
        return false;
    }

    if (candle_dev_interal_open(dev)) {
        dev->state = CANDLE_DEVSTATE_AVAIL;
        candle_dev_close(dev);
    } else {
        dev->state = CANDLE_DEVSTATE_INUSE;
    }

    dev->last_error = CANDLE_ERR_OK;
    return true;
}

bool __stdcall candle_list_scan(candle_list_handle *list)
{
    if (list == NULL) {
        return false;
    }

    candle_list_t *l = (candle_list_t *)calloc(1, sizeof(candle_list_t));
    *list = l;
    if (l == NULL) {
        return false;
    }

    GUID guid;
    if (CLSIDFromString(L"{c15b4308-04d3-11e6-b3ea-6057189e6443}", &guid) != NOERROR) {
        l->last_error = CANDLE_ERR_CLSID;
        return false;
    }

    HDEVINFO hdi = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdi == INVALID_HANDLE_VALUE) {
        l->last_error = CANDLE_ERR_GET_DEVICES;
        return false;
    }

    bool rv = false;
    for (unsigned i = 0; i < CANDLE_MAX_DEVICES; i++) {
        SP_DEVICE_INTERFACE_DATA interfaceData;
        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        if (SetupDiEnumDeviceInterfaces(hdi, NULL, &guid, i, &interfaceData)) {
            if (!candle_read_di(hdi, interfaceData, &l->dev[i])) {
                l->last_error = l->dev[i].last_error;
                rv = false;
                break;
            }
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS) {
                l->num_devices = i;
                l->last_error = CANDLE_ERR_OK;
                rv = true;
            } else {
                l->last_error = CANDLE_ERR_SETUPDI_IF_ENUM;
                rv = false;
            }
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(hdi);
    return rv;
}

bool __stdcall DLL candle_list_free(candle_list_handle list)
{
    free(list);
    return true;
}

bool __stdcall DLL candle_list_length(candle_list_handle list, uint8_t *len)
{
    candle_list_t *l = (candle_list_t *)list;
    *len = l->num_devices;
    return true;
}

bool __stdcall DLL candle_dev_get(candle_list_handle list, uint8_t dev_num, candle_handle *hdev)
{
    candle_list_t *l = (candle_list_t *)list;
    if (l == NULL) {
        return false;
    }

    if (dev_num >= CANDLE_MAX_DEVICES) {
        l->last_error = CANDLE_ERR_DEV_OUT_OF_RANGE;
        return false;
    }

    candle_device_t *dev = calloc(1, sizeof(candle_device_t));
    *hdev = dev;
    if (dev == NULL) {
        l->last_error = CANDLE_ERR_MALLOC;
        return false;
    }

    memcpy(dev, &l->dev[dev_num], sizeof(candle_device_t));
    InitializeConditionVariable(&dev->rx_fifo.not_empty);
    InitializeSRWLock(&dev->rx_fifo.lock);
    l->last_error = CANDLE_ERR_OK;
    dev->last_error = CANDLE_ERR_OK;
    return true;
}

bool __stdcall DLL candle_dev_get_state(candle_handle hdev, candle_devstate_t *state)
{
    if (hdev == NULL) {
        return false;
    }

    candle_device_t *dev = (candle_device_t *)hdev;
    *state = dev->state;
    return true;
}

wchar_t* __stdcall DLL candle_dev_get_path(candle_handle hdev)
{
    if (hdev == NULL) {
        return NULL;
    }

    candle_device_t *dev = (candle_device_t *)hdev;
    return dev->path;
}

static bool candle_detect_protocol(candle_device_t *dev)
{
    dev->protocol = CANDLE_PROTOCOL_LEGACY;
    dev->supports_fd = false;
    dev->channel_index = dev->interfaceNumber;
    if (dev->channel_index > 0) {
        dev->channel_index--;
    }

    if (!candle_ctrl_get_capability(dev, 0, &dev->bt_const)) {
        return false;
    }

    if ((dev->bt_const.feature & CANDLE_MODE_PROTOCOL_ELMUE) != 0) {
        dev->protocol = CANDLE_PROTOCOL_ELMUE;
    }

    /* For the Elmue protocol, FD bittiming is supported via USB request 10
     * whenever CANDLE_MODE_CAN_FD is advertised, even if CANDLE_MODE_BITTIMING_FD
     * (0x400) is not set in the feature flags. */
    if ((dev->bt_const.feature & CANDLE_MODE_CAN_FD) != 0 &&
        (dev->bt_const.feature & CANDLE_MODE_BITTIMING_FD) != 0) {
        if (!candle_ctrl_get_capability_fd(dev, dev->channel_index, &dev->bt_const_fd)) {
            return false;
        }
        dev->supports_fd = true;
    } else if (dev->protocol == CANDLE_PROTOCOL_ELMUE &&
               (dev->bt_const.feature & CANDLE_MODE_CAN_FD) != 0) {
        /* Elmue firmware supports FD bittiming via USB request 10 even without
         * the CANDLE_MODE_BITTIMING_FD capability flag. BT_CONST_FD may or may
         * not be implemented; ignore failure. */
        (void)candle_ctrl_get_capability_fd(dev, dev->channel_index, &dev->bt_const_fd);
        dev->supports_fd = true;
    }

    return true;
}

static bool candle_dev_interal_open(candle_handle hdev)
{
    candle_device_t *dev = (candle_device_t *)hdev;

    dev->started = false;
    dev->start_flags = 0;
    dev->rx_thread = NULL;
    dev->rx_thread_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (dev->rx_thread_event == NULL) {
        dev->last_error = CANDLE_ERR_PREPARE_READ;
        return false;
    }
    dev->rx_thread_stop_req = false;
    dev->rx_fifo.read_index = 0;
    dev->rx_fifo.write_index = 0;
    dev->rx_fifo.count = 0;
    InitializeConditionVariable(&dev->rx_fifo.not_empty);
    InitializeSRWLock(&dev->rx_fifo.lock);

    dev->deviceHandle = CreateFile(
        dev->path,
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (dev->deviceHandle == INVALID_HANDLE_VALUE) {
        dev->last_error = CANDLE_ERR_CREATE_FILE;
        goto close_event;
    }

    if (!WinUsb_Initialize(dev->deviceHandle, &dev->winUSBHandle)) {
        dev->last_error = CANDLE_ERR_WINUSB_INITIALIZE;
        goto close_handle;
    }

    USB_INTERFACE_DESCRIPTOR ifaceDescriptor;
    if (!WinUsb_QueryInterfaceSettings(dev->winUSBHandle, 0, &ifaceDescriptor)) {
        dev->last_error = CANDLE_ERR_QUERY_INTERFACE;
        goto winusb_free;
    }

    dev->interfaceNumber = ifaceDescriptor.bInterfaceNumber;
    unsigned pipes_found = 0;

    for (uint8_t i = 0; i < ifaceDescriptor.bNumEndpoints; i++) {
        WINUSB_PIPE_INFORMATION pipeInfo;
        if (!WinUsb_QueryPipe(dev->winUSBHandle, 0, i, &pipeInfo)) {
            dev->last_error = CANDLE_ERR_QUERY_PIPE;
            goto winusb_free;
        }


        if (pipeInfo.PipeType == UsbdPipeTypeBulk && USB_ENDPOINT_DIRECTION_IN(pipeInfo.PipeId)) {
            dev->bulkInPipe = pipeInfo.PipeId;
            pipes_found++;
        } else if (pipeInfo.PipeType == UsbdPipeTypeBulk && USB_ENDPOINT_DIRECTION_OUT(pipeInfo.PipeId)) {
            dev->bulkOutPipe = pipeInfo.PipeId;
            pipes_found++;
        }
    }

    if (pipes_found != 2) {
        dev->last_error = CANDLE_ERR_PARSE_IF_DESCR;
        goto winusb_free;
    }

    char use_raw_io = 1;
    if (!WinUsb_SetPipePolicy(dev->winUSBHandle, dev->bulkInPipe, RAW_IO, sizeof(use_raw_io), &use_raw_io)) {
        dev->last_error = CANDLE_ERR_SET_PIPE_RAW_IO;
        goto winusb_free;
    }

    if (!candle_ctrl_set_host_format(dev)) {
        goto winusb_free;
    }

    if (!candle_ctrl_get_config(dev, &dev->dconf)) {
        goto winusb_free;
    }

    if (!candle_detect_protocol(dev)) {
        goto winusb_free;
    }

    dev->last_error = CANDLE_ERR_OK;
    return true;

winusb_free:
    if (dev->winUSBHandle != NULL) {
        WinUsb_Free(dev->winUSBHandle);
        dev->winUSBHandle = NULL;
    }

close_handle:
    if (dev->deviceHandle != NULL && dev->deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(dev->deviceHandle);
        dev->deviceHandle = NULL;
    }

close_event:
    if (dev->rx_thread_event != NULL) {
        CloseHandle(dev->rx_thread_event);
        dev->rx_thread_event = NULL;
    }
    return false;
}

static bool candle_rx_fifo_push(candle_device_t *dev, const candle_frame_any_t *frame)
{
    AcquireSRWLockExclusive(&dev->rx_fifo.lock);
    if (dev->rx_fifo.count >= CANDLE_RX_FIFO_SIZE) {
        dev->rx_fifo.read_index = (dev->rx_fifo.read_index + 1) % CANDLE_RX_FIFO_SIZE;
        dev->rx_fifo.count--;
    }
    dev->rx_fifo.frames[dev->rx_fifo.write_index] = *frame;
    dev->rx_fifo.write_index = (dev->rx_fifo.write_index + 1) % CANDLE_RX_FIFO_SIZE;
    dev->rx_fifo.count++;
    ReleaseSRWLockExclusive(&dev->rx_fifo.lock);
    WakeConditionVariable(&dev->rx_fifo.not_empty);
    return true;
}

static bool candle_rx_fifo_pop(candle_device_t *dev, candle_frame_any_t *frame, uint32_t timeout_ms)
{
    AcquireSRWLockExclusive(&dev->rx_fifo.lock);
    while (dev->rx_fifo.count == 0) {
        if (!SleepConditionVariableSRW(&dev->rx_fifo.not_empty, &dev->rx_fifo.lock, timeout_ms, 0)) {
            ReleaseSRWLockExclusive(&dev->rx_fifo.lock);
            dev->last_error = CANDLE_ERR_READ_TIMEOUT;
            return false;
        }
    }

    *frame = dev->rx_fifo.frames[dev->rx_fifo.read_index];
    dev->rx_fifo.read_index = (dev->rx_fifo.read_index + 1) % CANDLE_RX_FIFO_SIZE;
    dev->rx_fifo.count--;
    ReleaseSRWLockExclusive(&dev->rx_fifo.lock);
    return true;
}

static DWORD WINAPI candle_rx_thread_proc(LPVOID param)
{
    candle_device_t *dev = (candle_device_t *)param;
    OVERLAPPED ovl;
    memset(&ovl, 0, sizeof(ovl));
    ovl.hEvent = dev->rx_thread_event;
    uint8_t rxbuf[128];

    while (!dev->rx_thread_stop_req) {
        DWORD bytes_transfered = 0;
        ResetEvent(dev->rx_thread_event);

        if (!WinUsb_ReadPipe(dev->winUSBHandle, dev->bulkInPipe, rxbuf, sizeof(rxbuf), NULL, &ovl)) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                continue;
            }

            DWORD wait = WaitForSingleObject(dev->rx_thread_event, INFINITE);
            if (dev->rx_thread_stop_req) {
                break;
            }
            if (wait != WAIT_OBJECT_0) {
                continue;
            }

            if (!WinUsb_GetOverlappedResult(dev->winUSBHandle, &ovl, &bytes_transfered, FALSE)) {
                continue;
            }
        }

        candle_frame_any_t frame;
        bool ok = false;
        if (dev->protocol == CANDLE_PROTOCOL_ELMUE) {
            ok = candle_parse_elmue_packet(dev, rxbuf, bytes_transfered, &frame);
        } else if (bytes_transfered >= sizeof(candle_frame_t) - 4) {
            candle_frame_t legacy;
            memcpy(&legacy, rxbuf, sizeof(legacy));
            memset(&frame, 0, sizeof(frame));
            frame.frame_type = candle_frame_type(&legacy);
            frame.can_id = legacy.can_id;
            frame.can_dlc = legacy.can_dlc;
            frame.channel = legacy.channel;
            frame.flags = legacy.flags;
            memcpy(frame.data, legacy.data, 8);
            frame.timestamp_us = bytes_transfered < sizeof(candle_frame_t) ? 0 : legacy.timestamp_us;
            ok = true;
        }

        if (ok) {
            candle_rx_fifo_push(dev, &frame);
        }
    }

    return 0;
}

static bool candle_start_rx_thread(candle_device_t *dev)
{
    DWORD id = 0;
    dev->rx_thread_stop_req = false;
    dev->rx_thread = CreateThread(NULL, 0, candle_rx_thread_proc, dev, 0, &id);
    return dev->rx_thread != NULL;
}

static void candle_stop_rx_thread(candle_device_t *dev)
{
    if (dev->rx_thread != NULL) {
        dev->rx_thread_stop_req = true;
        SetEvent(dev->rx_thread_event);
        WaitForSingleObject(dev->rx_thread, INFINITE);
        CloseHandle(dev->rx_thread);
        dev->rx_thread = NULL;
    }
    if (dev->rx_thread_event != NULL) {
        CloseHandle(dev->rx_thread_event);
        dev->rx_thread_event = NULL;
    }
}

bool __stdcall DLL candle_dev_open(candle_handle hdev)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    if (!candle_dev_interal_open(dev)) {
        return false;
    }
    if (!candle_start_rx_thread(dev)) {
        dev->last_error = CANDLE_ERR_PREPARE_READ;
        candle_dev_close(dev);
        return false;
    }
    dev->last_error = CANDLE_ERR_OK;
    return true;
}

bool __stdcall DLL candle_dev_get_timestamp_us(candle_handle hdev, uint32_t *timestamp_us)
{
    return candle_ctrl_get_timestamp(hdev, timestamp_us);
}

bool __stdcall DLL candle_dev_close(candle_handle hdev)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    candle_stop_rx_thread(dev);

    if (dev->winUSBHandle != NULL) {
        WinUsb_Free(dev->winUSBHandle);
        dev->winUSBHandle = NULL;
    }
    if (dev->deviceHandle != NULL) {
        CloseHandle(dev->deviceHandle);
        dev->deviceHandle = NULL;
    }

    dev->last_error = CANDLE_ERR_OK;
    return true;
}

bool __stdcall DLL candle_dev_free(candle_handle hdev)
{
    free(hdev);
    return true;
}

candle_err_t __stdcall DLL candle_dev_last_error(candle_handle hdev)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    return dev->last_error;
}

bool __stdcall DLL candle_channel_count(candle_handle hdev, uint8_t *num_channels)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    *num_channels = dev->dconf.icount + 1;
    return true;
}

bool __stdcall DLL candle_channel_get_capabilities(candle_handle hdev, uint8_t ch, candle_capability_t *cap)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    memcpy(cap, &dev->bt_const, sizeof(candle_capability_t));
    return true;
}

bool __stdcall DLL candle_channel_get_capabilities_fd(candle_handle hdev, uint8_t ch, candle_capability_fd_t *cap)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    if (!dev->supports_fd) {
        dev->last_error = CANDLE_ERR_UNSUPPORTED_FRAME;
        return false;
    }
    memcpy(cap, &dev->bt_const_fd, sizeof(candle_capability_fd_t));
    return true;
}

bool __stdcall DLL candle_channel_set_timing(candle_handle hdev, uint8_t ch, candle_bittiming_t *data)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    if (!candle_ctrl_set_bittiming(dev, ch, data)) {
        return false;
    }

    if (!candle_read_feedback_ok(dev, ch)) {
        dev->last_error = CANDLE_ERR_SET_BITTIMING;
        return false;
    }

    return true;
}

bool __stdcall DLL candle_channel_set_timing_fd(candle_handle hdev, uint8_t ch, candle_bittiming_t *data)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    if (!dev->supports_fd) {
        dev->last_error = CANDLE_ERR_UNSUPPORTED_FRAME;
        return false;
    }
    if (!candle_ctrl_set_bittiming_fd(dev, ch, data)) {
        return false;
    }

    if (!candle_read_feedback_ok(dev, ch)) {
        dev->last_error = CANDLE_ERR_SET_BITTIMING_FD;
        return false;
    }

    return true;
}

bool __stdcall DLL candle_channel_set_bitrate(candle_handle hdev, uint8_t ch, uint32_t bitrate)
{
    candle_device_t *dev = (candle_device_t *)hdev;

    if (dev->bt_const.fclk_can != 48000000) {
        dev->last_error = CANDLE_ERR_BITRATE_FCLK;
        return false;
    }

    candle_bittiming_t t;
    t.prop_seg = 1;
    t.sjw = 1;
    t.phase_seg1 = 13 - t.prop_seg;
    t.phase_seg2 = 2;

    switch (bitrate) {
        case 10000: t.brp = 300; break;
        case 20000: t.brp = 150; break;
        case 50000: t.brp = 60; break;
        case 83333: t.brp = 36; break;
        case 100000: t.brp = 30; break;
        case 125000: t.brp = 24; break;
        case 250000: t.brp = 12; break;
        case 500000: t.brp = 6; break;
        case 800000:
            t.brp = 4;
            t.phase_seg1 = 12 - t.prop_seg;
            t.phase_seg2 = 2;
            break;
        case 1000000: t.brp = 3; break;
        default:
            dev->last_error = CANDLE_ERR_BITRATE_UNSUPPORTED;
            return false;
    }

    return candle_ctrl_set_bittiming(dev, ch, &t);
}

static bool candle_read_feedback_ok(candle_device_t *dev, uint16_t value)
{
    if (dev->protocol != CANDLE_PROTOCOL_ELMUE) {
        return true;
    }

    candle_feedback_t feedback;
    int usb_fail_count = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (!candle_ctrl_get_feedback(dev, value, &feedback)) {
            usb_fail_count++;
            /* If USB transfer consistently fails, the firmware likely does not
             * implement CANDLE_USB_REQ_GET_LAST_ERROR. Assume success after
             * a few retries so the channel can still be started. */
            if (usb_fail_count >= 3) {
                dev->last_error = CANDLE_ERR_OK;
                return true;
            }
            Sleep(1);
            continue;
        }

        if (feedback.feedback == 2) {
            dev->last_error = CANDLE_ERR_OK;
            return true;
        }

        /* feedback=1 means firmware rejected the command */
        if (feedback.feedback == 1) {
            dev->last_error = CANDLE_ERR_UNSUPPORTED_PROTOCOL;
            return false;
        }

        /* feedback=0 means not yet processed, keep polling */
        Sleep(1);
    }

    dev->last_error = CANDLE_ERR_UNSUPPORTED_PROTOCOL;
    return false;
}

bool __stdcall DLL candle_channel_start(candle_handle hdev, uint8_t ch, uint32_t flags)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    flags |= CANDLE_MODE_HW_TIMESTAMP;
    if (dev->protocol == CANDLE_PROTOCOL_ELMUE) {
        flags |= CANDLE_MODE_PROTOCOL_ELMUE;
    }
    if (!candle_ctrl_set_device_mode(dev, ch, CANDLE_DEVMODE_START, flags)) {
        return false;
    }
    if (!candle_read_feedback_ok(dev, ch)) {
        return false;
    }
    if (dev->protocol == CANDLE_PROTOCOL_ELMUE) {
        (void)candle_ctrl_set_termination(dev, ch, true);
        (void)candle_ctrl_set_busload_report(dev, ch, 7);
    }
    dev->started = true;
    dev->start_flags = flags;
    dev->last_error = CANDLE_ERR_OK;
    return true;
}

bool __stdcall DLL candle_channel_stop(candle_handle hdev, uint8_t ch)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    uint32_t flags = dev->protocol == CANDLE_PROTOCOL_ELMUE ? CANDLE_MODE_PROTOCOL_ELMUE : 0;
    if (!candle_ctrl_set_device_mode(dev, ch, CANDLE_DEVMODE_RESET, flags)) {
        return false;
    }
    if (!candle_read_feedback_ok(dev, ch)) {
        return false;
    }
    dev->started = false;
    dev->start_flags = 0;
    return true;
}

bool __stdcall DLL candle_frame_send(candle_handle hdev, uint8_t ch, candle_frame_t *frame)
{
    candle_frame_any_t any;
    memset(&any, 0, sizeof(any));
    any.frame_type = CANDLE_FRAMETYPE_RECEIVE;
    any.can_id = frame->can_id;
    any.can_dlc = frame->can_dlc;
    any.channel = ch;
    any.flags = frame->flags;
    memcpy(any.data, frame->data, 8);
    return candle_frame_send_any(hdev, ch, &any);
}

bool __stdcall DLL candle_frame_send_any(candle_handle hdev, uint8_t ch, candle_frame_any_t *frame)
{
    candle_device_t *dev = (candle_device_t *)hdev;

    if (dev->protocol == CANDLE_PROTOCOL_ELMUE) {
        candle_elmue_tx_frame_t tx;
        memset(&tx, 0, sizeof(tx));
        uint8_t len = candle_dlc_to_len(frame->can_dlc);
        if (len > 64) {
            dev->last_error = CANDLE_ERR_UNSUPPORTED_FRAME;
            return false;
        }

        tx.size = (uint8_t)(8 + len);
        tx.type = CANDLE_ELMUE_MSG_TX_FRAME;
        tx.flags = frame->flags;
        tx.can_id = frame->can_id;
        tx.marker = frame->marker;
        memcpy(tx.data, frame->data, len);


        unsigned long bytes_sent = 0;
        bool rc = WinUsb_WritePipe(
            dev->winUSBHandle,
            dev->bulkOutPipe,
            (uint8_t *)&tx,
            tx.size,
            &bytes_sent,
            0
        );


        dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_SEND_FRAME;
        return rc;
    }

    candle_frame_t legacy;
    memset(&legacy, 0, sizeof(legacy));
    legacy.echo_id = 0;
    legacy.can_id = frame->can_id;
    legacy.can_dlc = frame->can_dlc;
    legacy.channel = ch;
    legacy.flags = frame->flags;
    memcpy(legacy.data, frame->data, 8);

    unsigned long bytes_sent = 0;
    bool rc = WinUsb_WritePipe(
        dev->winUSBHandle,
        dev->bulkOutPipe,
        (uint8_t *)&legacy,
        sizeof(legacy),
        &bytes_sent,
        0
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_SEND_FRAME;
    return rc;
}

static uint8_t candle_dlc_to_len(uint8_t dlc)
{
    static const uint8_t map[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    return map[dlc & 0x0F];
}

static uint8_t candle_len_to_dlc(uint8_t len)
{
    if (len <= 8) return len;
    if (len <= 12) return 9;
    if (len <= 16) return 10;
    if (len <= 20) return 11;
    if (len <= 24) return 12;
    if (len <= 32) return 13;
    if (len <= 48) return 14;
    return 15;
}

static bool candle_parse_elmue_packet(candle_device_t *dev, const uint8_t *buf, DWORD size, candle_frame_any_t *frame)
{
    if (size < sizeof(candle_elmue_header_t)) {
        dev->last_error = CANDLE_ERR_READ_SIZE;
        return false;
    }

    const candle_elmue_header_t *hdr = (const candle_elmue_header_t *)buf;
    if (hdr->size > size) {
        dev->last_error = CANDLE_ERR_READ_SIZE;
        return false;
    }

    memset(frame, 0, sizeof(*frame));

    switch (hdr->type) {
        case CANDLE_ELMUE_MSG_RX_FRAME: {
            const candle_elmue_rx_frame_t *rx = (const candle_elmue_rx_frame_t *)buf;
            frame->frame_type = CANDLE_FRAMETYPE_RXFD;
            frame->can_id = rx->can_id;
            frame->flags = rx->flags;
            frame->channel = dev->channel_index;
            uint8_t payload_len = (uint8_t)(hdr->size - 7);
            uint8_t offset = 0;
            if ((dev->start_flags & CANDLE_MODE_HW_TIMESTAMP) != 0 && payload_len >= 4) {
                memcpy(&frame->timestamp_us, rx->timestamp_and_data, sizeof(uint32_t));
                offset = 4;
                payload_len -= 4;
            }
            if (payload_len > 64) payload_len = 64;
            memcpy(frame->data, rx->timestamp_and_data + offset, payload_len);
            frame->can_dlc = candle_len_to_dlc(payload_len);
            return true;
        }
        case CANDLE_ELMUE_MSG_TX_ECHO: {
            const candle_elmue_tx_echo_t *echo = (const candle_elmue_tx_echo_t *)buf;
            frame->frame_type = CANDLE_FRAMETYPE_TXECHOFD;
            frame->marker = echo->marker;
            if (hdr->size >= sizeof(candle_elmue_tx_echo_t)) {
                frame->timestamp_us = echo->timestamp_us;
            }
            return true;
        }
        case CANDLE_ELMUE_MSG_ERROR: {
            const candle_elmue_error_t *err = (const candle_elmue_error_t *)buf;
            frame->frame_type = CANDLE_FRAMETYPE_ERROR;
            frame->can_id = err->err_id;
            frame->can_dlc = 8;
            memcpy(frame->data, err->err_data, 8);
            if (hdr->size >= sizeof(candle_elmue_error_t)) {
                frame->timestamp_us = err->timestamp_us;
            }
            return true;
        }
        case CANDLE_ELMUE_MSG_STRING: {
            const candle_elmue_string_t *str = (const candle_elmue_string_t *)buf;
            frame->frame_type = CANDLE_FRAMETYPE_STRING;
            uint8_t payload_len = (uint8_t)(hdr->size - 2);
            if (payload_len > 64) payload_len = 64;
            frame->can_dlc = candle_len_to_dlc(payload_len);
            memcpy(frame->data, str->ascii, payload_len);
            return true;
        }
        case CANDLE_ELMUE_MSG_BUSLOAD: {
            const candle_elmue_busload_t *bus = (const candle_elmue_busload_t *)buf;
            frame->frame_type = CANDLE_FRAMETYPE_BUSLOAD;
            frame->can_dlc = 1;
            frame->data[0] = bus->busload;
            return true;
        }
        default:
            dev->last_error = CANDLE_ERR_UNSUPPORTED_FRAME;
            return false;
    }
}

bool __stdcall DLL candle_frame_read(candle_handle hdev, candle_frame_t *frame, uint32_t timeout_ms)
{
    candle_frame_any_t any;
    if (!candle_frame_read_any(hdev, &any, timeout_ms)) {
        return false;
    }

    if (any.frame_type != CANDLE_FRAMETYPE_RECEIVE && any.frame_type != CANDLE_FRAMETYPE_ECHO &&
        any.frame_type != CANDLE_FRAMETYPE_ERROR && any.frame_type != CANDLE_FRAMETYPE_RXFD) {
        return false;
    }

    memset(frame, 0, sizeof(*frame));
    frame->echo_id = any.frame_type == CANDLE_FRAMETYPE_ECHO ? 0 : 0xFFFFFFFF;
    frame->can_id = any.can_id;
    frame->can_dlc = any.can_dlc;
    frame->channel = any.channel;
    frame->flags = any.flags;
    memcpy(frame->data, any.data, 8);
    frame->timestamp_us = any.timestamp_us;
    return true;
}

bool __stdcall DLL candle_frame_read_any(candle_handle hdev, candle_frame_any_t *frame, uint32_t timeout_ms)
{
    candle_device_t *dev = (candle_device_t *)hdev;
    return candle_rx_fifo_pop(dev, frame, timeout_ms);
}

candle_frametype_t __stdcall DLL candle_frame_type(candle_frame_t *frame)
{
    if (frame->echo_id != 0xFFFFFFFF) {
        return CANDLE_FRAMETYPE_ECHO;
    }
    if (frame->can_id & CANDLE_ID_ERR) {
        return CANDLE_FRAMETYPE_ERROR;
    }
    return CANDLE_FRAMETYPE_RECEIVE;
}

uint32_t __stdcall DLL candle_frame_id(candle_frame_t *frame)
{
    return frame->can_id & 0x1FFFFFFF;
}

bool __stdcall DLL candle_frame_is_extended_id(candle_frame_t *frame)
{
    return (frame->can_id & CANDLE_ID_EXTENDED) != 0;
}

bool __stdcall DLL candle_frame_is_rtr(candle_frame_t *frame)
{
    return (frame->can_id & CANDLE_ID_RTR) != 0;
}

uint8_t __stdcall DLL candle_frame_dlc(candle_frame_t *frame)
{
    return frame->can_dlc;
}

uint8_t* __stdcall DLL candle_frame_data(candle_frame_t *frame)
{
    return frame->data;
}

uint32_t __stdcall DLL candle_frame_timestamp_us(candle_frame_t *frame)
{
    return frame->timestamp_us;
}

candle_frametype_t __stdcall DLL candle_frame_any_type(candle_frame_any_t *frame)
{
    return frame->frame_type;
}

uint32_t __stdcall DLL candle_frame_any_id(candle_frame_any_t *frame)
{
    return frame->can_id & 0x1FFFFFFF;
}

bool __stdcall DLL candle_frame_any_is_extended_id(candle_frame_any_t *frame)
{
    return (frame->can_id & CANDLE_ID_EXTENDED) != 0;
}

bool __stdcall DLL candle_frame_any_is_rtr(candle_frame_any_t *frame)
{
    return (frame->can_id & CANDLE_ID_RTR) != 0;
}

bool __stdcall DLL candle_frame_any_is_fd(candle_frame_any_t *frame)
{
    return (frame->flags & 0x02) != 0;
}

bool __stdcall DLL candle_frame_any_is_brs(candle_frame_any_t *frame)
{
    return (frame->flags & 0x04) != 0;
}

bool __stdcall DLL candle_frame_any_is_esi(candle_frame_any_t *frame)
{
    return (frame->flags & 0x08) != 0;
}

uint8_t __stdcall DLL candle_frame_any_dlc(candle_frame_any_t *frame)
{
    return frame->can_dlc;
}

uint8_t* __stdcall DLL candle_frame_any_data(candle_frame_any_t *frame)
{
    return frame->data;
}

uint32_t __stdcall DLL candle_frame_any_timestamp_us(candle_frame_any_t *frame)
{
    return frame->timestamp_us;
}
