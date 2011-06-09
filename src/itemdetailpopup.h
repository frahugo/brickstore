/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef ITEMDETAILPOPUP_H
#define ITEMDETAILPOPUP_H

#include <QDialog>

#include "ldraw.h"
#include "bricklinkfwd.h"

namespace LDraw {
class Model;
class RenderOffscreenWidget;
}

class QToolButton;
class QStackedWidget;
class QLabel;
class QTableView;


class ItemDetailPopup : public QDialog {
    Q_OBJECT

public:
    ItemDetailPopup(QWidget *parent);
    virtual ~ItemDetailPopup();

public slots:
    void setItem(const BrickLink::Item *item, const BrickLink::Color *color = 0);

protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);

private slots:
    void gotUpdate(BrickLink::Picture *pic);

private:
    void redraw();

private:
    LDraw::RenderOffscreenWidget *m_ldraw;
    QLabel *m_blpic;

    QWidget *m_bar;
    QStackedWidget *m_stack;
    QToolButton *m_close;
    QToolButton *m_play;
    QToolButton *m_stop;
    QToolButton *m_view;
    QPoint m_movepos;
    LDraw::Part *m_part;
    BrickLink::Picture *m_pic;
    bool m_pressed;

    bool m_connected;
};

#endif

