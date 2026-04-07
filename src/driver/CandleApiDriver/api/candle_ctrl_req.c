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

#include "candle_ctrl_req.h"
#include "ch_9.h"

static bool usb_control_msg(WINUSB_INTERFACE_HANDLE hnd, uint8_t request, uint8_t requesttype, uint16_t value, uint16_t index, void *data, uint16_t size)
{
    WINUSB_SETUP_PACKET packet;
    memset(&packet, 0, sizeof(packet));

    packet.Request = request;
    packet.RequestType = requesttype;
    packet.Value = value;
    packet.Index = index;
    packet.Length = size;

    unsigned long bytes_sent = 0;
    return WinUsb_ControlTransfer(hnd, packet, (uint8_t*)data, size, &bytes_sent, 0);
}

static bool candle_ctrl_vendor_out(candle_device_t *dev, uint8_t request, uint16_t value, uint16_t index, void *data, uint16_t size)
{
    return usb_control_msg(
        dev->winUSBHandle,
        request,
        USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
        value,
        index,
        data,
        size
    );
}

static bool candle_ctrl_vendor_in(candle_device_t *dev, uint8_t request, uint16_t value, uint16_t index, void *data, uint16_t size)
{
    return usb_control_msg(
        dev->winUSBHandle,
        request,
        USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
        value,
        index,
        data,
        size
    );
}

bool candle_ctrl_set_host_format(candle_device_t *dev)
{
    candle_host_config_t hconf;
    hconf.byte_order = 0x0000beef;

    bool rc = candle_ctrl_vendor_out(
        dev,
        CANDLE_USB_REQ_HOST_FORMAT,
        1,
        dev->interfaceNumber,
        &hconf,
        sizeof(hconf)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_SET_HOST_FORMAT;
    return rc;
}

bool candle_ctrl_set_device_mode(candle_device_t *dev, uint8_t channel, uint32_t mode, uint32_t flags)
{
    candle_device_mode_t dm;
    dm.mode = mode;
    dm.flags = flags;

    bool rc = candle_ctrl_vendor_out(
        dev,
        CANDLE_USB_REQ_MODE,
        channel,
        dev->interfaceNumber,
        &dm,
        sizeof(dm)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_SET_DEVICE_MODE;
    return rc;
}

bool candle_ctrl_get_config(candle_device_t *dev, candle_device_config_t *dconf)
{
    bool rc = candle_ctrl_vendor_in(
        dev,
        CANDLE_USB_REQ_DEVICE_CONFIG,
        1,
        dev->interfaceNumber,
        dconf,
        sizeof(*dconf)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_GET_DEVICE_INFO;
    return rc;
}

bool candle_ctrl_get_timestamp(candle_device_t *dev, uint32_t *current_timestamp)
{
    bool rc = candle_ctrl_vendor_in(
        dev,
        CANDLE_USB_REQ_TIMESTAMP_GET,
        1,
        dev->interfaceNumber,
        current_timestamp,
        sizeof(*current_timestamp)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_GET_TIMESTAMP;
    return rc;
}

bool candle_ctrl_get_capability(candle_device_t *dev, uint8_t channel, candle_capability_t *data)
{
    bool rc = candle_ctrl_vendor_in(
        dev,
        CANDLE_USB_REQ_BT_CONST,
        channel,
        0,
        data,
        sizeof(*data)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_GET_BITTIMING_CONST;
    return rc;
}

bool candle_ctrl_get_capability_fd(candle_device_t *dev, uint8_t channel, candle_capability_fd_t *data)
{
    bool rc = candle_ctrl_vendor_in(
        dev,
        CANDLE_USB_REQ_BT_CONST_FD,
        channel,
        dev->interfaceNumber,
        data,
        sizeof(*data)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_GET_BITTIMING_CONST_FD;
    return rc;
}

bool candle_ctrl_set_bittiming(candle_device_t *dev, uint8_t channel, candle_bittiming_t *data)
{
    bool rc = candle_ctrl_vendor_out(
        dev,
        CANDLE_USB_REQ_BITTIMING,
        channel,
        0,
        data,
        sizeof(*data)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_SET_BITTIMING;
    return rc;
}

bool candle_ctrl_set_bittiming_fd(candle_device_t *dev, uint8_t channel, candle_bittiming_t *data)
{
    bool rc = candle_ctrl_vendor_out(
        dev,
        CANDLE_USB_REQ_BITTIMING_FD,
        channel,
        dev->interfaceNumber,
        data,
        sizeof(*data)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_SET_BITTIMING_FD;
    return rc;
}

bool candle_ctrl_set_termination(candle_device_t *dev, uint8_t channel, bool enabled)
{
    uint32_t mode = enabled ? 1u : 0u;
    bool rc = candle_ctrl_vendor_out(
        dev,
        CANDLE_USB_REQ_TERMINATION_SET,
        channel,
        dev->interfaceNumber,
        &mode,
        sizeof(mode)
    );

    if (!rc) {
        dev->last_error = CANDLE_ERR_SET_DEVICE_MODE;
    }
    return rc;
}

bool candle_ctrl_set_busload_report(candle_device_t *dev, uint8_t channel, uint8_t interval)
{
    bool rc = candle_ctrl_vendor_out(
        dev,
        CANDLE_USB_REQ_SET_BUSLOAD_REPORT,
        channel,
        dev->interfaceNumber,
        &interval,
        sizeof(interval)
    );

    if (!rc) {
        dev->last_error = CANDLE_ERR_SET_DEVICE_MODE;
    }
    return rc;
}

bool candle_ctrl_get_feedback(candle_device_t *dev, uint16_t value, candle_feedback_t *feedback)
{
    bool rc = candle_ctrl_vendor_in(
        dev,
        CANDLE_USB_REQ_GET_LAST_ERROR,
        value,
        dev->interfaceNumber,
        feedback,
        sizeof(*feedback)
    );

    dev->last_error = rc ? CANDLE_ERR_OK : CANDLE_ERR_GET_DEVICE_INFO;
    return rc;
}
