// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "DiagnosticLink.h"
#include "Diagnostics.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QPointer>
#include <QSet>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>
#include <QtEndian>
#include <functional>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

namespace Companion {
namespace {
constexpr int AuthLimit=4096;
bool unicast(const QHostAddress& address) {
    return !address.isNull() && !address.isMulticast() && address!=QHostAddress::Any &&
        address!=QHostAddress::AnyIPv4 && address!=QHostAddress::AnyIPv6 && address!=QHostAddress::Broadcast;
}
QByteArray b64(const QByteArray& bytes) {return bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);}
bool decode64(const QString& text,int size,QByteArray& result) {
    if(text.size()>128) return false;
    result=QByteArray::fromBase64(text.toLatin1(),QByteArray::Base64UrlEncoding);
    return result.size()==size && b64(result)==text.toLatin1();
}
bool equalSecret(const QByteArray& a,const QByteArray& b) {
    return a.size()==32 && b.size()==32 && CRYPTO_memcmp(a.constData(),b.constData(),32)==0;
}
QByteArray pem(BIO* bio) {
    char* data=nullptr;const auto size=BIO_get_mem_data(bio,&data);
    return size>0 && size<16384?QByteArray(data,int(size)):QByteArray{};
}
bool identity(QSslCertificate& certificate,QSslKey& key,QByteArray& token) {
    using Context=std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)>;
    using Key=std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)>;
    using Cert=std::unique_ptr<X509,decltype(&X509_free)>;
    using Bio=std::unique_ptr<BIO,decltype(&BIO_free)>;
    Context context(EVP_PKEY_CTX_new_id(EVP_PKEY_EC,nullptr),EVP_PKEY_CTX_free);
    EVP_PKEY* raw=nullptr;
    if(!context || EVP_PKEY_keygen_init(context.get())<=0 ||
       EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context.get(),NID_X9_62_prime256v1)<=0 ||
       EVP_PKEY_keygen(context.get(),&raw)<=0) return false;
    Key privateKey(raw,EVP_PKEY_free);Cert cert(X509_new(),X509_free);
    if(!cert || X509_set_version(cert.get(),2)!=1 || ASN1_INTEGER_set(X509_get_serialNumber(cert.get()),1)!=1 ||
       !X509_gmtime_adj(X509_getm_notBefore(cert.get()),-60) ||
       !X509_gmtime_adj(X509_getm_notAfter(cert.get()),3600) || X509_set_pubkey(cert.get(),privateKey.get())!=1) return false;
    auto subject=X509_get_subject_name(cert.get());
    const auto name=QByteArray("NetLink temporary diagnostics");
    if(!subject || X509_NAME_add_entry_by_txt(subject,"CN",MBSTRING_ASC,
          reinterpret_cast<const unsigned char*>(name.constData()),name.size(),-1,0)!=1 ||
       X509_set_issuer_name(cert.get(),subject)!=1 || X509_sign(cert.get(),privateKey.get(),EVP_sha256())<=0) return false;
    Bio certBio(BIO_new(BIO_s_mem()),BIO_free),keyBio(BIO_new(BIO_s_mem()),BIO_free);
    if(!certBio || !keyBio || PEM_write_bio_X509(certBio.get(),cert.get())!=1 ||
       PEM_write_bio_PrivateKey(keyBio.get(),privateKey.get(),nullptr,nullptr,0,nullptr,nullptr)!=1) return false;
    certificate=QSslCertificate(pem(certBio.get()),QSsl::Pem);
    key=QSslKey(pem(keyBio.get()),QSsl::Ec,QSsl::Pem,QSsl::PrivateKey);
    token.resize(32);
    return !certificate.isNull() && !key.isNull() && RAND_bytes(reinterpret_cast<unsigned char*>(token.data()),token.size())==1;
}
class Listener : public QTcpServer {
public:
    std::function<void(qintptr)> accept;
    void incomingConnection(qintptr descriptor) override {
        if(accept) accept(descriptor);
        else {QTcpSocket rejected;rejected.setSocketDescriptor(descriptor);rejected.abort();}
    }
};
// Every peer has independent framing, deadline and state; no shared mutable parser.
class Peer : public QObject {
public:
    enum Stage {Authenticate,Ready,Report,Acknowledgement,Done};
    QSslSocket socket;
    QTimer deadline;
    Stage stage=Authenticate;
    QByteArray buffer;
    quint32 length=0;
    std::function<void(const QByteArray&)> frame;
    std::function<void(const QString&)> error;
    explicit Peer(QObject* parent):QObject(parent),socket(this),deadline(this) {
        deadline.setSingleShot(true);socket.setReadBufferSize(65536);
        connect(&deadline,&QTimer::timeout,this,[this]{fail("Tempo scaduto nel canale diagnostico. Verificare la porta temporanea e ripetere l'abbinamento.");});
        connect(&socket,QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),this,
            [this](QAbstractSocket::SocketError){fail("Canale diagnostico interrotto: "+socket.errorString());});
        connect(&socket,&QSslSocket::disconnected,this,[this]{if(stage!=Done) fail("Canale chiuso prima della conferma del rapporto.");else deleteLater();});
        connect(&socket,&QSslSocket::readyRead,this,[this]{read();});
        deadline.start(15000);
    }
    void fail(const QString& message) {
        if(stage==Done) return;
        stage=Done;deadline.stop();socket.abort();
        if(error) error(message);
        deleteLater();
    }
    bool write(const QByteArray& data) {
        if(stage==Done || data.size()>MaximumReportBytes) return false;
        quint32 size=qToBigEndian<quint32>(quint32(data.size()));
        QByteArray bytes(reinterpret_cast<const char*>(&size),sizeof(size));bytes+=data;
        if(socket.write(bytes)!=bytes.size()) {fail("Scrittura incompleta del rapporto diagnostico.");return false;}
        return true;
    }
    bool write(const QJsonObject& object) {return write(QJsonDocument(object).toJson(QJsonDocument::Compact));}
    void close() {
        stage=Done;deadline.stop();QObject::disconnect(&socket,nullptr,this,nullptr);socket.abort();deleteLater();
    }
    void read() {
        if(stage==Done || !socket.isEncrypted()) return;
        if(!length) {
            buffer+=socket.read(4-buffer.size());if(buffer.size()<4) return;
            length=qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(buffer.constData()));buffer.clear();
            const auto limit=stage==Report?MaximumReportBytes:AuthLimit;
            if(!length || length>quint32(limit)) {fail("Pacchetto diagnostico non valido o troppo grande.");return;}
        }
        buffer+=socket.read(qint64(length)-buffer.size());if(buffer.size()<int(length)) return;
        const auto bytes=buffer;buffer.clear();length=0;
        if(frame) frame(bytes);
        if(stage!=Done && socket.bytesAvailable()) QTimer::singleShot(0,this,[this]{read();});
    }
};
}

