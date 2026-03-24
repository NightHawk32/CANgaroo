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

#pragma once

#include <QAbstractItemModel>
#include <QMap>
#include <QList>
#include <QTimer>
#include <sys/time.h>

#include "BaseTraceViewModel.h"
#include <core/CanMessage.h>
#include <driver/CanInterface.h>

#include "AggregatedTraceViewItem.h"


class CanTrace;

class AggregatedTraceViewModel : public BaseTraceViewModel
{
    Q_OBJECT

public:
    using unique_key_t = uint64_t;
    using CanIdMap = QMap<unique_key_t, AggregatedTraceViewItem*>;

public:
    AggregatedTraceViewModel(Backend &backend);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
    virtual QModelIndex parent(const QModelIndex &child) const;
    virtual int rowCount(const QModelIndex &parent) const;

    virtual CanMessage getMessage(const QModelIndex &index) const override;

private:
    CanIdMap _map;
    AggregatedTraceViewItem *_rootItem;
    QTimer _fadeTimer;
    qint64 _fadeNowMs = 0;
    QList<CanMessage> _pendingMessageUpdates;
    QMap<unique_key_t, CanMessage> _pendingMessageInserts;

    unique_key_t makeUniqueKey(const CanMessage &msg) const;
    void createItem(const CanMessage &msg, AggregatedTraceViewItem *item, unique_key_t key);
protected:
    virtual QVariant data_DisplayRole(const QModelIndex &index, int role) const;
    virtual QVariant data_TextColorRole(const QModelIndex &index, int role) const;

private slots:
    void createItem(const CanMessage &msg);
    void updateItem(const CanMessage &msg);
    void onUpdateModel();
    void onSetupChanged();

    void beforeAppend(int num_messages);
    void beforeClear();
    void afterClear();
};
