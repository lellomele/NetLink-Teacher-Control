// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#include <rfb/rfbclient.h>
#pragma GCC diagnostic pop
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>
#include <thread>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include "libvncclient/write_timeout.h"
#include "libvncclient/tls.h"

// Build the exact patched vendor C sources in this isolated test executable;
// their private logging globals are intentionally not part of veyon-core's ABI.
static void discardVendorLog(const char*, ...) {}
extern "C" {
rfbClientLogProc rfbClientLog=discardVendorLog;
rfbClientLogProc rfbClientErr=discardVendorLog;
}

namespace {
using Context=std::unique_ptr<SSL_CTX,decltype(&SSL_CTX_free)>;
using Session=std::unique_ptr<SSL,decltype(&SSL_free)>;
struct SocketPair {
    QTcpServer listener;QTcpSocket writer;std::unique_ptr<QTcpSocket> reader;
    bool open(bool limited) {
        if(!listener.listen(QHostAddress::LocalHost,0)) return false;
        writer.connectToHost(QHostAddress::LocalHost,listener.serverPort());
        if(!writer.waitForConnected(3000) || !listener.waitForNewConnection(3000)) return false;
        reader.reset(listener.nextPendingConnection());if(!reader) return false;
        if(limited) {
            writer.setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,4096);
            reader->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,4096);
            reader->setReadBufferSize(1);
        }
        return true;
    }
};
bool makeIdentity(SSL_CTX* context) {
    using KeyContext=std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)>;
    using Key=std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)>;
    using Certificate=std::unique_ptr<X509,decltype(&X509_free)>;
    KeyContext generator(EVP_PKEY_CTX_new_id(EVP_PKEY_EC,nullptr),EVP_PKEY_CTX_free);
    EVP_PKEY* raw=nullptr;
    if(!generator || EVP_PKEY_keygen_init(generator.get())<=0 ||
       EVP_PKEY_CTX_set_ec_paramgen_curve_nid(generator.get(),NID_X9_62_prime256v1)<=0 ||
       EVP_PKEY_keygen(generator.get(),&raw)<=0) return false;
    Key key(raw,EVP_PKEY_free);Certificate certificate(X509_new(),X509_free);
    if(!certificate || X509_set_version(certificate.get(),2)!=1 ||
       ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()),1)!=1 ||
       !X509_gmtime_adj(X509_getm_notBefore(certificate.get()),-60) ||
       !X509_gmtime_adj(X509_getm_notAfter(certificate.get()),3600) ||
       X509_set_pubkey(certificate.get(),key.get())!=1) return false;
    auto subject=X509_get_subject_name(certificate.get());
    if(!subject || X509_NAME_add_entry_by_txt(subject,"CN",MBSTRING_ASC,
       reinterpret_cast<const unsigned char*>("Loopback write test"),-1,-1,0)!=1 ||
       X509_set_issuer_name(certificate.get(),subject)!=1 ||
       X509_sign(certificate.get(),key.get(),EVP_sha256())<=0) return false;
    return SSL_CTX_use_certificate(context,certificate.get())==1 && SSL_CTX_use_PrivateKey(context,key.get())==1;
}
bool handshake(SSL* session,rfbSocket socket,bool server) {
    const auto deadline=rfbWriteNowMs()+5000;
    while(rfbWriteNowMs()<deadline) {
        const int result=server?SSL_accept(session):SSL_connect(session);
        if(result==1) return true;
        const int error=SSL_get_error(session,result);
        if(error!=SSL_ERROR_WANT_READ && error!=SSL_ERROR_WANT_WRITE) return false;
        if(rfbWaitForWriteSocket(socket,error==SSL_ERROR_WANT_READ,deadline)<=0) return false;
    }
    return false;
}
struct TlsPair {
    SocketPair sockets;
    Context serverContext{SSL_CTX_new(TLS_server_method()),SSL_CTX_free};
    Context clientContext{SSL_CTX_new(TLS_client_method()),SSL_CTX_free};
    Session server{nullptr,SSL_free},client{nullptr,SSL_free};
    bool open(bool limited) {
        if(!serverContext || !clientContext || !makeIdentity(serverContext.get()) || !sockets.open(limited)) return false;
        SSL_CTX_set_min_proto_version(serverContext.get(),TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(serverContext.get(),TLS1_2_VERSION);
        SSL_CTX_set_min_proto_version(clientContext.get(),TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(clientContext.get(),TLS1_2_VERSION);
        SSL_CTX_set_verify(clientContext.get(),SSL_VERIFY_NONE,nullptr);
        server.reset(SSL_new(serverContext.get()));client.reset(SSL_new(clientContext.get()));
        if(!server || !client || SSL_set_fd(server.get(),int(sockets.reader->socketDescriptor()))!=1 ||
           SSL_set_fd(client.get(),int(sockets.writer.socketDescriptor()))!=1) return false;
        bool accepted=false;
        std::thread receiver([&]{accepted=handshake(server.get(),rfbSocket(sockets.reader->socketDescriptor()),true);});
        const bool connected=handshake(client.get(),rfbSocket(sockets.writer.socketDescriptor()),false);
        receiver.join();return accepted && connected;
    }
};
}

class SocketWriteTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rawUnresponsiveReaderHasAbsoluteDeadline() {
        SocketPair pair;QVERIFY(pair.open(true));
        rfbClient client{};client.sock=rfbSocket(pair.writer.socketDescriptor());client.serverPort=11100;client.readTimeout=1;
        // Windows loopback can accept one very large send despite SO_SNDBUF.
        // Saturate with small writes first, then exercise the actual wait path.
        const QByteArray filler(16384,'q');bool saturated=false;
        for(int i=0;i<4096;++i) {
            const int sent=::send(client.sock,filler.constData(),filler.size(),0);
            if(sent<0) {saturated=true;break;}
        }
        QVERIFY(saturated);
        const QByteArray payload(32*1024*1024,'s');QElapsedTimer elapsed;elapsed.start();
        QVERIFY(!WriteToRFBServer(&client,payload.constData(),unsigned(payload.size())));
        qInfo()<<"Raw slow-reader write deadline elapsed ms:"<<elapsed.elapsed();
        QVERIFY(elapsed.elapsed()<4000);
    }
    void rawResponsiveReaderPreservesBytes() {
        SocketPair pair;QVERIFY(pair.open(false));
        rfbClient client{};client.sock=rfbSocket(pair.writer.socketDescriptor());client.serverPort=11100;client.readTimeout=5;
        const QByteArray payload(1024*1024,'f');QByteArray received;
        std::thread receiver([&] {
            const auto socket=rfbSocket(pair.reader->socketDescriptor());const auto deadline=rfbWriteNowMs()+5000;
            char bytes[8192];
            while(received.size()<payload.size() && rfbWaitForWriteSocket(socket,1,deadline)>0) {
                const int size=::recv(socket,bytes,sizeof(bytes),0);
                if(size>0) received.append(bytes,size);else if(size==0) break;
            }
        });
        const bool sent=WriteToRFBServer(&client,payload.constData(),unsigned(payload.size()));
        receiver.join();QVERIFY(sent);QCOMPARE(received,payload);
    }
    void tlsUnresponsiveReaderHasAbsoluteDeadline() {
        TlsPair pair;QVERIFY(pair.open(true));
        rfbClient client{};client.sock=rfbSocket(pair.sockets.writer.socketDescriptor());
        client.serverPort=11100;client.readTimeout=1;client.tlsSession=pair.client.get();INIT_MUTEX(client.tlsRwMutex);
        const QByteArray payload(32*1024*1024,'t');QElapsedTimer elapsed;elapsed.start();
        const bool sent=WriteToRFBServer(&client,payload.constData(),unsigned(payload.size()));
        TINI_MUTEX(client.tlsRwMutex);
        QVERIFY(!sent);qInfo()<<"TLS slow-reader write deadline elapsed ms:"<<elapsed.elapsed();
        QVERIFY(elapsed.elapsed()<4000);
    }
    void tlsResponsiveReaderPreservesBytes() {
        TlsPair pair;QVERIFY(pair.open(false));
        rfbClient client{};client.sock=rfbSocket(pair.sockets.writer.socketDescriptor());
        client.serverPort=11100;client.readTimeout=5;client.tlsSession=pair.client.get();INIT_MUTEX(client.tlsRwMutex);
        const QByteArray payload(256*1024,'e');QByteArray received;
        std::thread receiver([&] {
            const auto socket=rfbSocket(pair.sockets.reader->socketDescriptor());const auto deadline=rfbWriteNowMs()+5000;
            char bytes[4096];
            while(received.size()<payload.size() && rfbWriteNowMs()<deadline) {
                const int size=SSL_read(pair.server.get(),bytes,sizeof(bytes));
                if(size>0) {received.append(bytes,size);continue;}
                const int error=SSL_get_error(pair.server.get(),size);
                if(error!=SSL_ERROR_WANT_READ && error!=SSL_ERROR_WANT_WRITE) break;
                if(rfbWaitForWriteSocket(socket,error==SSL_ERROR_WANT_READ,deadline)<=0) break;
            }
        });
        const bool sent=WriteToRFBServer(&client,payload.constData(),unsigned(payload.size()));
        receiver.join();TINI_MUTEX(client.tlsRwMutex);QVERIFY(sent);QCOMPARE(received,payload);
    }
    void expiredDeadlineNeverPollsInvalidSocket() {
        QCOMPARE(rfbWaitForWriteSocket(RFB_INVALID_SOCKET,0,rfbWriteNowMs()),0);
    }
};
QTEST_GUILESS_MAIN(SocketWriteTests)
#include "SocketWriteTests.moc"
