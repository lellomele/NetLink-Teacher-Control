// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#include <QtTest>
#include <QApplication>
#include <QFileInfo>
#include <QOpenGLContext>
#include "VeyonCore.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

class GraphicsInitializationTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rasterStartupDoesNotRequestAnOpenGLContext() {
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts));
        QVERIFY(QOpenGLContext::globalShareContext() == nullptr);
    }
#ifdef Q_OS_WIN
    void startupDoesNotLoadOpenGLDrivers() {
        HMODULE modules[1024]; DWORD bytes = 0;
        QVERIFY(EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes));
        QVERIFY(bytes <= sizeof(modules));
        const QStringList forbidden{"opengl32.dll", "opengl32sw.dll", "atio6axx.dll",
            "atioglxx.dll", "ig4icd64.dll", "ig7icd64.dll", "nvoglv64.dll",
            "netlink-failing-opengl-fixture.dll"};
        for(unsigned i=0;i<bytes/sizeof(HMODULE);++i) {
            wchar_t path[32768]; const auto length=GetModuleFileNameW(modules[i],path,32768);
            QVERIFY(length>0 && length<32768);
            const auto name=QFileInfo(QString::fromWCharArray(path,int(length))).fileName().toLower();
            QVERIFY2(!forbidden.contains(name),qPrintable("Unexpected OpenGL module: "+name));
        }
    }
    void faultFixtureReallyFailsWith1114() {
        SetLastError(ERROR_SUCCESS);
        const auto module=LoadLibraryW(QString::fromUtf8(NETLINK_FAILING_OPENGL_DLL).toStdWString().c_str());
        const auto error=GetLastError();
        if(module) FreeLibrary(module);
        QVERIFY(module==nullptr);
        QCOMPARE(error,DWORD(ERROR_DLL_INIT_FAILED));
    }
#endif
};

int main(int argc,char** argv) {
#ifdef Q_OS_WIN
    // A broken renderer is deliberately available. Correct raster startup
    // must never load it. No installed graphics driver is modified.
    qputenv("QT_OPENGL", "desktop");
    qputenv("QT_OPENGL_DLL", NETLINK_FAILING_OPENGL_DLL);
#endif
    VeyonCore::setupApplicationParameters();
    QApplication application(argc,argv);
    GraphicsInitializationTests tests;
    return QTest::qExec(&tests,argc,argv);
}
#include "GraphicsInitializationTests.moc"
