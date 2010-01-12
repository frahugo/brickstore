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
#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

#include <QWidget>

class TabWidget;
class QMenu;


class Workspace : public QWidget {
    Q_OBJECT

public:
    Workspace(QWidget *parent = 0, Qt::WindowFlags f = 0);

    void addWindow(QWidget *w);

    QWidget *activeWindow() const;
    QList<QWidget *> windowList() const;

    QMenu *windowMenu();

signals:
    void windowActivated(QWidget *);

public slots:
    void setActiveWindow(QWidget *);

protected:
    virtual bool eventFilter(QObject *o, QEvent *e);
    void resizeEvent(QResizeEvent *);

private slots:
    void closeWindow(int idx);
    void currentChangedHelper(int);

private:
    TabWidget *  m_tabwidget;
};

#endif
