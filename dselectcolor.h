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
#ifndef __DSELECTCOLOR_H__
#define __DSELECTCOLOR_H__

#include <QDialog>
#include "ui_selectcolor.h"

class BrickLink::Color;

class DSelectColor : public QDialog, private Ui::SelectColor {
    Q_OBJECT

public:
    DSelectColor(QWidget *parent = 0, Qt::WindowFlags f = 0);

    void setColor(const BrickLink::Color *);
    const BrickLink::Color *color() const;

    virtual int exec(const QRect &pos = QRect());

protected:
    virtual void showEvent(QShowEvent *);

private slots:
    void checkColor(const BrickLink::Color *, bool);
};

#endif