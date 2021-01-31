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

#include <QApplication>
#include <QCursor>
#include <QFileDialog>
#include <QClipboard>
#include <QRegExp>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QtMath>
#include <QStringBuilder>

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "utility.h"
#include "config.h"
#include "framework.h"
#include "messagebox.h"
#include "undo.h"
#include "progressdialog.h"

#include "importorderdialog.h"

#include "import.h"
#include "currency.h"
#include "qparallelsort.h"
#include "document.h"
#include "document_p.h"

using namespace std::chrono_literals;


enum {
    CID_Change,
    CID_AddRemove,
    CID_Currency
};


CurrencyCmd::CurrencyCmd(Document *doc, const QString &ccode, qreal crate)
    : QUndoCommand(qApp->translate("CurrencyCmd", "Changed currency"))
    , m_doc(doc)
    , m_ccode(ccode)
    , m_crate(crate)
    , m_prices(nullptr)
{ }

CurrencyCmd::~CurrencyCmd()
{
    delete [] m_prices;
}

int CurrencyCmd::id() const
{
    return CID_Currency;
}

void CurrencyCmd::redo()
{
    Q_ASSERT(!m_prices);

    QString oldccode = m_doc->currencyCode();
    m_doc->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}

void CurrencyCmd::undo()
{
    Q_ASSERT(m_prices);

    QString oldccode = m_doc->currencyCode();
    m_doc->changeCurrencyDirect(m_ccode, m_crate, m_prices);
    m_ccode = oldccode;
}


ChangeCmd::ChangeCmd(Document *doc, int pos, const Document::Item &item, bool merge_allowed)
    : QUndoCommand(qApp->translate("ChangeCmd", "Modified item")), m_doc(doc), m_position(pos), m_item(item), m_merge_allowed(merge_allowed)
{ }

int ChangeCmd::id() const
{
    return CID_Change;
}

void ChangeCmd::redo()
{
    m_doc->changeItemDirect(m_position, m_item);
}

void ChangeCmd::undo()
{
    redo();
}

bool ChangeCmd::mergeWith(const QUndoCommand *other)
{
#if 0 // untested
    const ChangeCmd *that = static_cast <const ChangeCmd *>(other);

    if ((m_merge_allowed && that->m_merge_allowed) &&
        (m_doc == that->m_doc) &&
        (m_position == that->m_position))
    {
        m_item = that->m_item;
        return true;
    }
#else
    Q_UNUSED(other)
    Q_UNUSED(m_merge_allowed)
#endif
    return false;
}


AddRemoveCmd::AddRemoveCmd(Type t, Document *doc, const QVector<int> &positions, const Document::ItemList &items, bool merge_allowed)
    : QUndoCommand(genDesc(t == Add, qMax(items.count(), positions.count()))),
      m_doc(doc), m_positions(positions), m_items(items), m_type(t), m_merge_allowed(merge_allowed)
{
    // for add: specify items and optionally also positions
    // for remove: specify items only
}

AddRemoveCmd::~AddRemoveCmd()
{
    if (m_type == Add)
        qDeleteAll(m_items);
}

int AddRemoveCmd::id() const
{
    return CID_AddRemove;
}

void AddRemoveCmd::redo()
{
    if (m_type == Add) {
        // Document::insertItemsDirect() adds all m_items at the positions given in m_positions
        // (or append them to the document in case m_positions is empty)
        m_doc->insertItemsDirect(m_items, m_positions);
        m_positions.clear();
        m_type = Remove;
    }
    else {
        // Document::removeItemsDirect() removes all m_items and records the positions in m_positions
        m_doc->removeItemsDirect(m_items, m_positions);
        m_type = Add;
    }
}

void AddRemoveCmd::undo()
{
    redo();
}

bool AddRemoveCmd::mergeWith(const QUndoCommand *other)
{
    const auto *that = static_cast <const AddRemoveCmd *>(other);

    if ((m_merge_allowed && that->m_merge_allowed) &&
        (m_doc == that->m_doc) &&
        (m_type == that->m_type)) {
        m_items     += that->m_items;
        m_positions += that->m_positions;
        setText(genDesc(m_type == Remove, qMax(m_items.count(), m_positions.count())));

        const_cast<AddRemoveCmd *>(that)->m_items.clear();
        const_cast<AddRemoveCmd *>(that)->m_positions.clear();
        return true;
    }
    return false;
}

QString AddRemoveCmd::genDesc(bool is_add, int count)
{
    if (is_add)
        return Document::tr("Added %n item(s)", nullptr, count);
    else
        return Document::tr("Removed %n item(s)", nullptr, count);
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

Document::Statistics::Statistics(const Document *doc, const ItemList &list, bool ignoreExcluded)
{
    m_lots = 0;
    m_items = 0;
    m_val = m_minval = m_cost = 0.;
    m_weight = .0;
    m_errors = 0;
    m_incomplete = 0;
    bool weight_missing = false;

    for (const Item *item : list) {
        if (ignoreExcluded && (item->status() == BrickLink::Status::Exclude))
            continue;

        ++m_lots;

        int qty = item->quantity();
        double price = item->price();

        m_val += (qty * price);
        m_cost += (qty * item->cost());

        for (int i = 0; i < 3; i++) {
            if (item->tierQuantity(i) && !qFuzzyIsNull(item->tierPrice(i)))
                price = item->tierPrice(i);
        }
        m_minval += (qty * price * (1.0 - double(item->sale()) / 100.0));
        m_items += qty;

        if (item->weight() > 0)
            m_weight += item->weight();
        else
            weight_missing = true;

        if (quint64 errors = doc->itemErrors(item)) {
            for (quint64 i = 1ULL << (FieldCount - 1); i;  i >>= 1) {
                if (errors & i)
                    m_errors++;
            }
        }

        if (item->isIncomplete())
            m_incomplete++;
    }
    if (weight_missing)
        m_weight = (m_weight == 0.) ? -DBL_MIN : -m_weight;
    m_ccode = doc->currencyCode();
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

QVector<Document *> Document::s_documents;

Document *Document::createTemporary(const BrickLink::InvItemList &list, const QVector<int> &fakeIndexes)
{
    auto *doc = new Document(1 /*dummy*/);
    doc->setBrickLinkItems(list); // the caller owns the items
    doc->setFakeIndexes(fakeIndexes);
    return doc;
}

Document::Document(int /*is temporary*/)
    : m_currencycode(Config::inst()->defaultCurrencyCode())
{
    MODELTEST_ATTACH(this)

    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            this, &Document::pictureUpdated);
}

// the caller owns the items
Document::Document(const BrickLink::InvItemList &items, const QString &currencyCode)
    : Document(0)
{
    if (!currencyCode.isEmpty())
        m_currencycode = currencyCode;

    m_undo = new UndoStack(this);
    connect(m_undo, &QUndoStack::cleanChanged,
            this, [this](bool clean) {
        // if the gui state is modified, the overall state stays at "modified"
        if (!m_gui_state_modified)
            emit modificationChanged(!clean);
    });

    setBrickLinkItems(items); // the caller owns the items

    s_documents.append(this);
}

Document::~Document()
{
    delete m_order;
    qDeleteAll(m_items);

    s_documents.removeAll(this);
}

const QVector<Document *> &Document::allDocuments()
{
    return s_documents;
}

const Document::ItemList &Document::items() const
{
    return m_items;
}

Document::Statistics Document::statistics(const ItemList &list) const
{
    return Statistics(this, list);
}

void Document::beginMacro(const QString &label)
{
    m_undo->beginMacro(label);
}

void Document::endMacro(const QString &label)
{
    m_undo->endMacro(label);
}

QUndoStack *Document::undoStack() const
{
    return m_undo;
}


bool Document::clear()
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, QVector<int>(), m_items));
    return true;
}

