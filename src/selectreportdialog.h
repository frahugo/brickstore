/* Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
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
#ifndef __SELECTREPORTDIALOG_H__
#define __SELECTREPORTDIALOG_H__

#include <QDialog>

#include "ui_selectreportdialog.h"

class Report;

class SelectReportDialog : public QDialog, private Ui::SelectReportDialog {
    Q_OBJECT

public:
	SelectReportDialog(QWidget *parent = 0, Qt::WindowFlags f = 0);

	const Report *report() const;

private slots:
	void reportChanged();
	void reportConfirmed();
};

#endif