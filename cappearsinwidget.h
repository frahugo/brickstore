/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __CAPPEARSINWIDGET_H__
#define __CAPPEARSINWIDGET_H__

#include <QTreeView>
#include "bricklink.h"


class CAppearsInWidgetPrivate;
class QAction;

class CAppearsInWidget : public QTreeView {
    Q_OBJECT

public:
    CAppearsInWidget(QWidget *parent = 0);
    virtual ~CAppearsInWidget();

    void setItem(const BrickLink::Item *item, const BrickLink::Color *color = 0);

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;

protected slots:
    void viewLargeImage();
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();

private slots:
    void showContextMenu(const QPoint &);
    void partOut();
    void resizeColumns();

private:
    const BrickLink::Item::AppearsInItem *appearsIn() const;

private:
    CAppearsInWidgetPrivate *d;
};

#endif
