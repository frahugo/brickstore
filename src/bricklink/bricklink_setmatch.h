/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#include <QBitArray>

#include "bricklink.h"

namespace BrickLink {


class SetMatch : public QObject
{
    Q_OBJECT

private:
    struct InvMatchItem
    {
        const Item *item;
        const Color *color;
        int qty;
    };

    class InvMatchList
    {
    public:
        InvMatchList() = default;
        InvMatchList(const InvItemList &list);
        InvMatchList(const InvMatchList &copy) = default;

        InvMatchList &operator=(const InvMatchList &copy) = default;

        void add(const InvItemList &list);
        //bool subtract(const InvItemList &list);
        bool subtract(const InvMatchList &list);

        int count() const;
        bool isEmpty() const;

        friend class SetMatch;

    private:
        QVector<InvMatchItem> m_list;
        int m_count = 0;
    };


public:
    enum Algorithm {
        Greedy
    };

    SetMatch(QObject *parent = nullptr);

    bool isActive() const;

    bool startAllPossibleSetMatch(const InvItemList &list);
    bool startMaximumPossibleSetMatch(const InvItemList &list, Algorithm a = Greedy);

    void setPartCountConstraint(int _min, int _max);
    void setYearReleasedConstraint(int _min, int _max);
    void setCategoryConstraint(const QVector<const Category *> &list);
    void setItemTypeConstraint(const QVector<const ItemType *> &list);

    enum GreedyPreference {
        PreferLargerSets  = 0,
        PreferSmallerSets = 1,
    };

    void setGreedyPreference(GreedyPreference p);

signals:
    void finished(const QVector<const BrickLink::Item *> &);
    void progress(int, int);

protected:
    QVector<const Item *> allPossibleSetMatch(const InvItemList &list);
    QVector<const Item *> maximumPossibleSetMatch(const InvItemList &list);

    friend class MatchThread;

private:
    QPair<int, QVector<const Item *>> set_match_greedy(InvMatchList &parts);

    void clear_inventory_list();
    void create_inventory_list();

    bool compare_pairs(const QPair<const Item *, InvMatchList> &p1, const QPair<const Item *, InvMatchList> &p2);


private:
    Algorithm m_algorithm = Greedy;

    // constraints
    int m_part_min = -1;
    int m_part_max = -1;
    int m_year_min = -1;
    int m_year_max = -1;
    QVector<const Category *> m_categories;
    QVector<const ItemType *> m_itemtypes;

    // greedy preferences
    GreedyPreference m_prefer = PreferLargerSets;

    // state
    int m_step = 0;
    QVector<QPair<const Item *, InvMatchList>> m_inventories;
    bool m_active = false;
};

} //namespace BrickLink
