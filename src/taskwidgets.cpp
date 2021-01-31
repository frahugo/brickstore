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

#include <cfloat>
#include <cmath>

#include <QDockWidget>
#include <QMainWindow>
#include <QEvent>
#include <QDesktopServices>
#include <QStatusTipEvent>
#include <QApplication>
#include <QStringBuilder>

#include "bricklink.h"
#include "utility.h"
#include "config.h"
#include "framework.h"

#include "taskwidgets.h"

using namespace std::chrono_literals;


TaskPriceGuideWidget::TaskPriceGuideWidget(QWidget *parent)
    : PriceGuideWidget(parent), m_win(nullptr), m_dock(nullptr)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        bool ok = (m_win && (m_selection.count() == 1));

        setPriceGuide(ok ? BrickLink::core()->priceGuide(m_selection.front()->item(),
                                                         m_selection.front()->color(), true)
                         : nullptr);
    });


    connect(FrameWork::inst(), &FrameWork::windowActivated,
            this, &TaskPriceGuideWidget::windowUpdate);
    connect(this, &PriceGuideWidget::priceDoubleClicked,
            this, &TaskPriceGuideWidget::setPrice);
    fixParentDockWindow();
}

void TaskPriceGuideWidget::windowUpdate(Window *win)
{
    if (m_win) {
        disconnect(m_win.data(), &Window::selectionChanged,
                   this, &TaskPriceGuideWidget::selectionUpdate);
        disconnect(m_win->document(), &Document::currencyCodeChanged,
                   this, &TaskPriceGuideWidget::currencyUpdate);
    }
    m_win = win;
    if (m_win) {
        connect(m_win.data(), &Window::selectionChanged,
                this, &TaskPriceGuideWidget::selectionUpdate);
        connect(m_win->document(), &Document::currencyCodeChanged,
                this, &TaskPriceGuideWidget::currencyUpdate);
    }

    setCurrencyCode(m_win ? m_win->document()->currencyCode() : Config::inst()->defaultCurrencyCode());
    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskPriceGuideWidget::currencyUpdate(const QString &ccode)
{
    setCurrencyCode(ccode);
}

void TaskPriceGuideWidget::selectionUpdate(const Document::ItemList &list)
{
    m_selection = list;
    m_delayTimer.start();
}

void TaskPriceGuideWidget::setPrice(double p)
{
    if (m_win && (m_win->selection().count() == 1)) {
        Document::Item *pos = m_win->selection().front();
        Document::Item item = *pos;

        auto doc = m_win->document();
        p *= Currency::inst()->rate(doc->currencyCode());
        item.setPrice(p);

        doc->changeItem(pos, item);
    }
}

bool TaskPriceGuideWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ParentChange)
        fixParentDockWindow();

    return PriceGuideWidget::event(e);
}

void TaskPriceGuideWidget::fixParentDockWindow()
{
    if (m_dock) {
        disconnect(m_dock, &QDockWidget::dockLocationChanged,
                   this, &TaskPriceGuideWidget::dockLocationChanged);
        disconnect(m_dock, &QDockWidget::topLevelChanged,
                   this, &TaskPriceGuideWidget::topLevelChanged);
    }

    m_dock = nullptr;

    for (QObject *p = parent(); p; p = p->parent()) {
        if (qobject_cast<QDockWidget *>(p)) {
            m_dock = static_cast<QDockWidget *>(p);
            break;
        }
    }

    if (m_dock) {
        connect(m_dock, &QDockWidget::dockLocationChanged,
                this, &TaskPriceGuideWidget::dockLocationChanged);
        connect(m_dock, &QDockWidget::topLevelChanged,
                this, &TaskPriceGuideWidget::topLevelChanged);
    }
}

void TaskPriceGuideWidget::topLevelChanged(bool b)
{
    if (b) {
        setLayout(PriceGuideWidget::Normal);
        setMaximumSize(minimumSize());
    }
}

void TaskPriceGuideWidget::dockLocationChanged(Qt::DockWidgetArea area)
{
    bool vertical = (area ==  Qt::LeftDockWidgetArea) || (area == Qt::RightDockWidgetArea);

    setLayout(vertical ? PriceGuideWidget::Vertical : PriceGuideWidget::Horizontal);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskInfoWidget::TaskInfoWidget(QWidget *parent)
    : QStackedWidget(parent), m_win(nullptr)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_pic = new PictureWidget(this);
    m_text = new QLabel(this);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_text->setIndent(8);

    m_text->setBackgroundRole(QPalette::Base);
    m_text->setAutoFillBackground(true);

    addWidget(m_pic);
    addWidget(m_text);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        delayedSelectionUpdate(m_selection);
    });

    connect(FrameWork::inst(), &FrameWork::windowActivated,
            this, &TaskInfoWidget::windowUpdate);
    connect(Config::inst(), &Config::measurementSystemChanged,
            this, &TaskInfoWidget::refresh);
}

