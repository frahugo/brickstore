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
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTableView>
#include <QBitmap>

#include "window.h"
#include "itemdetailpopup.h"
#include "ldraw/renderwidget.h"

#if defined(Q_WS_MACX)
#  include "macx.h"
#endif

class GlassButton : public QToolButton {
public:
    enum Type {
        Close,
        Play,
        Stop,
        View
    };

    GlassButton(Type t, QWidget *parent)
        : QToolButton(parent), m_type(t), m_hovered(false)
    {
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void enterEvent(QEvent *)
    {
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent *)
    {
        m_hovered = false;
        update();
    }

    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int side = qMin(width(), height());
        QRectF r = QRectF((width() - side)/2, (height() - side)/2, side, side);

        QPainterPath iconpath;

        switch(m_type) {
        case Close: {
            qreal s3  = side/3;
            qreal s18 = side/18;
            iconpath.setFillRule(Qt::WindingFill);
            iconpath.addRect(-s3, -s18, 2*s3, 2*s18);
            iconpath.addRect(-s18, -s3, 2*s18, 2*s3);
            QTransform rot45;
            rot45.translate(side/2, side/2).rotate(45, Qt::ZAxis);
            iconpath = rot45.map(iconpath);
            break;
        }
        case Play: {
            QPolygonF poly;
            qreal d = side/6;
            poly << QPointF(d+d, d) << QPointF(side-d, side/2) << QPointF(d+d, side-d);
            iconpath.addPolygon(poly);
            iconpath.closeSubpath();
            break;
        }
        case Stop: {
            qreal d = side / 4;
            iconpath.addRect(r.adjusted(d, d, -d, -d));
            break;
        }
        case View: {
            qreal s10 = side/10;
            qreal d = s10*1.5;
            qreal s2 = side/2;
            qreal s3 = side*0.35;
            iconpath.setFillRule(Qt::OddEvenFill);
            iconpath.addEllipse(r.adjusted(s3, s3, -s3, -s3));
            iconpath.moveTo(QPointF(s10, s2));
            iconpath.cubicTo(QPointF(s2-s10, d), QPointF(s2+s10, d), QPointF(side-s10, s2));
            iconpath.cubicTo(QPointF(s2+s10, side-d), QPointF(s2-s10, side-d), QPointF(s10, s2));
            iconpath.closeSubpath();
            break;
        }
        }

        int alpha = isDown() ? 172 : m_hovered ? 128 : 64;
        p.setBrush(QColor(255, 255, 255, alpha));
        p.setPen(Qt::NoPen);

        QPainterPath path;
        path.addEllipse(r);
        p.drawPath(path.subtracted(iconpath));
    }

private:
    Type m_type;
    bool m_hovered : 1;
};


ItemDetailPopup::ItemDetailPopup(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint), m_part(0), m_pic(0), m_pressed(false), m_connected(false)
{
    setAttribute(Qt::WA_TranslucentBackground);

    m_bar = new QWidget(this);
    m_stack = new QStackedWidget(this);
    m_close = new GlassButton(GlassButton::Close, m_bar);
    m_close->setToolTip(tr("Close 3D view"));
    m_close->setFixedSize(20, 20);
    m_play = new GlassButton(GlassButton::Play, m_bar);
    m_play->setFixedSize(20, 20);
    m_close->setToolTip(tr("Start animation"));
    m_stop = new GlassButton(GlassButton::Stop, m_bar);
    m_stop->setFixedSize(20, 20);
    m_close->setToolTip(tr("Stop animation"));
    m_view = new GlassButton(GlassButton::View, m_bar);
    m_view->setFixedSize(20, 20);
    m_close->setToolTip(tr("Reset 3D camera"));

    m_blpic = new QLabel(m_stack);
    m_blpic->setAlignment(Qt::AlignCenter);
    m_blpic->setMinimumSize(640, 480);
    m_stack->addWidget(m_blpic);

#if defined(QT_NO_OPENGL)
    m_ldraw = 0;
#else
    m_ldraw = new LDraw::RenderOffscreenWidget(this);
    m_stack->addWidget(m_ldraw);
#endif
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *hor = new QHBoxLayout(m_bar);
    hor->addWidget(m_close);
    hor->addStretch(10);
    hor->addWidget(m_play);
    hor->addSpacing(10);
    hor->addWidget(m_stop);
    hor->addStretch(10);
    hor->addWidget(m_view);
    lay->addWidget(m_bar);
    lay->addWidget(m_stack);

    connect(m_close, SIGNAL(clicked()), this, SLOT(close()));

#if !defined(QT_NO_OPENGL)
    connect(m_play, SIGNAL(clicked()), m_ldraw, SLOT(startAnimation()));
    connect(m_stop, SIGNAL(clicked()), m_ldraw, SLOT(stopAnimation()));
    connect(m_view, SIGNAL(clicked()), m_ldraw, SLOT(resetCamera()));
#endif
#ifdef Q_WS_MAC
    createWinId();
    MacExtra::setWindowShadow(this, false);
#endif
}

