// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "Diagnostics.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>
#include <vector>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <aclapi.h>
#include <sddl.h>
#include <wtsapi32.h>
#include <lm.h>
#include <initguid.h>
#include <netfw.h>
#endif

namespace Companion {
namespace {
QString text(const QJsonValue& value) {
    if(value.isNull() || value.isUndefined()) return "Non disponibile";
    if(value.isString()) return value.toString();
    if(value.isBool()) return value.toBool()?"Sì":"No";
    return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
}
QString list(const QJsonArray& values) {QStringList result;for(const auto& value:values) result << text(value);return result.join(", ");}
void error(QJsonObject& report,const QString& message) {auto errors=report["errors"].toArray();errors.append(message);report["errors"]=errors;}
QJsonArray decodeObjects(const QVariant& input) {
    const auto value=input.toString();
    if(value.size()>512*1024) return {};
    if(value.startsWith("@JsonValue(") && value.endsWith(')'))
        return QJsonDocument::fromJson(QByteArray::fromBase64(value.mid(11,value.size()-12).toLatin1())).object()["a"].toArray();
    return input.toJsonArray();
}
bool safeTree(const QJsonValue& value,int depth,int& nodes) {
    if(depth>12 || ++nodes>20000) return false;
    if(value.isString()) return value.toString().size()<=32768;
    if(value.isArray()) {const auto array=value.toArray();if(array.size()>2048) return false;for(const auto& entry:array) if(!safeTree(entry,depth+1,nodes)) return false;}
    if(value.isObject()) {const auto object=value.toObject();if(object.size()>256) return false;for(auto it=object.begin();it!=object.end();++it) if(it.key().size()>256 || !safeTree(it.value(),depth+1,nodes)) return false;}
    return true;
}
QString label(const QString& key) {
    static const QMap<QString,QString> labels{
        {"os","Sistema operativo"},{"architecture","Architettura"},{"computer","Nome PC"},{"dnsDomain","Dominio DNS"},{"windowsBuild","Build Windows"},{"windowsVersion","Versione Windows"},{"companionVersion","Versione Companion"},{"uptimeSeconds","Secondi dall'avvio"},{"manufacturer","Produttore"},{"model","Modello"},{"joinType","Appartenenza"},{"joinName","Dominio / gruppo di lavoro"},
        {"user","Account del processo"},{"domain","Dominio account (ambiente)"},{"sid","SID account"},{"elevated","Eseguito come amministratore"},{"consoleUser","Utente della console locale"},{"sessionId","Sessione del processo"},{"enabled","Abilitato"},{"denyOnly","Solo negazione"},
        {"name","Nome"},{"description","Descrizione"},{"up","Connessa"},{"loopback","Interfaccia locale loopback"},{"tunnel","Tunnel"},{"index","Indice scheda"},{"dhcp","DHCP IPv4"},{"dhcpServer","Server DHCP IPv4"},{"dnsSuffix","Suffisso DNS"},{"mac","Indirizzo MAC"},{"addresses","Indirizzi / prefissi"},{"address","Indirizzo"},{"dns","Server DNS"},{"gateways","Gateway"},
        {"state","Stato"},{"pid","ID processo"},{"imagePath","Programma del servizio"},{"image","Programma in ascolto"},{"account","Account servizio"},{"startType","Tipo avvio Windows"},{"port","Porta TCP"},{"queryError","Dato non leggibile (codice Windows)"},{"error","Errore di lettura"},{"permissionError","Permessi non verificabili (codice Windows)"},
        {"Authentication/Method","Autenticazione (0=account, 1=chiave)"},{"Authentication/PrivateKeyBaseDir","Directory chiavi private"},{"Authentication/PublicKeyBaseDir","Directory chiavi pubbliche"},{"AccessControl/AccessRestrictedToUserGroups","Accesso limitato ai gruppi"},{"AccessControl/AuthorizedUserGroups","Gruppi autorizzati"},{"AccessControl/AccessControlRulesProcessingEnabled","Regole avanzate di accesso"},{"accessRuleCount","Numero regole avanzate"},{"Network/VeyonServerPort","Porta servizio NetLink"},{"Network/DemoServerPort","Porta dimostrazione"},{"Network/LocalConnectOnly","Consenti solo collegamenti locali"},{"Network/FirewallExceptionEnabled","Eccezione firewall configurata"},{"School/DiscoveryEnabled","Discovery abilitato"},{"School/DiscoveryMaster","Destinatario annunci diretti"},{"Service/MultiSession","Modalità multisessione"},{"Windows/UseInterceptionDriver","Driver Interception configurato"},{"configurationRead","Configurazione di sistema leggibile"},{"registryStatus","Esito lettura registro (0=riuscita)"},
        {"type","Tipo"},{"path","Percorso"},{"ownerSid","SID proprietario / gruppo di accesso"},{"sddl","Descrittore dei permessi Windows"},{"readableByToolUser","Diritto di lettura nel token di Companion"},{"sha256","Impronta SHA-256 del file pubblico"},{"publicError","Chiave pubblica non verificata"},{"activeProfiles","Profili firewall attivi (1=dominio, 2=privato, 4=pubblico)"},{"profile","Profilo Windows (1=dominio, 2=privato, 4=pubblico)"},{"profiles","Profili della regola"},{"protocol","Protocollo (6=TCP, 17=UDP)"},{"application","Programma autorizzato"},{"allow","Regola di autorizzazione"},{"inbound","Traffico in ingresso"},{"missing","Regola non trovata"}
    };
    return labels.value(key,key);
}
QString row(const QString& key,const QString& value) {return "<tr><th align='left'>"+label(key).toHtmlEscaped()+"</th><td>"+value.toHtmlEscaped()+"</td></tr>";}
QString table(const QJsonObject& object) {QString result="<table cellspacing='6'>";for(auto it=object.begin();it!=object.end();++it) result+=row(it.key(),text(it.value()));return result+"</table>";}
bool running(const QJsonArray& services,const QString& name) {for(const auto& value:services){const auto service=value.toObject();if(service["name"]==name) return service["state"]=="running";}return false;}
QStringList addresses(const QJsonObject& report) {
    QStringList result;
    for(const auto& value:report["interfaces"].toArray()) {
        const auto iface=value.toObject();if(!iface["up"].toBool() || iface["loopback"].toBool()) continue;
        for(const auto& ip:iface["addresses"].toArray()) result<<ip.toObject()["address"].toString();
    }
    return result;
}
QJsonArray addressRecords(const QJsonObject& report) {
    QJsonArray result;
    for(const auto& value:report["interfaces"].toArray()) {const auto iface=value.toObject();if(!iface["up"].toBool() || iface["loopback"].toBool()) continue;for(const auto& address:iface["addresses"].toArray()) result.append(address);}
    return result;
}
#ifdef Q_OS_WIN
LPCWSTR wide(const QString& value) {return reinterpret_cast<LPCWSTR>(value.utf16());}
QString sidString(PSID sid) {LPWSTR value=nullptr;if(!ConvertSidToStringSidW(sid,&value)) return {};const auto result=QString::fromWCharArray(value);LocalFree(value);return result;}
QString computerName(COMPUTER_NAME_FORMAT format) {wchar_t buffer[512]{};DWORD count=512;return GetComputerNameExW(format,buffer,&count)?QString::fromWCharArray(buffer,int(count)):QString();}
QJsonObject token() {
    QJsonObject result;HANDLE handle=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&handle)) {result["error"]=int(GetLastError());return result;}
    DWORD needed=0;TOKEN_ELEVATION elevation{};
    if(GetTokenInformation(handle,TokenElevation,&elevation,sizeof(elevation),&needed)) result["elevated"]=bool(elevation.TokenIsElevated);
    GetTokenInformation(handle,TokenUser,nullptr,0,&needed);std::vector<BYTE> user(needed);
    if(needed && GetTokenInformation(handle,TokenUser,user.data(),needed,&needed)) result["sid"]=sidString(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid);
    GetTokenInformation(handle,TokenGroups,nullptr,0,&needed);std::vector<BYTE> groups(needed);QJsonArray groupList;
    if(needed && GetTokenInformation(handle,TokenGroups,groups.data(),needed,&needed)) {
        const auto data=reinterpret_cast<TOKEN_GROUPS*>(groups.data());
        for(DWORD i=0;i<data->GroupCount && i<256;++i) groupList.append(QJsonObject{{"sid",sidString(data->Groups[i].Sid)},{"enabled",bool(data->Groups[i].Attributes&SE_GROUP_ENABLED)},{"denyOnly",bool(data->Groups[i].Attributes&SE_GROUP_USE_FOR_DENY_ONLY)}});
    }
    result["groups"]=groupList;CloseHandle(handle);
    wchar_t name[512]{};DWORD count=512;if(GetUserNameW(name,&count)) result["user"]=QString::fromWCharArray(name);
    result["domain"]=qEnvironmentVariable("USERDOMAIN");
    DWORD session=0;if(ProcessIdToSessionId(GetCurrentProcessId(),&session)) result["sessionId"]=int(session);
    const DWORD active=WTSGetActiveConsoleSessionId();LPWSTR buffer=nullptr;DWORD bytes=0;
    if(active!=0xffffffff && WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,active,WTSUserName,&buffer,&bytes)) {result["consoleUser"]=QString::fromWCharArray(buffer);WTSFreeMemory(buffer);}
    return result;
}
QJsonObject service(SC_HANDLE manager,const wchar_t* name) {
    QJsonObject result{{"name",QString::fromWCharArray(name)}};
    auto handle=OpenServiceW(manager,name,SERVICE_QUERY_STATUS|SERVICE_QUERY_CONFIG);
    if(!handle) {result["queryError"]=int(GetLastError());return result;}
    SERVICE_STATUS_PROCESS status{};DWORD bytes=0;
    if(QueryServiceStatusEx(handle,SC_STATUS_PROCESS_INFO,reinterpret_cast<LPBYTE>(&status),sizeof(status),&bytes)) {
        result["state"]=status.dwCurrentState==SERVICE_RUNNING?"running":status.dwCurrentState==SERVICE_STOPPED?"stopped":"transition";
        result["pid"]=int(status.dwProcessId);
    } else result["queryError"]=int(GetLastError());
    QueryServiceConfigW(handle,nullptr,0,&bytes);
    if(bytes && bytes<65536) {std::vector<BYTE> buffer(bytes);auto config=reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());if(QueryServiceConfigW(handle,config,bytes,&bytes)){result["imagePath"]=QString::fromWCharArray(config->lpBinaryPathName);result["account"]=QString::fromWCharArray(config->lpServiceStartName);result["startType"]=int(config->dwStartType);}}
    CloseServiceHandle(handle);return result;
}
QJsonArray interfaces(QJsonObject& report) {
    QJsonArray result;ULONG size=16384;std::vector<BYTE> buffer(size);ULONG status=0;
    for(int attempt=0;attempt<3;++attempt) {
        status=GetAdaptersAddresses(AF_UNSPEC,GAA_FLAG_INCLUDE_GATEWAYS|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),&size);
        if(status!=ERROR_BUFFER_OVERFLOW) break;
        if(size>4*1024*1024) break;
        buffer.resize(size);
    }
    if(status!=NO_ERROR) {error(report,QString("Schede di rete non leggibili (Windows %1).").arg(status));return result;}
    int count=0;
    for(auto adapter=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());adapter && count++<128;adapter=adapter->Next) {
        QJsonObject item{{"name",QString::fromWCharArray(adapter->FriendlyName)},{"description",QString::fromWCharArray(adapter->Description)},{"up",adapter->OperStatus==IfOperStatusUp},{"loopback",adapter->IfType==IF_TYPE_SOFTWARE_LOOPBACK},{"tunnel",adapter->IfType==IF_TYPE_TUNNEL},{"index",int(adapter->IfIndex)},{"dhcp",bool(adapter->Flags&IP_ADAPTER_DHCP_ENABLED)},{"dnsSuffix",QString::fromWCharArray(adapter->DnsSuffix)}};
        QStringList mac;for(ULONG i=0;i<adapter->PhysicalAddressLength;++i) mac<<QString("%1").arg(adapter->PhysicalAddress[i],2,16,QChar('0'));item["mac"]=mac.join(':');
        QJsonArray ips,dns,gateways;
        int limit=0;for(auto address=adapter->FirstUnicastAddress;address && limit++<128;address=address->Next) ips.append(QJsonObject{{"address",QHostAddress(address->Address.lpSockaddr).toString()},{"prefix",int(address->OnLinkPrefixLength)}});
        limit=0;for(auto address=adapter->FirstDnsServerAddress;address && limit++<32;address=address->Next) dns.append(QHostAddress(address->Address.lpSockaddr).toString());
        limit=0;for(auto address=adapter->FirstGatewayAddress;address && limit++<32;address=address->Next) gateways.append(QHostAddress(address->Address.lpSockaddr).toString());
        if(adapter->Dhcpv4Server.lpSockaddr) item["dhcpServer"]=QHostAddress(adapter->Dhcpv4Server.lpSockaddr).toString();
        item["addresses"]=ips;item["dns"]=dns;item["gateways"]=gateways;result.append(item);
    }
    return result;
}
QString imagePath(DWORD pid) {auto handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!handle) return {};wchar_t buffer[32768]{};DWORD count=32768;const bool success=QueryFullProcessImageNameW(handle,0,buffer,&count);CloseHandle(handle);return success?QString::fromWCharArray(buffer,int(count)):QString();}
QJsonArray listeners(int port,QJsonObject& report) {
    QJsonArray result;
    for(const auto family:{AF_INET,AF_INET6}) {
        ULONG size=0;GetExtendedTcpTable(nullptr,&size,FALSE,family,TCP_TABLE_OWNER_PID_LISTENER,0);
        if(!size || size>4*1024*1024) {error(report,"Inventario porte TCP non disponibile.");continue;}
        std::vector<BYTE> buffer(size);
        const auto status=GetExtendedTcpTable(buffer.data(),&size,FALSE,family,TCP_TABLE_OWNER_PID_LISTENER,0);
        if(status!=NO_ERROR) {error(report,QString("Inventario TCP incompleto (Windows %1).").arg(status));continue;}
        const auto count=*reinterpret_cast<DWORD*>(buffer.data());
        for(DWORD i=0;i<count && i<10000;++i) {
            DWORD localPort=0,pid=0;QString address;
            if(family==AF_INET) {const auto entry=reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data())->table[i];localPort=ntohs(static_cast<u_short>(entry.dwLocalPort));pid=entry.dwOwningPid;address=QHostAddress(ntohl(entry.dwLocalAddr)).toString();}
            else {const auto entry=reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(buffer.data())->table[i];localPort=ntohs(static_cast<u_short>(entry.dwLocalPort));pid=entry.dwOwningPid;Q_IPV6ADDR ip{};memcpy(ip.c,entry.ucLocalAddr,16);address=QHostAddress(ip).toString();}
            if(int(localPort)!=port && localPort!=3702) continue;
            result.append(QJsonObject{{"address",address},{"port",int(localPort)},{"pid",int(pid)},{"image",imagePath(pid)}});
        }
    }
    return result;
}
QJsonObject firewall() {
    QJsonObject result;const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE) {result["error"]=QString::number(quint32(initialized),16);return result;}
    INetFwPolicy2* policy=nullptr;const auto status=CoCreateInstance(CLSID_NetFwPolicy2,nullptr,CLSCTX_INPROC_SERVER,IID_INetFwPolicy2,reinterpret_cast<void**>(&policy));
    if(SUCCEEDED(status)) {
        long active=0;if(SUCCEEDED(policy->get_CurrentProfileTypes(&active))) result["activeProfiles"]=int(active);
        QJsonArray profiles;for(const auto type:{NET_FW_PROFILE2_DOMAIN,NET_FW_PROFILE2_PRIVATE,NET_FW_PROFILE2_PUBLIC}) {VARIANT_BOOL enabled=VARIANT_FALSE;const auto hr=policy->get_FirewallEnabled(type,&enabled);QJsonObject item{{"profile",int(type)}};if(SUCCEEDED(hr)) item["enabled"]=enabled==VARIANT_TRUE;else item["queryError"]=QString::number(quint32(hr),16);profiles.append(item);}result["profiles"]=profiles;
        INetFwRules* rules=nullptr;QJsonArray discovery,serviceRules;
        if(SUCCEEDED(policy->get_Rules(&rules))) {
            for(int index=0;index<5;++index) {const auto name=index<3?QString("Veyon School discovery %1").arg(index):QString(index==3?"Veyon Server":"Veyon Worker");auto ruleName=SysAllocString(wide(name));INetFwRule* rule=nullptr;QJsonObject item{{"name",name}};
                if(SUCCEEDED(rules->Item(ruleName,&rule))) {VARIANT_BOOL enabled=VARIANT_FALSE;long profile=0,protocol=0;NET_FW_ACTION action=NET_FW_ACTION_BLOCK;NET_FW_RULE_DIRECTION direction=NET_FW_RULE_DIR_OUT;rule->get_Enabled(&enabled);rule->get_Profiles(&profile);rule->get_Protocol(&protocol);rule->get_Action(&action);rule->get_Direction(&direction);item["enabled"]=enabled==VARIANT_TRUE;item["profiles"]=int(profile);item["protocol"]=int(protocol);item["allow"]=action==NET_FW_ACTION_ALLOW;item["inbound"]=direction==NET_FW_RULE_DIR_IN;
                    BSTR path=nullptr;if(SUCCEEDED(rule->get_ApplicationName(&path))) {item["application"]=QString::fromWCharArray(path?path:L"");SysFreeString(path);}rule->Release();
                } else item["missing"]=true;SysFreeString(ruleName);if(index<3) discovery.append(item);else serviceRules.append(item);
            }rules->Release();
        }result["discoveryRules"]=discovery;result["serviceRules"]=serviceRules;policy->Release();
    } else result["error"]=QString::number(quint32(status),16);
    if(SUCCEEDED(initialized)) CoUninitialize();
    return result;
}
bool localPath(const QString& input) {
    const auto path=QDir::fromNativeSeparators(input);
    if(!QRegularExpression("^[A-Za-z]:/").match(path).hasMatch() || path.contains("/../")) return false;
    const auto drive=GetDriveTypeW(wide(path.left(3)));
    if(drive!=DRIVE_FIXED && drive!=DRIVE_REMOVABLE) return false;
    auto current=path;
    while(current.size()>3) {const auto attributes=GetFileAttributesW(wide(QDir::toNativeSeparators(current)));if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) return false;const auto parent=QFileInfo(current).path();if(parent==current) break;current=parent;}
    return true;
}
QJsonObject permissions(const QString& path) {
    QJsonObject result;PSECURITY_DESCRIPTOR descriptor=nullptr;PSID owner=nullptr;
    const auto status=GetNamedSecurityInfoW(wide(QDir::toNativeSeparators(path)),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,nullptr,nullptr,&descriptor);
    if(status!=ERROR_SUCCESS) {result["permissionError"]=int(status);return result;}
    result["ownerSid"]=sidString(owner);LPWSTR sddl=nullptr;
    if(ConvertSecurityDescriptorToStringSecurityDescriptorW(descriptor,SDDL_REVISION_1,OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&sddl,nullptr)) {result["sddl"]=QString::fromWCharArray(sddl);LocalFree(sddl);}
    HANDLE tokenHandle=nullptr,impersonation=nullptr;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE,&tokenHandle)) {
        if(DuplicateToken(tokenHandle,SecurityImpersonation,&impersonation)) {
            GENERIC_MAPPING mapping{FILE_GENERIC_READ,FILE_GENERIC_WRITE,FILE_GENERIC_EXECUTE,FILE_ALL_ACCESS};DWORD length=4096,granted=0;std::vector<BYTE> privilege(length);BOOL access=FALSE;
            if(AccessCheck(descriptor,impersonation,FILE_READ_DATA,&mapping,reinterpret_cast<PRIVILEGE_SET*>(privilege.data()),&length,&granted,&access)) result["readableByToolUser"]=bool(access);
            else result["permissionError"]=int(GetLastError());
            CloseHandle(impersonation);
        }CloseHandle(tokenHandle);
    }LocalFree(descriptor);return result;
}
QString expand(const QString& input) {
    auto path=input;const auto programData=qEnvironmentVariable("ProgramData","C:/ProgramData")+"/Veyon";
    for(const auto& name:{QString("%GLOBALAPPDATA%"),QString("$GLOBALAPPDATA")}) path.replace(name,programData);
    path.replace("%APPDATA%",qEnvironmentVariable("APPDATA")+"/Veyon").replace("$APPDATA",qEnvironmentVariable("APPDATA")+"/Veyon");
    path.replace("%HOME%",QDir::homePath()).replace("$HOME",QDir::homePath()).replace("%PROFILE%",QDir::homePath()).replace("$PROFILE",QDir::homePath());
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}
QJsonArray keys(const QString& base,const QString& type,QJsonObject& report,const QString& privateBase={}) {
    QJsonArray result;const auto root=expand(base);
    if(!localPath(root)) {error(report,"Percorso chiavi non locale o con collegamenti: ispezione omessa per evitare accessi di rete o a destinazioni indirette.");return result;}
    const auto attributes=GetFileAttributesW(wide(QDir::toNativeSeparators(root)));
    if(attributes==INVALID_FILE_ATTRIBUTES) {const auto status=GetLastError();if(status!=ERROR_FILE_NOT_FOUND && status!=ERROR_PATH_NOT_FOUND) error(report,QString("Directory chiavi non leggibile (Windows %1).").arg(status));return result;}
    const auto directoryHandle=CreateFileW(wide(QDir::toNativeSeparators(root)),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(directoryHandle==INVALID_HANDLE_VALUE) {error(report,QString("Elenco nomi chiavi non leggibile (Windows %1).").arg(GetLastError()));return result;}
    CloseHandle(directoryHandle);
    const auto dirs=QDir(root).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDir::Name);int count=0;
    for(const auto& dir:dirs) {
        if(count++>=64) {error(report,"Inventario chiavi limitato a 64 nomi per tipo.");break;}
        const auto path=dir.filePath()+"/key";if(!localPath(path) || !QFileInfo(path).isFile()) continue;
        auto item=permissions(path);item["type"]=type;item["name"]=dir.fileName();item["path"]=path;
        const auto privateRoot=expand(privateBase);
        const bool insidePrivate=type=="public" && !privateBase.isEmpty() && (path.compare(privateRoot,Qt::CaseInsensitive)==0 || path.startsWith(privateRoot+"/",Qt::CaseInsensitive));
        if(insidePrivate) item["publicError"]="Percorso pubblico sovrapposto alla directory privata: contenuto non letto";
        else if(type=="public") {QFile file(path);if(file.size()>65536) item["publicError"]="File pubblico troppo grande";
            else if(file.open(QIODevice::ReadOnly)) {const auto bytes=file.read(65537);if(bytes.size()>65536 || bytes.contains("PRIVATE KEY") || !bytes.contains("PUBLIC KEY")) item["publicError"]="Formato pubblico non riconosciuto";
                else item["sha256"]=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
            else item["publicError"]="File pubblico non leggibile";
        }result.append(item);
    }
    return result;
}
#endif
}

