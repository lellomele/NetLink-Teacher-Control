// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

// File naming/presence only: no VeyonCore initialization, registry or service access.
namespace NetLinkIdentity {
enum class Application { Master, Configurator, Service, Server, Worker, Cli, WCli, Companion };
inline QString component(Application application) {
    switch(application) {
    case Application::Master: return QStringLiteral("master");
    case Application::Configurator: return QStringLiteral("configurator");
    case Application::Service: return QStringLiteral("service");
    case Application::Server: return QStringLiteral("server");
    case Application::Worker: return QStringLiteral("worker");
    case Application::Cli: return QStringLiteral("cli");
    case Application::WCli: return QStringLiteral("wcli");
    case Application::Companion: return QStringLiteral("companion");
    }
    return {};
}
inline QString executableName(Application application) {
    const auto part=component(application);if(part.isEmpty()) return {};
#ifdef Q_OS_WIN
    return QStringLiteral("netlink-")+part+QStringLiteral(".exe");
#else
    return (application==Application::Companion?QStringLiteral("netlink-"):QStringLiteral("veyon-"))+part;
#endif
}
inline QString executablePath(Application application) {
    const auto name=executableName(application);if(name.isEmpty()) return {};
    return QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath(name));
}
// WFP cannot derive an App-ID from an absent file. The service is mandatory
// (keep its path so a damaged installation fails closed); Master is optional.
inline QStringList discoveryExecutablePaths(const QString& directory) {
    const QDir root(directory);
    QStringList paths{QDir::toNativeSeparators(root.filePath(executableName(Application::Service)))};
    const auto master=QDir::toNativeSeparators(root.filePath(executableName(Application::Master)));
    if(QFileInfo(master).isFile()) paths.append(master);
    return paths;
}
// Security checks accept only the current full installed path, never a legacy
// alias, basename match or arbitrary NetLink-looking executable.
inline bool matchesInstalledPath(const QString& observed,const QString& directory,Application application) {
    const auto name=executableName(application);
    if(name.isEmpty() || !QDir::isAbsolutePath(observed) || !QDir::isAbsolutePath(directory)) return false;
#ifdef Q_OS_WIN
    constexpr auto sensitivity=Qt::CaseInsensitive;
#else
    constexpr auto sensitivity=Qt::CaseSensitive;
#endif
    return QDir::cleanPath(QDir::fromNativeSeparators(observed)).compare(
        QDir::cleanPath(QDir(directory).filePath(name)),sensitivity)==0;
}
// Diagnostic labels are not trusted identities. Accept legacy names solely to
// retain older reports and distinguish a separately installed original Veyon.
inline bool matchesDiagnosticName(const QString& name,Application application) {
    const auto part=component(application);if(part.isEmpty()) return false;
    if(name.compare(executableName(application),Qt::CaseInsensitive)==0) return true;
    if(name.compare(QStringLiteral("netlink-")+part+QStringLiteral(".exe"),Qt::CaseInsensitive)==0) return true;
    return application!=Application::Companion &&
        name.compare(QStringLiteral("veyon-")+part+QStringLiteral(".exe"),Qt::CaseInsensitive)==0;
}
inline bool isDiagnosticApplication(const QString& name) {
    for(auto application:{Application::Master,Application::Configurator,Application::Service,Application::Server,
        Application::Worker,Application::Cli,Application::WCli,Application::Companion})
        if(matchesDiagnosticName(name,application)) return true;
    return false;
}
}
