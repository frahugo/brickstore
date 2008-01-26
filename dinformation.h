/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __DINFO_H__
#define __DINFO_H__

#include <QDialog>
#include <QMap>

#include "ui_information.h"

class DInformation : public QDialog, private Ui::Information {
    Q_OBJECT

public:
    DInformation(const QString &title, const QMap<QString, QString> &pages, bool delayok = false, QWidget *parent = 0, Qt::WindowFlags f = 0);

protected:
    virtual void reject();
    virtual void closeEvent(QCloseEvent *);
    virtual void changeEvent(QEvent *e);

private slots:
    void enableOk();
    void gotoPage(const QString &url);

private:
    QMap<QString, QString> m_pages;
};

#endif