struct DiagnosticLink::Impl {
    DiagnosticLink* owner;
    Listener listener;
    QTimer lifetime;
    QSslCertificate certificate;
    QSslKey key;
    QByteArray token;
    QString code;
    QSet<Peer*> peers;
    int attempts=0;
    explicit Impl(DiagnosticLink* parent):owner(parent) {
        lifetime.setSingleShot(true);
        QObject::connect(&lifetime,&QTimer::timeout,owner,[this]{owner->stop();Q_EMIT owner->failed("Abbinamento scaduto dopo il tempo previsto. Generare un nuovo codice.");});
        listener.accept=[this](qintptr descriptor){accept(descriptor);};
    }
    Peer* peer() {
        auto result=new Peer(owner);peers.insert(result);
        QObject::connect(result,&QObject::destroyed,owner,[this,result]{peers.remove(result);});
        result->error=[this](const QString& message){Q_EMIT owner->failed(message);};
        return result;
    }
    void accept(qintptr descriptor) {
        if(peers.size()>=2 || ++attempts>8 || token.isEmpty()) {
            QTcpSocket rejected;rejected.setSocketDescriptor(descriptor);rejected.abort();
            if(attempts>8) {owner->stop();Q_EMIT owner->failed("Troppi tentativi: ricezione terminata. Generare un nuovo abbinamento.");}
            return;
        }
        auto session=peer();session->socket.setLocalCertificate(certificate);session->socket.setPrivateKey(key);
        session->socket.setProtocol(QSsl::TlsV1_2OrLater);session->socket.setPeerVerifyMode(QSslSocket::VerifyNone);
        session->frame=[this,session](const QByteArray& bytes) {
            if(session->stage==Peer::Authenticate) {
                const auto auth=QJsonDocument::fromJson(bytes).object();QByteArray provided;
                if(auth["format"]!="netlink-diagnostic-link" || auth["version"].toInt()!=1 ||
                   !decode64(auth["token"].toString(),32,provided) || !equalSecret(provided,token)) {
                    session->fail("Abbinamento rifiutato: codice non valido o scaduto.");return;
                }
                // Claim the one-shot session before accepting any report bytes.
                listener.close();lifetime.stop();token.clear();code.clear();
                const auto others=peers;for(auto other:others) if(other!=session) other->close();
                session->stage=Peer::Report;session->deadline.start(30000);
                session->write(QJsonObject{{"status","ready"}});return;
            }
            if(session->stage!=Peer::Report) {session->fail("Sequenza diagnostica non valida.");return;}
            QJsonObject report;QString message;
            if(!parseReport(bytes,report,message)) {session->fail("Rapporto remoto rifiutato: "+message);return;}
            if(!session->write(QJsonObject{{"status","received"}})) return;
            session->stage=Peer::Done;session->deadline.stop();
            session->socket.disconnectFromHost();
            QTimer::singleShot(3000,session,[session]{session->close();});
            Q_EMIT owner->reportReceived(report);
        };
        if(!session->socket.setSocketDescriptor(descriptor)) {session->fail("Impossibile accettare il collegamento TLS.");return;}
        session->socket.startServerEncryption();
    }
};

