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

#include <QTreeView>
#include <QScopedPointer>

#include "currency.h"
#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QAction)
class PriceGuideWidgetPrivate;


// This isn't a QTreeView, let alone an item-view at all, but the Vista and macOS styles will
// only style widgets derived from at least QTreeView correctly when it comes to hovering and
// headers

class PriceGuideWidget : public QTreeView
{
    Q_OBJECT
public:
    PriceGuideWidget(QWidget *parent = nullptr);
    ~PriceGuideWidget() override;

    BrickLink::PriceGuide *priceGuide() const;

    enum Layout {
        Normal,
        Horizontal,
        Vertical
    };

    QSize sizeHint() const override;

    QString currencyCode() const;

public slots:
    void setLayout(PriceGuideWidget::Layout l);
    void setPriceGuide(BrickLink::PriceGuide *pg);
    void setCurrencyCode(const QString &code);

signals:
    void priceDoubleClicked(double p);

protected:
    void recalcLayout();
    void recalcLayoutNormal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb);
    void recalcLayoutHorizontal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb);
    void recalcLayoutVertical(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb);

    void resizeEvent(QResizeEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
    void changeEvent(QEvent *e) override;

protected slots:
    void doUpdate();
    void gotUpdate(BrickLink::PriceGuide *pg);
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();

private:
    void paintHeader(QPainter *p, const QRect &r, Qt::Alignment align, const QString &str,
                     bool bold = false, bool left = false, bool right = false);
    void paintCell(QPainter *p, const QRect &r, Qt::Alignment align, const QString &str,
                   bool alternate = false, bool mouseOver = false);
    void updateNonStaticCells() const;

private:
    QScopedPointer<PriceGuideWidgetPrivate> d;
};
