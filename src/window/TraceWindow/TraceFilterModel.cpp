#include "TraceFilterModel.h"
#include "BaseTraceViewModel.h"

#include <QRegularExpression>
#include <QSortFilterProxyModel>

#include "core/BusMessage.h"

TraceFilterModel::TraceFilterModel(QObject *parent)
    : QSortFilterProxyModel{parent},
    _filterText("")
{
   setRecursiveFilteringEnabled(false);
   setDynamicSortFilter(false);
}


void TraceFilterModel::setFilterText(QString filtertext)
{
    _filterText = filtertext;
    QRegularExpression re(filtertext, QRegularExpression::CaseInsensitiveOption);
    _regexValid = re.isValid();
    if (_regexValid)
    {
        _cachedRegex = re;
    }
}

void TraceFilterModel::setShowTx(bool show)
{
    _showTx = show;
}

void TraceFilterModel::setShowRx(bool show)
{
    _showRx = show;
}

void TraceFilterModel::setHiddenMessageIds(const QSet<uint32_t> &ids)
{
    _hiddenMessageIds = ids;
}

void TraceFilterModel::setHiddenInterfaces(const QSet<CanInterfaceId> &ids)
{
    _hiddenInterfaces = ids;
}

bool TraceFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    // Get the underlying BaseTraceViewModel to access the CanMessage directly
    auto *baseModel = qobject_cast<BaseTraceViewModel *>(sourceModel());
    QSortFilterProxyModel *proxySource = nullptr;
    if (!baseModel)
    {
        proxySource = qobject_cast<QSortFilterProxyModel *>(sourceModel());
        if (proxySource)
        {
            baseModel = qobject_cast<BaseTraceViewModel *>(proxySource->sourceModel());
        }
    }

    if (baseModel)
    {
        QModelIndex baseIdx;
        if (proxySource)
        {
            QModelIndex proxyIdx = proxySource->index(source_row, 0, source_parent);
            baseIdx = proxySource->mapToSource(proxyIdx);
        }
        else
        {
            baseIdx = sourceModel()->index(source_row, 0, source_parent);
        }
        CanMessage msg = baseModel->getMessage(baseIdx);

        if (!_showTx && !msg.isRX())
        {
            return false;
        }
        if (!_showRx && msg.isRX())
        {
            return false;
        }
        if (_hiddenMessageIds.contains(msg.getId()))
        {
            return false;
        }
        if (_hiddenInterfaces.contains(msg.getInterfaceId()))
        {
            return false;
        }
    }

    // Text/regex filter
    if (_filterText.length() == 0)
    {
        return true;
    }

    static const int columns[] = {
        BaseTraceViewModel::column_canid,
        BaseTraceViewModel::column_name,
        BaseTraceViewModel::column_channel,
        BaseTraceViewModel::column_sender,
        BaseTraceViewModel::column_type,
    };

    for (int col : columns)
    {
        QModelIndex idx = sourceModel()->index(source_row, col, source_parent);
        QString datastr = sourceModel()->data(idx).toString();

        if (_regexValid)
        {
            if (datastr.contains(_cachedRegex))
            {
                return true;
            }
        }
        else
        {
            if (datastr.contains(_filterText, Qt::CaseInsensitive))
            {
                return true;
            }
        }
    }

    return false;
}