int Document::positionOf(Item *item) const
{
    return m_items.indexOf(item);
}

Document::Item *Document::itemAt(int position)
{
    return (position >= 0 && position < m_items.count()) ? m_items.at(position) : nullptr;
}

bool Document::insertItems(const QVector<int> &positions, const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Add, this, positions, items /*, true*/));
    return true;
}

bool Document::removeItem(Item *item)
{
    return removeItems({ item });
}

bool Document::removeItems(const ItemList &items)
{
    m_undo->push(new AddRemoveCmd(AddRemoveCmd::Remove, this, QVector<int>(), items /*, true*/));
    return true;
}

bool Document::appendItem(Item *item)
{
    return insertItems({ }, { item });
}

bool Document::changeItem(Item *item, const Item &value)
{
    return changeItem(positionOf(item), value);
}

bool Document::changeItem(int position, const Item &value)
{
    m_undo->push(new ChangeCmd(this, position, value /*, true*/));
    return true;
}

void Document::insertItemsDirect(ItemList &items, QVector<int> &positions)
{
    auto pos = positions.constBegin();
    bool single = (items.count() == 1);
    QModelIndexList before;
    QModelIndex root;

    if (!single) {
        emit layoutAboutToBeChanged();
        before = persistentIndexList();
    }

    for (Item *item : qAsConst(items)) {
        int rows = rowCount(root);

        if (pos != positions.constEnd()) {
            if (single)
                beginInsertRows(root, *pos, *pos);
            m_items.insert(*pos, item);
            ++pos;
        } else {
            if (single)
                beginInsertRows(root, rows, rows);
            m_items.append(item);
        }
        updateErrors(item);
        if (single)
            endInsertRows();
    }

    if (!single) {
        QModelIndexList after;
        foreach (const QModelIndex &idx, before)
            after.append(index(item(idx), idx.column()));
        changePersistentIndexList(before, after);
        emit layoutChanged();
    }

    emitStatisticsChanged();
}

void Document::removeItemsDirect(ItemList &items, QVector<int> &positions)
{
    positions.resize(items.count());

    bool single = (items.count() == 1);
    QModelIndexList before;

    if (!single) {
        emit layoutAboutToBeChanged();
        before = persistentIndexList();
    }

    for (int i = items.count() - 1; i >= 0; --i) {
        Item *item = items[i];
        int idx = m_items.indexOf(item);
        if (single)
            beginRemoveRows(QModelIndex(), idx, idx);
        positions[i] = idx;
        m_items.removeAt(idx);
        if (single)
            endRemoveRows();
    }

    if (!single) {
        QModelIndexList after;
        foreach (const QModelIndex &idx, before)
            after.append(index(item(idx), idx.column()));
        changePersistentIndexList(before, after);
        emit layoutChanged();
    }

    emitStatisticsChanged();
}

void Document::changeItemDirect(int position, Item &item)
{
    Item *olditem = m_items[position];
    std::swap(*olditem, item);

    QModelIndex idx1 = index(olditem);
    QModelIndex idx2 = createIndex(idx1.row(), columnCount(idx1.parent()) - 1, idx1.internalPointer());

    emitDataChanged(idx1, idx2);
    updateErrors(olditem);
    emitStatisticsChanged();
}

void Document::changeCurrencyDirect(const QString &ccode, qreal crate, double *&prices)
{
    m_currencycode = ccode;

    if (!qFuzzyCompare(crate, qreal(1))) {
        bool createPrices = (prices == nullptr);
        if (createPrices)
            prices = new double[5 * m_items.count()];

        for (int i = 0; i < m_items.count(); ++i) {
            Item *item = m_items[i];
            if (createPrices) {
                prices[i * 5] = item->origPrice();
                prices[i * 5 + 1] = item->price();
                prices[i * 5 + 2] = item->tierPrice(0);
                prices[i * 5 + 3] = item->tierPrice(1);
                prices[i * 5 + 4] = item->tierPrice(2);

                item->setOrigPrice(prices[i * 5] * crate);
                item->setPrice(prices[i * 5 + 1] * crate);
                item->setTierPrice(0, prices[i * 5 + 2] * crate);
                item->setTierPrice(1, prices[i * 5 + 3] * crate);
                item->setTierPrice(2, prices[i * 5 + 4] * crate);
            } else {
                item->setOrigPrice(prices[i * 5]);
                item->setPrice(prices[i * 5 + 1]);
                item->setTierPrice(0, prices[i * 5 + 2]);
                item->setTierPrice(1, prices[i * 5 + 3]);
                item->setTierPrice(2, prices[i * 5 + 4]);
            }
        }

        if (!createPrices) {
            delete [] prices;
            prices = nullptr;
        }

        emitDataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        emitStatisticsChanged();
    }
    emit currencyCodeChanged(m_currencycode);
}

void Document::emitDataChanged(const QModelIndex &tl, const QModelIndex &br)
{
    if (!m_delayedEmitOfDataChanged) {
        m_delayedEmitOfDataChanged = new QTimer(this);
        m_delayedEmitOfDataChanged->setSingleShot(true);
        m_delayedEmitOfDataChanged->setInterval(0);

        static auto resetNext = [](decltype(m_nextDataChangedEmit) &next) {
            next = {
                QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()),
                QPoint(std::numeric_limits<int>::min(), std::numeric_limits<int>::min())
            };
        };

        resetNext(m_nextDataChangedEmit);

        connect(m_delayedEmitOfDataChanged, &QTimer::timeout,
                this, [this]() {

            emit dataChanged(index(m_nextDataChangedEmit.first.y(),
                                   m_nextDataChangedEmit.first.x()),
                             index(m_nextDataChangedEmit.second.y(),
                                   m_nextDataChangedEmit.second.x()));

            resetNext(m_nextDataChangedEmit);
        });
    }

    if (tl.row() < m_nextDataChangedEmit.first.y())
        m_nextDataChangedEmit.first.setY(tl.row());
    if (tl.column() < m_nextDataChangedEmit.first.x())
        m_nextDataChangedEmit.first.setX(tl.column());
    if (br.row() > m_nextDataChangedEmit.second.y())
        m_nextDataChangedEmit.second.setY(br.row());
    if (br.column() > m_nextDataChangedEmit.second.x())
        m_nextDataChangedEmit.second.setX(br.column());

    m_delayedEmitOfDataChanged->start();
}

void Document::emitStatisticsChanged()
{
    if (!m_delayedEmitOfStatisticsChanged) {
        m_delayedEmitOfStatisticsChanged = new QTimer(this);
        m_delayedEmitOfStatisticsChanged->setSingleShot(true);
        m_delayedEmitOfStatisticsChanged->setInterval(0);

        connect(m_delayedEmitOfStatisticsChanged, &QTimer::timeout,
                this, &Document::statisticsChanged);
    }
    m_delayedEmitOfStatisticsChanged->start();
}

