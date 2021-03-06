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

#include <QDateTime>
#include <QDate>
#include <QFile>
#include <QBuffer>
#include <QTextStream>
#include <QRegExp>
#include <QUrlQuery>

#include "lzmadec.h"

#include "config.h"
#include "bricklink.h"
#include "progressdialog.h"
#include "import.h"


ImportBLStore::ImportBLStore(ProgressDialog *pd)
    : m_progress(pd)
{
    connect(pd, &ProgressDialog::transferFinished,
            this, &ImportBLStore::gotten);

    pd->setHeaderText(tr("Importing BrickLink Store"));
    pd->setMessageText(tr("Download: %p"));

    QUrl url("https://www.bricklink.com/invExcelFinal.asp");
    QUrlQuery query;
    query.addQueryItem("itemType",      "");
    query.addQueryItem("catID",         "");
    query.addQueryItem("colorID",       "");
    query.addQueryItem("invNew",        "");
    query.addQueryItem("itemYear",      "");
    query.addQueryItem("viewType",      "x");    // XML
    query.addQueryItem("invStock",      "Y");
    query.addQueryItem("invStockOnly",  "");
    query.addQueryItem("invQty",        "");
    query.addQueryItem("invQtyMin",     "0");
    query.addQueryItem("invQtyMax",     "0");
    query.addQueryItem("invBrikTrak",   "");
    query.addQueryItem("invDesc",       "");
    query.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
    query.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);
    url.setQuery(query);

    pd->post(url);

    pd->layout();
}

ImportBLStore::~ImportBLStore()
{
    qDeleteAll(m_items);
}

const BrickLink::InvItemList &ImportBLStore::items() const
{
    return m_items;
}

QString ImportBLStore::currencyCode() const
{
    return m_currencycode;
}

void ImportBLStore::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;

    if (data && !data->isEmpty()) {
        QBuffer store_buffer(data);

        if (store_buffer.open(QIODevice::ReadOnly)) {
            QString emsg;
            int eline = 0, ecol = 0;
            QDomDocument doc;

            if (doc.setContent(&store_buffer, &emsg, &eline, &ecol)) {
                QDomElement root = doc.documentElement();

                BrickLink::Core::ParseItemListXMLResult result = BrickLink::core()->parseItemListXML(root, BrickLink::XMLHint_MassUpload);

                if (result.items) {
                    m_items = *result.items;
                    m_currencycode = result.currencyCode;
                    delete result.items;
                    ok = true;
                }
                else
                    m_progress->setErrorText(tr("Could not parse the XML data for the store inventory."));
            }
            else {
                if (data->startsWith("<HTML>") && data->contains("Invalid password"))
                    m_progress->setErrorText(tr("Either your username or password are incorrect."));
                else
                    m_progress->setErrorText(tr("Could not parse the XML data for the store inventory:<br /><i>Line %1, column %2: %3</i>").arg(eline).arg(ecol).arg(emsg));
            }
        }
    }
    m_progress->setFinished(ok);
}


ImportBLOrder::ImportBLOrder(const QDate &from, const QDate &to, BrickLink::OrderType type, ProgressDialog *pd)
    : m_progress(pd), m_order_from(from), m_order_to(to), m_order_type(type)
{
    init();
}

ImportBLOrder::ImportBLOrder(const QString &order, BrickLink::OrderType type, ProgressDialog *pd)
    : m_progress(pd), m_order_id(order), m_order_type(type)
{
    init();
}

