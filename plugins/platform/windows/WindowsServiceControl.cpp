/*
 * WindowsServiceControl.h - class for managing a Windows service
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

#include <QOperatingSystemVersion>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>

#include "WindowsServiceControl.h"
#include "WindowsServicePolicy.h"

namespace {
bool waitForServiceState(SC_HANDLE service, DWORD wanted, DWORD pending)
{
	QElapsedTimer deadline;
	deadline.start();
	while(deadline.elapsed() < 30000)
	{
		SERVICE_STATUS status{};
		if(!QueryServiceStatus(service, &status)) return false;
		if(status.dwCurrentState == wanted) return true;
		if(status.dwCurrentState != pending) {
			SetLastError(status.dwWin32ExitCode?status.dwWin32ExitCode:ERROR_SERVICE_NOT_ACTIVE);return false;
		}
		Sleep(200);
	}
	SetLastError(ERROR_TIMEOUT);return false;
}
}


WindowsServiceControl::WindowsServiceControl( const QString& name, bool readOnly ) :
	m_name( name ),
	m_serviceManager( nullptr ),
	m_serviceHandle( nullptr )
{
	m_serviceManager = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_CONNECT | (readOnly?0:SC_MANAGER_CREATE_SERVICE) );

	if( m_serviceManager )
	{
		m_serviceHandle = OpenServiceW( m_serviceManager, reinterpret_cast<LPCWSTR>(m_name.utf16()),
			readOnly?SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS:SERVICE_ALL_ACCESS );
		if( m_serviceHandle == nullptr )
		{
			const auto error = GetLastError();
			m_openError=error;
			vCritical() << qUtf8Printable( QStringLiteral("failed to open service \"%1\" (error %2)")
											   .arg(m_name).arg(error) );
		}
	}
	else
	{
		const auto error = GetLastError();
		m_openError=error;
		vCritical() << qUtf8Printable( QStringLiteral("failed to contact the Service Control Manager (error %1)").arg(error) );
	}
}



WindowsServiceControl::~WindowsServiceControl()
{
	if(m_serviceHandle) CloseServiceHandle( m_serviceHandle );
	if(m_serviceManager) CloseServiceHandle( m_serviceManager );
}



bool WindowsServiceControl::isRegistered()
{
	return m_serviceHandle != nullptr;
}



bool WindowsServiceControl::isRunning()
{
	if( checkService() == false )
	{
		return false;
	}

	SERVICE_STATUS status{};
	if( QueryServiceStatus( m_serviceHandle, &status ) )
	{
		return status.dwCurrentState == SERVICE_RUNNING;
	}

	return false;
}



bool WindowsServiceControl::start()
{
	if( checkService() == false )
	{
		return false;
	}

	SERVICE_STATUS status{};
	if(!QueryServiceStatus(m_serviceHandle, &status)) return false;
	if(status.dwCurrentState == SERVICE_RUNNING) return true;
	if(status.dwCurrentState == SERVICE_STOP_PENDING && !waitForServiceState(m_serviceHandle,SERVICE_STOPPED,SERVICE_STOP_PENDING)) return false;
	if(status.dwCurrentState != SERVICE_START_PENDING &&
		!StartServiceW(m_serviceHandle, 0, nullptr)) {
		vCritical()<<"StartService failed for"<<m_name<<"Windows error"<<GetLastError();return false;
	}
	if(!waitForServiceState(m_serviceHandle, SERVICE_RUNNING, SERVICE_START_PENDING))
	{
		vWarning() << "service" << m_name << "could not be started; Windows error" << GetLastError();
		return false;
	}

	return true;
}




bool WindowsServiceControl::stop()
{
	if( checkService() == false )
	{
		return false;
	}

	SERVICE_STATUS status{};
	if(!QueryServiceStatus(m_serviceHandle, &status)) return false;
	if(status.dwCurrentState == SERVICE_STOPPED) return true;
	if(status.dwCurrentState != SERVICE_STOP_PENDING &&
		!ControlService(m_serviceHandle, SERVICE_CONTROL_STOP, &status)) return false;
	if(!waitForServiceState(m_serviceHandle, SERVICE_STOPPED, SERVICE_STOP_PENDING))
	{
		vWarning() << "failed to stop service within deadline" << m_name;
		return false;
	}
	return true;
}



bool WindowsServiceControl::install( const QString& filePath, const QString& displayName  )
{
	const auto binaryPath = QStringLiteral("\"%1\"").arg( QString( filePath ).replace( QLatin1Char('"'), QString() ) );

	// Remote control needs TCP/RPC/session services, not the optional SMB client.
	const wchar_t* dependencies = QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows10 ?
			L"Tcpip\0RpcSs\0\0" : L"Tcpip\0RpcSs\0LSM\0\0";

	if(!m_serviceManager) return false;
	if(!m_serviceHandle) m_serviceHandle = CreateServiceW(
				m_serviceManager,		// SCManager database
				reinterpret_cast<LPCWSTR>(m_name.utf16()),	// name of service
				reinterpret_cast<LPCWSTR>(displayName.utf16()),// name to display
				SERVICE_ALL_ACCESS,	// desired access
				SERVICE_WIN32_OWN_PROCESS,
				// service type
				SERVICE_AUTO_START,	// start type
				SERVICE_ERROR_NORMAL,	// error control type
				reinterpret_cast<LPCWSTR>(binaryPath.utf16()),		// service's binary
				nullptr,			// no load ordering group
				nullptr,			// no tag identifier
				dependencies,		// dependencies
				nullptr,			// LocalSystem account
				nullptr );			// no password

	if( m_serviceHandle == nullptr )
	{
		const auto error = GetLastError();
		if( error == ERROR_SERVICE_EXISTS )
		{
			vCritical() << qUtf8Printable( tr( "The service \"%1\" is already installed." ).arg( m_name ) );
		}
		else
		{
			vCritical() << qUtf8Printable( tr("The service \"%1\" could not be installed (error %2).")
											   .arg( m_name ).arg( error ) );
		}

		return false;
	}

	// Repair an existing registration too, but never take over another service.
	alignas(QUERY_SERVICE_CONFIGW) std::array<BYTE,8192> configBytes{};DWORD size=0;
	if(!QueryServiceConfigW(m_serviceHandle,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(configBytes.data()),DWORD(configBytes.size()),&size)) return false;
	const auto config=reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(configBytes.data());
	auto existing=QString::fromWCharArray(config->lpBinaryPathName);
	if(existing.startsWith(QLatin1Char('"')) && existing.endsWith(QLatin1Char('"'))) existing=existing.mid(1,existing.size()-2);
	const QFileInfo owner(existing),expected(filePath);
	if(QDir::cleanPath(owner.absolutePath()).compare(QDir::cleanPath(expected.absolutePath()),Qt::CaseInsensitive)!=0 ||
	   (owner.fileName().compare(QStringLiteral("netlink-service.exe"),Qt::CaseInsensitive)!=0 && owner.fileName().compare(QStringLiteral("veyon-service.exe"),Qt::CaseInsensitive)!=0)) {
		vCritical()<<"Refusing to change service owned by another executable"<<m_name;return false;
	}
	if(!ChangeServiceConfigW(m_serviceHandle,SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,SERVICE_ERROR_NORMAL,
		reinterpret_cast<LPCWSTR>(binaryPath.utf16()),nullptr,nullptr,dependencies,nullptr,nullptr,reinterpret_cast<LPCWSTR>(displayName.utf16()))) {
		vCritical()<<"Could not repair service registration"<<m_name<<"Windows error"<<GetLastError();return false;
	}
	DWORD policyError=0;
	if(!WindowsServicePolicy::configure(m_serviceHandle,&policyError) || !setStartType(SERVICE_AUTO_START)) {
		vCritical()<<"Could not configure/verify service startup recovery"<<m_name<<"Windows error"<<policyError;return false;
	}

	// Everything went fine
	vInfo() << qUtf8Printable( tr( "The service \"%1\" has been installed successfully." ).arg( m_name ) );

	return true;
}



bool WindowsServiceControl::uninstall()
{
	if( checkService() == false )
	{
		return false;
	}

	if( stop() == false )
	{
		return false;
	}

	if( DeleteService( m_serviceHandle ) == false )
	{
		const auto error = GetLastError();
		vCritical() << qUtf8Printable( tr( "The service \"%1\" could not be uninstalled (error %2)." )
										   .arg( m_name ).arg( error ) );
		return false;
	}

	vInfo() << qUtf8Printable( tr( "The service \"%1\" has been uninstalled successfully." ).arg( m_name ) );

	return true;
}



bool WindowsServiceControl::setStartType( int startType )
{
	if( checkService() == false )
	{
		return false;
	}

	if( ChangeServiceConfigW( m_serviceHandle,
							 SERVICE_NO_CHANGE,	// dwServiceType
							 static_cast<DWORD>( startType ),
							 SERVICE_NO_CHANGE,	// dwErrorControl
							 nullptr,	// lpBinaryPathName
							 nullptr,	// lpLoadOrderGroup
							 nullptr,	// lpdwTagId
							 nullptr,	// lpDependencies
							 nullptr,	// lpServiceStartName
							 nullptr,	// lpPassword
							 nullptr	// lpDisplayName
							 ) == false )
	{
		const auto error = GetLastError();
		vCritical() << qUtf8Printable( tr("The start type of service \"%1\" could not be changed (error %2).")
										   .arg( m_name ).arg( error ) );
		return false;
	}

	if(startType==SERVICE_AUTO_START) {
		SERVICE_DELAYED_AUTO_START_INFO delayed{FALSE};DWORD error=0;
		if(!ChangeServiceConfig2W(m_serviceHandle,SERVICE_CONFIG_DELAYED_AUTO_START_INFO,&delayed) ||
		   !WindowsServicePolicy::configure(m_serviceHandle,&error)) {
			vCritical()<<"Could not verify automatic service startup/recovery"<<m_name<<"Windows error"<<(error?error:GetLastError());return false;
		}
	}
	alignas(QUERY_SERVICE_CONFIGW) std::array<BYTE,8192> bytes{};DWORD size=0;
	if(!QueryServiceConfigW(m_serviceHandle,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(bytes.data()),DWORD(bytes.size()),&size) ||
	   reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(bytes.data())->dwStartType!=DWORD(startType)) {
		vCritical()<<"Service start type read-back failed"<<m_name;return false;
	}
	return true;
}

QVariantMap WindowsServiceControl::details() const
{
	if(!m_serviceHandle) return {{QStringLiteral("queryError"),quint32(m_openError)}};
	return WindowsServicePolicy::details(m_serviceHandle);
}



bool WindowsServiceControl::checkService() const
{
	if( m_serviceHandle == nullptr )
	{
		vCritical() << qUtf8Printable( tr( "Service \"%1\" could not be found." ).arg( m_name ) );
		return false;
	}

	return true;
}