void Document::updateErrors(Item *item)
{
    quint64 errors = 0;

    if (item->price() <= 0)
        errors |= (1ULL << Price);

    if (item->quantity() <= 0)
        errors |= (1ULL << Quantity);

    if (item->color() && item->itemType() && ((item->color()->id() != 0) && !item->itemType()->hasColors()))
        errors |= (1ULL << Color);

    if (item->tierQuantity(0) && ((item->tierPrice(0) <= 0) || (item->tierPrice(0) >= item->price())))
        errors |= (1ULL << TierP1);

    if (item->tierQuantity(1) && ((item->tierPrice(1) <= 0) || (item->tierPrice(1) >= item->tierPrice(0))))
        errors |= (1ULL << TierP2);

    if (item->tierQuantity(1) && (item->tierQuantity(1) <= item->tierQuantity(0)))
        errors |= (1ULL << TierQ2);

    if (item->tierQuantity(2) && ((item->tierPrice(2) <= 0) || (item->tierPrice(2) >= item->tierPrice(1))))
        errors |= (1ULL << TierP3);

    if (item->tierQuantity(2) && (item->tierQuantity(2) <= item->tierQuantity(1)))
        errors |= (1ULL << TierQ3);

    setItemErrors(item, errors);
}

QString Document::currencyCode() const
{
    return m_currencycode;
}

void Document::setCurrencyCode(const QString &ccode, qreal crate)
{
    if (ccode != m_currencycode)
        m_undo->push(new CurrencyCmd(this, ccode, crate));
}

bool Document::hasGuiState() const
{
    return !m_gui_state.isNull();
}

QDomElement Document::guiState() const
{
    return m_gui_state;
}

void Document::setGuiState(QDomElement dom)
{
    m_gui_state = dom;
}

void Document::clearGuiState()
{
    m_gui_state.clear();
}

void Document::setGuiStateModified(bool modified)
{
    bool wasModified = isModified();
    m_gui_state_modified = modified;
    if (wasModified != isModified())
        emit modificationChanged(!wasModified);
}

Document *Document::fileNew()
{
    auto *doc = new Document();
    doc->setTitle(tr("Untitled"));
    return doc;
}

Document *Document::fileOpen()
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";
    filters << tr("All Files") + "(*.*)";

    return fileOpen(QFileDialog::getOpenFileName(FrameWork::inst(), tr("Open File"), Config::inst()->documentDir(), filters.join(";;")));
}

Document *Document::fileOpen(const QString &s)
{
    if (s.isEmpty())
        return nullptr;

    QString abs_s = QFileInfo(s).absoluteFilePath();

    for (Document *doc : qAsConst(s_documents)) {
        if (QFileInfo(doc->fileName()).absoluteFilePath() == abs_s)
            return doc;
    }

    return fileLoadFrom(s, "bsx");
}

Document *Document::fileImportBrickLinkInventory(const BrickLink::Item *item, int quantity,
                                                 BrickLink::Condition condition)
{
    if (item && !item->hasInventory())
        return nullptr;

    if (item && (quantity > 0)) {
        BrickLink::InvItemList items = item->consistsOf();

        if (!items.isEmpty()) {
            for (BrickLink::InvItem *item : items) {
                item->setQuantity(item->quantity() * quantity);
                item->setCondition(condition);
            }

            auto *doc = new Document(items); // we own the items
            doc->setTitle(tr("Inventory for %1").arg(item->id()));

            qDeleteAll(items);
            return doc;
        } else {
            MessageBox::warning(nullptr, { }, tr("Internal error: Could not create an Inventory object for item %1").arg(CMB_BOLD(item->id())));
        }
    }
    return nullptr;
}

QVector<Document *> Document::fileImportBrickLinkOrders()
{
    QVector<Document *> docs;

    ImportOrderDialog dlg(FrameWork::inst());

    if (dlg.exec() == QDialog::Accepted) {
        QVector<QPair<BrickLink::Order *, BrickLink::InvItemList *> > orders = dlg.orders();

        for (auto it = orders.constBegin(); it != orders.constEnd(); ++it) {
            const auto &order = *it;

            if (order.first && order.second) {
                auto *doc = new Document(*order.second, order.first->currencyCode()); // ImportOrderDialog owns the items
                doc->setTitle(tr("Order #%1").arg(order.first->id()));
                doc->m_order = new BrickLink::Order(*order. first);
                docs.append(doc);
            }
        }
    }
    return docs;
}

Document *Document::fileImportBrickLinkStore()
{
    Transfer trans;
    ProgressDialog d(tr("Import BrickLink Store Inventory"), &trans, FrameWork::inst());
    ImportBLStore import(&d);

    if (d.exec() == QDialog::Accepted) {
        auto *doc = new Document(import.items(), import.currencyCode()); // ImportBLStore owns the items
        doc->setTitle(tr("Store %1").arg(QDate::currentDate().toString(Qt::LocalDate)));
        return doc;
    }
    return nullptr;
}

Document *Document::fileImportBrickLinkCart()
{
    QString url = QApplication::clipboard()->text(QClipboard::Clipboard);
    QRegExp rx_valid(QLatin1String(R"(https://www\.bricklink\.com/storeCart\.asp\?h=[0-9]+&b=[-0-9]+)"));

    if (!rx_valid.exactMatch(url))
        url = QLatin1String("https://www.bricklink.com/storeCart.asp?h=______&b=______");

    if (MessageBox::getString(nullptr, { }, tr("Enter the URL of your current BrickLink shopping cart:"
                               "<br /><br />Right-click on the <b>View Cart</b> button "
                               "in your browser and copy the URL to the clipboard by choosing "
                               "<b>Copy Link Location</b> (Firefox), <b>Copy Link</b> (Safari) "
                               "or <b>Copy Shortcut</b> (Internet Explorer).<br /><br />"
                               "<em>Super-lots and custom items are <b>not</b> supported</em>."), url)) {
        QRegExp rx(QLatin1String("\\?h=([0-9]+)&b=([-0-9]+)"));
        (void) rx.indexIn(url);
        int shopid = rx.cap(1).toInt();
        int cartid = rx.cap(2).toInt();

        if (shopid && cartid) {
            Transfer trans;
            ProgressDialog d(tr("Import BrickLink Shopping Cart"), &trans, FrameWork::inst());
            ImportBLCart import(shopid, cartid, &d);

            if (d.exec() == QDialog::Accepted) {
                auto *doc = new Document(import.items(), import.currencyCode()); // ImportBLCart owns the items
                doc->setTitle(tr("Cart in Shop %1").arg(shopid));
                return doc;
            }
        }
        else
            QApplication::beep();
    }
    return nullptr;
}

Document *Document::fileImportBrickLinkXML()
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";
    filters << tr("All Files") + "(*.*)";

    QString s = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), Config::inst()->documentDir(), filters.join(";;"));

    if (!s.isEmpty()) {
        Document *doc = fileLoadFrom(s, "xml", true);

        if (doc)
            doc->setTitle(tr("Import of %1").arg(QFileInfo(s).fileName()));
        return doc;
    }
    else
        return nullptr;
}