void ImportBLOrder::init()
{
    m_current_address = -1;

    m_retry_placed = (m_order_type == BrickLink::OrderType::Any);

    if (!m_order_id.isEmpty()) {
        m_order_to = QDate::currentDate();
        m_order_from = m_order_to.addDays(-1);
    }

    connect(m_progress, &ProgressDialog::transferFinished,
            this, &ImportBLOrder::gotten);

    m_progress->setHeaderText(tr("Importing BrickLink Order"));
    m_progress->setMessageText(tr("Download: %p"));

    QUrl url("https://www.bricklink.com/orderExcelFinal.asp");
    QUrlQuery query;
    query.addQueryItem("action",        "save");
    query.addQueryItem("orderType",     m_order_type == BrickLink::OrderType::Placed ? "placed" : "received");
    query.addQueryItem("viewType",      "X");    // XML - this has to go last, otherwise we get HTML
    query.addQueryItem("getOrders",     m_order_id.isEmpty() ? "date" : "");
    query.addQueryItem("fMM",           QString::number(m_order_from.month()));
    query.addQueryItem("fDD",           QString::number(m_order_from.day()));
    query.addQueryItem("fYY",           QString::number(m_order_from.year()));
    query.addQueryItem("tMM",           QString::number(m_order_to.month()));
    query.addQueryItem("tDD",           QString::number(m_order_to.day()));
    query.addQueryItem("tYY",           QString::number(m_order_to.year()));
    query.addQueryItem("getStatusSel",  "I");
    query.addQueryItem("getFiled",      "Y");    // regardless of filed state
    query.addQueryItem("getDetail",     "y");    // get items (that's why we do this in the first place...)
    query.addQueryItem("orderID",       m_order_id);
    query.addQueryItem("getDateFormat", "0");    // MM/DD/YYYY
    query.addQueryItem("frmUsername",   Config::inst()->loginForBrickLink().first);
    query.addQueryItem("frmPassword",   Config::inst()->loginForBrickLink().second);
    url.setQuery(query);

    m_url = url;
    m_progress->post(url, nullptr, true /* no redirects allowed */);
    m_progress->layout();
}

const QVector<QPair<BrickLink::Order *, BrickLink::InvItemList *> > &ImportBLOrder::orders() const
{
    return m_orders;
}

