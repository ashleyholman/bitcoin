// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peertablemodel.h"

#include "clientmodel.h"

#include "net.h"
#include "sync.h"

#include <QDebug>
#include <QList>
#include <QTimer>

bool NodeLessThan::operator()(const CNodeStats &left, const CNodeStats &right) const
{
    const CNodeStats *pLeft = &left;
    const CNodeStats *pRight = &right;

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case PeerTableModel::Address:
        return pLeft->addrName.compare(pRight->addrName) < 0;
    case PeerTableModel::Subversion:
        return pLeft->cleanSubVer.compare(pRight->cleanSubVer) < 0;
    case PeerTableModel::Ping:
        return pLeft->dPingTime < pRight->dPingTime;
    }

    return false;
}

// private implementation
class PeerTablePriv
{
public:
    /** Local cache of peer information */
    QList<CNodeStats> cachedNodeStats;
    /** Column to sort nodes by */
    int sortColumn;
    /** Order (ascending or descending) to sort nodes by */
    Qt::SortOrder sortOrder;
    /** Index of rows by node ID */
    std::map<NodeId, int> mapNodeRows;

    /** Pull a full list of peers from vNodes into our cache */
    void refreshPeers() {
        TRY_LOCK(cs_vNodes, lockNodes);
        {
            if (!lockNodes)
            {
                // skip the refresh if we can't immediately get the lock
                return;
            }
            cachedNodeStats.clear();
#if QT_VERSION >= 0x040700
            cachedNodeStats.reserve(vNodes.size());
#endif
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                CNodeStats stats;
                pnode->copyStats(stats);
                cachedNodeStats.append(stats);
            }
        }

        if (sortColumn >= 0)
            // sort cacheNodeStats (use stable sort to prevent rows jumping around unneceesarily)
            qStableSort(cachedNodeStats.begin(), cachedNodeStats.end(), NodeLessThan(sortColumn, sortOrder));

        // build index map
        mapNodeRows.clear();
        int row = 0;
        BOOST_FOREACH(CNodeStats &stats, cachedNodeStats)
        {
            mapNodeRows.insert(std::pair<NodeId, int>(stats.nodeid, row++));
        }
    }

    int size()
    {
        return cachedNodeStats.size();
    }

    CNodeStats *index(int idx)
    {
        if(idx >= 0 && idx < cachedNodeStats.size()) {
            return &cachedNodeStats[idx];
        }
        else
        {
            return 0;
        }
    }

};

PeerTableModel::PeerTableModel(ClientModel *parent) :
    QAbstractTableModel(parent),clientModel(parent),timer(0)
{
    columns << tr("Address") << tr("Subversion") << tr("Ping (secs)");
    priv = new PeerTablePriv();
    // default to unsorted
    priv->sortColumn = -1;

    // set up timer for auto refresh
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), SLOT(refresh()));

    // load initial data
    refresh();
}

void PeerTableModel::startAutoRefresh(int msecs)
{
    timer->setInterval(1000);
    timer->start();
}

void PeerTableModel::stopAutoRefresh()
{
    timer->stop();
}

int PeerTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int PeerTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
}

QVariant PeerTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    CNodeStats *rec = static_cast<CNodeStats*>(index.internalPointer());

    if(role == Qt::DisplayRole)
    {
        switch(index.column())
        {
        case Address:
            return QVariant(rec->addrName.c_str());
        case Subversion:
            return QVariant(rec->cleanSubVer.c_str());
        case Ping:
            return QString::number(rec->dPingTime, 'f', 3);
        }
    }
    return QVariant();
}

QVariant PeerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags PeerTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex PeerTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    CNodeStats *data = priv->index(row);

    if (data)
    {
        return createIndex(row, column, data);
    }
    else
    {
        return QModelIndex();
    }
}

const CNodeStats *PeerTableModel::getNodeStats(int idx) {
    return priv->index(idx);
}

void PeerTableModel::refresh()
{
    emit layoutAboutToBeChanged();
    priv->refreshPeers();
    emit layoutChanged();
}

int PeerTableModel::getRowByNodeId(NodeId nodeid)
{
    std::map<NodeId, int>::iterator it = priv->mapNodeRows.find(nodeid);
    if (it == priv->mapNodeRows.end())
        return -1;

    return it->second;
}

void PeerTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}
