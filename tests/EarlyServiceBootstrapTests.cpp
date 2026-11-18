// SPDX-License-Identifier: GPL-2.0-or-later
#include <windows.h>
#include <QtTest>
#include <QProcess>
#include <mutex>
#include <vector>
#include "PlatformServiceFunctions.h"
namespace {
std::vector<SERVICE_STATUS> states;
std::mutex statesLock;
LPHANDLER_FUNCTION_EX controller=nullptr;void* controlContext=nullptr;
bool failDispatcher=false,failRegistration=false;
BOOL WINAPI fakeDispatch(const SERVICE_TABLE_ENTRYW* table) {
    if(failDispatcher){SetLastError(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT);return FALSE;}
    table[0].lpServiceProc(0,nullptr);return TRUE;
}
SERVICE_STATUS_HANDLE WINAPI fakeRegister(LPCWSTR,LPHANDLER_FUNCTION_EX handler,void* context) {
    controller=handler;controlContext=context;if(failRegistration){SetLastError(ERROR_ACCESS_DENIED);return nullptr;}
    return reinterpret_cast<SERVICE_STATUS_HANDLE>(1);
}
BOOL WINAPI fakeReport(SERVICE_STATUS_HANDLE,SERVICE_STATUS* state) {
    std::lock_guard<std::mutex> guard(statesLock);states.push_back(*state);return TRUE;
}
}
#define StartServiceCtrlDispatcherW fakeDispatch
#define RegisterServiceCtrlHandlerExW fakeRegister
#define SetServiceStatus fakeReport
#include "../service/src/WindowsServiceBootstrap.h"
#undef StartServiceCtrlDispatcherW
#undef RegisterServiceCtrlHandlerExW
#undef SetServiceStatus
#include "../packaging/StartupCheckPolicy.h"
#include "../plugins/platform/common/SessionProcessSupervisor.h"
#include "WindowsSessionStopLoop.h"
class EarlyServiceBootstrapTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init(){states.clear();failDispatcher=false;failRegistration=false;}
    void attachesBeforeHeavyInitializationAndHandsOff(){
        WindowsServiceBootstrap bootstrap;QVERIFY(bootstrap.isConnected());
        QCOMPARE(states.size(),size_t(1));QCOMPARE(states[0].dwCurrentState,DWORD(SERVICE_START_PENDING));
        QCOMPARE(states[0].dwControlsAccepted,DWORD(0));QCOMPARE(states[0].dwWaitHint,DWORD(120000));
        bootstrap.progress();QCOMPARE(states.size(),size_t(2));QCOMPARE(states[1].dwCheckPoint,DWORD(2));
        QCOMPARE(controller(SERVICE_CONTROL_STOP,0,nullptr,controlContext),DWORD(ERROR_SERVICE_CANNOT_ACCEPT_CTRL));
        const auto applicationThread=GetCurrentThreadId();DWORD callbackThread=0;bool controlled=false;
        QVERIFY(bootstrap.context.run([&]{callbackThread=GetCurrentThreadId();controlled=controller(SERVICE_CONTROL_INTERROGATE,0,nullptr,controlContext)==123;},
            [](unsigned long,unsigned long,void*)->unsigned long{return 123;}));
        QVERIFY(callbackThread!=applicationThread);QVERIFY(controlled);
    }
    void abandonedInitializationReportsFailedStop(){
        {WindowsServiceBootstrap bootstrap;QVERIFY(bootstrap.isConnected());}
        QCOMPARE(states.back().dwCurrentState,DWORD(SERVICE_STOPPED));QVERIFY(states.back().dwWin32ExitCode!=NO_ERROR);
    }
    void failedAttachAndFailedRegistrationDoNotHang(){
        failDispatcher=true;{WindowsServiceBootstrap bootstrap;QVERIFY(!bootstrap.isConnected());}
        failDispatcher=false;failRegistration=true;{WindowsServiceBootstrap bootstrap;QVERIFY(!bootstrap.isConnected());}
    }
    void delayAndEligibility(){
        using namespace StartupCheckPolicy;
        QCOMPARE(delay(L"3"),3);QCOMPARE(delay(L"5"),5);QCOMPARE(delay(L"off"),0);
        for(auto value:{L"0",L"4",L"",L"3 --foreign",L"300"})QCOMPARE(delay(value),-1);
        QCOMPARE(action(SERVICE_AUTO_START,SERVICE_STOPPED,false,false),Action::Start);
        QCOMPARE(action(SERVICE_AUTO_START,SERVICE_RUNNING,false,false),Action::Done);
        QCOMPARE(action(SERVICE_AUTO_START,SERVICE_START_PENDING,false,false),Action::Wait);
        QCOMPARE(action(SERVICE_AUTO_START,SERVICE_STOP_PENDING,false,false),Action::Wait);
        for(auto type:{SERVICE_DEMAND_START,SERVICE_DISABLED})
            QCOMPARE(action(type,SERVICE_STOPPED,false,false),Action::Skip);
        QCOMPARE(action(SERVICE_AUTO_START,SERVICE_STOPPED,true,false),Action::Skip);
        QCOMPARE(action(SERVICE_AUTO_START,SERVICE_STOPPED,false,true),Action::Skip);
        QCOMPARE(Attempts,3u);QCOMPARE(RetryMilliseconds,120000u);
    }
    void failedAndExitedSessionProcessesAreRetriedWithoutReopeningSessions(){
        struct Process {bool running=false;bool isRunning()const{return running;}};
        SessionProcessSupervisor<int,Process> supervisor;std::vector<int> sessions{1,2};
        int opens=0,closes=0,starts=0;bool stopping=false,fail=true;Process* first=nullptr;
        auto reconcile=[&] {supervisor.reconcile(sessions,[&](int){++opens;},[&](int){++closes;},
            [&](Process& process,int id){++starts;process.running=!fail;if(id==1)first=&process;},[&]{return stopping;});};
        reconcile();QCOMPARE(opens,2);QCOMPARE(starts,2);
        fail=false;reconcile();QCOMPARE(opens,2);QCOMPARE(starts,4);
        reconcile();QCOMPARE(starts,4); // healthy children are not restarted
        first->running=false;reconcile();QCOMPARE(starts,5);QCOMPARE(opens,2);
        sessions={2};reconcile();QCOMPARE(closes,1);
        sessions={2,3};stopping=true;reconcile();QCOMPARE(opens,2);QCOMPARE(starts,5);
        supervisor.clear();
    }
    void stopSignalsAreIsolatedStickyAndSurviveLateOpen(){
        WindowsSessionStop::Event first,second,a,b,late,standalone,bad;
        QVERIFY(first.create());QVERIFY(second.create());QVERIFY(first.name()!=second.name());
        QVERIFY(a.open(first.name(),SYNCHRONIZE));QVERIFY(b.open(second.name(),SYNCHRONIZE));
        QVERIFY(first.signal());QVERIFY(late.open(first.name(),SYNCHRONIZE));
        QCOMPARE(WaitForSingleObject(a.handle(),0),DWORD(WAIT_OBJECT_0));
        QCOMPARE(WaitForSingleObject(late.handle(),0),DWORD(WAIT_OBJECT_0));
        QCOMPARE(WaitForSingleObject(b.handle(),0),DWORD(WAIT_TIMEOUT));
        QVERIFY(standalone.open({},SYNCHRONIZE));QVERIFY(second.signal());
        QCOMPARE(WaitForSingleObject(b.handle(),0),DWORD(WAIT_OBJECT_0));
        QCOMPARE(WaitForSingleObject(standalone.handle(),0),DWORD(WAIT_TIMEOUT));
        QVERIFY(!bad.open(QStringLiteral("unexpected-name"),SYNCHRONIZE));QVERIFY(!bad.handle());
        QVERIFY(!bad.open(WindowsSessionStop::prefix()+QUuid::createUuid().toString(QUuid::WithoutBraces),SYNCHRONIZE));
    }
    void twoSupervisedProcessesStopIndependently() {
        WindowsSessionStop::Event first,second;QVERIFY(first.create());QVERIFY(second.create());
        QProcess a,b;const auto exe=QCoreApplication::applicationFilePath();
        a.start(exe,{QStringLiteral("--stop-waiter"),first.name()});b.start(exe,{QStringLiteral("--stop-loop"),second.name()});
        QVERIFY(a.waitForStarted());QVERIFY(b.waitForStarted());
        QVERIFY(first.signal());QVERIFY(a.waitForFinished(5000));QCOMPARE(a.exitCode(),0);
        QVERIFY(!b.waitForFinished(200));QVERIFY(second.signal());QVERIFY(b.waitForFinished(5000));QCOMPARE(b.exitCode(),0);
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()==3 && args.at(1)==QStringLiteral("--stop-waiter")) {
        WindowsSessionStop::Event stop;if(!stop.open(args.at(2),SYNCHRONIZE)) return 2;
        return WaitForSingleObject(stop.handle(),8000)==WAIT_OBJECT_0?0:3;
    }
    if(args.size()==3 && args.at(1)==QStringLiteral("--stop-loop")) return WindowsSessionStop::waitInEventLoop(args.at(2))?0:4;
    EarlyServiceBootstrapTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "EarlyServiceBootstrapTests.moc"
