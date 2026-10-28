/*
 * ConfigurationLocalStore.cpp - local configuration repository
 * Copyright (c) 2009-2022 Tobias Junghans <tobydox@veyon.io>
 * Copyright (C) 2026 prof. Ing. Raffaele Mele
 * This file is part of Veyon - https://veyon.io
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <memory>
#include "Configuration/LocalStore.h"
#include "Configuration/Object.h"
#include "Configuration/SettingsTransaction.h"
#include "NetLinkUi.h"

namespace Configuration {
using SettingsTransaction::Values;
namespace {
struct LockedSettings {
    // QSettings may retry a deferred write in its destructor: release it BEFORE the lock.
    std::unique_ptr<SettingsLock> lock;
    std::unique_ptr<QSettings> settings;
    explicit LockedSettings(QSettings* source):settings(source) {lock.reset(new SettingsLock(*source));}
};
}
LocalStore::LocalStore(Scope scope):Store(Store::LocalBackend,scope){}
static QVariant serialized(const QVariant& value) {
    const auto type=value.userType();
    if(type==QMetaType::QJsonArray || type==QMetaType::QJsonObject) {
        const QJsonObject json=type==QMetaType::QJsonArray?QJsonObject{{QStringLiteral("a"),value.toJsonArray()}}:QJsonObject{{QStringLiteral("o"),value.toJsonObject()}};
        return QStringLiteral("@JsonValue(%1)").arg(QString::fromLatin1(QJsonDocument(json).toJson(QJsonDocument::Compact).toBase64()));
    }
    if(QMetaType(type).flags().testFlag(QMetaType::IsEnumeration)) return value.toInt();
    return value;
}
static bool flatten(const Object::DataMap& data,Values& values,const QString& parent={}) {
    for(auto it=data.cbegin();it!=data.cend();++it) {
        const auto key=parent+it.key();if(SettingsTransaction::reserved(key)) return false;
        if(it.value().type()==QVariant::Map) {if(!flatten(it.value().toMap(),values,key+QLatin1Char('/'))) return false;}
        else values.insert(key,serialized(it.value()));
    }
    return true;
}
static bool decode(const Values& values,Object& object,QString& error) {
    for(auto it=values.cbegin();it!=values.cend();++it) {
        auto value=it.value();const auto string=value.toString();
        if(string.startsWith(QStringLiteral("@JsonValue(")) && string.endsWith(QLatin1Char(')'))) {
            QJsonParseError parse;const auto document=QJsonDocument::fromJson(QByteArray::fromBase64(string.mid(11,string.size()-12).toLatin1()),&parse);
            const auto json=document.object();
            if(parse.error!=QJsonParseError::NoError || (!json.value(QStringLiteral("a")).isArray() && !json.value(QStringLiteral("o")).isObject())) {
                error=NetLinkUi::text("Configuration contains invalid data (%1). No partial configuration was loaded.").arg(it.key());return false;
            }
            value=json.value(QStringLiteral("a")).isArray()?QVariant(json.value(QStringLiteral("a")).toArray()):QVariant(json.value(QStringLiteral("o")).toObject());
        }
        const auto slash=it.key().lastIndexOf(QLatin1Char('/'));
        object.setValue(it.key().mid(slash+1),value,slash<0?QString{}:it.key().left(slash));
    }
    return true;
}
static QString busy() {return NetLinkUi::text("Configuration is busy or its lock is inaccessible. Nothing was saved. Close other configuration operations and retry.");}
void LocalStore::load(Object* object) {
    m_error.clear();LockedSettings transaction(createSettingsObject());auto* settings=transaction.settings.get();
    Values raw;Object loaded;
    if(!transaction.lock->locked()) m_error=busy();
    else if(SettingsTransaction::recover(*settings,m_error) && SettingsTransaction::read(*settings,raw,m_error)) decode(raw,loaded,m_error);
    object->setProperty("netlinkConfigurationReadError",m_error);
    if(!m_error.isEmpty()) {object->m_localBaselineValid=false;return;}
    object->m_data=loaded.data();object->m_localBaseline=raw;
    object->m_localStoreIdentity=SettingsTransaction::identity(*settings);object->m_localBaselineValid=true;
    Q_EMIT object->configurationChanged();
}
bool LocalStore::saveSnapshot(const Object& object,Object* updated,QString& error) {
    error.clear();LockedSettings transaction(createSettingsObject());auto* settings=transaction.settings.get();
    if(!transaction.lock->locked()) {error=busy();return false;}
    Values current,desired,merged;
    if(!SettingsTransaction::recover(*settings,error) || !SettingsTransaction::read(*settings,current,error)) return false;
    const auto identity=SettingsTransaction::identity(*settings);
    if((object.m_localBaselineValid && object.m_localStoreIdentity!=identity) || (!object.m_localBaselineValid && !current.isEmpty()) || !object.property("netlinkConfigurationReadError").toString().isEmpty()) {
        error=NetLinkUi::text("Configuration has no valid saved baseline. Export pending edits, reload configuration and retry. Nothing was saved.");return false;
    }
    if(!flatten(object.data(),desired)) {error=NetLinkUi::text("Reserved configuration fields cannot be edited. Nothing was saved.");return false;}
    if(!SettingsTransaction::merge(object.m_localBaselineValid?object.m_localBaseline:Values{},desired,current,merged,error)) return false;
    Object decoded;if(!decode(merged,decoded,error)) return false;
    if(!SettingsTransaction::commit(*settings,current,merged,error)) return false;
    if(updated) {
        const bool changed=updated->data()!=decoded.data();updated->m_data=decoded.data();
        updated->m_localBaseline=merged;updated->m_localStoreIdentity=identity;updated->m_localBaselineValid=true;
        updated->setProperty("netlinkConfigurationReadError",QString{});
        if(changed) Q_EMIT updated->configurationChanged();
    }
    return true;
}
bool LocalStore::save(Object& object,QString& error) {const auto ok=saveSnapshot(object,&object,error);m_error=error;return ok;}
void LocalStore::flush(const Object* object) {if(!saveSnapshot(*object,nullptr,m_error)) qWarning()<<m_error;}
bool LocalStore::compareAndWriteValue(const QString& key,const QVariant& expected,const QVariant& value,const QString& parent,QString& error) {
    error.clear();const auto path=parent.isEmpty()?key:parent+QLatin1Char('/')+key;
    if(key.isEmpty() || key.contains(QLatin1Char('/')) || SettingsTransaction::reserved(path)) {error=NetLinkUi::text("Invalid configuration field. Nothing was saved.");return false;}
    LockedSettings transaction(createSettingsObject());auto* settings=transaction.settings.get();
    if(!transaction.lock->locked()) {error=busy();return false;}
    Values current;if(!SettingsTransaction::recover(*settings,error) || !SettingsTransaction::read(*settings,current,error)) return false;
    if(current.value(path,serialized(expected))!=serialized(expected)) {error=NetLinkUi::text("The computer list changed in another application. Nothing was imported. Reload the Console and retry.");return false;}
    auto after=current;after.insert(path,serialized(value));const auto ok=SettingsTransaction::commit(*settings,current,after,error);m_error=error;return ok;
}
bool LocalStore::writeValue(const QString& key,const QVariant& value,const QString& parent) {
    m_error.clear();const auto path=parent.isEmpty()?key:parent+QLatin1Char('/')+key;
    if(key.isEmpty() || key.contains(QLatin1Char('/')) || SettingsTransaction::reserved(path)) return false;
    LockedSettings transaction(createSettingsObject());auto* settings=transaction.settings.get();
    if(!transaction.lock->locked()) {m_error=busy();return false;}
    Values current;if(!SettingsTransaction::recover(*settings,m_error) || !SettingsTransaction::read(*settings,current,m_error)) return false;
    auto after=current;after.insert(path,serialized(value));return SettingsTransaction::commit(*settings,current,after,m_error);
}
bool LocalStore::clearChecked(QString& error) {
    error.clear();LockedSettings transaction(createSettingsObject());auto* settings=transaction.settings.get();
    if(!transaction.lock->locked()) {error=busy();return false;}
    Values current;if(!SettingsTransaction::recover(*settings,error) || !SettingsTransaction::read(*settings,current,error)) return false;
    const auto ok=SettingsTransaction::commit(*settings,current,{},error);m_error=error;return ok;
}
void LocalStore::clear() {if(!clearChecked(m_error)) qWarning()<<m_error;}
bool LocalStore::isWritable() const {const std::unique_ptr<QSettings> settings(createSettingsObject());return settings->isWritable();}
QSettings* LocalStore::createSettingsObject() const {return new QSettings(scope()==System?QSettings::SystemScope:QSettings::UserScope,QSettings().organizationName(),QSettings().applicationName());}
}
