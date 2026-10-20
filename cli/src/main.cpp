/*
 * main.cpp - main file for Veyon CLI
 *
 * Copyright (c) 2017-2022 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QApplication>
#include <QImage>
#include <QLabel>

#include <openssl/crypto.h>

#include "ConfigCommands.h"
#include "FeatureCommands.h"
#include "Logger.h"
#include "PluginCommands.h"
#include "PluginManager.h"
#include "ServiceControlCommands.h"
#include "ShellCommands.h"


int main( int argc, char **argv )
{
	VeyonCore::setupApplicationParameters();

	QCoreApplication* app = nullptr;

	// Informational commands must work without any window-system plugin or GPU.
	const auto informationalCommand = argc == 2 ? QString::fromLocal8Bit(argv[1]) : QString{};
	if( informationalCommand == QLatin1String("about") ||
		informationalCommand == QLatin1String("-v") ||
		informationalCommand == QLatin1String("--version") )
	{
		app = new QCoreApplication( argc, argv );
	}
	else
	{
#ifdef Q_OS_LINUX
	// do not create graphical application if DISPLAY is not available
	if( qEnvironmentVariableIsSet( "DISPLAY" ) == false )
	{
		app = new QCoreApplication( argc, argv );
	}
	else
	{
		app = new QApplication( argc, argv );
	}
#else
	app = new QApplication( argc, argv );
#endif
	}

	const auto arguments = QCoreApplication::arguments();

	if( arguments.count() == 2 )
	{
		if( arguments.last() == QLatin1String("graphics-check") )
		{
			if( qobject_cast<QApplication*>(app) == nullptr )
			{
				CommandLineIO::print(QStringLiteral("NetLink raster graphics startup: graphical session required"));
				delete app;
				return 1;
			}
			// Deliberately do not initialize VeyonCore/plugins or read/apply
			// machine configuration: this is a harmless pre-install raster test.
			QLabel label(QStringLiteral("NetLink raster startup check"));
			label.resize(320,80);
			label.ensurePolished();
			QImage image(label.size(),QImage::Format_ARGB32_Premultiplied);
			image.fill(Qt::transparent);
			label.render(&image);
			const bool successful = !image.isNull() &&
				!QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts);
			CommandLineIO::print(successful ?
				QStringLiteral("NetLink raster graphics startup: OK; OpenGL sharing: disabled; Qt %1").arg(QLatin1String(qVersion())) :
				QStringLiteral("NetLink raster graphics startup: FAILED"));
			return successful ? 0 : 1;
		}
		if( arguments.last() == QLatin1String("-v") || arguments.last() == QLatin1String("--version") )
		{
			CommandLineIO::print( VeyonCore::versionString() );
			delete app;
			return 0;
		}
		if( arguments.last() == QLatin1String("about") )
		{
			CommandLineIO::print( QStringLiteral("Veyon: %1 (%2)").arg( VeyonCore::versionString() ).arg( QLatin1String(__DATE__) ) );
			CommandLineIO::print( QStringLiteral("Qt: %1 (built against %2/%3)").
								  arg( QLatin1String(qVersion() ) ).
								  arg( QLatin1String(QT_VERSION_STR) ).
								  arg( QSysInfo::buildAbi() ) );
			CommandLineIO::print( QStringLiteral("OpenSSL: %1").arg( QLatin1String(SSLeay_version(SSLEAY_VERSION)) ) );
			delete app;
			return 0;
		}
	}

	// disable logging at all in order to avoid clobbering
	if( qEnvironmentVariableIsEmpty( Logger::logLevelEnvironmentVariable() ) )
	{
		qputenv( Logger::logLevelEnvironmentVariable(), QByteArray::number( static_cast<int>(Logger::LogLevel::Nothing) ) );
	}

	auto core = new VeyonCore( app, VeyonCore::Component::CLI, QStringLiteral("CLI") );
	VeyonCore::pluginManager().registerExtraPluginInterface( new ConfigCommands( core ) );
	VeyonCore::pluginManager().registerExtraPluginInterface( new FeatureCommands( core ) );
	VeyonCore::pluginManager().registerExtraPluginInterface( new PluginCommands( core ) );
	VeyonCore::pluginManager().registerExtraPluginInterface( new ServiceControlCommands( core ) );
	VeyonCore::pluginManager().registerExtraPluginInterface( new ShellCommands( core ) );

	QHash<CommandLinePluginInterface *, QObject *> commandLinePluginInterfaces;
	const auto pluginObjects = VeyonCore::pluginManager().pluginObjects();
	for( auto pluginObject : pluginObjects )
	{
		auto commandLinePluginInterface = qobject_cast<CommandLinePluginInterface *>( pluginObject );
		if( commandLinePluginInterface )
		{
			commandLinePluginInterfaces[commandLinePluginInterface] = pluginObject;
		}
	}

	const auto module = arguments.value( 1 );

	for( auto it = commandLinePluginInterfaces.constBegin(), end = commandLinePluginInterfaces.constEnd(); it != end; ++it )
	{
		if( it.key()->commandLineModuleName() == module )
		{
			auto runResult = CommandLinePluginInterface::Unknown;

			if( arguments.count() > 2 )
			{
				const auto handler = QStringLiteral( "handle_%1(QStringList)" ).arg( arguments[2] );
				if( it.value()->metaObject()->indexOfMethod( handler.toUtf8().constData() ) >= 0 )
				{
					QMetaObject::invokeMethod( it.value(),
											   QStringLiteral( "handle_%1" ).arg( arguments[2] ).toUtf8().constData(),
							Qt::DirectConnection,
							Q_RETURN_ARG(CommandLinePluginInterface::RunResult, runResult),
							Q_ARG( QStringList, arguments.mid( 3 ) ) );
				}
				else if( arguments[2] != QLatin1String("help") )
				{
					runResult = CommandLinePluginInterface::InvalidCommand;
				}
			}
			else if( it.value()->metaObject()->indexOfMethod("handle_main()") >= 0 )
			{
				QMetaObject::invokeMethod( it.value(),
										   "handle_main",
										   Qt::DirectConnection,
										   Q_RETURN_ARG(CommandLinePluginInterface::RunResult, runResult) );
			}
			else
			{
				runResult = CommandLinePluginInterface::NotEnoughArguments;
			}

			switch( runResult )
			{
			case CommandLinePluginInterface::NoResult:
				return 0;
			case CommandLinePluginInterface::Successful:
				CommandLineIO::print( VeyonCore::tr( "[OK]" ) );
				return 0;
			case CommandLinePluginInterface::Failed:
				CommandLineIO::print( VeyonCore::tr( "[FAIL]" ) );
				return -1;
			case CommandLinePluginInterface::InvalidCommand:
				CommandLineIO::error( VeyonCore::tr( "Invalid command!" ) );
				break;
			case CommandLinePluginInterface::InvalidArguments:
				CommandLineIO::error( VeyonCore::tr( "Invalid arguments given" ) );
				return -1;
			case CommandLinePluginInterface::NotEnoughArguments:
				CommandLineIO::error( VeyonCore::tr( "Not enough arguments given - "
													 "use \"%1 help\" for more information" ).arg( module ) );
				return -1;
			case CommandLinePluginInterface::NotLicensed:
				CommandLineIO::error( VeyonCore::tr( "Plugin not licensed" ) );
				return -1;
			case CommandLinePluginInterface::Unknown:
				break;
			default:
				CommandLineIO::error( VeyonCore::tr( "Unknown result!" ) );
				return -1;
			}

			auto commands = it.key()->commands();
			std::sort( commands.begin(), commands.end() );

			CommandLineIO::print( VeyonCore::tr( "Available commands:" ) );
			for( const auto& command : qAsConst(commands) )
			{
				CommandLineIO::print( QStringLiteral("    %1 - %2").arg( command, it.key()->commandHelp( command ) ) );
			}

			delete core;
			delete app;
			return -1;
		}
	}

	int rc = -1;

	if( module == QLatin1String("help") )
	{
		CommandLineIO::print( VeyonCore::tr( "Available modules:" ) );
		rc = 0;
	}
	else
	{
		CommandLineIO::error( VeyonCore::tr( "No module specified or module not found - available modules are:" ) );
	}

	QStringList modulesHelpStrings;
	for( auto it = commandLinePluginInterfaces.constBegin(), end = commandLinePluginInterfaces.constEnd(); it != end; ++it )
	{
		modulesHelpStrings.append( QStringLiteral( "%1 - %2" ).arg( it.key()->commandLineModuleName(),
																	it.key()->commandLineModuleHelp() ) );
	}

	std::sort( modulesHelpStrings.begin(), modulesHelpStrings.end() );
	std::for_each( modulesHelpStrings.begin(), modulesHelpStrings.end(), [](const QString& s) {
		CommandLineIO::print( QStringLiteral( "    " ) + s ); } );

	delete core;
	delete app;

	return rc;
}
