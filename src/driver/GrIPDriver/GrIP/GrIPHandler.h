#ifndef GRIPHANDLER_H
#define GRIPHANDLER_H


#include "GrIP.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include "core/CanMessage.h"
#include <QSerialPort>


/**
 * @brief High-level interface to a GrIP-capable device over a serial port.
 *
 * GrIPHandler owns the serial connection and runs a background worker thread
 * that continuously reads incoming bytes, feeds them into the GrIP protocol
 * stack, and dispatches decoded packets. Outgoing frames are serialised through
 * the same thread to avoid concurrent port access.
 *
 * Typical usage:
 * @code
 *   GrIPHandler handler("/dev/ttyUSB0");
 *   handler.Start();
 *   handler.RequestVersion();
 *   handler.CAN_SetBaudrate(0, 500000);
 *   handler.EnableChannel(0, true);
 *
 *   while (handler.CanAvailable(0))
 *       process(handler.ReceiveCan(0));
 *
 *   handler.Stop();
 * @endcode
 *
 * @note All public methods are thread-safe unless noted otherwise.
 */
class GrIPHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the handler for the given serial port.
     * @param name  Platform-specific port name (e.g. "/dev/ttyUSB0", "COM3").
     *              The port is not opened until Start() is called.
     */
    GrIPHandler(const QString &name);

    /** @brief Stops the worker thread and closes the serial port. */
    ~GrIPHandler();

    GrIPHandler(const GrIPHandler &) = delete;
    GrIPHandler &operator=(const GrIPHandler &) = delete;

    /**
     * @brief Opens the serial port and starts the background worker thread.
     * @return true on success (currently always true; port errors are logged).
     */
    bool Start();

    /**
     * @brief Signals the worker thread to stop and blocks until it exits,
     *        then flushes and closes the serial port.
     */
    void Stop();

    /**
     * @brief Sends a SYSTEM_SET_STATUS command to the device.
     * @param open  true  → notify device that the host has opened the session,
     *              false → notify device that the host has closed the session.
     */
    void SetStatus(bool open);

    /**
     * @brief Enables or disables TX echo on the device.
     *
     * When enabled the device echoes every transmitted CAN frame back to the
     * host with the TX flag set, which allows the host to confirm delivery.
     *
     * @param enable  true to enable TX echo, false to disable.
     */
    void SetEchoTx(bool enable);

    /**
     * @brief Sends a SYSTEM_REPORT_INFO request and waits 10 ms for the reply.
     *
     * The device responds asynchronously; the reply is processed by the worker
     * thread and stored via ProcessData(). Call GetVersion() and Channels_CAN()
     * after the reply has been received.
     */
    void RequestVersion();

    /**
     * @brief Returns the firmware version string reported by the device.
     *
     * The string has the form "major.minor-<build-date>" and is populated after
     * RequestVersion() completes. Returns an empty string if not yet received.
     *
     * @return Firmware version string (thread-safe copy).
     */
    std::string GetVersion() const;

    /** @return Number of classic CAN channels reported by the device. */
    int Channels_CAN() const;

    /** @return Number of CAN-FD channels reported by the device. */
    int Channels_CANFD() const;

    /**
     * @brief Enables or disables a CAN channel.
     *
     * Sends the full channel enable/disable state for all channels at once,
     * so toggling one channel does not affect the others.
     *
     * @param ch      Zero-based channel index.
     * @param enable  true to enable, false to disable.
     */
    void EnableChannel(uint8_t ch, bool enable);

    /**
     * @brief Sets the operating mode of a CAN channel.
     * @param ch           Zero-based channel index.
     * @param listen_only  true for listen-only (bus monitoring) mode,
     *                     false for normal read/write mode.
     */
    void Mode(uint8_t ch, bool listen_only);

    /**
     * @brief Configures the nominal bit rate of a CAN channel.
     *
     * The baud rate is sent big-endian over the wire. A 5 ms settling delay is
     * applied after the command to allow the device to reconfigure its hardware.
     *
     * @param ch    Zero-based channel index.
     * @param baud  Desired bit rate in bits/s (e.g. 500000).
     */
    void CAN_SetBaudrate(uint8_t ch, uint32_t baud);

    /**
     * @brief Returns true if at least one received CAN frame is queued for @p ch.
     * @param ch  Zero-based channel index.
     */
    bool CanAvailable(uint8_t ch) const;

    /**
     * @brief Dequeues and returns the oldest received CAN frame on @p ch.
     *
     * Returns a default-constructed CanMessage if the queue is empty or the
     * channel index is out of range.
     *
     * @param ch  Zero-based channel index.
     */
    CanMessage ReceiveCan(uint8_t ch);

    /**
     * @brief Transmits a CAN frame on @p ch.
     * @param ch   Zero-based channel index.
     * @param msg  Frame to transmit.
     * @return true if the frame was handed to the GrIP layer successfully.
     */
    bool CanTransmit(uint8_t ch, const CanMessage &msg);

    /**
     * @brief Low-level send — transmits a pre-built GrIP PDU.
     * @param ProtType    GrIP protocol type.
     * @param MsgType     GrIP message type.
     * @param ReturnCode  Return/status code.
     * @param pdu         Pointer to the PDU descriptor.
     */
    void Send(GrIP_ProtocolType_e ProtType, GrIP_MessageType_e MsgType, GrIP_ReturnType_e ReturnCode, const GrIP_Pdu_t *pdu);

    /**
     * @brief Low-level send — convenience overload that wraps a raw byte buffer.
     * @param ProtType    GrIP protocol type.
     * @param MsgType     GrIP message type.
     * @param ReturnCode  Return/status code.
     * @param data        Pointer to payload bytes.
     * @param len         Payload length in bytes.
     */
    void Send(GrIP_ProtocolType_e ProtType, GrIP_MessageType_e MsgType, GrIP_ReturnType_e ReturnCode, const uint8_t *data, uint16_t len);