Document *Document::fileLoadFrom(const QString &name, const char *type, bool import_only)
{
    BrickLink::ItemListXMLHint hint;

    if (qstrcmp(type, "bsx") == 0)
        hint = BrickLink::XMLHint_BrickStore;
    else if (qstrcmp(type, "xml") == 0)
        hint = BrickLink::XMLHint_MassUpload;
    else
        return nullptr;


    QFile f(name);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(name)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    BrickLink::Core::ParseItemListXMLResult result;

    QString emsg;
    int eline = 0, ecol = 0;
    QDomDocument domdoc;
    QDomElement gui_state;

    if (domdoc.setContent(&f, &emsg, &eline, &ecol)) {
        QDomElement root = domdoc.documentElement();
        QDomElement item_elem;

        if (hint == BrickLink::XMLHint_BrickStore) {
            for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
                if (!n.isElement())
                    continue;

                if (n.nodeName() == QLatin1String("Inventory"))
                    item_elem = n.toElement();
                else if (n.nodeName() == QLatin1String("GuiState"))
                    gui_state = n.cloneNode(true).toElement();
            }
        }
        else {
            item_elem = root;
        }

        result = BrickLink::core()->parseItemListXML(item_elem, hint); // we own the items now
    }
    else {
        MessageBox::warning(nullptr, { }, tr("Could not parse the XML data in file %1:<br /><i>Line %2, column %3: %4</i>").arg(CMB_BOLD(name)).arg(eline).arg(ecol).arg(emsg));
        QApplication::restoreOverrideCursor();
        return nullptr;
    }

    QApplication::restoreOverrideCursor();

    Document *doc = nullptr;

    if (result.items) {
        uint fixedCount = 0;

        if (result.invalidItemCount) {
            fixedCount = uint(BrickLink::core()->applyChangeLogToItems(*result.items));
            result.invalidItemCount -= fixedCount;

            if (result.invalidItemCount) {
                if (MessageBox::information(nullptr, { },
                                            tr("This file contains %n unknown item(s).<br /><br />Do you still want to open this file?",
                                               nullptr, result.invalidItemCount),
                                            QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                    result.invalidItemCount = 0;
                }
            }
        }

        if (!result.invalidItemCount) {
            doc = new Document(*result.items, result.currencyCode);  // we own the items
            doc->setGuiState(gui_state);

            doc->setFileName(import_only ? QString() : name);
            if (!import_only)
                Config::inst()->addToRecentFiles(name);

            if (fixedCount) {
                QString fixedMsg = tr("While loading, the item and color ids of %n item(s) have been adjusted automatically according to the current BrickLink catalog change log.",
                                      nullptr, fixedCount);

                if (!import_only) {
                    if (MessageBox::question(nullptr, { },
                                             fixedMsg + "<br><br>" + tr("Do you want to save these changes now?")
                                             ) == MessageBox::Yes) {
                        doc->fileSaveTo(name, type, true, *result.items);
                    }
                } else {
                    MessageBox::information(nullptr, { }, fixedMsg);

                }
            }
        }

        qDeleteAll(*result.items);
        delete result.items;
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not parse the XML data in file %1.").arg(CMB_BOLD(name)));
    }

    return doc;
}

Document *Document::fileImportLDrawModel()
{
    QStringList filters;
    filters << tr("LDraw Models") + " (*.dat;*.ldr;*.mpd)";
    filters << tr("All Files") + "(*.*)";

    QString s = QFileDialog::getOpenFileName(FrameWork::inst(), tr("Import File"), Config::inst()->documentDir(), filters.join(";;"));

    if (s.isEmpty())
        return nullptr;

    QFile f(s);

    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::warning(nullptr, { }, tr("Could not open file %1 for reading.").arg(CMB_BOLD(s)));
        return nullptr;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    uint invalid_items = 0;
    BrickLink::InvItemList items; // we own the items

    bool b = BrickLink::core()->parseLDrawModel(f, items, &invalid_items);
    Document *doc = nullptr;

    QApplication::restoreOverrideCursor();

    if (b && !items.isEmpty()) {
        if (invalid_items) {
            if (MessageBox::information(nullptr, { },
                                        tr("This file contains %n unknown item(s).<br /><br />Do you still want to open this file?",
                                           nullptr, invalid_items),
                                        QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                invalid_items = 0;
            }
        }

        if (!invalid_items) {
            doc = new Document(items); // we own the items
            doc->setTitle(tr("Import of %1").arg(QFileInfo(s).fileName()));
        }
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not parse the LDraw model in file %1.").arg(CMB_BOLD(s)));
    }

    qDeleteAll(items);
    return doc;
}

void Document::setBrickLinkItems(const BrickLink::InvItemList &bllist)
{
    //TODO: switch to std::unique_ptr and remove this

    ItemList items;
    QVector<int> pos;

    for (const BrickLink::InvItem *blitem : bllist)
        items.append(new Item(*blitem));

    insertItemsDirect(items, pos);
}

void Document::setFakeIndexes(const QVector<int> &fakeIndexes)
{
    m_fakeIndexes = fakeIndexes;
}

QString Document::fileName() const
{
    return m_filename;
}

void Document::setFileName(const QString &str)
{
    if (str != m_filename) {
        m_filename = str;
        emit fileNameChanged(str);
    }
}

QString Document::fileNameOrTitle() const
{
    QFileInfo fi(m_filename);
    if (fi.exists())
        return QDir::toNativeSeparators(fi.absoluteFilePath());

    if (!m_title.isEmpty())
        return m_title;

    return m_filename;
}

QString Document::title() const
{
    return m_title;
}

void Document::setTitle(const QString &str)
{
    if (str != m_title) {
        m_title = str;
        emit titleChanged(m_title);
    }
}

bool Document::isModified() const
{
    return !m_undo->isClean() || m_gui_state_modified;
}

void Document::fileSave()
{
    if (fileName().isEmpty())
        fileSaveAs();
    else if (isModified())
        fileSaveTo(fileName(), "bsx", false, items());
}


void Document::fileSaveAs()
{
    QStringList filters;
    filters << tr("BrickStore XML Data") + " (*.bsx)";

    QString fn = fileName();

    if (fn.isEmpty() && !title().isEmpty()) {
        QDir d(Config::inst()->documentDir());
        if (d.exists()) {
            QString t = Utility::sanitizeFileName(title());
            fn = d.filePath(t);
        }
    }
    if (fn.right(4) == ".xml")
        fn.truncate(fn.length() - 4);

    fn = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Save File as"), fn, filters.join(";;"));

    if (!fn.isNull()) {
        if (fn.right(4) != ".bsx")
            fn += ".bsx";

        fileSaveTo(fn, "bsx", false, items());
    }
}


bool Document::fileSaveTo(const QString &s, const char *type, bool export_only, const ItemList &itemlist)
{
    BrickLink::ItemListXMLHint hint;

    if (qstrcmp(type, "bsx") == 0)
        hint = BrickLink::XMLHint_BrickStore;
    else if (qstrcmp(type, "xml") == 0)
        hint = BrickLink::XMLHint_MassUpload;
    else
        return false;


    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        QDomDocument doc((hint == BrickLink::XMLHint_BrickStore) ? QString("BrickStoreXML") : QString());
        doc.appendChild(doc.createProcessingInstruction("xml", R"(version="1.0" encoding="UTF-8")"));

        QDomElement item_elem = BrickLink::core()->createItemListXML(doc, hint, itemlist, m_currencycode);

        if (hint == BrickLink::XMLHint_BrickStore) {
            QDomElement root = doc.createElement("BrickStoreXML");
            root.appendChild(item_elem);

            if (hasGuiState())
                root.appendChild(guiState());

            doc.appendChild(root);
        }
        else {
            doc.appendChild(item_elem);
        }

        QByteArray output = doc.toByteArray();
        bool ok = (f.write(output.data(), output.size()) == qint64(output.size()));

        QApplication::restoreOverrideCursor();

        if (ok) {
            if (!export_only) {
                m_undo->setClean();
                setGuiStateModified(false);
                setFileName(s);

                Config::inst()->addToRecentFiles(s);
            }
            return true;
        }
        else
            MessageBox::warning(nullptr, { }, tr("Failed to save data in file %1.").arg(CMB_BOLD(s)));
    }
    else
        MessageBox::warning(nullptr, { }, tr("Failed to open file %1 for writing.").arg(CMB_BOLD(s)));

    return false;
}

