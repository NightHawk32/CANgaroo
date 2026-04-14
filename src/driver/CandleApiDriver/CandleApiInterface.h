#ifndef CANDLEAPIINTERFACE_H
#define CANDLEAPIINTERFACE_H

#include "driver/CanInterface.h"
#include "core/MeasurementInterface.h"

#include <QList>
#include <QMutex>

#include <windows.h>

#include "CandleApiTiming.h"
#include "api/candle.h"

class CandleApiDriver;

using namespace std;

class CandleApiInterface : public CanInterface
{
    Q_OBJECT
public:
    CandleApiInterface(CandleApiDriver *driver, candle_handle handle);
    virtual ~CandleApiInterface();

    QString getName() const override;
    QString getDetailsStr() const override;

    void applyConfig(const MeasurementInterface &mi) override;

    unsigned getBitrate() override;

    uint32_t getCapabilities() override;
    QList<CanTiming> getAvailableBitrates() override;

    void open() override;
    bool isOpen() override;
    void close() override;

    void sendMessage(const CanMessage &msg) override;
    bool readMessage(QList<CanMessage> &msglist, unsigned int timeout_ms) override;

    bool updateStatistics() override;
    uint32_t getState() override;
    int getNumRxFrames() override;
    int getNumRxErrors() override;
    int getNumTxFrames() override;
    int getNumTxErrors() override;
    int getNumRxOverruns() override;
    int getNumTxDropped() override;

    wstring getPath() const;

    void update(candle_handle dev);

private:

    uint64_t _hostOffsetStart;
    uint32_t _deviceTicksStart;
    bool _isOpen;
    bool _isFdEnabled;

    candle_handle _handle;
    MeasurementInterface _settings;
    Backend &_backend;

    uint64_t _numRx;
    uint64_t _numTx;
    uint64_t _numTxErr;

    QList<CandleApiTiming> _timings;
    QList<CandleApiTiming> _fdTimings;

    bool setBitTiming(uint32_t bitrate, uint32_t samplePoint);
    bool setDataBitTiming(uint32_t bitrate, uint32_t samplePoint);

    QMutex _txMutex;
    QList<CanMessage> _txMsgList;
};

#endif // CANDLEAPIINTERFACE_H