ItemDetailPopup::~ItemDetailPopup()
{
    if (m_pic)
        m_pic->release();
    if (m_part)
        m_part->release();
}

void ItemDetailPopup::setItem(const BrickLink::Item *item, const BrickLink::Color *color)
{
    if (m_pic)
        m_pic->release();
    m_pic = 0;
    if (m_part)
        m_part->release();
    m_part = 0;

    if (item) {
        m_pic = BrickLink::core()->largePicture(item, true);
        m_pic->addRef();
        m_part = LDraw::core() ? LDraw::core()->partFromId(item->id()) : 0;
        if (m_part)
            m_part->addRef();
        m_blpic->setText(QString());

#if !defined(QT_NO_OPENGL)
        if (m_part && m_ldraw) {
            m_ldraw->setPartAndColor(m_part, color ? color->ldrawId() : 7 /* light gray */);
            m_stack->setCurrentWidget(m_ldraw);
            m_blpic->setText(QString());
        } else
#endif
        {
            redraw();
            m_stack->setCurrentWidget(m_blpic);
#if !defined(QT_NO_OPENGL)
            m_ldraw->setPartAndColor(0, -1);
#endif
        }
    } else {
        m_blpic->setText(QString());
#if !defined(QT_NO_OPENGL)
        m_ldraw->setPartAndColor(0, -1);
#endif
    }

    if (!m_connected && item)
        m_connected = connect(BrickLink::core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(gotUpdate(BrickLink::Picture *)));

}

void ItemDetailPopup::gotUpdate(BrickLink::Picture *pic)
{
    if (pic == m_pic) {
        redraw();
    }
}

void ItemDetailPopup::redraw()
{
    if (m_pic) {
        //setWindowTitle(QString(d->m_pic->item()->id()) + " " + d->m_pic->item()->name());

        if (m_pic->updateStatus() == BrickLink::Updating)
            m_blpic->setText(QLatin1String("<center><i>") +
                    tr("Please wait ...updating") +
                    QLatin1String("</i></center>"));
        else if (m_pic->valid()) {
        //    // unfortunately, this looks very crappy for most of BLs pictures
        //    QPixmap pix = m_pic->pixmap();
        //    if (!m_pic->image().hasAlphaChannel())
        //        pix.setMask(QBitmap::fromImage(m_pic->image().createHeuristicMask()));
        //    m_blpic->setPixmap(pix);
            m_blpic->setPixmap(m_pic->pixmap());
        }
        else
            m_blpic->setText(QString::null);
    }
}

void ItemDetailPopup::paintEvent(QPaintEvent *)
{
    qreal f = qreal(.7) * m_bar->height() / height();

    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0, QColor(80, 80, 80, 180));
    grad.setColorAt(f, QColor(0, 0, 0, 180));
    grad.setColorAt(1-f, QColor(0, 0, 0, 180));
    grad.setColorAt(1, QColor(80, 80, 80, 180));

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(rect(), 20, 20);
}

void ItemDetailPopup::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Space)
        close();
    else
        e->ignore();
}

void ItemDetailPopup::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_movepos = e->globalPos();
        m_pressed = true;
    }
}

void ItemDetailPopup::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_movepos = QPoint();
        m_pressed = false;
    }
}

void ItemDetailPopup::mouseMoveEvent(QMouseEvent *e)
{
    if (m_pressed && e->buttons() == Qt::LeftButton) {
       move(pos() + e->globalPos() - m_movepos);
       m_movepos = e->globalPos();
    }
}
