#ifndef TRACEFILTER_H
#define TRACEFILTER_H

#include <QSortFilterProxyModel>
#include <QRegularExpression>

class TraceFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TraceFilterModel(QObject *parent = nullptr);

public slots:
    void setFilterText(QString filtertext);

private:
    QString _filterText;
    QRegularExpression _cachedRegex;
    bool _regexValid = false;
protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override;
};

#endif // TRACEFILTER_H
