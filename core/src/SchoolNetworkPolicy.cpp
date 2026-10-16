// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "SchoolNetworkPolicy.h"
#include "NetLinkIdentity.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <initguid.h>
#include <fwpmu.h>
#include "SchoolWfpGuids.h"
#include <iphlpapi.h>
#include <netfw.h>
#endif

namespace SchoolNetwork {
bool configureDiscoveryFirewall(bool enabled) {
#ifndef Q_OS_WIN
    Q_UNUSED(enabled)
    return true;
#else
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE) return false;
    INetFwPolicy2* policy=nullptr;INetFwRules* rules=nullptr;
    HRESULT error=CoCreateInstance(CLSID_NetFwPolicy2,nullptr,CLSCTX_INPROC_SERVER,IID_INetFwPolicy2,reinterpret_cast<void**>(&policy));
    if(SUCCEEDED(error)) error=policy->get_Rules(&rules);
    for(int index=0;index<3 && SUCCEEDED(error);++index) {
        const QString name=QString("Veyon School discovery %1").arg(index);
        auto ruleName=SysAllocString(reinterpret_cast<const OLECHAR*>(name.utf16()));
        rules->Remove(ruleName); // exact names owned by this installer only
        if(enabled) {
            INetFwRule* rule=nullptr;
            error=CoCreateInstance(CLSID_NetFwRule,nullptr,CLSCTX_INPROC_SERVER,IID_INetFwRule,reinterpret_cast<void**>(&rule));
            const QString app=NetLinkIdentity::executablePath(index==0?NetLinkIdentity::Application::Service:NetLinkIdentity::Application::Master);
            auto appName=SysAllocString(reinterpret_cast<const OLECHAR*>(app.utf16()));auto port=SysAllocString(L"3702");
            if(SUCCEEDED(error)) error=rule->put_Name(ruleName);
            if(SUCCEEDED(error)) error=rule->put_ApplicationName(appName);
            if(SUCCEEDED(error)) error=rule->put_Protocol(index==2 ? NET_FW_IP_PROTOCOL_TCP : NET_FW_IP_PROTOCOL_UDP);
            if(SUCCEEDED(error)) error=rule->put_LocalPorts(port);
            if(SUCCEEDED(error)) error=rule->put_Direction(NET_FW_RULE_DIR_IN);
            if(SUCCEEDED(error)) error=rule->put_Profiles(NET_FW_PROFILE2_DOMAIN|NET_FW_PROFILE2_PRIVATE);
            if(SUCCEEDED(error)) error=rule->put_Action(NET_FW_ACTION_ALLOW);
            if(SUCCEEDED(error)) error=rule->put_Enabled(VARIANT_TRUE);
            if(SUCCEEDED(error)) error=rules->Add(rule);
            if(rule) rule->Release();
            SysFreeString(appName);SysFreeString(port);
        }
        SysFreeString(ruleName);
    }
    if(rules) rules->Release();
    if(policy) policy->Release();
    if(SUCCEEDED(initialized)) CoUninitialize();
    return SUCCEEDED(error);
#endif
}
bool validAddress(const QString& text) {
    const auto parts = text.split('/');
    QHostAddress address(parts.value(0));
    if (address.isNull() || address.isMulticast() || address.isLoopback() ||
        address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6 ||
        !address.scopeId().isEmpty()) return false;
    if (parts.size() == 1) return true;
    bool ok = false;
    const auto prefix = parts.value(1).toInt(&ok);
    return parts.size() == 2 && ok && prefix > 0 &&
        prefix <= (address.protocol() == QAbstractSocket::IPv4Protocol ? 32 : 128);
}
static QJsonObject result(bool ok, const QString& status, quint32 code = 0) {
    return {{"ok", ok}, {"status", status}, {"code", double(code)}};
}
#ifdef Q_OS_WIN
static bool trustedPipePeer(QLocalSocket* socket, NetLinkIdentity::Application expected, bool server) {
    ULONG pid = 0;
    HANDLE pipe = reinterpret_cast<HANDLE>(socket->socketDescriptor());
    if (!(server ? GetNamedPipeServerProcessId(pipe, &pid) : GetNamedPipeClientProcessId(pipe, &pid))) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    wchar_t path[32768]; DWORD size = 32768;
    bool ok = QueryFullProcessImageNameW(process, 0, path, &size) &&
        NetLinkIdentity::matchesInstalledPath(QString::fromWCharArray(path,int(size)),QCoreApplication::applicationDirPath(),expected);
    HANDLE token = nullptr;
    if (ok && OpenProcessToken(process, TOKEN_QUERY, &token)) {
        alignas(TOKEN_USER) BYTE data[512]; DWORD used = 0; BYTE sid[SECURITY_MAX_SID_SIZE]; DWORD sidSize = sizeof(sid);
        ok = GetTokenInformation(token, TokenUser, data, sizeof(data), &used) &&
            CreateWellKnownSid(WinLocalSystemSid, nullptr, sid, &sidSize) &&
            EqualSid(reinterpret_cast<TOKEN_USER*>(data)->User.Sid, sid);
        CloseHandle(token);
    } else ok = false;
    CloseHandle(process);
    return ok;
}
#endif
QJsonObject request(const QJsonObject& command) {
    QLocalSocket socket;
    socket.setReadBufferSize(32769);
    socket.connectToServer(QString::fromLatin1(PipeName));
    if (!socket.waitForConnected(2000)) return result(false, "service-unavailable");
#ifdef Q_OS_WIN
    if (!trustedPipePeer(&socket, NetLinkIdentity::Application::Service, true)) return result(false, "untrusted-service");
#endif
    const auto bytes = QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n';
    if (bytes.size() > 32768 || socket.write(bytes) != bytes.size()) return result(false, "invalid-request");
    QElapsedTimer deadline; deadline.start();
    while (!socket.canReadLine() && deadline.elapsed() < 5000) {
        const auto remaining=5000-deadline.elapsed();
        if (remaining<=0 || !socket.waitForReadyRead(int(remaining))) break;
        if (socket.bytesAvailable() > 32768) return result(false, "oversized-response");
    }
    if (socket.bytesAvailable() > 32768) return result(false, "oversized-response");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(socket.readLine(32769), &error);
    return error.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : result(false, "service-timeout");
}
PolicyDaemon::PolicyDaemon(QObject* parent) : PolicyDaemon(parent,true,[](void* engine) -> quint32 {
#ifdef Q_OS_WIN
    return FwpmEngineClose0(engine);
#else
    Q_UNUSED(engine)
    return 0;
#endif
}) {}
PolicyDaemon::PolicyDaemon(QObject* parent,bool initializePipe,CloseEngine closeEngine)
    : QObject(parent),m_server(this),m_timer(this),m_closeEngine(std::move(closeEngine)) {
    connect(&m_timer,&QTimer::timeout,this,&PolicyDaemon::maintainLease);
    if(!initializePipe) return;
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (auto socket = m_server.nextPendingConnection()) {
            socket->setParent(this);
            socket->setReadBufferSize(32769);
            if(m_pipeClients>=16) {socket->abort();socket->deleteLater();continue;}
#ifdef Q_OS_WIN
            if (!trustedPipePeer(socket, NetLinkIdentity::Application::Server, false)) { socket->abort(); socket->deleteLater(); continue; }
#endif
            ++m_pipeClients;
            connect(socket,&QObject::destroyed,this,[this] {--m_pipeClients;});
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(6000, socket, &QLocalSocket::abort);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                if (socket->property("handled").toBool()) return;
                if (socket->bytesAvailable() > 32768) { socket->abort(); return; }
                if (!socket->canReadLine()) return;
                socket->setProperty("handled", true);
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(socket->readLine(32769), &error);
                const auto reply = error.error == QJsonParseError::NoError && document.isObject()
                    ? apply(document.object()) : result(false, "invalid-json");
                socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            });
        }
    });
    // Never remove an existing pipe: a second service must fail instead of taking it over.
    if (!m_server.listen(QString::fromLatin1(PipeName))) qWarning("School policy pipe unavailable");
    m_timer.start(1000);
}
PolicyDaemon::~PolicyDaemon() { release(); }
void PolicyDaemon::maintainLease() {
    if(!m_engine) return;
    if(m_cleanupError) {
        // Retain the handle until the API confirms cleanup. Avoid retry storms
        // when BFE/RPC is failing: 2,4,8,16,30 seconds, then at most every 30s.
        const int delay=qMin(30000,2000*(1<<qMin(qMax(m_cleanupAttempts-1,0),4)));
        if(!m_cleanupRetry.isValid() || m_cleanupRetry.elapsed()>=delay) release();
    } else if(m_lease.isValid() && m_lease.elapsed()>=90000) release();
}
quint32 PolicyDaemon::verifyFilters() const {
#ifdef Q_OS_WIN
    if(!m_engine || m_filters.isEmpty()) return ERROR_INVALID_HANDLE;
    for(auto id:m_filters) {
        FWPM_FILTER0* filter=nullptr;
        const DWORD error=FwpmFilterGetById0(m_engine,id,&filter);
        if(filter) FwpmFreeMemory0(reinterpret_cast<void**>(&filter));
        if(error) return error;
    }
    return ERROR_SUCCESS;
#else
    return 1;
#endif
}
quint32 PolicyDaemon::release() {
    if(!m_engine) return m_cleanupError;
    const quint32 error=m_closeEngine(m_engine);
    if(error) {
        // A failed close is not proof that dynamic filters disappeared. Keep
        // ownership and the handle so cleanup can be retried, never claim free.
        m_cleanupError=error;m_cleanupRetry.restart();m_cleanupAttempts=qMin(m_cleanupAttempts+1,1000);
        qWarning("NetLink WFP cleanup pending (code %lu, attempt %d); policy state is unknown",static_cast<unsigned long>(error),m_cleanupAttempts);
        return error;
    }
    m_engine = nullptr; m_filters.clear(); m_owner.clear(); m_digest.clear();
    m_cleanupError=0;m_cleanupAttempts=0;m_cleanupRetry.invalidate();m_lease.invalidate();
    return 0;
}

