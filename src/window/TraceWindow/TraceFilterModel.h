#ifndef TRACEFILTER_H
#define TRACEFILTER_H

#include <QSet>
#include <QSortFilterProxyModel>
#include <QRegularExpression>

#include <driver/CanDriver.h>

class TraceFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TraceFilterModel(QObject *parent = nullptr);

    void setShowTx(bool show);
    void setShowRx(bool show);
    void setHiddenMessageIds(const QSet<uint32_t> &ids);
    void setHiddenInterfaces(const QSet<CanInterfaceId> &ids);

public slots:
    void setFilterText(QString filtertext);

private:
    QString _filterText;
    QRegularExpression _cachedRegex;
    bool _regexValid = false;

    bool _showTx = true;
    bool _showRx = true;
    QSet<uint32_t> _hiddenMessageIds;
    QSet<CanInterfaceId> _hiddenInterfaces;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

#endif // TRACEFILTER_H