private:
    /**
     * @brief Decodes a fully-assembled GrIP packet and dispatches it.
     *
     * Called from the worker thread. Handles MSG_SYSTEM_CMD (firmware info,
     * channel configuration), MSG_DATA / MSG_DATA_NO_RESPONSE (CAN and LIN
     * frames), and MSG_NOTIFICATION (device log messages).
     *
     * @param packet  Decoded packet to process. Passed by reference because
     *                GrIP_Receive() fills it in place.
     */
    void ProcessData(GrIP_Packet_t &packet, qint64 rxTimestamp_ms);

    /**
     * @brief Worker thread entry point.
     *
     * Creates the QSerialPort on this thread (required by Qt), opens the port,
     * then enters a polling loop that:
     *  -# Reads any available bytes and forwards them to GrIP_RxCallback().
     *  -# Writes any bytes queued by GrIP_GetTxData() to the port.
     *  -# Runs up to 512 GrIP_Update() ticks to advance protocol state machines.
     *  -# Dispatches up to 32 fully-decoded packets via ProcessData().
     *
     * Exits when m_Exit is set by Stop().
     */
    void WorkerThread();

    /** @brief Qt slot invoked when the serial port reports an error. */
    void handleSerialError(QSerialPort::SerialPortError error);

    // --- Serial port (owned by the worker thread) ---
    QSerialPort *m_SerialPort = nullptr; ///< Created in WorkerThread(), deleted in destructor.
    QString m_PortName;                  ///< Port name passed to the constructor.
    mutable std::mutex m_MutexSerial;    ///< Guards all access to m_SerialPort and GrIP TX calls.

    // --- Worker thread ---
    std::unique_ptr<std::thread> m_pWorkerThread;
    std::atomic<bool> m_Exit;          ///< Set to true by Stop() to signal the worker loop to exit.

    // Signalled by the worker thread once the serial port open attempt completes
    // (successfully or not), so Start() can return instead of sleeping.
    std::mutex m_MutexReady;
    std::condition_variable m_CvReady;
    bool m_WorkerReady = false;        ///< Predicate for m_CvReady.

    // --- Received frame queues (one per channel) ---
    mutable std::mutex m_MutexData;                    ///< Guards m_ReceiveQueue, m_Version, and channel counts.
    std::vector<std::queue<CanMessage>> m_ReceiveQueue; ///< Per-channel inbound frame queues, populated by ProcessData().

    // --- Device state ---
    std::string m_Version;                ///< Firmware version string, set on SYSTEM_REPORT_INFO reply.
    int m_ChannelsCAN = 0;                ///< Number of classic CAN channels, set on SYSTEM_REPORT_INFO reply.
    int m_ChannelsCANFD = 0;              ///< Number of CAN-FD channels, set on SYSTEM_REPORT_INFO reply.
    std::vector<bool> m_Channel_StatusCAN; ///< Per-channel enabled state, indexed identically to m_ReceiveQueue.
};


#endif // GRIPHANDLER_H
