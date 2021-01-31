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

#include <QApplication>
#include <QStringList>
#include <QScopedPointer>
#include <QPointer>
#include <QPlainTextEdit>

class FrameWork;
QT_FORWARD_DECLARE_CLASS(QTranslator)


class Application : public QObject
{
    Q_OBJECT
public:
    Application(int &argc, char **argv);
    ~Application() override;

    static Application *inst() { return s_inst; }

    void enableEmitOpenDocument(bool b = true);

    QString applicationUrl() const;

    bool isOnline() const;

    QStringList externalResourceSearchPath(const QString &subdir = QString()) const;

    QPlainTextEdit *logWidget() const;

public slots:
    void updateTranslations();

signals:
    void openDocument(const QString &);
    void onlineStateChanged(bool isOnline);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private slots:
    void doEmitOpenDocument();
    void clientMessage();
    void checkNetwork();

private:
    bool isClient(int timeout = 1000);
    void setupLogging();
    void setIconTheme();

    bool initBrickLink();
    void exitBrickLink();

private:
    QStringList m_files_to_open;
    bool m_enable_emit = false;

    bool m_online = true;
    qreal m_default_fontsize = 0;

    QScopedPointer<QTranslator> m_trans_qt;
    QScopedPointer<QTranslator> m_trans_brickstore_en;
    QScopedPointer<QTranslator> m_trans_brickstore;

    QPointer<QPlainTextEdit> m_logWidget = nullptr;
    QtMessageHandler m_defaultMessageHandler = nullptr;
    int m_logGuiLock = 0;

    static Application *s_inst;

    friend class LogHighlighter;
};
