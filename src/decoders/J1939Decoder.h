#pragma once

#include "IDecoder.h"
#include <QMap>

class J1939Decoder : public IDecoder {
public:
    J1939Decoder();
    ~J1939Decoder() override = default;

    DecodeStatus tryDecode(const BusMessage& frame, ProtocolMessage& outMsg) override;
    void reset() override;

private:
    struct J1939Session {
        QVector<BusMessage> frames;
        QByteArray data;
        uint32_t pgn = 0;
        int expectedSize = 0;
        int expectedPackets = 0;
        int receivedPackets = 0;
        uint8_t sa = 0;
        uint8_t da = 0;
    };

    // Key is (SA << 8) | DA for CM, or just SA for BAM (since DA=255)
    QMap<uint32_t, J1939Session> m_sessions;

    uint32_t extractPgn(uint32_t id);
};
