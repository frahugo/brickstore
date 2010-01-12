/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <QToolButton>
#include <QMenu>
#include <QPainter>
#include <QLineEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolTip>
#include <QIcon>
#include <QTableView>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QStyleOptionFrameV2>
#include <QStyle>
#include <QApplication>

#include "documentdelegate.h"
#include "selectitemdialog.h"
#include "selectcolordialog.h"
#include "utility.h"

QVector<QColor> DocumentDelegate::s_shades;
QHash<BrickLink::Status, QIcon> DocumentDelegate::s_status_icons;


DocumentDelegate::DocumentDelegate(Document *doc, DocumentProxyModel *view, QTableView *table)
    : QStyledItemDelegate(view), m_doc(doc), m_view(view), m_table(table),
      m_select_item(0), m_select_color(0), m_read_only(false)
{
}

QColor DocumentDelegate::shadeColor(int idx, qreal alpha)
{
    if (s_shades.isEmpty()) {
        s_shades.resize(13);
        for (int i = 0; i < 13; i++)
            s_shades[i] = QColor::fromHsv(i == 0 ? -1 : (i - 1) * 30, 255, 255);
    }
    QColor c = s_shades[idx % s_shades.size()];
    if (alpha)
        c.setAlphaF(alpha);
    return c;
}

QIcon::Mode DocumentDelegate::iconMode(QStyle::State state) const
{
    if (!(state & QStyle::State_Enabled)) return QIcon::Disabled;
    if (state & QStyle::State_Selected) return QIcon::Selected;
    return QIcon::Normal;
}

QIcon::State DocumentDelegate::iconState(QStyle::State state) const
{
    return state & QStyle::State_Open ? QIcon::On : QIcon::Off;
}

int DocumentDelegate::defaultItemHeight(const QWidget *w) const
{
    static QSize picsize = BrickLink::core()->itemType('P')->pictureSize();
    QFontMetrics fm(w ? w->font() : QApplication::font("QTableView"));

    return 4 + qMax(fm.height() * 2, picsize.height() / 2);
}

QSize DocumentDelegate::sizeHint(const QStyleOptionViewItem &option1, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QSize();

    static QSize picsize = BrickLink::core()->itemType('P')->pictureSize();
    int w = -1;

    if (idx.column() == Document::Picture)
        w = picsize.width() / 2 + 4;
    else
        w = QStyledItemDelegate::sizeHint(option1, idx).width();

    QStyleOptionViewItemV4 option(option1);
    return QSize(w, defaultItemHeight(option.widget));
}

