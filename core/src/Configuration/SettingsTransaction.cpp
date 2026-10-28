// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "SettingsTransaction.h"
#include "NetLinkUi.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QDataStream>
#include <QBuffer>
#include <QLockFile>
#include <QSet>
#ifdef Q_OS_WIN
#include <windows.h>
#include <sddl.h>
#include <wincrypt.h>
#endif

namespace Configuration {
struct SettingsLock::State {
#ifdef Q_OS_WIN
    HANDLE handle=nullptr;bool owned=false;
#else
    std::unique_ptr<QLockFile> file;
#endif
};
SettingsLock::SettingsLock(const QSettings& settings):m_state(new State) {
#ifdef Q_OS_WIN
    const auto hash=QCryptographicHash::hash(SettingsTransaction::identity(settings).toUtf8(),QCryptographicHash::Sha256).toHex();
    const QString name=QStringLiteral("Global\\NetLink.Configuration.")+QString::fromLatin1(hash);
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    // Admin/SYSTEM manage it; ordinary authenticated readers may only wait/release.
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x00100001;;;AU)",SDDL_REVISION_1,&descriptor,nullptr)) return;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
    m_state->handle=CreateMutexExW(&attributes,reinterpret_cast<LPCWSTR>(name.utf16()),0,SYNCHRONIZE|MUTEX_MODIFY_STATE);LocalFree(descriptor);
    if(m_state->handle) {const auto result=WaitForSingleObject(m_state->handle,0);m_state->owned=result==WAIT_OBJECT_0 || result==WAIT_ABANDONED;}
#else
    m_state->file.reset(new QLockFile(settings.fileName()+QStringLiteral(".netlink-transaction.lock")));
    m_state->file->tryLock(0);
#endif
}
SettingsLock::~SettingsLock() {
#ifdef Q_OS_WIN
    if(m_state->owned) ReleaseMutex(m_state->handle);
    if(m_state->handle) CloseHandle(m_state->handle);
#endif
}
bool SettingsLock::locked() const {
#ifdef Q_OS_WIN
    return m_state->owned;
#else
    return m_state->file && m_state->file->isLocked();
#endif
}
namespace SettingsTransaction {
static const QString Journal=QStringLiteral("_NetLinkTransaction/RecoveryV1");
static constexpr int MaximumJournal=32*1024*1024;
bool reserved(const QString& key) {return key.startsWith(QStringLiteral("_NetLinkTransaction/"),Qt::CaseInsensitive) || key.compare(QStringLiteral("_NetLinkTransaction"),Qt::CaseInsensitive)==0;}
QString identity(const QSettings& settings) {
    auto path=settings.fileName();
#ifdef Q_OS_WIN
    path=path.toCaseFolded();
#endif
    return QString::number(int(settings.format()))+QLatin1Char(':')+path;
}
bool read(QSettings& settings,Values& values,QString& error) {
    settings.setFallbacksEnabled(false);settings.sync();
    if(settings.status()!=QSettings::NoError) {error=NetLinkUi::text("Could not read configuration. Nothing was saved.");return false;}
    values.clear();for(const auto& key:settings.allKeys()) if(!reserved(key)) values.insert(key,settings.value(key));
    return true;
}
static bool apply(QSettings& settings,const Values& from,const Values& to) {
    for(auto it=to.cbegin();it!=to.cend();++it) if(!from.contains(it.key()) || from.value(it.key())!=it.value()) settings.setValue(it.key(),it.value());
    // Remove only explicitly obsolete values, never clear the settings tree first.
    for(auto it=from.cbegin();it!=from.cend();++it) if(!to.contains(it.key())) settings.remove(it.key());
    settings.sync();return settings.status()==QSettings::NoError;
}
static QByteArray encode(const Values& before,const Values& after) {
    QByteArray payload;QDataStream stream(&payload,QIODevice::WriteOnly);stream.setVersion(QDataStream::Qt_5_12);stream<<before<<after;
    if(stream.status()!=QDataStream::Ok || payload.size()>MaximumJournal-36) return {};
    return QByteArray("NLT1",4)+QCryptographicHash::hash(payload,QCryptographicHash::Sha256)+payload;
}
static bool decode(const QByteArray& bytes,Values& before,Values& after) {
    if(bytes.size()<36 || bytes.size()>MaximumJournal || bytes.left(4)!=QByteArray("NLT1",4)) return false;
    const auto payload=bytes.mid(36);if(QCryptographicHash::hash(payload,QCryptographicHash::Sha256)!=bytes.mid(4,32)) return false;
    QDataStream stream(payload);stream.setVersion(QDataStream::Qt_5_12);stream>>before>>after;
    if(stream.status()!=QDataStream::Ok || !stream.atEnd() || before.size()>65536 || after.size()>65536) return false;
    for(const auto& map:{before,after}) for(auto it=map.cbegin();it!=map.cend();++it) if(it.key().isEmpty() || reserved(it.key())) return false;
    return true;
}
static QByteArray protect(const QSettings& settings,const QByteArray& bytes,bool decrypt) {
#ifdef Q_OS_WIN
    if(settings.format()==QSettings::NativeFormat) {
        QByteArray input=bytes;
        if(decrypt) {if(!input.startsWith("NLE1")) return {};input.remove(0,4);}
        DATA_BLOB source{DWORD(input.size()),reinterpret_cast<BYTE*>(input.data())},output{};
        const bool ok=decrypt?CryptUnprotectData(&source,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output):
            CryptProtectData(&source,L"NetLink configuration recovery",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output);
        if(!ok) return {};
        QByteArray result(reinterpret_cast<const char*>(output.pbData),int(output.cbData));
        SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
        return decrypt?result:QByteArray("NLE1",4)+result;
    }
#else
    Q_UNUSED(settings);Q_UNUSED(decrypt);
#endif
    return bytes;
}
bool recover(QSettings& settings,QString& error) {
    Values current;if(!read(settings,current,error)) return false;
    if(!settings.contains(Journal)) return true;
    const auto raw=settings.value(Journal).toByteArray();
    if(raw.size()>MaximumJournal+16384) {error=NetLinkUi::text("Configuration recovery data is invalid. No settings were changed. Contact the administrator.");return false;}
    Values before,after;if(!decode(protect(settings,raw,true),before,after)) {error=NetLinkUi::text("Configuration recovery is unavailable. Reopen Configurator as administrator with the Windows account that saved it. No partial configuration was loaded.");return false;}
    if(current!=before && current!=after) {
        if(!apply(settings,current,before)) {error=NetLinkUi::text("Configuration recovery is pending. Run Configurator as administrator; no partial configuration was loaded.");return false;}
        Values checked;if(!read(settings,checked,error) || checked!=before) {error=NetLinkUi::text("Could not verify configuration recovery. No partial configuration was loaded.");return false;}
    }
    // Recovery can require the Windows identity that created the protected record.
    // A completed before/after state is never rolled back unnecessarily.
    return true;
}
QByteArray recoveryRecord(const QSettings& settings,const Values& before,const Values& after) {
    const auto bytes=encode(before,after);return bytes.isEmpty()?QByteArray{}:protect(settings,bytes,false);
}
bool commit(QSettings& settings,const Values& before,const Values& after,QString& error) {
    if(before==after) return true;
    const auto journal=recoveryRecord(settings,before,after);
    if(journal.isEmpty()) {error=NetLinkUi::text("Could not prepare configuration recovery. Nothing was saved.");return false;}
    settings.setValue(Journal,journal);settings.sync();
    if(settings.status()!=QSettings::NoError || settings.value(Journal).toByteArray()!=journal) {error=NetLinkUi::text("Could not prepare configuration recovery. Nothing was saved.");return false;}
    if(!apply(settings,before,after)) {error=NetLinkUi::text("Configuration write failed. Recovery data was retained; reopen Configurator as administrator before retrying.");return false;}
    Values checked;if(!read(settings,checked,error) || checked!=after) {error=NetLinkUi::text("Configuration verification failed. Recovery data was retained.");return false;}
    settings.remove(Journal);settings.sync();
    if(settings.status()!=QSettings::NoError) {error=NetLinkUi::text("Configuration was written but recovery cleanup failed. Reopen Configurator before retrying.");return false;}
    return true;
}
bool merge(const Values& baseline,const Values& desired,const Values& current,Values& merged,QString& error) {
    merged=current;QSet<QString> keys;for(auto it=baseline.cbegin();it!=baseline.cend();++it) keys.insert(it.key());for(auto it=desired.cbegin();it!=desired.cend();++it) keys.insert(it.key());
    const auto equal=[](const Values& a,const Values& b,const QString& key){return a.contains(key)==b.contains(key) && a.value(key)==b.value(key);};
    for(const auto& key:keys) {
        if(equal(baseline,desired,key)) continue;
        if(!equal(current,baseline,key) && !equal(current,desired,key)) {error=NetLinkUi::text("Configuration changed in another application (%1). Nothing was saved. Export your pending edits, reload configuration and retry.").arg(key);return false;}
        if(desired.contains(key)) merged.insert(key,desired.value(key));else merged.remove(key);
    }
    return true;
}
}
}