void Document::fileExportBrickLinkInvReqClipboard(const ItemList &itemlist)
{
    QDomDocument doc(QString {});
    doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_Inventory, itemlist, m_currencycode));

    QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

    if (Config::inst()->value("/General/Export/OpenBrowser", true).toBool())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_InventoryRequest));
}

void Document::fileExportBrickLinkWantedListClipboard(const ItemList &itemlist)
{
    QString wantedlist;

    if (MessageBox::getString(nullptr, { }, tr("Enter the ID number of Wanted List (leave blank for the default Wanted List)"), wantedlist)) {
        QMap <QString, QString> extra;
        if (!wantedlist.isEmpty())
            extra.insert("WANTEDLISTID", wantedlist);

        QDomDocument doc(QString {});
        doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_WantedList, itemlist, m_currencycode, extra.isEmpty() ? nullptr : &extra));

        QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

        if (Config::inst()->value("/General/Export/OpenBrowser", true).toBool())
            QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_WantedListUpload));
    }
}

void Document::fileExportBrickLinkXMLClipboard(const ItemList &itemlist)
{
    QDomDocument doc(QString {});
    doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_MassUpload, itemlist, m_currencycode));

    QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

    if (Config::inst()->value("/General/Export/OpenBrowser", true).toBool())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_InventoryUpload));
}

void Document::fileExportBrickLinkUpdateClipboard(const ItemList &itemlist)
{
    for (const Item *item : itemlist) {
        if (!item->lotId()) {
            if (MessageBox::warning(nullptr, { }, tr("This list contains items without a BrickLink Lot-ID.<br /><br />Do you really want to export this list?"), MessageBox::Yes, MessageBox::No) != MessageBox::Yes)
                return;
            else
                break;
        }
    }

    QDomDocument doc(QString {});
    doc.appendChild(BrickLink::core()->createItemListXML(doc, BrickLink::XMLHint_MassUpdate, itemlist, m_currencycode));

    QApplication::clipboard()->setText(doc.toString(), QClipboard::Clipboard);

    if (Config::inst()->value(QLatin1String("/General/Export/OpenBrowser"), true).toBool())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_InventoryUpdate));
}

void Document::fileExportBrickLinkXML(const ItemList &itemlist)
{
    QStringList filters;
    filters << tr("BrickLink XML File") + " (*.xml)";

    QString s = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Export File"), Config::inst()->documentDir(), filters.join(";;"));

    if (!s.isNull()) {
        if (s.right(4) != QLatin1String(".xml"))
            s += QLatin1String(".xml");

        fileSaveTo(s, "xml", true, itemlist);
    }
}

quint64 Document::errorMask() const
{
    return m_error_mask;
}

void Document::setErrorMask(quint64 em)
{
    m_error_mask = em;
    emitStatisticsChanged();
    emitDataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

quint64 Document::itemErrors(const Item *item) const
{
    return m_errors.value(item, 0) & m_error_mask;
}

void Document::setItemErrors(Item *item, quint64 errors)
{
    if (!item)
        return;

    quint64 oldErrors = m_errors.value(item, 0);
    if (oldErrors != errors) {
        if (errors)
            m_errors.insert(item, errors);
        else
            m_errors.remove(item);

        emit errorsChanged(item);
        emitStatisticsChanged();
    }
}

const BrickLink::Order *Document::order() const
{
    return m_order;
}

void Document::resetDifferences(const ItemList &items)
{
    beginMacro(tr("Reset differences"));

    for (Item *pos : items) {
        if ((pos->origQuantity() != pos->quantity()) ||
                (!qFuzzyCompare(pos->origPrice(), pos->price()))) {
            Item item = *pos;

            item.setOrigQuantity(item.quantity());
            item.setOrigPrice(item.price());
            changeItem(pos, item);
        }
    }
    endMacro();
}








////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
// Itemviews API
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

QModelIndex Document::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, m_items.at(row));
    return {};
}

Document::Item *Document::item(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<Item *>(idx.internalPointer()) : nullptr;
}

QModelIndex Document::index(const Item *ci, int column) const
{
    Item *i = const_cast<Item *>(ci);

    return i ? createIndex(m_items.indexOf(i), column, i) : QModelIndex();
}


int Document::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : items().size();
}

int Document::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : FieldCount;
}

Qt::ItemFlags Document::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemFlags();

    Qt::ItemFlags ifs = QAbstractItemModel::flags(index);

    switch (index.column()) {
    case Index       :
    case Total       :
    case ItemType    :
    case Category    :
    case YearReleased:
    case LotId       : break;
    case Retain      : ifs |= Qt::ItemIsUserCheckable; Q_FALLTHROUGH();
    default          : ifs |= Qt::ItemIsEditable; break;
    }
    return ifs;
}

bool Document::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        Item *itemp = items().at(index.row());
        Item item = *itemp;
        auto f = static_cast<Field>(index.column());

        switch (f) {
        case Document::PartNo      : {
            char itid = item.itemType() ? item.itemType()->id() : 'P';
            const BrickLink::Item *newitem = BrickLink::core()->item(itid, value.toString().toLatin1().constData());
            if (newitem)
                item.setItem(newitem);
            break;
        }
        case Comments    : item.setComments(value.toString()); break;
        case Remarks     : item.setRemarks(value.toString()); break;
        case Reserved    : item.setReserved(value.toString()); break;
        case Sale        : item.setSale(value.toInt()); break;
        case Bulk        : item.setBulkQuantity(value.toInt()); break;
        case TierQ1      : item.setTierQuantity(0, value.toInt()); break;
        case TierQ2      : item.setTierQuantity(1, value.toInt()); break;
        case TierQ3      : item.setTierQuantity(2, value.toInt()); break;
        case TierP1      : item.setTierPrice(0, Currency::fromString(value.toString())); break;
        case TierP2      : item.setTierPrice(1, Currency::fromString(value.toString())); break;
        case TierP3      : item.setTierPrice(2, Currency::fromString(value.toString())); break;
        case Weight      : item.setWeight(Utility::stringToWeight(value.toString(), Config::inst()->measurementSystem())); break;
        case Quantity    : item.setQuantity(value.toInt()); break;
        case QuantityDiff: item.setQuantity(itemp->origQuantity() + value.toInt()); break;
        case Price       : item.setPrice(Currency::fromString(value.toString())); break;
        case PriceDiff   : item.setPrice(itemp->origPrice() + Currency::fromString(value.toString())); break;
        case Cost        : item.setCost(Currency::fromString(value.toString())); break;
        default          : break;
        }
        if (!(item == *itemp)) {
            changeItem(index.row(), item);
            emitDataChanged(index, index);
            return true;
        }
    }
    return false;
}