QJsonObject collect() {
    QJsonObject report{{"format","netlink-companion"},{"schema",1},{"identifiersIncluded",true},{"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"errors",QJsonArray{}}};
    report["system"]=QJsonObject{{"os",QSysInfo::prettyProductName()},{"architecture",QSysInfo::currentCpuArchitecture()},{"companionVersion",QCoreApplication::applicationVersion()}};
#ifdef Q_OS_WIN
    auto system=report["system"].toObject();system["computer"]=computerName(ComputerNameDnsHostname);system["dnsDomain"]=computerName(ComputerNameDnsDomain);system["uptimeSeconds"]=qint64(GetTickCount64()/1000);
    QSettings windows("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",QSettings::NativeFormat);system["windowsBuild"]=windows.value("CurrentBuild").toString()+"."+windows.value("UBR").toString();system["windowsVersion"]=windows.value("DisplayVersion").toString();
    QSettings hardware("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS",QSettings::NativeFormat);system["manufacturer"]=hardware.value("SystemManufacturer").toString();system["model"]=hardware.value("SystemProductName").toString();
    LPWSTR joined=nullptr;NETSETUP_JOIN_STATUS joinedStatus=NetSetupUnknownStatus;if(NetGetJoinInformation(nullptr,&joined,&joinedStatus)==NERR_Success) {system["joinType"]=joinedStatus==NetSetupDomainName?"domain":joinedStatus==NetSetupWorkgroupName?"workgroup":"unknown";system["joinName"]=QString::fromWCharArray(joined?joined:L"");NetApiBufferFree(joined);}report["system"]=system;
    report["identity"]=token();report["interfaces"]=interfaces(report);
    HKEY configHandle=nullptr;const auto configStatus=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Veyon Solutions\\Veyon",0,KEY_READ|KEY_WOW64_64KEY,&configHandle);
    if(configStatus==ERROR_SUCCESS) RegCloseKey(configHandle);
    else error(report,QString("Configurazione NetLink/Veyon non presente o non leggibile (Windows %1): i valori mostrati possono essere predefiniti.").arg(configStatus));
    QSettings config("HKEY_LOCAL_MACHINE\\SOFTWARE\\Veyon Solutions\\Veyon",QSettings::NativeFormat);config.setFallbacksEnabled(false);QJsonObject settings;
    const QList<QPair<QString,QVariant>> allowed{
        {"Authentication/Method",0},{"Authentication/PrivateKeyBaseDir","%GLOBALAPPDATA%/keys/private"},{"Authentication/PublicKeyBaseDir","%GLOBALAPPDATA%/keys/public"},
        {"AccessControl/AccessRestrictedToUserGroups",false},{"AccessControl/AuthorizedUserGroups",QStringList{}},{"AccessControl/AccessControlRulesProcessingEnabled",false},
        {"Network/VeyonServerPort",11100},{"Network/DemoServerPort",11400},{"Network/LocalConnectOnly",false},{"Network/FirewallExceptionEnabled",true},
        {"School/DiscoveryEnabled",true},{"School/DiscoveryMaster",""},{"Service/MultiSession",false},{"Windows/UseInterceptionDriver",false}
    };
    for(const auto& entry:allowed) {
        auto value=config.value(entry.first,entry.second);
        if(entry.second.type()==QVariant::Int) {bool ok=false;const auto number=value.toInt(&ok);if(!ok) error(report,"Un valore numerico della configurazione non è valido: "+entry.first);value=ok?number:entry.second.toInt();}
        else if(entry.second.type()==QVariant::Bool) value=value.toBool();
        else if(entry.second.type()==QVariant::StringList) value=value.toStringList();
        else value=value.toString();
        settings[entry.first]=QJsonValue::fromVariant(value);
    }
    settings["registryStatus"]=int(config.status());settings["configurationRead"]=configStatus==ERROR_SUCCESS && config.status()==QSettings::NoError;
    settings["accessRuleCount"]=decodeObjects(config.value("AccessControl/AccessControlRules")).size();report["settings"]=settings;
    QJsonArray targets;for(const auto& entry:decodeObjects(config.value("BuiltinDirectory/NetworkObjects"))) {const auto object=entry.toObject();const auto host=object["HostAddress"].toString();if(!host.isEmpty() && targets.size()<1024) targets.append(QJsonObject{{"name",object["Name"].toString().left(256)},{"address",host.left(512)}});}report["configuredComputers"]=targets;
    const auto privateBase=settings["Authentication/PrivateKeyBaseDir"].toString();
    auto keyList=keys(settings["Authentication/PublicKeyBaseDir"].toString(),"public",report,privateBase);for(const auto& key:keys(privateBase,"private",report)) keyList.append(key);report["keys"]=keyList;
    QJsonArray services;auto manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(manager) {for(const auto name:{L"VeyonSchoolService",L"VeyonService",L"BFE",L"MpsSvc"}) services.append(service(manager,name));CloseServiceHandle(manager);}else error(report,QString("Servizi non leggibili (Windows %1).").arg(GetLastError()));report["services"]=services;
    report["listeners"]=listeners(settings["Network/VeyonServerPort"].toInt(11100),report);report["firewall"]=firewall();
#else
    error(report,"Questa prima versione della diagnostica è per Windows 10 x64.");
#endif
    return report;
}

bool validTarget(const QString& target) {
    if(target.isEmpty() || target.size()>253 || target!=target.trimmed()) return false;
    QHostAddress ip(target);if(!ip.isNull()) return !ip.isMulticast() && ip!=QHostAddress::AnyIPv4 && ip!=QHostAddress::AnyIPv6 && ip!=QHostAddress::Broadcast;
    if(QRegularExpression("^[0-9.]+$").match(target).hasMatch()) return false;
    for(const auto& part:target.split('.')) if(part.isEmpty() || part.size()>63 || !QRegularExpression("^[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?$").match(part).hasMatch()) return false;
    return true;
}
bool parseReport(const QByteArray& bytes,QJsonObject& report,QString& message) {
    report={};if(bytes.size()>MaximumReportBytes) {message="Rapporto troppo grande (massimo 2 MiB).";return false;}
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(bytes,&parse);int nodes=0;
    if(parse.error!=QJsonParseError::NoError || !doc.isObject() || !safeTree(doc.object(),0,nodes)) {message="Rapporto JSON non valido o troppo complesso.";return false;}
    const auto object=doc.object();if(object["format"]!="netlink-companion" || object["schema"]!=1 || !object["settings"].isObject() || !object["interfaces"].isArray() || !object["keys"].isArray() || !object["identifiersIncluded"].isBool()) {message="Rapporto Companion non riconosciuto o incompleto.";return false;}
    if(object["keys"].toArray().size()>128 || object["interfaces"].toArray().size()>64) {message="Inventario troppo grande per il confronto.";return false;}
    int addressesCount=0;
    for(const auto& entry:object["interfaces"].toArray()) {if(!entry.isObject() || !entry.toObject()["addresses"].isArray()) {message="Inventario rete non valido.";return false;}addressesCount+=entry.toObject()["addresses"].toArray().size();}
    if(addressesCount>256) {message="Troppi indirizzi nel rapporto (massimo 256).";return false;}
    report=object;message.clear();return true;
}
QJsonObject publicReport(const QJsonObject& report,bool includeIdentifiers) {
    if(includeIdentifiers) return report;
    // Whitelist projection: identifiers can never leak from new/unknown fields.
    const auto source=report["system"].toObject();QJsonObject system;for(const auto& field:{"os","architecture","companionVersion","windowsBuild","windowsVersion"}) system[field]=source[field];
    QJsonArray services;for(const auto& value:report["services"].toArray()) {const auto service=value.toObject();services.append(QJsonObject{{"name",service["name"]},{"state",service["state"]},{"queryError",service["queryError"]}});}
    QJsonObject settings;const auto input=report["settings"].toObject();for(const auto& field:{"Authentication/Method","Network/VeyonServerPort","Network/LocalConnectOnly","School/DiscoveryEnabled"}) settings[field]=input[field];
    return {{"format","netlink-companion"},{"schema",1},{"identifiersIncluded",false},{"createdUtc",report["createdUtc"]},{"system",system},{"services",services},{"settings",settings},{"interfaces",QJsonArray{}},{"keys",QJsonArray{}}};
}
QStringList recommendations(const QJsonObject& report,const QString& role) {
    QStringList result;const auto settings=report["settings"].toObject();const auto services=report["services"].toArray();
    if(!report["errors"].toArray().isEmpty()) result<<"Raccolta parziale: leggere le informazioni non disponibili; non equivale a una configurazione corretta.";
    if(!running(services,"VeyonSchoolService")) result<<"Il servizio NetLink non risulta in esecuzione o non è stato possibile verificarlo. Controllare nel Configurator prima di provarne l'accesso.";
    if(running(services,"VeyonSchoolService") && running(services,"VeyonService")) result<<"Sono attivi sia NetLink sia Veyon originale: possibile conflitto di porte. Non fermarli automaticamente durante una lezione.";
    if(!running(services,"BFE")) result<<"Il servizio motore di filtraggio Windows (BFE) non risulta attivo o non è verificabile: controllarlo prima del collaudo InternetGuard.";
    if(settings["Network/LocalConnectOnly"].toBool()) result<<"L'accesso è limitato al computer locale: i collegamenti da altri PC possono essere rifiutati.";
    if(settings.contains("Network/FirewallExceptionEnabled") && !settings["Network/FirewallExceptionEnabled"].toBool())
        result<<"ATTENZIONE: eccezione firewall automatica disabilitata. Un client può rispondere su 127.0.0.1 ma non dal docente. Servono regole manuali equivalenti oppure, sul PC di prova in rete fidata, Configurator come amministratore → Vista → Avanzate → Servizio → Abilita le eccezioni del firewall. Le regole discovery NON autorizzano da sole la porta del servizio. Non è ancora una prova della policy effettiva.";
    if(report.contains("listeners")) {
        const int port=settings["Network/VeyonServerPort"].toInt(11100);bool found=false,network=false;
        for(const auto& value:report["listeners"].toArray()) {const auto listener=value.toObject();if(listener["port"].toInt()!=port) continue;found=true;const QHostAddress address(listener["address"].toString());if(!address.isNull() && !address.isLoopback()) network=true;}
        if(!found && running(services,"VeyonSchoolService")) result<<QString("ATTENZIONE: servizio avviato, ma nessun listener TCP rilevato sulla porta %1. Verificare porta configurata, avvio del processo server e possibili conflitti.").arg(port);
        else if(found && !network) result<<QString("ATTENZIONE: porta %1 in ascolto solo su loopback. Può funzionare da questo PC, ma non dal docente; verificare anche il processo effettivamente avviato.").arg(port);
        else if(found) result<<QString("Porta %1 in ascolto anche su un'interfaccia di rete. Non garantisce che il firewall la consenta o che l'autenticazione riesca.").arg(port);
    }
    if(report["firewall"].toObject()["activeProfiles"].toInt()&4) result<<"È presente un profilo Pubblico su almeno un'interfaccia, anche virtuale: non significa automaticamente che il Wi-Fi sia Pubblico. Le eccezioni discovery del fork sono limitate a Privato/Dominio. Verificare il profilo dell'interfaccia effettiva senza disattivare il firewall.";
    const auto version=report["system"].toObject()["companionVersion"].toString();
    for(int alpha=1;alpha<=4;++alpha) if(version.endsWith(QString("0.1.0-alpha.%1").arg(alpha))) {
        result<<"Questa alpha precede la correzione del lettore di autenticazione del Master: con TCP raggiungibile può comparire Authentication failed prima dello scambio delle chiavi. Aggiornare la suite prima di rigenerare altre chiavi.";break;}
    if(report["identity"].toObject()["elevated"].toBool()) result<<"Companion è elevato: l'accessibilità delle chiavi si riferisce a questo account/token, NON prova che il Master avviato normalmente possa leggerle. Ripetere come utente docente non elevato.";
    if(settings["Authentication/Method"].toInt()==1) {
        int pub=0,priv=0,readable=0;for(const auto& value:report["keys"].toArray()) {const auto key=value.toObject();if(key["type"]=="public") ++pub;else if(key["type"]=="private") {++priv;if(key["readableByToolUser"].toBool()) ++readable;}}
        if(role=="teacher" && !readable) result<<"Non risulta una chiave privata leggibile dall'utente di Companion. Verificare nome e gruppo di accesso della chiave nel Configurator docente.";
        if(role=="client" && !pub) result<<"Non risultano chiavi pubbliche inventariabili sul client: verificare percorso, permessi e importazione dal docente.";
        if(role=="client" && priv) result<<"Sono presenti chiavi private sul PC indicato come client: normalmente devono restare solo sul docente. Verificarne la necessità senza cancellarle automaticamente.";
    } else result<<"È configurata l'autenticazione tramite account, non tramite chiavi. Per riutilizzare una coppia di chiavi, entrambi i PC devono scegliere il metodo corrispondente.";
    result<<"Subnet diverse non significano errore: servono routing/ACL compatibili e DiscoveryMaster sul client per gli annunci diretti. Il multicast non attraversa automaticamente le subnet.";
    result<<"I controlli locali non provano l'autenticazione Veyon né il blocco InternetGuard: per quello serve una connessione reale dal Master e il relativo feedback.";
    result<<"Per isolare un timeout: dal docente provare IP client:porta servizio; sul client confrontare 127.0.0.1:stessa porta. Il collegamento client → docente è una direzione diversa e non prova l'ingresso sul client. User vuoto in una notifica può indicare una prova TCP o uno scambio interrotto prima dell'identificazione, non necessariamente una chiave errata.";
    return result;
}
QStringList compare(const QJsonObject& teacher,const QJsonObject& client) {
    QStringList result;
    if(!teacher["identifiersIncluded"].toBool() || !client["identifiersIncluded"].toBool()) return {"Per il confronto occorrono due rapporti completi, esportati includendo identificativi e indirizzi. I rapporti ridotti omettono intenzionalmente questi dati."};
    const auto a=teacher["settings"].toObject(),b=client["settings"].toObject();
    result<<"Confronto statico di rapporti non firmati: non verifica identità, raggiungibilità o autorizzazione del computer remoto.";
    if(!a["Authentication/Method"].isDouble() || !b["Authentication/Method"].isDouble() || !a["Network/VeyonServerPort"].isDouble() || !b["Network/VeyonServerPort"].isDouble()) result<<"ATTENZIONE: mancano impostazioni numeriche fondamentali per il confronto di autenticazione e porte; confrontarle nel Configurator.";
    if(a["Authentication/Method"]!=b["Authentication/Method"]) result<<"ERRORE: i metodi di autenticazione dei due PC sono diversi.";
    if(a["Network/VeyonServerPort"]!=b["Network/VeyonServerPort"]) result<<"ATTENZIONE: le porte di servizio configurate differiscono. Verificare la porta associata al client nel Master; può essere una scelta intenzionale.";
    if(a["Authentication/Method"].toInt()==1 && b["Authentication/Method"].toInt()==1) {
        bool shared=false,privateName=false;
        for(const auto& x:teacher["keys"].toArray()) for(const auto& y:client["keys"].toArray()) {
            const auto key=x.toObject(),other=y.toObject();if(other["type"]!="public") continue;
            if(key["type"]=="private" && key["name"]==other["name"]) privateName=true;
            if(key["type"]=="public" && !key["sha256"].toString().isEmpty() && key["sha256"]==other["sha256"]) shared=true;
        }
        if(!privateName) result<<"ATTENZIONE: nessun nome di chiave privata docente coincide con una chiave pubblica client inventariata.";
        result<< (shared?"Trovato almeno un file di chiave pubblica identico sui due PC. Non prova la corrispondenza con la chiave privata o i diritti di accesso.":"Non risulta un file di chiave pubblica con la stessa impronta sui due PC; l'assenza o una differente codifica non prova da sola una coppia errata.");
    }
    bool sameSubnet=false;
    for(const auto& localValue:addressRecords(teacher)) for(const auto& remoteValue:addressRecords(client)) {
        const auto localRecord=localValue.toObject(),remoteRecord=remoteValue.toObject();const QHostAddress local(localRecord["address"].toString()),remote(remoteRecord["address"].toString());
        const int localPrefix=localRecord["prefix"].toInt(-1),remotePrefix=remoteRecord["prefix"].toInt(-1),maximum=remote.protocol()==QAbstractSocket::IPv4Protocol?32:128;
        if(!local.isNull() && !remote.isNull() && localPrefix>0 && localPrefix<=maximum && remotePrefix>0 && remotePrefix<=maximum && local.protocol()==remote.protocol() && !local.isLoopback() && !local.isLinkLocal() && !remote.isLinkLocal() && local.isInSubnet(remote,remotePrefix) && remote.isInSubnet(local,localPrefix)) sameSubnet=true;
    }
    result<< (sameSubnet?"Le interfacce dichiarano almeno una subnet IP comune; questo NON esclude isolamento Wi-Fi, VPN o filtri di rete.":"Non risulta una subnet IP comune. È possibile comunicare con routing consentito; configurare gli annunci diretti verso il docente per il discovery.");
    const auto master=b["School/DiscoveryMaster"].toString();
    if(master.isEmpty() && !sameSubnet) result<<"ATTENZIONE: DiscoveryMaster è vuoto sul client: gli annunci diretti tra subnet non sono configurati.";
    if(!master.isEmpty() && !QHostAddress(master).isNull() && !addresses(teacher).contains(master)) result<<"ATTENZIONE: DiscoveryMaster non coincide testualmente con gli IP attivi dichiarati dal docente. Verificare alias, indirizzi IPv6 equivalenti, NAT o un indirizzo non aggiornato; il confronto non li risolve automaticamente.";
    for(const auto& address:addresses(teacher)) if(addresses(client).contains(address) && !QHostAddress(address).isLoopback() && !QHostAddress(address).isLinkLocal()) {result<<"ATTENZIONE: i rapporti condividono un indirizzo IP. Potrebbero provenire dallo stesso PC o usare una rete virtuale; verificare anche possibili IP duplicati.";break;}
    return result;
}
QString html(const QJsonObject& report,const QString& role) {
    QString result="<h1>NetLink Companion</h1><p>Diagnosi locale in sola lettura. Nessuna impostazione modificata.</p><h2>Indicazioni</h2><ul>";
    for(const auto& recommendation:recommendations(report,role)) result+="<li>"+recommendation.toHtmlEscaped()+"</li>";
    result+="</ul><h2>Computer</h2>"+table(report["system"].toObject());
    auto identity=report["identity"].toObject();const auto groups=identity.take("groups").toArray();result+="<h2>Account che esegue Companion</h2>"+table(identity)+"<p>I gruppi sono identificati dal SID; i gruppi denyOnly non concedono accesso nel token attuale.</p>";
    result+="<h2>Rete</h2>";for(const auto& value:report["interfaces"].toArray()) {auto iface=value.toObject();QStringList ips;for(const auto& entry:iface.take("addresses").toArray()) ips<<entry.toObject()["address"].toString()+"/"+QString::number(entry.toObject()["prefix"].toInt());iface["addresses"]=ips.join(", ");iface["dns"]=list(iface["dns"].toArray());iface["gateways"]=list(iface["gateways"].toArray());result+=table(iface);}
    result+="<h2>Servizi e porte TCP in ascolto</h2>";for(const auto& value:report["services"].toArray()) result+=table(value.toObject());for(const auto& value:report["listeners"].toArray()) result+=table(value.toObject());
    auto firewall=report["firewall"].toObject();const auto profiles=firewall.take("profiles").toArray(),rules=firewall.take("discoveryRules").toArray(),serviceRules=firewall.take("serviceRules").toArray();
    result+="<h2>Firewall Windows</h2>"+table(firewall)+"<p>Inventario parziale delle regole del fork, non una simulazione della policy effettiva: criteri di dominio e altri prodotti possono prevalere.</p>";
    for(const auto& value:profiles) result+=table(value.toObject());
    for(const auto& value:rules) result+=table(value.toObject());
    for(const auto& value:serviceRules) result+=table(value.toObject());
    result+="<h2>Configurazione NetLink/Veyon</h2>"+table(report["settings"].toObject())+"<h2>PC già configurati</h2>";for(const auto& value:report["configuredComputers"].toArray()) result+=table(value.toObject());
    result+="<h2>Chiavi: metadati e permessi</h2><p>Nessun contenuto di chiave privata viene letto o esportato. Le impronte SHA-256 riguardano soltanto i file pubblici, non la validità di una coppia crittografica.</p>";for(const auto& value:report["keys"].toArray()) result+=table(value.toObject());
    result+="<h2>Informazioni non disponibili</h2><ul>";for(const auto& value:report["errors"].toArray()) result+="<li>"+value.toString().toHtmlEscaped()+"</li>";
    result+="</ul><h2>Dettaglio tecnico: gruppi del token</h2>";for(const auto& value:groups) result+=table(value.toObject());
    return result+"<p>Copyright © 2025-26 prof. Ing. Raffaele Mele. Basato su NetLink/Veyon; attribuzioni originali conservate.</p>";
}
}