void TaskInfoWidget::windowUpdate(Window *win)
{
    if (m_win) {
        disconnect(m_win.data(), &Window::selectionChanged,
                   this, &TaskInfoWidget::selectionUpdate);
        disconnect(m_win->document(), &Document::currencyCodeChanged,
                   this, &TaskInfoWidget::currencyUpdate);
    }
    m_win = win;
    if (m_win) {
        connect(m_win.data(), &Window::selectionChanged,
                this, &TaskInfoWidget::selectionUpdate);
        connect(m_win->document(), &Document::currencyCodeChanged,
                this, &TaskInfoWidget::currencyUpdate);
    }

    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskInfoWidget::currencyUpdate()
{
    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskInfoWidget::selectionUpdate(const Document::ItemList &list)
{
    m_selection = list;
    m_delayTimer.start();
}

void TaskInfoWidget::delayedSelectionUpdate(const Document::ItemList &list)
{
    if (!m_win || (list.count() == 0)) {
        m_pic->setItemAndColor(nullptr);
        setCurrentWidget(m_pic);
    } else if (list.count() == 1) {
        m_pic->setItemAndColor(list.front()->item(), list.front()->color());
        setCurrentWidget(m_pic);
    }
    else {
        Document::Statistics stat(m_win->document(), list);

        QString valstr, wgtstr;
        QString ccode = m_win->document()->currencyCode();

        if (!qFuzzyCompare(stat.value(), stat.minValue())) {
            valstr = QString("%1 (%2 %3)").arg(Currency::toString(stat.value(), ccode, Currency::LocalSymbol),
                                               tr("min."),
                                               Currency::toString(stat.minValue(), ccode, Currency::LocalSymbol));
        } else {
            valstr = Currency::toString(stat.value(), ccode, Currency::LocalSymbol);
        }
        QString coststr = Currency::toString(stat.cost(), ccode, Currency::LocalSymbol);
        QString profitstr;
        if (!qFuzzyIsNull(stat.cost())) {
            int percent = int(std::round(stat.value() / stat.cost() * 100. - 100.));
            profitstr = (percent > 0 ? u"( +" : u"( ") % QString::number(percent) % u" % )";
        }


        if (qFuzzyCompare(stat.weight(), -DBL_MIN)) {
            wgtstr = "-";
        } else {
            double weight = stat.weight();

            if (weight < 0) {
                weight = -weight;
                wgtstr = tr("min.") + " ";
            }

            wgtstr += Utility::weightToString(weight, Config::inst()->measurementSystem(), true, true);
        }

        static const char *fmt = "<h3>%1</h3>"
                                 "&nbsp;&nbsp;%2: %3<br>"
                                 "&nbsp;&nbsp;%4: %5<br><br>"
                                 "&nbsp;&nbsp;%6: %7&nbsp;&nbsp;%8<br><br>"
                                 "&nbsp;&nbsp;%9: %10<br><br>"
                                 "&nbsp;&nbsp;%11: %12";

        QString s = QString::fromLatin1(fmt).arg(
                tr("Multiple lots selected"),
                tr("Lots"), QLocale().toString(stat.lots()),
                tr("Items"), QLocale().toString(stat.items()),
                tr("Cost"), coststr, profitstr).arg(
                tr("Value"), valstr,
                tr("Weight"), wgtstr);

//  if (( stat.errors ( ) > 0 ) && Config::inst ( )->showInputErrors ( ))
//   s += QString ( "<br /><br />&nbsp;&nbsp;%1: %2" ).arg ( tr( "Errors" )).arg ( stat.errors ( ));

        m_pic->setItemAndColor(nullptr);
        m_text->setText(s);
        setCurrentWidget(m_text);
    }
}

void TaskInfoWidget::languageChange()
{
    refresh();
}

void TaskInfoWidget::refresh()
{
    if (m_win)
        selectionUpdate(m_win->selection());
}

void TaskInfoWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QStackedWidget::changeEvent(e);
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskAppearsInWidget::TaskAppearsInWidget(QWidget *parent)
    : AppearsInWidget(parent), m_win(nullptr)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    connect(FrameWork::inst(), &FrameWork::windowActivated, this, &TaskAppearsInWidget::windowUpdate);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        if (!m_win || m_selection.isEmpty())
            setItem(nullptr, nullptr);
        else if (m_selection.count() == 1)
            setItem(m_selection.first()->item(), m_selection.first()->color());
        else
            setItems(m_selection);
    });
}

void TaskAppearsInWidget::windowUpdate(Window *win)
{
    if (m_win) {
        disconnect(m_win.data(), &Window::selectionChanged,
                   this, &TaskAppearsInWidget::selectionUpdate);
    }
    m_win = win;
    if (m_win) {
        connect(m_win.data(), &Window::selectionChanged,
                this, &TaskAppearsInWidget::selectionUpdate);
    }

    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskAppearsInWidget::selectionUpdate(const Document::ItemList &list)
{
    m_selection = list;
    m_delayTimer.start();
}

#include "moc_taskwidgets.cpp"