DiagnosticLink::DiagnosticLink(QObject* parent):QObject(parent),d(new Impl(this)) {}
DiagnosticLink::~DiagnosticLink() {
    // A parent widget can already have destroyed its controls when deleting
    // this child. Destruction must never emit callbacks into that partial UI.
    blockSignals(true);stop();
}
void DiagnosticLink::stop() {
    d->listener.close();d->lifetime.stop();d->token.clear();d->code.clear();d->key=QSslKey{};d->certificate=QSslCertificate{};
    const auto peers=d->peers;d->peers.clear();for(auto peer:peers) {QObject::disconnect(peer,nullptr,this,nullptr);peer->close();}
    Q_EMIT stopped();
}
QString DiagnosticLink::pairingCode() const {return d->code;}
bool DiagnosticLink::decodeCode(const QString& code,Endpoint& endpoint) {
    endpoint=Endpoint{};
    if(code.size()>1024 || !code.startsWith("NL1.")) return false;
    const auto encoded=code.mid(4).toLatin1();const auto bytes=QByteArray::fromBase64(encoded,QByteArray::Base64UrlEncoding);
    if(b64(bytes)!=encoded || bytes.size()>768) return false;
    const auto object=QJsonDocument::fromJson(bytes).object();const auto port=object["port"].toInt();
    endpoint.address=QHostAddress(object["ip"].toString());
    if(!unicast(endpoint.address) || port<1 || port>65535 ||
       !decode64(object["pin"].toString(),32,endpoint.pin) || !decode64(object["token"].toString(),32,endpoint.token)) {endpoint=Endpoint{};return false;}
    endpoint.port=quint16(port);return true;
}
bool DiagnosticLink::receiveOnce(const QHostAddress& address,quint16 port,int lifetimeMs) {
    stop();
    if(!unicast(address) || !QSslSocket::supportsSsl() || !identity(d->certificate,d->key,d->token)) {
        Q_EMIT failed("Ricezione non disponibile: indirizzo locale o supporto TLS non valido.");return false;
    }
    if(!d->listener.listen(address,port)) {d->token.clear();Q_EMIT failed("Porta diagnostica non disponibile: "+d->listener.errorString());return false;}
    d->attempts=0;
    const QJsonObject object{{"ip",address.toString()},{"port",d->listener.serverPort()},
        {"pin",QString::fromLatin1(b64(d->certificate.digest(QCryptographicHash::Sha256)))},{"token",QString::fromLatin1(b64(d->token))}};
    d->code="NL1."+QString::fromLatin1(b64(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    d->lifetime.start(qBound(1,lifetimeMs,300000));return true;
}
bool DiagnosticLink::sendReport(const QString& code,const QJsonObject& report) {
    stop();Endpoint endpoint;QJsonObject checked;QString error;
    const auto bytes=QJsonDocument(report).toJson(QJsonDocument::Compact);
    if(!decodeCode(code,endpoint) || !QSslSocket::supportsSsl() || !parseReport(bytes,checked,error)) {
        Q_EMIT failed("Codice di abbinamento, rapporto o supporto TLS non valido.");return false;
    }
    auto session=d->peer();session->stage=Peer::Ready;session->socket.setProtocol(QSsl::TlsV1_2OrLater);
    session->socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    QObject::connect(&session->socket,QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),session,
        [session,endpoint](const QList<QSslError>& errors) {
            // A pairing-code pin replaces public-CA/hostname trust only for this socket.
            if(session->socket.peerCertificate().digest(QCryptographicHash::Sha256)==endpoint.pin)
                session->socket.ignoreSslErrors(errors);
            else session->fail("Identità TLS diversa dal codice di abbinamento. Nessun rapporto inviato.");
        });
    QObject::connect(&session->socket,&QSslSocket::encrypted,session,[session,endpoint] {
        if(session->socket.peerCertificate().digest(QCryptographicHash::Sha256)!=endpoint.pin) {
            session->fail("Impronta TLS non corrispondente. Nessun rapporto inviato.");return;
        }
        session->write(QJsonObject{{"format","netlink-diagnostic-link"},{"version",1},{"token",QString::fromLatin1(b64(endpoint.token))}});
    });
    session->frame=[this,session,bytes](const QByteArray& packet) {
        const auto status=QJsonDocument::fromJson(packet).object()["status"].toString();
        if(session->stage==Peer::Ready && status=="ready") {
            session->stage=Peer::Acknowledgement;session->deadline.start(30000);session->write(bytes);return;
        }
        if(session->stage!=Peer::Acknowledgement || status!="received") {session->fail("Conferma remota non valida: rapporto non confermato.");return;}
        session->stage=Peer::Done;session->deadline.stop();session->socket.disconnectFromHost();
        QTimer::singleShot(3000,session,[session]{session->close();});
        Q_EMIT reportSent();
    };
    session->socket.connectToHostEncrypted(endpoint.address.toString(),endpoint.port);return true;
}
}