#ifdef Q_OS_WIN
static const GUID SchoolLayer = {0x8ee6ae25,0x8791,0x4fa8,{0xa4,0x81,0x2c,0xcc,0x4b,0x79,0x65,0x42}};
static DWORD addFilter(HANDLE engine, const GUID& layer, bool allow, const QString& address,
                       int protocol, int port, const QString& application, QVector<quint64>& ids, int localPort = -1,
                       const GUID* sublayer = nullptr) {
    FWPM_FILTER0 filter{};
    filter.displayData.name = const_cast<wchar_t*>(L"Veyon School network policy");
    filter.layerKey = layer; filter.subLayerKey = sublayer ? *sublayer : SchoolLayer;
    filter.action.type = allow ? FWP_ACTION_PERMIT : FWP_ACTION_BLOCK;
    UINT64 weight = allow ? 1000 : 1;
    filter.weight.type = FWP_UINT64; filter.weight.uint64 = &weight;
    FWPM_FILTER_CONDITION0 conditions[5]{}; int count = 0;
    FWP_V4_ADDR_AND_MASK v4{}; FWP_V6_ADDR_AND_MASK v6{};
    if (!address.isEmpty()) {
        const auto parts = address.split('/'); QHostAddress host(parts.first());
        auto& condition = conditions[count++]; condition.fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
        condition.matchType = FWP_MATCH_EQUAL;
        if (host.protocol() == QAbstractSocket::IPv4Protocol) {
            v4.addr = host.toIPv4Address(); const int prefix = parts.size() == 2 ? parts[1].toInt() : 32;
            v4.mask = prefix == 32 ? 0xffffffffu : (0xffffffffu << (32-prefix));
            v4.addr &= v4.mask;
            condition.conditionValue.type = FWP_V4_ADDR_MASK; condition.conditionValue.v4AddrMask = &v4;
        } else {
            const auto bytes = host.toIPv6Address(); memcpy(v6.addr, bytes.c, 16);
            v6.prefixLength = BYTE(parts.size() == 2 ? parts[1].toInt() : 128);
            condition.conditionValue.type = FWP_V6_ADDR_MASK; condition.conditionValue.v6AddrMask = &v6;
        }
    }
    if (protocol >= 0) {
        auto& c = conditions[count++]; c.fieldKey = FWPM_CONDITION_IP_PROTOCOL; c.matchType = FWP_MATCH_EQUAL;
        c.conditionValue.type = FWP_UINT8; c.conditionValue.uint8 = UINT8(protocol);
    }
    if (port >= 0) {
        auto& c = conditions[count++]; c.fieldKey = FWPM_CONDITION_IP_REMOTE_PORT; c.matchType = FWP_MATCH_EQUAL;
        c.conditionValue.type = FWP_UINT16; c.conditionValue.uint16 = UINT16(port);
    }
    FWP_BYTE_BLOB* app = nullptr;
    if (localPort >= 0) {
        auto& c = conditions[count++]; c.fieldKey = FWPM_CONDITION_IP_LOCAL_PORT; c.matchType = FWP_MATCH_EQUAL;
        c.conditionValue.type = FWP_UINT16; c.conditionValue.uint16 = UINT16(localPort);
    }
    if (!application.isEmpty()) {
        const auto path = QDir::toNativeSeparators(application).toStdWString();
        const DWORD error = FwpmGetAppIdFromFileName0(path.c_str(), &app);
        if (error) return error;
        auto& c = conditions[count++]; c.fieldKey = FWPM_CONDITION_ALE_APP_ID; c.matchType = FWP_MATCH_EQUAL;
        c.conditionValue.type = FWP_BYTE_BLOB_TYPE; c.conditionValue.byteBlob = app;
    }
    filter.numFilterConditions = UINT32(count); filter.filterCondition = conditions;
    UINT64 id = 0; const DWORD error = FwpmFilterAdd0(engine, &filter, nullptr, &id);
    if (app) FwpmFreeMemory0(reinterpret_cast<void**>(&app));
    if (!error) ids.append(id);
    return error;
}
static QString socketAddress(const SOCKET_ADDRESS& address) {
    if (!address.lpSockaddr) return {};
    if (address.lpSockaddr->sa_family == AF_INET)
        return QHostAddress(ntohl(reinterpret_cast<sockaddr_in*>(address.lpSockaddr)->sin_addr.s_addr)).toString();
    if (address.lpSockaddr->sa_family == AF_INET6) {
        Q_IPV6ADDR bytes{};
        memcpy(bytes.c, &reinterpret_cast<sockaddr_in6*>(address.lpSockaddr)->sin6_addr, 16);
        return QHostAddress(bytes).toString();
    }
    return {};
}
struct Infrastructure { QStringList dns, dhcp4, dhcp6; };
static Infrastructure infrastructure() {
    Infrastructure result;
    ULONG size = 16384;
    QByteArray buffer(int(size), '\0');
    ULONG error = ERROR_BUFFER_OVERFLOW;
    for (int retry = 0; retry < 3 && error == ERROR_BUFFER_OVERFLOW; ++retry) {
        if (size > 1024 * 1024) return result;
        buffer.resize(int(size));
        error = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr,
            reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    }
    if (error != NO_ERROR) return result;
    for (auto adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto dns = adapter->FirstDnsServerAddress; dns; dns = dns->Next) {
            const auto address = socketAddress(dns->Address);
            if (validAddress(address)) result.dns.append(address);
        }
        const auto dhcp4 = socketAddress(adapter->Dhcpv4Server), dhcp6 = socketAddress(adapter->Dhcpv6Server);
        if (validAddress(dhcp4)) result.dhcp4.append(dhcp4);
        if (validAddress(dhcp6)) result.dhcp6.append(dhcp6);
    }
    result.dns.removeDuplicates(); result.dhcp4.removeDuplicates(); result.dhcp6.removeDuplicates();
    return result;
}
#endif
QJsonObject nativeSelfTest() {
#ifndef Q_OS_WIN
    return result(false,"windows-required");
#else
    // Safe primitive test: no catch-all rule, only documentation-reserved addresses.
    const GUID testLayer={0x26c4ba09,0x0901,0x475c,{0xac,0x09,0xb9,0x57,0x51,0xab,0x2c,0x31}};
    HANDLE engine=nullptr;FWPM_SESSION0 session{};session.flags=FWPM_SESSION_FLAG_DYNAMIC;session.txnWaitTimeoutInMSec=2000;
    DWORD error=FwpmEngineOpen0(nullptr,RPC_C_AUTHN_WINNT,nullptr,&session,&engine);
    if(error) return result(false,"selftest-engine-open",error);
    FWPM_SUBLAYER0 sublayer{};sublayer.subLayerKey=testLayer;sublayer.weight=0x1000;
    sublayer.displayData.name=const_cast<wchar_t*>(L"Veyon School reserved-address self-test");
    error=FwpmSubLayerAdd0(engine,&sublayer,nullptr);
    if(error) {FwpmEngineClose0(engine);return result(false,"selftest-sublayer",error);}
    error=FwpmTransactionBegin0(engine,0);QVector<quint64> ids;
    const GUID layers[]={FWPM_LAYER_ALE_AUTH_CONNECT_V4,FWPM_LAYER_ALE_AUTH_CONNECT_V6,FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6};
    const auto app=QCoreApplication::applicationFilePath();
    for(int index=0;index<4 && !error;++index) {
        const QString address=index%2==0 ? "192.0.2.254" : "2001:db8::ffff";
        error=addFilter(engine,layers[index],false,address,-1,-1,{},ids,-1,&testLayer);
        if(!error) error=addFilter(engine,layers[index],true,address,17,3702,app,ids,-1,&testLayer);
        if(!error) error=addFilter(engine,layers[index],true,address,17,index%2==0 ? 67 : 547,app,ids,index%2==0 ? 68 : 546,&testLayer);
        if(!error) error=addFilter(engine,layers[index],true,address,6,53,{},ids,-1,&testLayer);
    }
    if(!error) error=FwpmTransactionCommit0(engine);else FwpmTransactionAbort0(engine);
    for(auto id:ids) if(!error) {
        FWPM_FILTER0* filter=nullptr;error=FwpmFilterGetById0(engine,id,&filter);
        if(filter) FwpmFreeMemory0(reinterpret_cast<void**>(&filter));
    }
    if(!error) error=FwpmTransactionBegin0(engine,0);
    if(!error) {
        for(auto id:ids) {error=FwpmFilterDeleteById0(engine,id);if(error) break;}
        FwpmTransactionAbort0(engine); // deletions must be rolled back
        FWPM_FILTER0* filter=nullptr;
        if(!error) error=FwpmFilterGetById0(engine,ids.first(),&filter);
        if(filter) FwpmFreeMemory0(reinterpret_cast<void**>(&filter));
    }
    const auto count=ids.size();
    const DWORD closeError=FwpmEngineClose0(engine);if(!error) error=closeError;
    auto reply=result(!error,error ? "selftest-failed" : "selftest-passed",error);
    reply["filtersTested"]=count;reply["scope"]="reserved-addresses-only";
    return reply;
#endif
}
QJsonObject PolicyDaemon::apply(const QJsonObject& command) {
    const auto op = command.value("op").toString();
    const auto owner = command.value("owner").toString();
    if (m_engine && !m_cleanupError && m_lease.isValid() && m_lease.elapsed() >= 90000) release();
    if (op == "status") {
        if(m_cleanupError) {
            auto reply=result(false,"unknown",m_cleanupError);
            reply["cleanupPending"]=true;reply["remainingMs"]=0;reply["filters"]=m_filters.size();return reply;
        }
        if(m_engine) {
            const auto error=verifyFilters();
            if(error) {const auto closeError=release();return result(false,closeError ? "unknown" : "filters-unavailable",closeError ? closeError : error);}
        }
        auto reply = result(!m_cleanupError, m_engine ? "blocked" : m_cleanupError ? "unknown" : "allowed",m_cleanupError);
        reply["remainingMs"] = m_engine ? 90000 - int(m_lease.elapsed()) : 0;
        reply["filters"] = m_filters.size(); return reply;
    }
    if (owner.size() != 36 || QUuid(owner).isNull()) return result(false, "invalid-owner");
    if (m_engine && owner != m_owner) return result(false, "owned-by-another-session");
    if (op == "release") {const auto error=release();return result(!error,error ? "unknown" : "allowed",error);}
    if (op != "apply") return result(false, "invalid-operation");
    if(m_cleanupError) return result(false,"cleanup-pending",m_cleanupError);
    const auto addresses = command.value("addresses").toArray();
    if (addresses.isEmpty() || addresses.size() > 256) return result(false, "invalid-address-list");
    for (const auto& item : addresses) if (!validAddress(item.toString())) return result(false, "invalid-address");
    QByteArray policyData=QJsonDocument(addresses).toJson(QJsonDocument::Compact);
#ifdef Q_OS_WIN
    const auto infra = infrastructure();
    policyData+=infra.dns.join('\n').toUtf8()+infra.dhcp4.join('\n').toUtf8()+infra.dhcp6.join('\n').toUtf8();
#endif
    const auto digest = QCryptographicHash::hash(policyData,QCryptographicHash::Sha256);
    if(m_engine) {
        const auto error=verifyFilters();
        if(error) {
            // BFE/session loss must not produce a cached success or renew a stale lease.
            const auto closeError=release();
            if(closeError) return result(false,"session-reset-failed",closeError);
        }
    }
    if (m_engine && digest == m_digest) { m_lease.restart(); return result(true, "blocked"); }
#ifndef Q_OS_WIN
    return result(false, "windows-required");
#else
    DWORD error = 0;
    if (!m_engine) {
        FWPM_SESSION0 session{}; session.flags = FWPM_SESSION_FLAG_DYNAMIC; session.txnWaitTimeoutInMSec = 2000;
        error = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &m_engine);
        if (error) return result(false, "engine-open-failed", error);
        // Ownership starts with the dynamic session, not only after successful
        // filter installation: even partial setup may need an explicit retry.
        m_owner=owner;
        FWPM_SUBLAYER0 sublayer{}; sublayer.subLayerKey = SchoolLayer; sublayer.weight = 0x8000;
        sublayer.displayData.name = const_cast<wchar_t*>(L"Veyon School");
        error = FwpmSubLayerAdd0(m_engine, &sublayer, nullptr);
        if (error) { release(); return result(false, "sublayer-failed", error); }
    }
    error = FwpmTransactionBegin0(m_engine, 0);
    if (error) { if (m_filters.isEmpty()) release(); return result(false, "transaction-failed", error); }
    QVector<quint64> next;
    for (auto id : m_filters) { error = FwpmFilterDeleteById0(m_engine, id); if (error) break; }
    const GUID layers[] = {FWPM_LAYER_ALE_AUTH_CONNECT_V4, FWPM_LAYER_ALE_AUTH_CONNECT_V6,
        FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6};
    const auto systemHost = QDir::fromNativeSeparators(qEnvironmentVariable("SystemRoot")) + "/System32/svchost.exe";
    for (int index = 0; index < 4 && !error; ++index) {
        const bool ipv4 = index % 2 == 0;
        error = addFilter(m_engine, layers[index], true, ipv4 ? "127.0.0.0/8" : "::1", -1, -1, {}, next);
        for (const auto& item : addresses) {
            if (error) break;
            const QString address = item.toString();
            if ((QHostAddress(address.section('/',0,0)).protocol() == QAbstractSocket::IPv4Protocol) == ipv4)
                error = addFilter(m_engine, layers[index], true, address, -1, -1, {}, next);
            if (error) break;
        }
        // Discovery is restricted to the installed, protected service/master binaries.
        for (auto app : {NetLinkIdentity::Application::Service, NetLinkIdentity::Application::Master}) {
            if (!error) error = addFilter(m_engine, layers[index], true, {}, 17, 3702,
                NetLinkIdentity::executablePath(app), next);
        }
        for (const auto& dns : infra.dns) {
            if ((QHostAddress(dns).protocol() == QAbstractSocket::IPv4Protocol) != ipv4) continue;
            for (int protocol : {6, 17}) if (!error)
                error = addFilter(m_engine, layers[index], true, dns, protocol, 53, {}, next);
        }
        auto dhcp = ipv4 ? infra.dhcp4 : infra.dhcp6;
        dhcp.append(ipv4 ? "255.255.255.255" : "ff02::1:2");
        dhcp.append(ipv4 ? "0.0.0.0" : "fe80::/10");
        for (const auto& address : dhcp) if (!error)
            error = addFilter(m_engine, layers[index], true, address, 17, ipv4 ? 67 : 547, systemHost, next, ipv4 ? 68 : 546);
        if (!ipv4 && !error) error = addFilter(m_engine, layers[index], true, "fe80::/10", 58, -1, {}, next);
        if (!ipv4 && !error) error = addFilter(m_engine, layers[index], true, "ff02::/16", 58, -1, {}, next);
        if (!error) error = addFilter(m_engine, layers[index], false, {}, -1, -1, {}, next);
    }
    if (error) {
        FwpmTransactionAbort0(m_engine);
        if (m_filters.isEmpty()) release();
        return result(false, "filter-update-failed", error);
    }
    error = FwpmTransactionCommit0(m_engine);
    if (error) { FwpmTransactionAbort0(m_engine); if (m_filters.isEmpty()) release(); return result(false, "commit-failed", error); }
    m_filters=next;
    error=verifyFilters();
    if(error) {release();return result(false,"filter-verification-failed",error);}
    m_owner = owner; m_digest = digest; m_lease.start();m_cleanupError=0;
    auto reply = result(true, "blocked"); reply["filters"] = m_filters.size(); return reply;
#endif
}
}
