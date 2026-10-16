// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "NetLinkUi.h"
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTranslator>

namespace {
struct Entry { QString source, italian; };
const QHash<QString,Entry>& catalog()
{
    // Immutable and initialized once: no settings reads, disk I/O or locks on
    // every tooltip. Aliases accept legacy Italian text without changing IDs.
    static const auto entries=[] {
        QHash<QString,Entry> result;
        QFile file(QStringLiteral(":/core/netlink-ui.json"));
        if(!file.open(QIODevice::ReadOnly)) {qCritical("NetLink UI catalog unavailable");return result;}
        for(const auto& item:QJsonDocument::fromJson(file.readAll()).array()) {
            const auto object=item.toObject();
            const Entry entry{object.value(QStringLiteral("source")).toString(),object.value(QStringLiteral("it")).toString()};
            if(entry.source.isEmpty() || entry.italian.isEmpty()) continue;
            result.insert(entry.source,entry);result.insert(entry.italian,entry);
        }
        return result;
    }();
    return entries;
}
}
QString NetLinkUi::text(const QString& source)
{
    const auto entry=catalog().value(source,Entry{source,source});
    const auto utf8=entry.source.toUtf8();
    const auto translated=QCoreApplication::translate("NetLink",utf8.constData());
    if(translated!=entry.source) return translated;
    return QLocale().language()==QLocale::Italian?entry.italian:entry.source;
}
QString NetLinkUi::text(const char* source) { return text(QString::fromUtf8(source)); }

namespace {
class StandardButtonsTranslator final : public QTranslator {
public:
    explicit StandardButtonsTranslator(QObject* parent) : QTranslator(parent) {}
    bool isEmpty() const override {return false;}
    QString translate(const char* context,const char* source,const char*,int) const override {
        const QByteArray name(context);
        if(QLocale().language()!=QLocale::Italian ||
           (name!="QPlatformTheme" && name!="QDialogButtonBox" && name!="QMessageBox")) return {};
        const auto translated=NetLinkUi::text(source);
        return translated==QString::fromUtf8(source)?QString():translated;
    }
};
}
void NetLinkUi::installStandardTranslations()
{
    auto app=QCoreApplication::instance();
    if(!app || app->findChild<QTranslator*>(QStringLiteral("netlinkStandardButtons"))) return;
    auto translator=new StandardButtonsTranslator(app);
    translator->setObjectName(QStringLiteral("netlinkStandardButtons"));
    QCoreApplication::installTranslator(translator);
}
