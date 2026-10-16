// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "HealthDiagnostics.h"
#include "NetLinkIdentity.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QXmlStreamReader>
#include <vector>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winevt.h>
#endif

namespace Companion {
namespace {
bool suiteApplication(const QString& name) {
    return NetLinkIdentity::isDiagnosticApplication(name);
}
QString basename(QString path) {return path.replace('\\','/').section('/',-1).left(256);}
}

QJsonObject crashEvent(const QByteArray& xml)
{
    if(xml.size()>128*1024) return {};
    QXmlStreamReader reader(xml);
    QStringList path, fields;
    QMap<QString,QString> named;
    QString provider, timestamp, id, data, dataName;
    int nodes=0;
    while(!reader.atEnd()) {
        const auto token=reader.readNext();
        if(token==QXmlStreamReader::DTD || token==QXmlStreamReader::EntityReference) return {};
        if(token==QXmlStreamReader::StartElement) {
            if(++nodes>512 || path.size()>=16) return {};
            path.append(reader.name().toString());
            if(path==QStringList{"Event","System","Provider"}) provider=reader.attributes().value("Name").toString();
            if(path==QStringList{"Event","System","TimeCreated"}) timestamp=reader.attributes().value("SystemTime").toString();
            if(path==QStringList{"Event","EventData","Data"}) {data.clear();dataName=reader.attributes().value("Name").toString();}
        } else if(token==QXmlStreamReader::Characters) {
            if(path==QStringList{"Event","System","EventID"}) id+=reader.text();
            else if(path==QStringList{"Event","EventData","Data"}) data+=reader.text();
            if(data.size()>4096 || id.size()>16) return {};
        } else if(token==QXmlStreamReader::EndElement) {
            if(path==QStringList{"Event","EventData","Data"}) {
                if(fields.size()>=32) return {};
                fields.append(data); if(!dataName.isEmpty()) named[dataName]=data;
            }
            if(path.isEmpty()) return {};
            path.removeLast();
        }
    }
    if(reader.hasError() || provider!="Application Error" || id!="1000") return {};
    const auto field=[&](const QString& name,int index) {return named.contains(name)?named.value(name):fields.value(index);};
    const auto application=basename(field("AppName",0));
    if(application.compare("dwm.exe",Qt::CaseInsensitive)!=0 && !suiteApplication(application)) return {};
    const auto modulePath=field("ModulePath",11).replace('\\','/');
    const auto module=basename(field("ModuleName",3));
    const bool withinSuite=modulePath.contains("/Veyon School/",Qt::CaseInsensitive) || modulePath.contains("/NetLink/",Qt::CaseInsensitive);
    const auto time=QDateTime::fromString(timestamp,Qt::ISODateWithMs);
    const auto exception=field("ExceptionCode",6).left(16),offset=field("FaultingOffset",7).left(32);
    return {{"application",application},{"module",module},{"exception",exception},{"offset",offset},
        {"timeUtc",time.isValid()?time.toUTC().toString(Qt::ISODateWithMs):QString()},
        {"moduleFromSuitePath",withinSuite},{"kind",application.compare("dwm.exe",Qt::CaseInsensitive)==0?"desktop-crash":"suite-crash"}};
}

QJsonObject collectHealth()
{
    QJsonObject result{{"hours",24},{"scanLimit",64},{"eventLogAvailable",false}};
#ifdef Q_OS_WIN
    QElapsedTimer elapsed;elapsed.start();
    const auto query=EvtQuery(nullptr,L"Application",
        L"*[System[Provider[@Name='Application Error'] and (EventID=1000) and TimeCreated[timediff(@SystemTime) <= 86400000]]]",
        EvtQueryChannelPath|EvtQueryReverseDirection);
    QJsonArray events;int scanned=0;
    if(query) {
        result["eventLogAvailable"]=true;
        while(scanned<64 && events.size()<20 && elapsed.elapsed()<1500) {
            EVT_HANDLE event=nullptr;DWORD returned=0;
            if(!EvtNext(query,1,&event,0,0,&returned)) {
                const auto status=GetLastError();
                if(status!=ERROR_NO_MORE_ITEMS && status!=ERROR_TIMEOUT) result["queryError"]=int(status);
                break;
            }
            if(!returned || !event) break;
            ++scanned;DWORD needed=0,properties=0;
            EvtRender(nullptr,event,EvtRenderEventXml,0,nullptr,&needed,&properties);
            if(GetLastError()==ERROR_INSUFFICIENT_BUFFER && needed>0 && needed<=65536) {
                std::vector<wchar_t> buffer((needed+sizeof(wchar_t)-1)/sizeof(wchar_t)+1,0);
                if(EvtRender(nullptr,event,EvtRenderEventXml,needed,buffer.data(),&needed,&properties)) {
                    const auto item=crashEvent(QString::fromWCharArray(buffer.data()).toUtf8());
                    if(!item.isEmpty()) events.append(item);
                } else result["renderError"]=int(GetLastError());
            }
            EvtClose(event);
        }
        EvtClose(query);
    } else result["queryError"]=int(GetLastError());
    result["events"]=events;result["scannedEvents"]=scanned;
    result["limitReached"]=scanned>=64 || events.size()>=20 || elapsed.elapsed()>=1500;
    struct Windows {QJsonArray applications;QSet<DWORD> seen;int count=0;} windows;
    EnumWindows([](HWND window,LPARAM parameter)->BOOL {
        auto& state=*reinterpret_cast<Windows*>(parameter);
        if(++state.count>1024) return FALSE;
        if(!IsWindowVisible(window)) return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(!pid || state.seen.contains(pid)) return TRUE;
        auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!process) return TRUE;
        wchar_t path[32768]{};DWORD length=32768;
        const bool read=QueryFullProcessImageNameW(process,0,path,&length);
        CloseHandle(process);
        const auto name=read?basename(QString::fromWCharArray(path,int(length))):QString();
        if(!suiteApplication(name)) return TRUE;
        state.seen.insert(pid);
        state.applications.append(QJsonObject{{"application",name},{"notResponding",bool(IsHungAppWindow(window))}});
        return state.applications.size()<16;
    },reinterpret_cast<LPARAM>(&windows));
    result["applications"]=windows.applications;
    result["collectionMilliseconds"]=elapsed.elapsed();
#else
    result["queryError"]="Diagnostica disponibile solo su Windows";
#endif
    return result;
}
}
