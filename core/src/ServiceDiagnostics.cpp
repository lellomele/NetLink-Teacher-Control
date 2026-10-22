// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "ServiceDiagnostics.h"
#include "NetLinkUi.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace {
constexpr int MaximumPayload=2048;
constexpr qint64 MaximumFrameAge=604800000; // Saturate at seven days, not "never".
bool integer(const QJsonValue& value,qint64 minimum,qint64 maximum) {
    const auto number=value.toDouble(-2);
    return value.isDouble() && std::isfinite(number) && number>=minimum && number<=maximum && std::floor(number)==number;
}
QString nonce(const FeatureMessage& message) {
    const auto value=message.argument(0);
    if(value.userType()!=QMetaType::QString) return {};
    const auto text=value.toString();
    return text.size()==36 && !QUuid(text).isNull()?text:QString{};
}
}
QUuid ServiceDiagnostics::uid() {return QUuid(QStringLiteral("4ca580de-fc41-4f68-a8a7-ded5f05cba29"));}
bool ServiceDiagnostics::validQuery(const FeatureMessage& message) {
    return message.featureUid()==uid() && message.command()==Query && message.arguments().size()==2 &&
        !nonce(message).isEmpty() && message.argument(1).userType()==QMetaType::Int && message.argument(1).toInt()==1;
}
QByteArray ServiceDiagnostics::encode(const Snapshot& snapshot) {
    return QJsonDocument(QJsonObject{{"schema",1},{"plugin",snapshot.plugin.left(100)},
        {"external",snapshot.external},{"internalPort",snapshot.internalPort},{"backend",int(snapshot.backend)},
        {"frameAgeMs",double(qBound<qint64>(-1,snapshot.frameAgeMs,MaximumFrameAge))}}).toJson(QJsonDocument::Compact);
}
bool ServiceDiagnostics::decode(const QByteArray& bytes,Snapshot* snapshot) {
    if(!snapshot || bytes.isEmpty() || bytes.size()>MaximumPayload) return false;
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError || !document.isObject()) return false;
    const auto object=document.object();const auto plugin=object.value("plugin");
    if(!integer(object.value("schema"),1,1) || !plugin.isString() || plugin.toString().isEmpty() || plugin.toString().size()>100 ||
       !object.value("external").isBool() || !integer(object.value("internalPort"),0,65535) ||
       !integer(object.value("backend"),0,4) || !integer(object.value("frameAgeMs"),-1,MaximumFrameAge)) return false;
    for(const auto c:plugin.toString()) if(c.category()==QChar::Other_Control || c.category()==QChar::Other_Format) return false;
    Snapshot result;result.plugin=plugin.toString();result.external=object.value("external").toBool();
    result.internalPort=object.value("internalPort").toInt();result.backend=Backend(object.value("backend").toInt());
    result.frameAgeMs=qint64(object.value("frameAgeMs").toDouble());*snapshot=result;return true;
}
QString ServiceDiagnostics::tooltip(const Snapshot& s,qint64 elapsedMs) {
    QString backend;
    switch(s.backend) {
    case Backend::Unchecked: backend=NetLinkUi::text("not checked");break;
    case Backend::TcpAvailable: backend=NetLinkUi::text("TCP available");break;
    case Backend::Ready: backend=NetLinkUi::text("VNC ready");break;
    case Backend::Connecting: backend=NetLinkUi::text("waiting for VNC");break;
    case Backend::Unreachable: backend=NetLinkUi::text("not reachable");break;
    }
    const auto video=s.frameAgeMs<0?NetLinkUi::text("No video frame observed on this connection"):
        NetLinkUi::text("Frame forwarded %1 s ago on this connection").arg((s.frameAgeMs+qMax<qint64>(0,elapsedMs))/1000);
    auto result=QStringLiteral("<br><b>%1</b><br>%2<br>%3<br>%4<br>%5")
        .arg(NetLinkUi::text("Client service diagnostics"),
             NetLinkUi::text("VNC plugin: %1 (%2)").arg(s.plugin,NetLinkUi::text(s.external?"external":"built-in")).toHtmlEscaped(),
             NetLinkUi::text("Internal VNC port: %1").arg(s.internalPort?QString::number(s.internalPort):NetLinkUi::text("unavailable")).toHtmlEscaped(),
             NetLinkUi::text("VNC backend: %1").arg(backend).toHtmlEscaped(),
             NetLinkUi::text("Video: %1").arg(video).toHtmlEscaped());
    if(s.backend==Backend::Unreachable)
        result+=QStringLiteral("<br><b>%1</b>").arg(NetLinkUi::text("VNC backend unavailable: check the plugin and internal port on the client.").toHtmlEscaped());
    result+=QStringLiteral("<br><small>%1</small>").arg(NetLinkUi::text("Forwarding is not proof of display; an unchanged desktop may send no new frames.").toHtmlEscaped());
    return result;
}
void ServiceDiagnostics::ClientStatus::reset(bool connected) {
    m_phase=connected?Phase::Waiting:Phase::Disconnected;m_snapshot={};m_pending.clear();
    m_deadline=-1;m_nextQuery=0;m_received=-1;
}
QString ServiceDiagnostics::ClientStatus::request(qint64 now) {
    if(m_phase==Phase::Disconnected || !m_pending.isEmpty() || now<m_nextQuery) return {};
    m_pending=QUuid::createUuid().toString(QUuid::WithoutBraces);m_deadline=now+ReplyTimeout;
    return m_pending;
}
bool ServiceDiagnostics::ClientStatus::expire(qint64 now) {
    if((!m_pending.isEmpty() && now>=m_deadline) ||
       (m_phase==Phase::Ready && now-m_received>=MaximumAge)) {
        m_phase=Phase::Unavailable;m_snapshot={};m_pending.clear();m_deadline=-1;m_received=-1;
        m_nextQuery=now+RetryInterval;return true;
    }
    return false;
}
bool ServiceDiagnostics::ClientStatus::receive(const FeatureMessage& message,qint64 now) {
    if(m_phase==Phase::Disconnected || m_pending.isEmpty() || message.featureUid()!=uid() ||
       message.command()!=Reply || nonce(message)!=m_pending) return false;
    if(now>=m_deadline) return expire(now);
    m_pending.clear();m_deadline=-1;
    if(message.arguments().size()!=2 || message.argument(1).userType()!=QMetaType::QByteArray ||
       !decode(message.argument(1).toByteArray(),&m_snapshot)) {
        m_snapshot={};m_phase=Phase::Invalid;m_received=-1;m_nextQuery=now+RetryInterval;return true;
    }
    m_phase=Phase::Ready;m_received=now;m_nextQuery=now+QueryInterval;return true;
}
QString ServiceDiagnostics::ClientStatus::tooltip(qint64 now) const {
    if(m_phase==Phase::Ready && now-m_received<MaximumAge) return ServiceDiagnostics::tooltip(m_snapshot,now-m_received);
    QString text;
    if(m_phase==Phase::Disconnected) text=NetLinkUi::text("Service diagnostics unavailable: NetLink is not connected.");
    else if(m_phase==Phase::Waiting) text=NetLinkUi::text("Waiting for client service diagnostics...");
    else if(m_phase==Phase::Invalid) text=NetLinkUi::text("Service diagnostics unavailable: invalid or unsupported reply.");
    else text=NetLinkUi::text("Service diagnostics unavailable: older client, expired data or no reply.");
    return QStringLiteral("<br>%1").arg(text.toHtmlEscaped());
}