QVariant Document::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        Item *it = items().at(index.row());
        auto f = static_cast<Field>(index.column());

        switch (role) {
        case Qt::DisplayRole      : return dataForDisplayRole(it, f, index.row());
        case Qt::DecorationRole   : return dataForDecorationRole(it, f);
        case Qt::ToolTipRole      : return dataForToolTipRole(it, f, index.row());
        case Qt::TextAlignmentRole: return dataForTextAlignmentRole(it, f);
        case Qt::EditRole         : return dataForEditRole(it, f);
        case Qt::CheckStateRole   : return dataForCheckStateRole(it, f);
        case Document::FilterRole : return dataForFilterRole(it, f, index.row());
        }
    }
    return QVariant();
}

QVariant Document::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        auto f = static_cast<Field>(section);

        switch (role) {
        case Qt::DisplayRole      : return headerDataForDisplayRole(f);
        case Qt::TextAlignmentRole: return headerDataForTextAlignmentRole(f);
        case Qt::UserRole         : return headerDataForDefaultWidthRole(f);
        }
    }
    return QVariant();
}

QVariant Document::dataForEditRole(Item *it, Field f) const
{
    switch (f) {
    case PartNo      : return it->itemId();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case Reserved    : return it->reserved();
    case Sale        : return it->sale();
    case Bulk        : return it->bulkQuantity();
    case TierQ1      : return it->tierQuantity(0);
    case TierQ2      : return it->tierQuantity(1);
    case TierQ3      : return it->tierQuantity(2);
    case TierP1      : return Currency::toString(!qFuzzyIsNull(it->tierPrice(0)) ? it->tierPrice(0) : it->price(),      currencyCode());
    case TierP2      : return Currency::toString(!qFuzzyIsNull(it->tierPrice(1)) ? it->tierPrice(1) : it->tierPrice(0), currencyCode());
    case TierP3      : return Currency::toString(!qFuzzyIsNull(it->tierPrice(2)) ? it->tierPrice(2) : it->tierPrice(1), currencyCode());
    case Weight      : return Utility::weightToString(it->weight(), Config::inst()->measurementSystem(), false);
    case Quantity    : return it->quantity();
    case QuantityDiff: return it->quantity() - it->origQuantity();
    case Price       : return Currency::toString(it->price(), currencyCode());
    case PriceDiff   : return Currency::toString(it->price() - it->origPrice(), currencyCode());
    case Cost        : return Currency::toString(it->cost(), currencyCode());
    default          : return QString();
    }
}

QString Document::dataForDisplayRole(Item *it, Field f, int row) const
{
    QString dash = QLatin1String("-");

    switch (f) {
    case Index       :
        if (m_fakeIndexes.isEmpty()) {
            return QString::number(row + 1);
        } else {
            auto fi = m_fakeIndexes.at(row);
            return fi >= 0 ? QString::number(fi + 1) : QString::fromLatin1("+");
        }
    case LotId       : return (it->lotId() == 0 ? dash : QString::number(it->lotId()));
    case PartNo      : return it->itemId();
    case Description : return it->itemName();
    case Comments    : return it->comments();
    case Remarks     : return it->remarks();
    case Quantity    : return QString::number(it->quantity());
    case Bulk        : return (it->bulkQuantity() == 1 ? dash : QString::number(it->bulkQuantity()));
    case Price       : return Currency::toString(it->price(), currencyCode());
    case Total       : return Currency::toString(it->total(), currencyCode());
    case Sale        : return (it->sale() == 0 ? dash : QString::number(it->sale()) + QLatin1Char('%'));
    case Condition   : {
        QString c = (it->condition() == BrickLink::Condition::New) ? tr("N", "List>Cond>New")
                                                                   : tr("U", "List>Cond>Used");
        if (it->itemType() && it->itemType()->hasSubConditions()
                && (it->subCondition() != BrickLink::SubCondition::None)) {
            c = c % u" / " % subConditionLabel(it->subCondition());
        }
        return c;
    }
    case Color       : return it->colorName();
    case Category    : return it->category() ? it->category()->name() : dash;
    case ItemType    : return it->itemType() ? it->itemType()->name() : dash;
    case TierQ1      : return (it->tierQuantity(0) == 0 ? dash : QString::number(it->tierQuantity(0)));
    case TierQ2      : return (it->tierQuantity(1) == 0 ? dash : QString::number(it->tierQuantity(1)));
    case TierQ3      : return (it->tierQuantity(2) == 0 ? dash : QString::number(it->tierQuantity(2)));
    case TierP1      : return Currency::toString(it->tierPrice(0), currencyCode());
    case TierP2      : return Currency::toString(it->tierPrice(1), currencyCode());
    case TierP3      : return Currency::toString(it->tierPrice(2), currencyCode());
    case Reserved    : return it->reserved();
    case Weight      : return qFuzzyIsNull(it->weight()) ? dash : Utility::weightToString(it->weight(), Config::inst()->measurementSystem(), true, true);
    case YearReleased: return (it->itemYearReleased() == 0) ? dash : QString::number(it->itemYearReleased());

    case PriceOrig   : return Currency::toString(it->origPrice(), currencyCode());
    case PriceDiff   : return Currency::toString(it->price() - it->origPrice(), currencyCode());
    case Cost        : return Currency::toString(it->cost(), currencyCode());
    case QuantityOrig: return QString::number(it->origQuantity());
    case QuantityDiff: return QString::number(it->quantity() - it->origQuantity());
    default          : return QString();
    }
}

QVariant Document::dataForFilterRole(Item *it, Field f, int row) const
{
    switch (f) {
    case Status:
        switch (it->status()) {
        case BrickLink::Status::Include: return tr("I", "Filter>Status>Include"); break;
        case BrickLink::Status::Extra  : return tr("X", "Filter>Status>Extra"); break;
        default:
        case BrickLink::Status::Exclude: return tr("E", "Filter>Status>Exclude"); break;
        }
    case Stockroom:
        switch (it->stockroom()) {
        case BrickLink::Stockroom::A: return QString("A");
        case BrickLink::Stockroom::B: return QString("B");
        case BrickLink::Stockroom::C: return QString("C");
        default                     : return QString("-");
        }
    case Index       : return row + 1;
    case Retain      : return it->retain() ? tr("Y", "Filter>Retain>Yes")
                                           : tr("N", "Filter>Retain>No");
    case Price       : return it->price();
    case PriceDiff   : return it->price() - it->origPrice();
    case Cost        : return it->cost();
    case TierP1      : return !qFuzzyIsNull(it->tierPrice(0)) ? it->tierPrice(0) : it->price();
    case TierP2      : return !qFuzzyIsNull(it->tierPrice(1)) ? it->tierPrice(1) : it->tierPrice(0);
    case TierP3      : return !qFuzzyIsNull(it->tierPrice(2)) ? it->tierPrice(2) : it->tierPrice(1);

    default: {
        QVariant v = dataForEditRole(it, f);
        if (v.isNull())
            v = dataForDisplayRole(it, f, row);
        return v;
    }
    }
}

QVariant Document::dataForDecorationRole(Item *it, Field f) const
{
    switch (f) {
    case Picture: return it->image();
    default     : return QPixmap();
    }
}

Qt::CheckState Document::dataForCheckStateRole(Item *it, Field f) const
{
    switch (f) {
    case Retain   : return it->retain() ? Qt::Checked : Qt::Unchecked;
    default       : return Qt::Unchecked;
    }
}