void DocumentDelegate::paint(QPainter *p, const QStyleOptionViewItem &option1, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return;

    Document::Item *it = m_view->item(idx);
    if (!it)
        return;

    QStyleOptionViewItemV4 option(option1);

    QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
//    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
//        cg = QPalette::Inactive;

    int x = option.rect.x(), y = option.rect.y();
    int w = option.rect.width();
    int h = option.rect.height();
    int margin = 2;
    int align = (m_view->data(idx, Qt::TextAlignmentRole).toInt() & ~Qt::AlignVertical_Mask) | Qt::AlignVCenter;
    QString has_inv_tag;


    QPixmap pix;
    QIcon ico;
    QString str = idx.model()->data(idx, Qt::DisplayRole).toString();

    QColor bg;
    QColor fg;
    int checkmark = 0;

    bg = option.palette.color(cg, option.features & QStyleOptionViewItemV2::Alternate ? QPalette::AlternateBase : QPalette::Base);
    fg = option.palette.color(cg, QPalette::Text);

    switch (idx.column()) {
    case Document::Status:
        ico = s_status_icons[it->status()];
        if (ico.isNull()) {
            switch (it->status()) {
            case BrickLink::Exclude: ico = QIcon(":/images/status_exclude"); break;
            case BrickLink::Extra  : ico = QIcon(":/images/status_extra"); break;
            default                :
            case BrickLink::Include: ico = QIcon(":/images/status_include"); break;
            }
            s_status_icons.insert(it->status(), ico);
        }
        break;

    case Document::Description:
        if (it->item()->hasInventory())
            has_inv_tag = tr("Inv");
        break;

    case Document::Picture: {
        QImage img = it->image();
        pix = QPixmap::fromImage(img.scaled(img.size() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        break;
    }
    case Document::Color:
        if (const QPixmap *pixptr = BrickLink::core()->colorImage(it->color(), option.decorationSize.width(), option.rect.height()))
            pix = *pixptr;
        break;

    case Document::ItemType:
        bg = shadeColor(it->itemType()->id(), 0.1f);
        break;

    case Document::Category:
        bg = shadeColor(it->category()->id(), 0.2f);
        break;

    case Document::Quantity:
        if (it->quantity() <= 0)
            bg = (it->quantity() == 0) ? QColor::fromRgbF(1, 1, 0, 0.4f)
                 : QColor::fromRgbF(1, 0, 0, 0.4f);
        break;

    case Document::QuantityDiff:
        if (it->origQuantity() < it->quantity())
            bg = QColor::fromRgbF(0, 1, 0, 0.3f);
        else if (it->origQuantity() > it->quantity())
            bg = QColor::fromRgbF(1, 0, 0, 0.3f);
        break;

    case Document::PriceOrig:
    case Document::QuantityOrig:
        fg.setAlphaF(0.5f);
        break;

    case Document::PriceDiff:
        if (it->origPrice() < it->price())
            bg = QColor::fromRgbF(0, 1, 0, 0.3f);
        else if (it->origPrice() > it->price())
            bg = QColor::fromRgbF(1, 0, 0, 0.3f);
        break;

    case Document::Total:
        bg = QColor::fromRgbF(1, 1, 0, 0.1f);
        break;

    case Document::Condition:
        if (it->condition() != BrickLink::New) {
            bg = fg;
            bg.setAlphaF(0.3f);
        }
        if (it->itemType()->hasSubConditions() && it->subCondition() != BrickLink::None)
            str = QString("%1<br /><i>%2</i>" ).arg(str, m_doc->subConditionLabel(it->subCondition()));
        break;

    case Document::TierP1:
    case Document::TierQ1:
        bg = fg;
        bg.setAlphaF(0.06f);
        break;

    case Document::TierP2:
    case Document::TierQ2:
        bg = fg;
        bg.setAlphaF(0.12f);
        break;

    case Document::TierP3:
    case Document::TierQ3:
        bg = fg;
        bg.setAlphaF(0.18f);
        break;

    case Document::Retain:
        checkmark = it->retain() ? 1 : -1;
        break;

    case Document::Stockroom:
        checkmark = it->stockroom() ? 1 : -1;
        break;
    }

    if (option.state & QStyle::State_Selected) {
        if (m_read_only) {
            bg = Utility::gradientColor(bg, option.palette.color(cg, QPalette::Highlight));
        } else {
            bg = option.palette.color(cg, QPalette::Highlight);
            if (!(option.state & QStyle::State_HasFocus))
                bg.setAlphaF(0.7f);
            fg = option.palette.color(cg, QPalette::HighlightedText);
        }
    }

    if (!has_inv_tag.isEmpty()) {
        int itw = option.fontMetrics.width(has_inv_tag) + 2;
        int ith = option.fontMetrics.height() + 2;

        QRadialGradient grad(option.rect.bottomRight(), itw + ith);
        QColor col = fg;
        col.setAlphaF(0.2f);
        grad.setColorAt(0, col);
        grad.setColorAt(0.5, col);
        grad.setColorAt(1, bg);

        p->fillRect(option.rect, grad);

        p->setPen(bg);
        p->drawText(option.rect, Qt::AlignRight | Qt::AlignBottom, has_inv_tag);
    }
    else
        p->fillRect(option.rect, bg);


    if ((it->errors() & m_doc->errorMask() & (1ULL << idx.column()))) {
        p->setPen(QColor::fromRgbF(1, 0, 0, 0.75f));
        p->drawRect(QRectF(x+.5, y+.5, w-1, h-1));
        p->setPen(QColor::fromRgbF(1, 0, 0, 0.50f));
        p->drawRect(QRectF(x+1.5, y+1.5, w-3, h-3));
    }

    p->setPen(fg);

    x++; // extra spacing
    w -=2;

    if (checkmark != 0) {
        QStyleOptionViewItem opt(option);
        opt.state &= ~QStyle::State_HasFocus;
        opt.state |= ((checkmark > 0) ? QStyle::State_On : QStyle::State_Off);
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_IndicatorViewItemCheck, &opt, p, option.widget);
    }
    else if (!pix.isNull()) {
        // clip the pixmap here ..this is cheaper than a cliprect

        int rw = w - 2 * margin;
        int rh = h; // - 2 * margin;

        int sw, sh;

        if (pix.height() <= rh) {
            sw = qMin(rw, pix.width());
            sh = qMin(rh, pix.height());
        }
        else {
            sw = pix.width() * rh / pix.height();
            sh = rh;
        }

        int px = x + margin;
        int py = y + /*margin +*/ (rh - sh) / 2;

        if (align == Qt::AlignCenter)
            px += (rw - sw) / 2;   // center if there is enough room

        if (pix.height() <= rh)
            p->drawPixmap(px, py, pix, 0, 0, sw, sh);
        else
            p->drawPixmap(QRect(px, py, sw, sh), pix);

        w -= (margin + sw);
        x += (margin + sw);
    }
    else if (!ico.isNull()) {
        ico.paint(p, x, y, w, h, Qt::AlignCenter, iconMode(option.state), iconState(option.state));
    }

    if (!str.isEmpty()) {
        int rw = w - 2 * margin;

        if (!(align & Qt::AlignVertical_Mask))
            align |= Qt::AlignVCenter;

        const QFontMetrics &fm = p->fontMetrics();


        bool do_elide = false;
        int lcount = (h + fm.leading()) / fm.lineSpacing();
        int height = 0;
        qreal widthUsed = 0;

        QTextDocument td;
        td.setHtml(str);
        td.setDefaultFont(option.font);
        QTextLayout *tlp = td.firstBlock().layout();
        QTextOption to = tlp->textOption();
        to.setAlignment(Qt::Alignment(align));
        tlp->setTextOption(to);
        tlp->beginLayout();

        QString lstr = td.firstBlock().text();

        for (int i = 0; i < lcount; i++) {
            QTextLine line = tlp->createLine();
            if (!line.isValid())
                break;

            line.setLineWidth(rw);
            height += fm.leading();
            line.setPosition(QPoint(0, height));
            height += int(line.height());
            widthUsed = line.naturalTextWidth();

            if ((i == (lcount - 1)) && ((line.textStart() + line.textLength()) < lstr.length())) {
                do_elide = true;
                QString elide = QLatin1String("...");
                int elide_width = fm.width(elide) + 2;

                line.setLineWidth(rw - elide_width);
                widthUsed = line.naturalTextWidth();
            }
        }
        tlp->endLayout();

        tlp->draw(p, QPoint(x + margin, y + (h - height)/2));
        if (do_elide)
            p->drawText(QPoint(x + margin + int(widthUsed), y + (h - height)/2 + (lcount - 1) * fm.lineSpacing() + fm.ascent()), QLatin1String("..."));
    }
}

bool DocumentDelegate::editorEvent(QEvent *e, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    if (m_read_only)
        return false;

    if (!e || !model || !idx.isValid())
        return false;

    Document::Item *it = m_view->item(idx);
    if (!it)
        return false;

    switch (e->type()) {
    case QEvent::KeyPress: {
        //no break
    }
    case QEvent::MouseButtonDblClick: {
        if (nonInlineEdit(e, it, option, idx))
            return true;
        break;
    }
    default: break;
    }

    return QStyledItemDelegate::editorEvent(e, model, option, idx);
}

bool DocumentDelegate::nonInlineEdit(QEvent *e, Document::Item *it, const QStyleOptionViewItem &option, const QModelIndex &idx)
{
    bool accept = true;

    bool dblclick = (e->type() == QEvent::MouseButtonDblClick);
    bool keypress = (e->type() == QEvent::KeyPress);
    bool editkey = false;
    int key = -1;

    if (keypress) {
        key = static_cast<QKeyEvent *>(e)->key();

        if (key == Qt::Key_Space ||
            key == Qt::Key_Return ||
#if defined( Q_WS_MAC )
            (key == Qt::Key_O && static_cast<QKeyEvent *>(e)->modifiers() & Qt::ControlModifier)
#else
            key == Qt::Key_F2
#endif
           ) {
            editkey = true;
        }
    }


    switch (idx.column()) {
    case Document::Retain:
        if (dblclick || (keypress && editkey)) {
            Document::Item item = *it;
            item.setRetain(!it->retain());
            m_doc->changeItem(it, item);
        }
        break;

    case Document::Stockroom:
        if (dblclick || (keypress && editkey)) {
            Document::Item item = *it;
            item.setStockroom(!it->stockroom());
            m_doc->changeItem(it, item);
        }
        break;

    case Document::Condition:
        if (dblclick || (keypress && (editkey || key == Qt::Key_N || key == Qt::Key_U))) {
            BrickLink::Condition cond;
            if (key == Qt::Key_N)
                cond = BrickLink::New;
            else if (key == Qt::Key_U)
                cond = BrickLink::Used;
            else
                cond = (it->condition() == BrickLink::New) ? BrickLink::Used : BrickLink::New;

            Document::Item item = *it;
            item.setCondition(cond);
            m_doc->changeItem(it, item);
        }
        break;

    case Document::Status:
        if (dblclick || (keypress && (editkey || key == Qt::Key_I || key == Qt::Key_E || key == Qt::Key_X))) {
            BrickLink::Status st = it->status();
            if (key == Qt::Key_I)
                st = BrickLink::Include;
            else if (key == Qt::Key_E)
                st = BrickLink::Exclude;
            else if (key == Qt::Key_X)
                st = BrickLink::Extra;
            else
                switch (st) {
                        case BrickLink::Include: st = BrickLink::Exclude; break;
                        case BrickLink::Exclude: st = BrickLink::Extra; break;
                        case BrickLink::Extra  :
                        default                : st = BrickLink::Include; break;
                }

            Document::Item item = *it;
            item.setStatus(st);
            m_doc->changeItem(it, item);
        }
        break;

    case Document::Picture:
    case Document::Description:
        if (dblclick || (keypress && editkey)) {
            if (!m_select_item) {
                m_select_item = new SelectItemDialog(false, m_table, Qt::Tool);
                m_select_item->setWindowTitle(tr("Modify Item"));
            }
            m_select_item->setItem(it->item());

            if (m_select_item->exec(QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()), option.rect.size())) == QDialog::Accepted) {
                Document::Item item = *it;
                item.setItem(m_select_item->item());
                m_doc->changeItem(it, item);
            }
        }
        break;

    case Document::Color:
        if (dblclick || (keypress && editkey)) {
            if (!m_select_color) {
                m_select_color = new SelectColorDialog(m_table, Qt::Tool);
                m_select_color->setWindowTitle(tr("Modify Color"));
            }
            m_select_color->setColor(it->color());

            if (m_select_color->exec(QRect(m_table->viewport()->mapToGlobal(option.rect.topLeft()), option.rect.size())) == QDialog::Accepted) {
                Document::Item item = *it;
                item.setColor(m_select_color->color());
                m_doc->changeItem(it, item);
            }
        }
        break;

    default:
        accept = false;
        break;
    }
    return accept;
}

