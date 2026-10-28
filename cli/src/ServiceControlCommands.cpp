/*
 * ServiceControlCommands.cpp - implementation of ServiceControlCommands class
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

#include "CommandLineIO.h"
#include "ServiceControlCommands.h"
#include "VeyonServiceControl.h"
#include "PlatformServiceFunctions.h"
#include "VeyonConfiguration.h"
#include <QJsonDocument>
#include <QJsonObject>

ServiceControlCommands::ServiceControlCommands( QObject* parent ) :
	QObject( parent ),
	m_commands( {
{ QStringLiteral("register"), tr( "Register Veyon Service" ) },
{ QStringLiteral("unregister"), tr( "Unregister Veyon Service" ) },
{ QStringLiteral("start"), tr( "Start Veyon Service" ) },
{ QStringLiteral("stop"), tr( "Stop Veyon Service" ) },
{ QStringLiteral("restart"), tr( "Restart Veyon Service" ) },
{ QStringLiteral("status"), tr( "Query status of Veyon Service" ) },
{ QStringLiteral("verify"), tr( "Verify service startup mode, recovery policy and running state" ) },
				} )
{
}



CommandLinePluginInterface::RunResult ServiceControlCommands::handle_register( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	VeyonServiceControl serviceControl;
	return serviceControl.registerService() && serviceControl.isServiceRegistered() ? Successful : Failed;
}



CommandLinePluginInterface::RunResult ServiceControlCommands::handle_unregister( const QStringList& arguments )
{
	Q_UNUSED(arguments)
	VeyonServiceControl serviceControl;
	return serviceControl.unregisterService() && !serviceControl.isServiceRegistered() ? Successful : Failed;
}



CommandLinePluginInterface::RunResult ServiceControlCommands::handle_start( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	VeyonServiceControl serviceControl;
	return serviceControl.startService() && serviceControl.isServiceRunning() ? Successful : Failed;
}



CommandLinePluginInterface::RunResult ServiceControlCommands::handle_stop( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	VeyonServiceControl serviceControl;
	return serviceControl.stopService() && !serviceControl.isServiceRunning() ? Successful : Failed;
}



CommandLinePluginInterface::RunResult ServiceControlCommands::handle_restart( const QStringList& arguments )
{
	if(handle_stop(arguments)!=Successful) return Failed;
	return handle_start( arguments );
}



CommandLinePluginInterface::RunResult ServiceControlCommands::handle_status( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	if( VeyonServiceControl().isServiceRunning() )
	{
		CommandLineIO::print( tr( "Service is running" ) );
	}
	else
	{
		CommandLineIO::print( tr( "Service is not running" ) );
	}

	return NoResult;
}

CommandLinePluginInterface::RunResult ServiceControlCommands::handle_verify(const QStringList& arguments)
{
	Q_UNUSED(arguments)
	const auto details=VeyonCore::platform().serviceFunctions().serviceDetails(VeyonServiceControl::name());
	CommandLineIO::print(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(details)).toJson(QJsonDocument::Compact)));
#ifdef Q_OS_WIN
	const auto automatic=VeyonCore::config().autostartService();
	const auto expected=automatic?2:3; // SERVICE_AUTO_START / SERVICE_DEMAND_START.
	if(!automatic) CommandLineIO::print(QStringLiteral("WARNING: automatic startup is disabled in the preserved configuration. Enable Service/Autostart in Configurator to start after reboot."));
	return !details.contains(QStringLiteral("queryError")) && details.value(QStringLiteral("state")).toString()==QStringLiteral("running") &&
		details.value(QStringLiteral("startType")).toInt()==expected && details.value(QStringLiteral("recoveryPolicyVerified")).toBool()?Successful:Failed;
#else
	return VeyonServiceControl().isServiceRunning()?Successful:Failed;
#endif
}