int Document::dataForTextAlignmentRole(Item *, Field f) const
{
    switch (f) {
    case Status      :
    case Retain      :
    case Stockroom   :
    case Picture     :
    case Condition   : return Qt::AlignVCenter | Qt::AlignHCenter;
    case Index       :
    case LotId       :
    case PriceOrig   :
    case PriceDiff   :
    case Cost        :
    case Price       :
    case Total       :
    case Sale        :
    case TierP1      :
    case TierP2      :
    case TierP3      :
    case Quantity    :
    case Bulk        :
    case QuantityOrig:
    case QuantityDiff:
    case TierQ1      :
    case TierQ2      :
    case TierQ3      :
    case YearReleased:
    case Weight      : return Qt::AlignRight | Qt::AlignVCenter;
    default          : return Qt::AlignLeft | Qt::AlignVCenter;
    }
}

QString Document::dataForToolTipRole(Item *it, Field f, int row) const
{
    switch (f) {
    case Status: {
        QString str;
        switch (it->status()) {
        case BrickLink::Status::Exclude: str = tr("Exclude"); break;
        case BrickLink::Status::Extra  : str = tr("Extra"); break;
        case BrickLink::Status::Include: str = tr("Include"); break;
        default                : break;
        }
        if (it->counterPart())
            str += QLatin1String("\n(") + tr("Counter part") + QLatin1Char(')');
        else if (it->alternateId())
            str += QLatin1String("\n(") + tr("Alternate match id: %1").arg(it->alternateId()) + QLatin1Char(')');
        return str;
    }
    case Picture: {
        return dataForDisplayRole(it, PartNo, row) + QLatin1Char(' ') + dataForDisplayRole(it, Description, row);
    }
    case Condition: {
        QString c = (it->condition() == BrickLink::Condition::New) ? tr("New") : tr("Used");
        if (it->itemType() && it->itemType()->hasSubConditions()
                && (it->subCondition() != BrickLink::SubCondition::None)) {
            c = c % u" / " % subConditionLabel(it->subCondition());
        }
        return c;
    }
    case Category: {
        if (!it->item())
            break;

        const auto allcats = it->item()->allCategories();

        if (allcats.size() == 1) {
            return allcats[0]->name();
        }
        else {
            QString str = QLatin1String("<b>") + allcats[0]->name() + QLatin1String("</b>");
            for (int i = 1; i < allcats.size(); ++i)
                str = str + QLatin1String("<br />") + allcats[i]->name();
            return str;
        }
    }
    default: {
        return dataForDisplayRole(it, f, row);
    }
    }
    return QString();
}


QString Document::headerDataForDisplayRole(Field f)
{
    switch (f) {
    case Index       : return tr("Index");
    case Status      : return tr("Status");
    case Picture     : return tr("Image");
    case PartNo      : return tr("Part #");
    case Description : return tr("Description");
    case Comments    : return tr("Comments");
    case Remarks     : return tr("Remarks");
    case QuantityOrig: return tr("Qty.Orig");
    case QuantityDiff: return tr("Qty.Diff");
    case Quantity    : return tr("Qty.");
    case Bulk        : return tr("Bulk");
    case PriceOrig   : return tr("Pr.Orig");
    case PriceDiff   : return tr("Pr.Diff");
    case Cost        : return tr("Cost");
    case Price       : return tr("Price");
    case Total       : return tr("Total");
    case Sale        : return tr("Sale");
    case Condition   : return tr("Cond.");
    case Color       : return tr("Color");
    case Category    : return tr("Category");
    case ItemType    : return tr("Item Type");
    case TierQ1      : return tr("Tier Q1");
    case TierP1      : return tr("Tier P1");
    case TierQ2      : return tr("Tier Q2");
    case TierP2      : return tr("Tier P2");
    case TierQ3      : return tr("Tier Q3");
    case TierP3      : return tr("Tier P3");
    case LotId       : return tr("Lot Id");
    case Retain      : return tr("Retain");
    case Stockroom   : return tr("Stockroom");
    case Reserved    : return tr("Reserved");
    case Weight      : return tr("Weight");
    case YearReleased: return tr("Year");
    default          : return QString();
    }
}

int Document::headerDataForTextAlignmentRole(Field f) const
{
    return dataForTextAlignmentRole(nullptr, f);
}

int Document::headerDataForDefaultWidthRole(Field f) const
{
    int width = 0;
    QSize picsize = BrickLink::core()->standardPictureSize();

    switch (f) {
    case Index       : width = 3; break;
    case Status      : width = 6; break;
    case Picture     : width = -picsize.width(); break;
    case PartNo      : width = 10; break;
    case Description : width = 28; break;
    case Comments    : width = 8; break;
    case Remarks     : width = 8; break;
    case QuantityOrig: width = 5; break;
    case QuantityDiff: width = 5; break;
    case Quantity    : width = 5; break;
    case Bulk        : width = 5; break;
    case PriceOrig   : width = 8; break;
    case PriceDiff   : width = 8; break;
    case Cost        : width = 8; break;
    case Price       : width = 8; break;
    case Total       : width = 8; break;
    case Sale        : width = 5; break;
    case Condition   : width = 5; break;
    case Color       : width = 15; break;
    case Category    : width = 12; break;
    case ItemType    : width = 12; break;
    case TierQ1      : width = 5; break;
    case TierP1      : width = 8; break;
    case TierQ2      : width = 5; break;
    case TierP2      : width = 8; break;
    case TierQ3      : width = 5; break;
    case TierP3      : width = 8; break;
    case LotId       : width = 8; break;
    case Retain      : width = 8; break;
    case Stockroom   : width = 8; break;
    case Reserved    : width = 8; break;
    case Weight      : width = 8; break;
    case YearReleased: width = 5; break;
    default          : break;
    }
    return width;
}


void Document::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item())
        return;

    int row = 0;
    for (const Item *it : qAsConst(m_items)) {
        if ((pic->item() == it->item()) && (pic->color() == it->color())) {
            QModelIndex idx = index(row, Picture);
            emitDataChanged(idx, idx);
        }
        row++;
    }
}


QString Document::subConditionLabel(BrickLink::SubCondition sc) const
{
    switch (sc) {
    case BrickLink::SubCondition::None      : return tr("-", "no subcondition");
    case BrickLink::SubCondition::Sealed    : return tr("Sealed");
    case BrickLink::SubCondition::Complete  : return tr("Complete");
    case BrickLink::SubCondition::Incomplete: return tr("Incomplete");
    default                                 : return QString();
    }
}




DocumentProxyModel::DocumentProxyModel(Document *model, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_lastSortColumn[0] = m_lastSortColumn[1] = -1;

    setDynamicSortFilter(false);
    setSourceModel(model);

    m_parser = new Filter::Parser();

    languageChange();
}

DocumentProxyModel::~DocumentProxyModel()
{
    delete m_parser;
}

void DocumentProxyModel::setFilterExpression(const QString &str)
{
    if (str == m_filter_expression)
        return;

    bool had_filter = !m_filter.isEmpty();

    m_filter_expression = str;
    m_filter = m_parser->parse(str);

    if (had_filter || !m_filter.isEmpty())
        invalidateFilter();

    emit filterExpressionChanged(str);
}

QString DocumentProxyModel::filterExpression() const
{
    return m_filter_expression;
}

QString DocumentProxyModel::filterToolTip() const
{
    return m_parser->toolTip();
}