QWidget *DocumentDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &idx) const
{
    if (m_read_only)
        return 0;

    Document::Item *it = m_view->item(idx);
    if (!it)
        return false;

    QValidator *valid = 0;
    switch (idx.column()) {
    case Document::Sale        : valid = new QIntValidator(-1000, 99, 0); break;
    case Document::Quantity    :
    case Document::QuantityDiff: valid = new QIntValidator(-99999, 99999, 0); break;
    case Document::Bulk        : valid = new QIntValidator(1, 99999, 0); break;
    case Document::TierQ1      :
    case Document::TierQ2      :
    case Document::TierQ3      : valid = new QIntValidator(0, 99999, 0); break;
    case Document::Price       :
    case Document::TierP1      :
    case Document::TierP2      :
    case Document::TierP3      : valid = new CurrencyValidator(0, 10000, 3, 0); break;
    case Document::PriceDiff   : valid = new CurrencyValidator(-10000, 10000, 3, 0); break;
    case Document::Weight      : valid = new QDoubleValidator(0., 100000., 4, 0); break;
    default                    : break;
    }

    if (!m_lineedit)
        m_lineedit = new QLineEdit(parent);

    m_lineedit->setAlignment(Qt::Alignment(idx.data(Qt::TextAlignmentRole).toInt()));
    if (valid)
        m_lineedit->setValidator(valid);

    return m_lineedit;
}

void DocumentDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

void DocumentDelegate::setReadOnly(bool ro)
{
    m_read_only = ro;
}

bool DocumentDelegate::isReadOnly() const
{
    return m_read_only;
}