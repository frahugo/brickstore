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
#include "ui_selectdocumentdialog.h"

class Document;
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)


class SelectDocumentDialog : public QDialog, private Ui::SelectDocumentDialog
{
    Q_OBJECT
public:
    SelectDocumentDialog(const Document *self, const QString &headertext,
                         QWidget *parent = nullptr);
    ~SelectDocumentDialog();

    BrickLink::InvItemList items() const;

private slots:
    void updateButtons();
    void itemActivated(QListWidgetItem *item);
    
private:
    BrickLink::InvItemList m_clipboard_list;
};

