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

#include <QDialog>

#include "bricklinkfwd.h"
#include "ui_importorderdialog.h"


class ImportOrderDialog : public QDialog, private Ui::ImportOrderDialog
{
    Q_OBJECT
public:
    ImportOrderDialog(QWidget *parent = nullptr);

    QVector<QPair<BrickLink::Order *, BrickLink::InvItemList *>> orders() const;

protected:
    virtual void accept();
    virtual void changeEvent(QEvent *e);

protected slots:
    void checkId();
    void checkSelected();
    void activateItem();

    void start();
    void download();

private:
    BrickLink::OrderType orderType() const;

    static bool  s_last_by_number;
    static QDate s_last_from;
    static QDate s_last_to;
    static int   s_last_type;
};