void ImportBLOrder::gotten()
{
    TransferJob *j = m_progress->job();

    QByteArray *data = j->data();

    if (data && !data->isEmpty()) {
        if (m_current_address >= 0) {
            QString s = QString::fromUtf8(data->data(), data->size());
            QString a;

            QRegularExpression regExp(R"(<TD WIDTH="25%" VALIGN="TOP">&nbsp;Name & Address:</TD>\s*<TD WIDTH="75%">(.*?)</TD>)");
            auto matches = regExp.globalMatch(s);
            if (matches.hasNext()) {
                matches.next();
                if (matches.hasNext()) { // skip our own address
                    QRegularExpressionMatch match = matches.next();
                    a = match.captured(1);
                    a.replace(QRegularExpression(R"(<[bB][rR] ?/?>)"), QLatin1String("\n"));
                    m_orders[m_current_address].first->setAddress(a);

                }
            }
        }
        else {
            QBuffer order_buffer(data);

            if (order_buffer.open(QIODevice::ReadOnly)) {
                QString emsg;
                int eline = 0, ecol = 0;
                QDomDocument doc;

                if (doc.setContent(&order_buffer, &emsg, &eline, &ecol)) {
                    QDomElement root = doc.documentElement();

                    if ((root.nodeName() == QLatin1String("ORDERS")) && (root.firstChild().nodeName() == QLatin1String("ORDER"))) {
                        for (QDomNode ordernode = root.firstChild(); !ordernode.isNull(); ordernode = ordernode.nextSibling()) {
                            if (!ordernode.isElement())
                                continue;

                            // we own the items now
                            BrickLink::Core::ParseItemListXMLResult result =
                                    BrickLink::core()->parseItemListXML(ordernode.toElement(),
                                                                        BrickLink::XMLHint_Order);

                            if (result.items) {
                                BrickLink::Order *order = new BrickLink::Order(QLatin1String(""), BrickLink::OrderType::Placed);

                                for (QDomNode node = ordernode.firstChild(); !node.isNull(); node = node.nextSibling()) {
                                    if (!node.isElement())
                                        continue;

                                    QString tag = node.toElement().tagName();
                                    QString val = node.toElement().text();

                                    if (tag == QLatin1String("BUYER"))
                                        order->setBuyer(val);
                                    else if (tag == QLatin1String("SELLER"))
                                        order->setSeller(val);
                                    else if (tag == QLatin1String("ORDERID"))
                                        order->setId(val);
                                    else if (tag == QLatin1String("ORDERDATE"))
                                        order->setDate(QDateTime(mdy2date(val)));
                                    else if (tag == QLatin1String("ORDERSTATUSCHANGED"))
                                        order->setStatusChange(QDateTime(mdy2date(val)));
                                    else if (tag == QLatin1String("ORDERSHIPPING"))
                                        order->setShipping(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERINSURANCE"))
                                        order->setInsurance(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERADDCHRG1"))
                                        order->setDelivery(order->delivery() + QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERADDCHRG2"))
                                        order->setDelivery(order->delivery() + QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERCREDIT"))
                                        order->setCredit(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("BASEGRANDTOTAL"))
                                        order->setGrandTotal(QLocale::c().toDouble(val));
                                    else if (tag == QLatin1String("ORDERSTATUS"))
                                        order->setStatus(val);
                                    else if (tag ==QLatin1String("PAYMENTTYPE"))
                                        order->setPayment(val);
                                    else if (tag == QLatin1String("ORDERREMARKS"))
                                        order->setRemarks(val);
                                    else if (tag == QLatin1String("BASECURRENCYCODE"))
                                        order->setCurrencyCode(val);
                                    else if (tag == QLatin1String("LOCATION"))
                                        order->setCountryName(val.section(QLatin1String(", "), 0, 0));
                                }

                                if (!order->id().isEmpty()) {
                                    m_orders << qMakePair(order, result.items);
                                }
                                else {
                                    qDeleteAll(*result.items);
                                    delete result.items;
                                }
                            }
                        }
                    }
                }
                // find a better way - we shouldn't display widgets here
                //else
                //    MessageBox::warning(nullptr, { }, tr( "Could not parse the XML data for your orders:<br /><i>Line %1, column %2: %3</i>" ). arg ( eline ). arg ( ecol ). arg ( emsg ));
            }
        }
    }

    if (m_retry_placed) {
        QList<QPair<QString, QString> > items = QUrlQuery(m_url).queryItems();
        items[0].second = QLatin1String("placed");
        QUrlQuery query;
        query.setQueryItems(items);
        m_url.setQuery(query);

        m_progress->post(m_url);
        m_progress->layout();

        m_retry_placed = false;
    }
    else if ((m_current_address + 1) < m_orders.size()) {
        m_current_address++;

        QString url = QLatin1String("https://www.bricklink.com/orderDetail.asp?ID=") + m_orders[m_current_address].first->id();
        m_progress->setHeaderText(tr("Importing address records"));
        m_progress->get(url);
        m_progress->layout();
    }
    else {
        m_progress->setFinished(true);
    }
}

QDate ImportBLOrder::mdy2date(const QString &mdy)
{
    QDate d;
    QStringList sl = mdy.split(QLatin1Char('/'));
    d.setDate(sl[2].toInt(), sl[0].toInt(), sl[1].toInt());
    return d;
}


ImportBLCart::ImportBLCart(int shopid, int cartid, ProgressDialog *pd)
    : m_progress(pd)
{
    connect(pd, &ProgressDialog::transferFinished,
            this, &ImportBLCart::gotten);

    pd->setHeaderText(tr("Importing BrickLink Shopping Cart"));
    pd->setMessageText(tr("Download: %p"));

    QUrl url("https://www.bricklink.com/storeCart.asp");
    QUrlQuery query;
    query.addQueryItem("h", QString::number(shopid));
    query.addQueryItem("b", QString::number(cartid));
    url.setQuery(query);

    pd->get(url);
    pd->layout();
}

ImportBLCart::~ImportBLCart()
{
    qDeleteAll(m_items);
}

const BrickLink::InvItemList &ImportBLCart::items() const
{
    return m_items;
}

QString ImportBLCart::currencyCode() const
{
    return m_currencycode;
}

void ImportBLCart::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;

    if (data && !data->isEmpty()) {
        QBuffer cart_buffer(data);

        if (cart_buffer.open(QIODevice::ReadOnly)) {
            QTextStream ts(&cart_buffer);
            QString line;
            QString items_line;
            QRegExp sep(R"(<TR CLASS="tm"( ID="row_[0-9]*")?><TD HEIGHT="[0-9]*" ALIGN="CENTER">)", Qt::CaseInsensitive);
            int invalid_items = 0;
            bool parsing_items = false;

            while (true) {
                line = ts.readLine();
                if (line.isNull())
                    break;
                if ((sep.indexIn(line) == 0) && !parsing_items)
                    parsing_items = true;

                if (parsing_items)
                    items_line += line;

                if ((line.compare(QLatin1String("</TABLE>"), Qt::CaseInsensitive) == 0) && parsing_items)
                    break;
            }

            const QStringList strlist = items_line.split(sep, QString::SkipEmptyParts);

            for (const QString &str : strlist) {
                BrickLink::InvItem *ii = nullptr;

                QRegExp rx_ids(QLatin1String("HEIGHT='[0-9]*' SRC='http[s]://img.bricklink.com/([A-Z])/([^ ]+).(gif|jpg|png|jpeg)' NAME="), Qt::CaseInsensitive);
                QRegExp rx_qty_price(QLatin1String(" VALUE=\"([0-9]+)\">(&nbsp;\\(x[0-9]+\\))?<BR>Qty Available: <B>[0-9]+</B><BR>Each:&nbsp;<B>([A-Z $]+)([0-9.]+)</B>"), Qt::CaseInsensitive);
                QRegExp rx_names(QLatin1String("</TD><TD>(.+)</TD><TD VALIGN=\"TOP\" NOWRAP>"), Qt::CaseInsensitive);
                QString str_cond(QLatin1String("<B>New</B>"));

                (void) rx_ids.indexIn(str);
                (void) rx_names.indexIn(str);

                const BrickLink::Item *item = nullptr;
                const BrickLink::Color *col = nullptr;

                if (rx_ids.cap(1).length() == 1) {
                    int slash = rx_ids.cap(2).indexOf(QLatin1Char('/'));

                    if (slash >= 0) {   // with color
                        item = BrickLink::core()->item(rx_ids.cap(1).at(0).toLatin1(), rx_ids.cap(2).mid(slash + 1));
                        col = BrickLink::core()->color(rx_ids.cap(2).leftRef(slash).toUInt());
                    }
                    else {
                        item = BrickLink::core()->item(rx_ids.cap(1).at(0).toLatin1(), rx_ids.cap(2));
                        col = BrickLink::core()->color(0);
                    }
                }

                QString color_and_item = rx_names.cap(1).trimmed();

                if (!col || !color_and_item.startsWith(col->name())) {
                    int longest_match = 0;

                    const auto colors = BrickLink::core()->colors();
                    for (const BrickLink::Color *blcolor : colors) {
                        QString n(blcolor->name());

                        if ((n.length() > longest_match) &&
                                (color_and_item.startsWith(n))) {
                            longest_match = n.length();
                            col = blcolor;
                        }
                    }

                    if (!longest_match)
                        col = BrickLink::core()->color(0);
                }

                if (!item /*|| !color_and_item.endsWith ( item->name ( ))*/) {
                    int longest_match = 0;

                    const QVector<const BrickLink::Item *> &all_items = BrickLink::core()->items();
                    for (int i = 0; i < all_items.count(); i++) {
                        const BrickLink::Item *it = all_items [i];
                        QString n(it->name());
                        n = n.trimmed();

                        if ((n.length() > longest_match) &&
                                (color_and_item.indexOf(n)) >= 0) {
                            longest_match = n.length();
                            item = it;
                        }
                    }

                    if (!longest_match)
                        item = nullptr;
                }

                if (item && col) {
                    (void) rx_qty_price.indexIn(str);

                    int qty = rx_qty_price.cap(1).toInt();
                    if (m_currencycode.isEmpty()) {
                        m_currencycode = rx_qty_price.cap(3).trimmed();
                        m_currencycode.replace(QLatin1String(" $"), QLatin1String("D")); // 'US $' -> 'USD', 'AU $' -> 'AUD'
                        if (m_currencycode.length() != 3)
                            m_currencycode.clear();
                    }
                    double price = QLocale::c().toDouble(rx_qty_price.cap(4));

                    BrickLink::Condition cond = (str.indexOf(str_cond, 0, Qt::CaseInsensitive) >= 0
                                                 ? BrickLink::Condition::New : BrickLink::Condition::Used);

                    QString comment;
                    int comment_pos = color_and_item.indexOf(item->name());

                    if (comment_pos >= 0)
                        comment = color_and_item.mid(comment_pos + QString(item->name()).length() + 1);

                    if (qty && !qFuzzyIsNull(price)) {
                        ii = new BrickLink::InvItem(col, item);
                        ii->setCondition(cond);
                        ii->setQuantity(qty);
                        ii->setPrice(price);
                        ii->setComments(comment);
                    }
                }

                if (ii)
                    m_items << ii;
                else
                    invalid_items++;
            }

            if (!m_items.isEmpty()) {
                ok = true;

                if (invalid_items) {
                    m_progress->setMessageText(tr("%1 lots of your Shopping Cart could not be imported.").arg(invalid_items));
                    m_progress->setAutoClose(false);
                }
            }
            else {
                m_progress->setErrorText(tr("Could not parse the Shopping Cart contents."));
            }
        }
    }
    m_progress->setFinished(ok);
}

#include "moc_import.cpp"
