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
#ifndef __CSELECTCOLOR_H__
#define __CSELECTCOLOR_H__

#include <QDialog>

#include "bricklinkfwd.h"

class QTreeView;

class CSelectColor : public QWidget {
    Q_OBJECT
public:
    CSelectColor(QWidget *parent = 0, Qt::WindowFlags f = 0);

    void setWidthToContents(bool b);

    void setCurrentColor(const BrickLink::Color *);
    const BrickLink::Color *currentColor() const;

signals:
    void colorSelected(const BrickLink::Color *, bool);

protected slots:
    void colorChanged();
    void colorConfirmed();

protected:
    virtual void changeEvent(QEvent *);
    virtual void showEvent(QShowEvent *);
    void recalcHighlightPalette();

protected:
    QTreeView *w_colors;

//    friend class DSelectColor;
};

#endif
