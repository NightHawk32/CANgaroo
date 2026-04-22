#pragma once

#include "IDecoder.h"
#include <QMap>

class UdsDecoder : public IDecoder {
public:
    UdsDecoder();
    ~UdsDecoder() override = default;

    DecodeStatus tryDecode(const BusMessage& frame, ProtocolMessage& outMsg) override;
    void reset() override;

private:
    struct IsotpSession {
        QVector<BusMessage> frames;
        QByteArray data;
        int expectedSize = 0;
        int nextSn = 1;
        uint32_t rxId = 0;
    };

    QMap<uint32_t, IsotpSession> m_sessions;

    QString interpretService(uint8_t sid);
    QString interpretNrc(uint8_t nrc);
};
