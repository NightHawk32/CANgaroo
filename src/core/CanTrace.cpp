/*

  Copyright (c) 2015, 2016 Hubert Denkmair <hubert@denkmair.de>

  This file is part of cangaroo.

  cangaroo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  cangaroo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with cangaroo.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "CanTrace.h"
#include <QMutexLocker>
#include <QFile>
#include <QTextStream>
#include <QDataStream>

#include <core/Backend.h>
#include <core/CanMessage.h>
#include <core/CanDbMessage.h>
#include <core/CanDbSignal.h>
#include <driver/CanInterface.h>

CanTrace::CanTrace(Backend &backend, QObject *parent, int flushInterval)
  : QObject(parent),
    _backend(backend),
    _maxSize(50000),
    _isTimerRunning(false),
    _mutex(),
    _timerMutex(),
    _flushTimer(this)
{
    clear();
    _flushTimer.setSingleShot(true);
    _flushTimer.setInterval(flushInterval);
    connect(&_flushTimer, &QTimer::timeout, this, &CanTrace::flushQueue);
}

unsigned long CanTrace::size()
{
    QMutexLocker locker(&_mutex);
    return _dataRowsUsed;
}

void CanTrace::clear()
{
    QMutexLocker locker(&_mutex);
    emit beforeClear();
    _data.resize(pool_chunk_size);
    _dataRowsUsed = 0;
    _newRows = 0;
    emit afterClear();
}

CanMessage CanTrace::getMessage(int idx)
{
    QMutexLocker locker(&_mutex);
    if (idx >= (_dataRowsUsed + _newRows)) {
        return CanMessage();
    } else {
        return _data[idx];
    }
}

void CanTrace::enqueueMessage(const CanMessage &msg, bool more_to_follow)
{
    QMutexLocker locker(&_mutex);

    int idx = size() + _newRows;
    if (idx>=_data.size()) {
        _data.resize(_data.size() + pool_chunk_size);
    }

    _data[idx].cloneFrom(msg);
    _newRows++;

    if (!more_to_follow) {
        startTimer();
    }

    emit messageEnqueued(idx);
}

void CanTrace::flushQueue()
{
    {
        QMutexLocker locker(&_timerMutex);
        _isTimerRunning = false;
    }

    QMutexLocker locker(&_mutex);
    if (_newRows) {
        emit beforeAppend(_newRows);

        // see if we have muxed messages. cache muxed values, if any.
        MeasurementSetup &setup = _backend.getSetup();
        for (int i=_dataRowsUsed; i<_dataRowsUsed + _newRows; i++) {
            CanMessage &msg = _data[i];
            CanDbMessage *dbmsg = setup.findDbMessage(msg);
            if (dbmsg && dbmsg->getMuxer()) {
                for (auto *signal : dbmsg->getSignals()) {
                    if (signal->isMuxed() && signal->isPresentInMessage(msg)) {
                        _muxCache[signal] = signal->extractRawDataFromMessage(msg);
                    }
                }
            }
        }

        _dataRowsUsed += _newRows;
        _newRows = 0;
        emit afterAppend();

        // Hard limit check - prune if we exceed maxSize
        if (_dataRowsUsed > _maxSize) {
            int toRemove = _maxSize / 10; // Remove 10% when limit hit
            if (toRemove > 0) {
                emit beforeRemove(toRemove);
                _data.remove(0, toRemove);
                _dataRowsUsed -= toRemove;
                emit afterRemove(toRemove);
            }
        }
    }
}

void CanTrace::setMaxSize(int maxSize)
{
    QMutexLocker locker(&_mutex);
    _maxSize = maxSize;
}

void CanTrace::startTimer()
{
    QMutexLocker locker(&_timerMutex);
    if (!_isTimerRunning) {
        _isTimerRunning = true;
        QMetaObject::invokeMethod(&_flushTimer, "start", Qt::QueuedConnection);
    }
}

void CanTrace::saveCanDump(QFile &file)
{
    QMutexLocker locker(&_mutex);
    QTextStream stream(&file);
    for (unsigned int i=0; i<size(); i++) {
        CanMessage *msg = &_data[i];
        QString line;
        line.append(QString().asprintf("(%.6f) ", msg->getFloatTimestamp()));
        line.append(_backend.getInterfaceName(msg->getInterfaceId()));
        if (msg->isErrorFrame()) {
            // Error frame: error flag in ID, 8 bytes of zero data
            line.append(QString().asprintf(" %08X#", 0x20000000 | msg->getId()));
            line.append("0000000000000000");
        } else if (msg->isFD()) {
            // CANFD: use ## separator with flags byte (bit 0 = BRS, bit 1 = ESI)
            uint8_t flags = msg->isBRS() ? 1 : 0;
            if (msg->isExtended()) {
                line.append(QString().asprintf(" %08X##%X", msg->getId(), flags));
            } else {
                line.append(QString().asprintf(" %03X##%X", msg->getId(), flags));
            }
            for (int i=0; i<msg->getLength(); i++) {
                line.append(QString().asprintf("%02X", msg->getByte(i)));
            }
        } else if (msg->isRTR()) {
            // RTR: #R followed by DLC
            if (msg->isExtended()) {
                line.append(QString().asprintf(" %08X#R%d", msg->getId(), msg->getLength()));
            } else {
                line.append(QString().asprintf(" %03X#R%d", msg->getId(), msg->getLength()));
            }
        } else {
            if (msg->isExtended()) {
                line.append(QString().asprintf(" %08X#", msg->getId()));
            } else {
                line.append(QString().asprintf(" %03X#", msg->getId()));
            }
            for (int i=0; i<msg->getLength(); i++) {
                line.append(QString().asprintf("%02X", msg->getByte(i)));
            }
        }
        stream << line << Qt::endl;
    }
}

void CanTrace::saveVectorAsc(QFile &file)
{
    QMutexLocker locker(&_mutex);
    QTextStream stream(&file);

    if (_data.length()<1) {
        return;
    }


    auto firstMessage = _data.first();
    double t_start = firstMessage.getFloatTimestamp();

    QLocale locale_c(QLocale::C);
    QString dt_start = locale_c.toString(firstMessage.getDateTime(), "ddd MMM dd hh:mm:ss.zzz ap yyyy");

    stream << "date " << dt_start << Qt::endl;
    stream << "base hex  timestamps absolute" << Qt::endl;
    stream << "internal events logged" << Qt::endl;
    stream << "// version 8.5.0" << Qt::endl;
    stream << "Begin Triggerblock " << dt_start << Qt::endl;
    stream << "   0.000000 Start of measurement" << Qt::endl;

    // Build sequential channel numbers from interface IDs
    QMap<CanInterfaceId, int> channelMap;
    int nextChannel = 1;
    for (unsigned int i = 0; i < size(); i++) {
        CanInterfaceId ifaceId = _data[i].getInterfaceId();
        if (!channelMap.contains(ifaceId)) {
            channelMap[ifaceId] = nextChannel++;
        }
    }

    for (unsigned int i=0; i<size(); i++) {
        CanMessage &msg = _data[i];

        double t_current = msg.getFloatTimestamp();
        int channel = channelMap.value(msg.getInterfaceId(), 1);

        if (msg.isErrorFrame()) {
            QString line = QString().asprintf(
                "%11.6lf %d  ErrorFrame",
                t_current-t_start,
                channel
            );
            stream << line << Qt::endl;
            continue;
        }

        QString id_hex_str = QString().asprintf("%x", msg.getId());
        QString id_dec_str = QString().asprintf("%d", msg.getId());
        if (msg.isExtended()) {
            id_hex_str.append("x");
            id_dec_str.append("x");
        }

        QString line;
        if (msg.isFD()) {
            // Vector ASC CANFD format:
            // timestamp CANFD channel Rx/Tx ID_hex flags 0 0 DLC DataLength data
            // flags: bit 0 = BRS, bit 2 = ESI
            int flags = 0;
            if (msg.isBRS()) { flags |= 0x1; }

            line = QString().asprintf(
                "%11.6lf CANFD %3d %s %15s %d 0 0 %d %d %s",
                t_current-t_start,
                channel,
                msg.isRX() ? "Rx" : "Tx",
                id_hex_str.toStdString().c_str(),
                flags,
                msg.getLength(),
                msg.getLength(),
                msg.getDataHexString().toStdString().c_str()
            );
        } else {
            line = QString().asprintf(
                "%11.6lf %d  %-15s %s   %c %d %s  Length = %d BitCount = %d ID = %s",
                t_current-t_start,
                channel,
                id_hex_str.toStdString().c_str(),
                msg.isRX() ? "Rx" : "Tx",
                msg.isRTR() ? 'r' : 'd',
                msg.getLength(),
                msg.getDataHexString().toStdString().c_str(),
                0, // TODO Length (transfer time in ns)
                0, // TODO BitCount (overall frame length, including stuff bits)
                id_dec_str.toStdString().c_str()
            );
        }

        stream << line << Qt::endl;
    }

    stream << "End TriggerBlock" << Qt::endl;
}

void CanTrace::saveVectorMdf(QFile &file)
{
    QMutexLocker locker(&_mutex);
    if (_dataRowsUsed == 0) return;

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::DoublePrecision);

    // Record layout per CAN frame:
    //   timestamp (float64, 8) + CAN_ID (uint32, 4) + DLC (uint8, 1)
    //   + Dir (uint8, 1) + DataBytes (8 bytes) = 22 bytes
    const quint32 recordSize = 22;
    const quint64 recordCount = _dataRowsUsed;

    // --- Helper: TXBLOCK size (24-byte header + text + NUL, 8-byte aligned) ---
    auto txSize = [](const QByteArray &t) -> quint64 {
        return ((24 + t.size() + 1) + 7) & ~7ULL;
    };

    // --- Pre-compute all block sizes ---
    const quint64 szId = 64;
    const quint64 szHd = 88;   // 24 + 6*8 + 16
    const quint64 szFh = 56;   // 24 + 2*8 + 16
    const quint64 szDg = 64;   // 24 + 4*8 + 8
    const quint64 szCg = 104;  // 24 + 6*8 + 32
    const quint64 szCn = 160;  // 24 + 8*8 + 72

    QByteArray txFhStr   = "CANgaroo MDF4 export";
    QByteArray txCgStr   = "CAN";
    QByteArray txTime    = "t";
    QByteArray txTimeSec = "s";
    QByteArray txId      = "CAN_ID";
    QByteArray txDlc     = "DLC";
    QByteArray txDir     = "Dir";
    QByteArray txData    = "DataBytes";

    // --- Pre-compute all block offsets ---
    quint64 off = 0;
    quint64 oId         = off; off += szId;
    quint64 oHd         = off; off += szHd;
    quint64 oFh         = off; off += szFh;
    quint64 oTxFh       = off; off += txSize(txFhStr);
    quint64 oDg         = off; off += szDg;
    quint64 oCg         = off; off += szCg;
    quint64 oTxCg       = off; off += txSize(txCgStr);
    quint64 oCnTime     = off; off += szCn;
    quint64 oTxTime     = off; off += txSize(txTime);
    quint64 oTxTimeSec  = off; off += txSize(txTimeSec);
    quint64 oCnId       = off; off += szCn;
    quint64 oTxId       = off; off += txSize(txId);
    quint64 oCnDlc      = off; off += szCn;
    quint64 oTxDlc      = off; off += txSize(txDlc);
    quint64 oCnDir      = off; off += szCn;
    quint64 oTxDir      = off; off += txSize(txDir);
    quint64 oCnData     = off; off += szCn;
    quint64 oTxData     = off; off += txSize(txData);
    quint64 oDt         = off;

    (void)oId; // suppress unused

    // --- Helpers ---
    auto writeBlockHeader = [&ds](const char *id, quint64 length, quint64 linkCount) {
        ds.writeRawData(id, 4);
        ds << (quint32)0;
        ds << length;
        ds << linkCount;
    };

    auto writePad = [&ds](int bytes) {
        for (int i = 0; i < bytes; i++) ds << (quint8)0;
    };

    auto writeTx = [&](const QByteArray &text) {
        quint64 sz = txSize(text);
        writeBlockHeader("##TX", sz, 0);
        ds.writeRawData(text.constData(), text.size());
        ds << (quint8)0;
        int pad = sz - 24 - text.size() - 1;
        writePad(pad);
    };

    auto writeCn = [&](quint64 nextCn, quint64 nameOff, quint64 unitOff,
                       quint8 cnType, quint8 syncType, quint8 dataType,
                       quint32 byteOff, quint32 bitCount) {
        writeBlockHeader("##CN", szCn, 8);
        ds << nextCn;          // cn_cn_next
        ds << (quint64)0;     // cn_composition
        ds << nameOff;         // cn_tx_name
        ds << (quint64)0;     // cn_si_source
        ds << (quint64)0;     // cn_cc_conversion
        ds << (quint64)0;     // cn_data
        ds << unitOff;         // cn_md_unit
        ds << (quint64)0;     // cn_md_comment
        // Data section (72 bytes)
        ds << cnType;          // cn_type
        ds << syncType;        // cn_sync_type
        ds << dataType;        // cn_data_type
        ds << (quint8)0;       // cn_bit_offset
        ds << byteOff;         // cn_byte_offset
        ds << bitCount;        // cn_bit_count
        ds << (quint32)0;      // cn_flags
        ds << (quint32)0;      // cn_inval_bit_pos
        ds << (quint8)0;       // cn_precision
        ds << (quint8)0;       // reserved
        ds << (quint16)0;      // cn_attachment_count
        double zero = 0.0;
        for (int i = 0; i < 6; i++) ds << zero; // range/limit fields
    };

    // ===== IDBLOCK (64 bytes, no standard block header) =====
    ds.writeRawData("MDF     ", 8);
    ds.writeRawData("4.10    ", 8);
    ds.writeRawData("CANgaroo", 8);
    ds << (quint32)0;          // reserved
    ds << (quint16)410;        // version number
    ds << (quint16)0;          // reserved
    ds << (quint16)0;          // unfinalized flags
    ds << (quint16)0;          // custom unfin flags
    writePad(28);

    // ===== HDBLOCK (88 bytes) =====
    quint64 startTimeNs = static_cast<quint64>(_data[0].getTimestamp_us()) * 1000ULL;
    writeBlockHeader("##HD", szHd, 6);
    ds << oDg;                 // hd_dg_first
    ds << oFh;                 // hd_fh_first
    ds << (quint64)0;         // hd_ch_first
    ds << (quint64)0;         // hd_at_first
    ds << (quint64)0;         // hd_ev_first
    ds << (quint64)0;         // hd_md_comment
    ds << startTimeNs;         // start_time_ns
    ds << (qint16)0;           // tz_offset_min
    ds << (qint16)0;           // dst_offset_min
    ds << (quint8)2;           // time_flags (local time)
    ds << (quint8)0;           // time_class
    ds << (quint8)0;           // flags
    ds << (quint8)0;           // reserved

    // ===== FHBLOCK (56 bytes) =====
    writeBlockHeader("##FH", szFh, 2);
    ds << (quint64)0;         // fh_fh_next
    ds << oTxFh;               // fh_md_comment
    ds << startTimeNs;         // time_ns
    ds << (qint16)0;           // tz_offset_min
    ds << (qint16)0;           // dst_offset_min
    ds << (quint8)2;           // time_flags
    writePad(3);               // reserved

    // ===== TX: FH comment =====
    writeTx(txFhStr);

    // ===== DGBLOCK (64 bytes) =====
    writeBlockHeader("##DG", szDg, 4);
    ds << (quint64)0;         // dg_dg_next
    ds << oCg;                 // dg_cg_first
    ds << oDt;                 // dg_data
    ds << (quint64)0;         // dg_md_comment
    ds << (quint8)0;           // rec_id_size
    writePad(7);               // reserved

    // ===== CGBLOCK (104 bytes) =====
    writeBlockHeader("##CG", szCg, 6);
    ds << (quint64)0;         // cg_cg_next
    ds << oCnTime;             // cg_cn_first
    ds << oTxCg;               // cg_tx_acq_name
    ds << (quint64)0;         // cg_si_acq_source
    ds << (quint64)0;         // cg_sr_first
    ds << (quint64)0;         // cg_md_comment
    ds << (quint64)0;         // record_id
    ds << recordCount;         // cycle_count
    ds << (quint16)0;          // flags
    ds << (quint16)0;          // path_separator
    ds << (quint32)0;          // reserved
    ds << recordSize;          // data_bytes
    ds << (quint32)0;          // inval_bytes

    // ===== TX: CG name =====
    writeTx(txCgStr);

    // ===== CN channels (chained via cn_cn_next) =====
    // cn_type: 0=fixed, 2=master; cn_sync_type: 1=time; cn_data_type: 0=uint_le, 4=real_le, 10=byte_array
    writeCn(oCnId,  oTxTime,    oTxTimeSec, 2, 1, 4,  0,  64); // timestamp
    writeTx(txTime);
    writeTx(txTimeSec);

    writeCn(oCnDlc, oTxId,      0,          0, 0, 0,  8,  32); // CAN_ID
    writeTx(txId);

    writeCn(oCnDir, oTxDlc,     0,          0, 0, 0,  12,  8); // DLC
    writeTx(txDlc);

    writeCn(oCnData, oTxDir,    0,          0, 0, 0,  13,  8); // Dir
    writeTx(txDir);

    writeCn(0,       oTxData,   0,          0, 0, 10, 14, 64); // DataBytes
    writeTx(txData);

    // ===== DTBLOCK =====
    writeBlockHeader("##DT", 24 + recordCount * recordSize, 0);

    double t_start = _data[0].getFloatTimestamp();
    for (int i = 0; i < _dataRowsUsed; i++) {
        CanMessage &msg = _data[i];
        ds << (msg.getFloatTimestamp() - t_start); // 8 bytes
        ds << msg.getRawId();                       // 4 bytes
        ds << msg.getLength();                      // 1 byte
        ds << static_cast<quint8>(msg.isRX() ? 0 : 1); // 1 byte
        for (int j = 0; j < 8; j++) {              // 8 bytes
            ds << msg.getByte(j);
        }
    }
}

bool CanTrace::getMuxedSignalFromCache(const CanDbSignal *signal, uint64_t *raw_value)
{
    auto it = _muxCache.constFind(signal);
    if (it != _muxCache.constEnd()) {
        *raw_value = it.value();
        return true;
    }
    return false;
}
