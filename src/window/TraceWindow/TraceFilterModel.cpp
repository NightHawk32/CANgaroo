#include "TraceFilterModel.h"
#include "BaseTraceViewModel.h"
#include <QRegularExpression>

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
    if (_regexValid) {
        _cachedRegex = re;
    }
}

bool TraceFilterModel::filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
{
    // Pass all on no filter
    if(_filterText.length() == 0)
        return true;

    // Check columns that are likely to match first (ID, Name) before others
    static const int columns[] = {
        BaseTraceViewModel::column_canid,
        BaseTraceViewModel::column_name,
        BaseTraceViewModel::column_channel,
        BaseTraceViewModel::column_sender,
        BaseTraceViewModel::column_type,
    };

    for (int col : columns) {
        QModelIndex idx = sourceModel()->index(source_row, col, source_parent);
        QString datastr = sourceModel()->data(idx).toString();

        if (_regexValid) {
            if (datastr.contains(_cachedRegex))
                return true;
        } else {
            if (datastr.contains(_filterText, Qt::CaseInsensitive))
                return true;
        }
    }

    return false;
}
