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
#ifndef __DLGLOADORDERIMPL_H__
#define __DLGLOADORDERIMPL_H__

#include "bricklink.h"

#include "dlgloadorder.h"

class DlgLoadOrderImpl : public DlgLoadOrder {
    Q_OBJECT

public:
    DlgLoadOrderImpl ( QWidget *parent = 0, const char * name = 0, bool modal = true );
    ~DlgLoadOrderImpl ( );

	QString orderId ( ) const;
	BrickLink::Order::Type orderType ( ) const;

protected slots:
	void checkId ( );
};

#endif
