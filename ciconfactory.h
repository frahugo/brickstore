/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#ifndef __CICONFACTORY_H__
#define __CICONFACTORY_H__

#include <qiconset.h>


class CIconFactory : public QIconFactory {
public:
	CIconFactory ( );

	virtual QPixmap * createPixmap ( const QIconSet & iconSet, QIconSet::Size size, QIconSet::Mode mode, QIconSet::State state );

private:
	bool m_has_alpha;
};

#endif
