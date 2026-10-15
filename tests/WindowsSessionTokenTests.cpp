// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
// Read/duplicate token metadata only: never launch a process, impersonate a
// user, enable privileges, or alter services/configuration in these fixtures.
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <windows.h>
#include <array>
#include <memory>
#include <vector>
#include "WtsSessionManager.h"

namespace {
struct CloseToken {void operator()(HANDLE token) const {if(token) CloseHandle(token);}};
using Token=std::unique_ptr<void,CloseToken>;
}

class WindowsSessionTokenTests : public QObject {
    Q_OBJECT
    Token m_token;
    DWORD m_session=0;
    std::vector<BYTE> m_user;
    PSID sid() const {return reinterpret_cast<const TOKEN_USER*>(m_user.data())->User.Sid;}
    bool restricted() const {return IsTokenRestricted(m_token.get());}
private Q_SLOTS:
    void initTestCase() {
        HANDLE token=nullptr;
        QVERIFY(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE,&token));
        m_token.reset(token);
        DWORD size=0;
        QVERIFY(GetTokenInformation(token,TokenSessionId,&m_session,sizeof(m_session),&size));
        GetTokenInformation(token,TokenUser,nullptr,0,&size);
        QVERIFY(size>=sizeof(TOKEN_USER));m_user.resize(size);
        QVERIFY(GetTokenInformation(token,TokenUser,m_user.data(),size,&size));
        QVERIFY(IsValidSid(sid()));
    }
    void copyRetainsExactIdentitySessionAndPrimaryType() {
        if(restricted()) QSKIP("A restricted test runner cannot supply a desktop-worker token");
        Token copy(WtsSessionManager::copyMatchingUserToken(m_token.get(),sid(),m_session));
        QVERIFY(copy);
        DWORD size=0,session=0;TOKEN_TYPE type=TokenImpersonation;
        QVERIFY(GetTokenInformation(copy.get(),TokenSessionId,&session,sizeof(session),&size));
        QCOMPARE(session,m_session);
        QVERIFY(GetTokenInformation(copy.get(),TokenType,&type,sizeof(type),&size));
        QCOMPARE(type,TokenPrimary);
        GetTokenInformation(copy.get(),TokenUser,nullptr,0,&size);
        std::vector<BYTE> user(size);
        QVERIFY(GetTokenInformation(copy.get(),TokenUser,user.data(),size,&size));
        QVERIFY(EqualSid(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,sid()));
    }
    void differentSessionIsRejected() {
        const DWORD differentSession=m_session==0?1:0;
        Token copy(WtsSessionManager::copyMatchingUserToken(m_token.get(),sid(),differentSession));
        QVERIFY(!copy);
    }
    void differentIdentityIsRejected() {
        std::array<BYTE,SECURITY_MAX_SID_SIZE> other{};DWORD size=other.size();
        QVERIFY(CreateWellKnownSid(WinWorldSid,nullptr,other.data(),&size));
        QVERIFY(!EqualSid(sid(),other.data()));
        Token copy(WtsSessionManager::copyMatchingUserToken(m_token.get(),other.data(),m_session));
        QVERIFY(!copy);
    }
    void invalidInputsAreRejected() {
        QVERIFY(!WtsSessionManager::copyMatchingUserToken(nullptr,sid(),m_session));
        QVERIFY(!WtsSessionManager::copyMatchingUserToken(m_token.get(),nullptr,m_session));
        QVERIFY(!WtsSessionManager::queryUserToken(QString(),m_session));
        QVERIFY(!WtsSessionManager::queryUserToken(QStringLiteral("unused"),WtsSessionManager::InvalidSession));
    }
    void restrictedSourceIsRejected() {
        std::array<BYTE,SECURITY_MAX_SID_SIZE> world{};DWORD size=world.size();
        QVERIFY(CreateWellKnownSid(WinWorldSid,nullptr,world.data(),&size));
        SID_AND_ATTRIBUTES restriction{world.data(),0};HANDLE restrictedToken=nullptr;
        QVERIFY(CreateRestrictedToken(m_token.get(),0,0,nullptr,0,nullptr,1,&restriction,&restrictedToken));
        Token source(restrictedToken);QVERIFY(IsTokenRestricted(source.get()));
        Token copy(WtsSessionManager::copyMatchingUserToken(source.get(),sid(),m_session));
        QVERIFY(!copy);
    }
    void repeatedCopiesDoNotLeakHandles() {
        if(restricted()) QSKIP("A restricted test runner cannot supply a desktop-worker token");
        {Token warmup(WtsSessionManager::copyMatchingUserToken(m_token.get(),sid(),m_session));QVERIFY(warmup);}
        DWORD before=0,after=0;
        QVERIFY(GetProcessHandleCount(GetCurrentProcess(),&before));
        for(int i=0;i<256;++i) {
            Token copy(WtsSessionManager::copyMatchingUserToken(m_token.get(),sid(),m_session));
            QVERIFY(copy);
        }
        QVERIFY(GetProcessHandleCount(GetCurrentProcess(),&after));QCOMPARE(after,before);
    }
    void userLookupReturnsOnlyExactSession() {
        if(restricted()) QSKIP("A restricted test runner cannot supply a desktop-worker token");
        DWORD nameSize=0,domainSize=0;SID_NAME_USE use;
        LookupAccountSidW(nullptr,sid(),nullptr,&nameSize,nullptr,&domainSize,&use);
        QVERIFY(nameSize>0);
        std::vector<wchar_t> name(nameSize),domain(domainSize?domainSize:1);
        QVERIFY(LookupAccountSidW(nullptr,sid(),name.data(),&nameSize,domain.data(),&domainSize,&use));
        auto user=QString::fromWCharArray(name.data());
        if(domainSize) user=QString::fromWCharArray(domain.data())+QLatin1Char('\\')+user;
        Token copy(WtsSessionManager::queryUserToken(user,m_session));QVERIFY(copy);
        DWORD session=0,size=0;
        QVERIFY(GetTokenInformation(copy.get(),TokenSessionId,&session,sizeof(session),&size));
        QCOMPARE(session,m_session);
        Token absent(WtsSessionManager::queryUserToken(user,0xfffffffeU));QVERIFY(!absent);
    }
};
QTEST_GUILESS_MAIN(WindowsSessionTokenTests)
#include "WindowsSessionTokenTests.moc"