bool DocumentProxyModel::filterAcceptsColumn(int, const QModelIndex &) const
{
    return true;
}

bool DocumentProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid() || source_row < 0 || source_row >= sourceModel()->rowCount())
        return false;
    else if (m_filter.isEmpty())
        return true;

    bool result = false;
    Filter::Combination nextcomb = Filter::Or;

    for (const Filter &f : m_filter) {
        int firstcol = f.field();
        int lastcol = firstcol;
        if (firstcol < 0) {
            firstcol = 0;
            lastcol = sourceModel()->columnCount() - 1;
        }

        bool localresult = false;
        for (int c = firstcol; c <= lastcol && !localresult; ++c) {
            QVariant v = sourceModel()->data(sourceModel()->index(source_row, c), Document::FilterRole);
            if (v.isNull())
                v = sourceModel()->data(sourceModel()->index(source_row, c), Qt::DisplayRole).toString();
            localresult = f.matches(v);
        }
        if (nextcomb == Filter::And)
            result = result && localresult;
        else
            result = result || localresult;

        nextcomb = f.combination();
    }
    return result;
}

bool DocumentProxyModel::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QSortFilterProxyModel::event(e);
}

void DocumentProxyModel::languageChange()
{
    auto model = sourceModel();

    m_parser->setStandardCombinationTokens(Filter::And | Filter::Or);
    m_parser->setStandardComparisonTokens(Filter::Matches | Filter::DoesNotMatch |
                                          Filter::Is | Filter::IsNot |
                                          Filter::Less | Filter::LessEqual |
                                          Filter::Greater | Filter::GreaterEqual |
                                          Filter::StartsWith | Filter::DoesNotStartWith |
                                          Filter::EndsWith | Filter::DoesNotEndWith);

    QMultiMap<int, QString> fields;
    QString str;
    for (int i = 0; i < model->columnCount(QModelIndex()); ++i) {
        str = model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!str.isEmpty())
            fields.insert(i, str);
    }
    fields.insert(-1, tr("Any"));

    m_parser->setFieldTokens(fields);
}

void DocumentProxyModel::sort(int column, Qt::SortOrder order)
{
    if (column == -1) {
        m_lastSortColumn[0] = m_lastSortColumn[1] = -1;
    } else {
        m_lastSortColumn[1] = m_lastSortColumn[0];
        m_lastSortColumn[0] = sortColumn();
    }

    QSortFilterProxyModel::sort(column, order);
}

bool DocumentProxyModel::lessThan(const QModelIndex &idx1, const QModelIndex &idx2) const
{
    const Document::Item *i1 = static_cast<Document *>(sourceModel())->item(idx1);
    const Document::Item *i2 = static_cast<Document *>(sourceModel())->item(idx2);

    int result = compare(i1, i2, sortColumn());
    if (!result && m_lastSortColumn[0] > -1)
        result = compare(i1, i2, m_lastSortColumn[0]);
    if (!result && m_lastSortColumn[1] > -1)
        result = compare(i1, i2, m_lastSortColumn[1]);
    return result < 0;
}

static inline int boolCompare(bool b1, bool b2)
{
    return (b1 ? 1 : 0) - (b2 ? 1 : 0);
}

static inline int uintCompare(uint u1, uint u2)
{
    return (u1 == u2) ? 0 : ((u1 < u2) ? -1 : 1);
}

static inline int doubleCompare(double d1, double d2)
{
    return qFuzzyCompare(d1, d2) ? 0 : ((d1 < d2) ? -1 : 1);
}

int DocumentProxyModel::compare(const Document::Item *i1, const Document::Item *i2, int sortColumn) const
{
    switch (sortColumn) {
    case Document::Index       : return static_cast<Document *>(sourceModel())->index(i1).row()
                                        - static_cast<Document *>(sourceModel())->index(i2).row();
    case Document::Status      : {
        if (i1->counterPart() != i2->counterPart())
            return boolCompare(i1->counterPart(), i2->counterPart());
        else if (i1->alternateId() != i2->alternateId())
            return uintCompare(i1->alternateId(), i2->alternateId());
        else if (i1->alternate() != i2->alternate())
            return boolCompare(i1->alternate(), i2->alternate());
        else
            return int(i1->status()) - int(i2->status());
    }
    case Document::Picture     :
    case Document::PartNo      : return Utility::naturalCompare(i1->itemId(),
                                                                i2->itemId());
    case Document::Description : return Utility::naturalCompare(i1->itemName(),
                                                                i2->itemName());

    case Document::Color       : return i1->colorName().localeAwareCompare(i2->colorName());
    case Document::Category    : return i1->categoryName().localeAwareCompare(i2->categoryName());
    case Document::ItemType    : return i1->itemTypeName().localeAwareCompare(i2->itemTypeName());

    case Document::Comments    : return i1->comments().localeAwareCompare(i2->comments());
    case Document::Remarks     : return i1->remarks().localeAwareCompare(i2->remarks());

    case Document::LotId       : return uintCompare(i1->lotId(), i2->lotId());
    case Document::Quantity    : return i1->quantity() - i2->quantity();
    case Document::Bulk        : return i1->bulkQuantity() - i2->bulkQuantity();
    case Document::Price       : return doubleCompare(i1->price(), i2->price());
    case Document::Total       : return doubleCompare(i1->total(), i2->total());
    case Document::Sale        : return i1->sale() - i2->sale();
    case Document::Condition   : {
        if (i1->condition() == i2->condition())
            return int(i1->subCondition()) - int(i2->subCondition());
        else
            return int(i1->condition()) - int(i2->condition());
    }
    case Document::TierQ1      : return i1->tierQuantity(0) - i2->tierQuantity(0);
    case Document::TierQ2      : return i1->tierQuantity(1) - i2->tierQuantity(1);
    case Document::TierQ3      : return i1->tierQuantity(2) - i2->tierQuantity(2);
    case Document::TierP1      : return doubleCompare(i1->tierPrice(0), i2->tierPrice(0));
    case Document::TierP2      : return doubleCompare(i1->tierPrice(1), i2->tierPrice(1));
    case Document::TierP3      : return doubleCompare(i1->tierPrice(2), i2->tierPrice(2));
    case Document::Retain      : return boolCompare(i1->retain(), i2->retain());
    case Document::Stockroom   : return int(i1->stockroom()) - int(i2->stockroom());
    case Document::Reserved    : return i1->reserved().compare(i2->reserved());
    case Document::Weight      : return doubleCompare(i1->weight(), i2->weight());
    case Document::YearReleased: return i1->itemYearReleased() - i2->itemYearReleased();
    case Document::PriceOrig   : return doubleCompare(i1->origPrice(), i2->origPrice());
    case Document::PriceDiff   : return doubleCompare((i1->price() - i1->origPrice()), (i2->price() - i2->origPrice()));
    case Document::QuantityOrig: return i1->origQuantity() - i2->origQuantity();
    case Document::QuantityDiff: return (i1->quantity() - i1->origQuantity()) - (i2->quantity() - i2->origQuantity());
    }
    return false;
}

Document::ItemList DocumentProxyModel::sortItemList(const Document::ItemList &list) const
{
    Document::ItemList result(list);
    qParallelSort(result.begin(), result.end(), [this](const auto &i1, const auto &i2) {
        return index(i1).row() < index(i2).row();
    });
    return result;
}

#include "moc_document.cpp"
