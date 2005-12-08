/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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
#ifndef __DLGINCOMPLETEITEMIMPL_H__
#define __DLGINCOMPLETEITEMIMPL_H__

#include "bricklink.h"

#include "dlgincompleteitem.h"

class DlgIncompleteItemImpl : public DlgIncompleteItem {
	Q_OBJECT

public:
	DlgIncompleteItemImpl ( BrickLink::InvItem *ii, QWidget *parent = 0, const char *name = 0, bool modal = true );
	~DlgIncompleteItemImpl ( );

private slots:
	void fixItem ( );
	void fixColor ( );
	
private:
	void checkOk ( );
	QString createDisplayString ( BrickLink::InvItem *ii );
	QString createDisplaySubString ( const QString &what, const QString &realname, const QString &id, const QString &name );

private:
	BrickLink::InvItem *m_ii;
};

#endif
