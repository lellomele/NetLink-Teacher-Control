/*
 * WindowsServiceCore.cpp - implementation of WindowsServiceCore class
 *
 * Copyright (c) 2006-2022 Tobias Junghans <tobydox@veyon.io>
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

#include <windows.h>
#include <array>

#include <QElapsedTimer>
#include <QThread>

#include "Filesystem.h"
#include "WindowsServiceCore.h"
#include "SasEventListener.h"
#include "PlatformUserFunctions.h"
#include "VeyonConfiguration.h"
#include "WindowsCoreFunctions.h"
#include "WindowsInputDeviceFunctions.h"
#include "WtsSessionManager.h"
#include "WindowsServicePolicy.h"
#include <QMutexLocker>


// clazy:excludeall=rule-of-three
class VeyonServerProcess
{
public:
	VeyonServerProcess() = default;

	~VeyonServerProcess()
	{
		stop();
	}

	void start( DWORD wtsSessionId, const ServiceDataManager::Token& token )
	{
		const auto baseProcessId = WtsSessionManager::findWinlogonProcessId( wtsSessionId );
		const auto user = WtsSessionManager::querySessionInformation( wtsSessionId, WtsSessionManager::SessionInfo::UserName );

		const QStringList extraEnv{
			QStringLiteral("%1=%2").arg( QLatin1String( ServiceDataManager::serviceDataTokenEnvironmentVariable() ),
										 QString::fromUtf8( token.toByteArray() ) )
		};

		vInfo() << "Starting server for WTS session" << wtsSessionId << "with user" << user;
		m_subProcessHandle = WindowsCoreFunctions::runProgramInSession( VeyonCore::filesystem().serverFilePath(), {},
																		extraEnv,
																		baseProcessId, {} );
		if( m_subProcessHandle == nullptr )
		{
			vCritical() << "Failed to start server!";
		}
	}

	void stop()
	{
		if( m_subProcessHandle )
		{
			vInfo() << "Waiting for server to shutdown";
			if( WaitForSingleObject( m_subProcessHandle, ServerWaitTime ) == WAIT_TIMEOUT )
			{
				vWarning() << "Terminating server";
				TerminateProcess( m_subProcessHandle, 0 );
				WaitForSingleObject(m_subProcessHandle, ServerWaitTime);
			}
			CloseHandle( m_subProcessHandle );
			m_subProcessHandle = nullptr;
		}
	}

	bool isRunning() const
	{
		if( m_subProcessHandle &&
				WaitForSingleObject( m_subProcessHandle, ServerQueryTime ) == WAIT_TIMEOUT )
		{
			return true;
		}

		return false;
	}


private:
	static constexpr auto ServerQueryTime = 10;
	static constexpr auto ServerWaitTime = 3000;

	HANDLE m_subProcessHandle{nullptr};

} ;



WindowsServiceCore* WindowsServiceCore::s_instance = nullptr;


WindowsServiceCore::WindowsServiceCore( const QString& name,
										const PlatformServiceFunctions::ServiceEntryPoint& serviceEntryPoint ) :
	m_name( WindowsCoreFunctions::toWCharArray( name ) ),
	m_serviceEntryPoint( serviceEntryPoint )
{
	s_instance = this;
}

WindowsServiceCore::~WindowsServiceCore()
{
	if(s_instance==this) s_instance=nullptr;
}



WindowsServiceCore* WindowsServiceCore::instance()
{
	if( s_instance == nullptr )
	{
		vCritical() << "invalid instance!";
	}

	return s_instance;
}



bool WindowsServiceCore::runAsService()
{
	static const std::array<SERVICE_TABLE_ENTRY, 2> dispatchTable = { {
		{ m_name.data(), serviceMainStatic },
		{ nullptr, nullptr }
	} } ;

	if( !StartServiceCtrlDispatcher( dispatchTable.data() ) )
	{
		vCritical() << "StartServiceCtrlDispatcher failed; Windows error" << GetLastError();
		return false;
	}

	return m_serviceError.loadAcquire()==NO_ERROR;
}



void WindowsServiceCore::manageServerInstances()
{
	m_serverShutdownEvent = CreateEvent( nullptr, false, false, L"Global\\SessionEventVeyonSchool" );
	if(!m_serverShutdownEvent) {m_serviceError.storeRelease(GetLastError());vCritical()<<"Server shutdown event creation failed"<<m_serviceError.loadAcquire();return;}
	ResetEvent( m_serverShutdownEvent );

	if( m_sessionManager.mode() != PlatformSessionManager::Mode::Local )
	{
		manageServersForAllSessions();
	}
	else
	{
		manageServerForConsoleSession();
	}

	CloseHandle( m_serverShutdownEvent );m_serverShutdownEvent=nullptr;
}



void WindowsServiceCore::manageServersForAllSessions()
{
	QMap<WtsSessionManager::SessionId, VeyonServerProcess*> serverProcesses;

	const auto activeSessionOnly = m_sessionManager.mode() == PlatformSessionManager::Mode::Active;

	do
	{
		auto wtsSessionIds = WtsSessionManager::activeSessions();

		const auto consoleSessionId = WtsSessionManager::activeConsoleSession();
		if( consoleSessionId != WtsSessionManager::InvalidSession &&
			wtsSessionIds.contains( consoleSessionId ) == false )
		{
			wtsSessionIds.append( consoleSessionId );
		}

		const auto includeConsoleSession = activeSessionOnly &&
										   wtsSessionIds.size() == 1 &&
										   wtsSessionIds.first() == consoleSessionId;
		const auto excludeConsoleSession = activeSessionOnly &&
										   wtsSessionIds.size() > 1;

		if( excludeConsoleSession )
		{
			wtsSessionIds.removeAll( consoleSessionId );
		}

		for( auto it = serverProcesses.begin(); it != serverProcesses.end(); )
		{
			if( wtsSessionIds.contains( it.key() ) == false )
			{
				delete it.value();
				if( it.key() != consoleSessionId || excludeConsoleSession )
				{
					m_sessionManager.closeSession( QString::number(it.key() ) );
				}
				it = serverProcesses.erase( it );
			}
			else
			{
				++it;
			}
		}

		for( auto wtsSessionId : wtsSessionIds )
		{
			if( serverProcesses.contains( wtsSessionId ) == false )
			{
				if( wtsSessionId != consoleSessionId || includeConsoleSession )
				{
					m_sessionManager.openSession( QString::number(wtsSessionId) );
				}

				auto serverProcess = new VeyonServerProcess;
				serverProcess->start( wtsSessionId, m_dataManager->token() );

				serverProcesses[wtsSessionId] = serverProcess;
			}
		}

		std::array<HANDLE, 2> events{m_sessionChangeEvent, m_stopServiceEvent};
		if(WaitForMultipleObjects(events.size(), events.data(), FALSE, SessionPollingInterval)==WAIT_FAILED) {
			m_serviceError.storeRelease(GetLastError());vCritical()<<"Service session wait failed"<<m_serviceError.loadAcquire();break;
		}

	} while (m_serviceStopRequested == 0);

	vInfo() << "Service shutdown";

	SetEvent( m_serverShutdownEvent );

	qDeleteAll(serverProcesses);
}



void WindowsServiceCore::manageServerForConsoleSession()
{
	VeyonServerProcess veyonServerProcess;

	auto oldWtsSessionId = WtsSessionManager::InvalidSession;

	QElapsedTimer lastServerStart;

	do {
		const auto sessionChanged = m_sessionChanged.testAndSetOrdered(1, 0);
		const auto wtsSessionId = WtsSessionManager::activeConsoleSession();

		if( oldWtsSessionId != wtsSessionId || sessionChanged )
		{
			vInfo() << "Session ID changed from" << oldWtsSessionId << "to" << wtsSessionId;

			if( oldWtsSessionId != WtsSessionManager::InvalidSession || sessionChanged )
			{
				// workaround for situations where server is stopped while it is still starting up
				do
				{
					SetEvent( m_serverShutdownEvent );
				}
				while( lastServerStart.elapsed() < MinimumServerUptimeTime && veyonServerProcess.isRunning() );

				veyonServerProcess.stop();
			}

			if( wtsSessionId != WtsSessionManager::InvalidSession || sessionChanged )
			{
				veyonServerProcess.stop();
				if (m_serviceStopRequested == 0 &&
					wtsSessionId != WtsSessionManager::InvalidSession)
				{
					veyonServerProcess.start( wtsSessionId, m_dataManager->token() );
					lastServerStart.restart();
				}
			}

			oldWtsSessionId = wtsSessionId;
		}
		else if (veyonServerProcess.isRunning() == false)
		{
			if (wtsSessionId != WtsSessionManager::InvalidSession)
			{
				veyonServerProcess.start( wtsSessionId, m_dataManager->token() );
				lastServerStart.restart();
			}

			oldWtsSessionId = wtsSessionId;
		}

		std::array<HANDLE, 2> events{m_sessionChangeEvent, m_stopServiceEvent};
		if(WaitForMultipleObjects(events.size(), events.data(), FALSE, SessionPollingInterval)==WAIT_FAILED) {
			m_serviceError.storeRelease(GetLastError());vCritical()<<"Service console wait failed"<<m_serviceError.loadAcquire();break;
		}

	} while (m_serviceStopRequested == 0);

	vInfo() << "Service shutdown";

	SetEvent( m_serverShutdownEvent );
	veyonServerProcess.stop();
}



void WindowsServiceCore::serviceMainStatic( DWORD argc, LPWSTR* argv )
{
	Q_UNUSED(argc)
	Q_UNUSED(argv)

	instance()->serviceMain();
}



DWORD WindowsServiceCore::serviceCtrlStatic( DWORD ctrlCode, DWORD eventType, LPVOID eventData, LPVOID context )
{
	return instance()->serviceCtrl( ctrlCode, eventType, eventData, context );
}



void WindowsServiceCore::serviceMain()
{
	DWORD context = 1;

	m_statusHandle = RegisterServiceCtrlHandlerEx( m_name.data(), serviceCtrlStatic, &context );

	if( m_statusHandle == nullptr )
	{
		m_serviceError.storeRelease(GetLastError());vCritical() << "Invalid service status handle; Windows error"<<m_serviceError.loadAcquire();
		return;
	}

	{QMutexLocker lock(&m_statusMutex);m_status={};m_status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;}

	if( reportStatus( SERVICE_START_PENDING, NO_ERROR, ServiceStartTimeout ) == false )
	{
		const auto error=GetLastError();m_serviceError.storeRelease(error?error:ERROR_SERVICE_REQUEST_TIMEOUT);
		vCritical() << "Could not report service initialization; Windows error"<<m_serviceError.loadAcquire();
		reportStatus( SERVICE_STOPPED, m_serviceError.loadAcquire(), 0 );
		return;
	}

	m_stopServiceEvent = CreateEvent( nullptr, false, false, nullptr );
	m_sessionChangeEvent = CreateEvent(nullptr, false, false, nullptr);
	if(!m_stopServiceEvent || !m_sessionChangeEvent) {
		const auto error=GetLastError();m_serviceError.storeRelease(error?error:ERROR_NOT_ENOUGH_MEMORY);
		vCritical()<<"Service control event creation failed; Windows error"<<m_serviceError.loadAcquire();
		closeControlEvents();reportStatus(SERVICE_STOPPED,m_serviceError.loadAcquire(),0);return;
	}
	// IPC/session initialization used to happen before connecting to the SCM.
	// At boot it could consume the entire dispatcher deadline without reporting
	// START_PENDING. Report real initialization milestones, never fake heartbeats.
	QElapsedTimer startup;startup.start();
	vInfo()<<"Service initialization started";
	m_dataManager.reset(new ServiceDataManager);
	if(m_sessionManager.mode()!=PlatformSessionManager::Mode::Active ||
	   (WtsSessionManager::activeSessions().size()<=1 && WtsSessionManager::activeConsoleSession()!=WtsSessionManager::InvalidSession))
		m_sessionManager.openSession(QStringLiteral("0 (console)"));
	WindowsCoreFunctions::enablePrivilege(SE_ASSIGNPRIMARYTOKEN_NAME,true);
	WindowsCoreFunctions::enablePrivilege(SE_INCREASE_QUOTA_NAME,true);
	WindowsInputDeviceFunctions::checkInterceptionInstallation();
	SasEventListener sasEventListener;
	if(!reportStatus(SERVICE_START_PENDING,NO_ERROR,ServiceStartTimeout)) {
		m_serviceError.storeRelease(GetLastError());m_dataManager.reset();closeControlEvents();
		reportStatus(SERVICE_STOPPED,m_serviceError.loadAcquire(),0);return;
	}

	if( reportStatus( SERVICE_RUNNING, NO_ERROR, 0 ) == false )
	{
		m_serviceError.storeRelease(GetLastError());vCritical() << "Could not report service as running; Windows error"<<m_serviceError.loadAcquire();
		m_dataManager.reset();closeControlEvents();reportStatus(SERVICE_STOPPED,m_serviceError.loadAcquire(),0);
		return;
	}

	vInfo()<<"Service running; initialization milliseconds"<<startup.elapsed();
	sasEventListener.start();

	m_serviceEntryPoint();

	closeControlEvents();

	sasEventListener.stop();
	if(!sasEventListener.wait(SasEventListener::WaitPeriod)) {
		vWarning()<<"SAS listener stop exceeded its deadline; waiting for safe cleanup";
		sasEventListener.wait();
	}
	m_dataManager.reset();

	// Tell the service manager that we've stopped.
	const auto exitCode=WindowsServicePolicy::stoppedExit(m_serviceStopRequested.loadAcquire()!=0,m_serviceError.loadAcquire());
	m_serviceError.storeRelease(exitCode);
	if(exitCode) vCritical()<<"Service ended unexpectedly; Windows error"<<exitCode;
	else vInfo()<<"Service stopped by explicit stop/shutdown request";
	reportStatus( SERVICE_STOPPED, exitCode, 0 );
}

void WindowsServiceCore::closeControlEvents()
{
	QMutexLocker lock(&m_eventMutex);
	if(m_sessionChangeEvent) {CloseHandle(m_sessionChangeEvent);m_sessionChangeEvent=nullptr;}
	if(m_stopServiceEvent) {CloseHandle(m_stopServiceEvent);m_stopServiceEvent=nullptr;}
}



DWORD WindowsServiceCore::serviceCtrl( DWORD ctrlCode, DWORD eventType, LPVOID eventData, LPVOID context )
{
	static const QMap<DWORD, const char *> controlMessages{
		{ SERVICE_CONTROL_SHUTDOWN, "SHUTDOWN" },
		{ SERVICE_CONTROL_STOP, "STOP" },
		{ SERVICE_CONTROL_PAUSE, "PAUSE" },
		{ SERVICE_CONTROL_CONTINUE, "CONTINUE" },
		{ SERVICE_CONTROL_INTERROGATE, "INTERROGATE" },
		{ SERVICE_CONTROL_PARAMCHANGE, "PARAM CHANGE" },
		{ SERVICE_CONTROL_DEVICEEVENT, "DEVICE EVENT" },
		{ SERVICE_CONTROL_HARDWAREPROFILECHANGE, "HARDWARE PROFILE CHANGE" },
		{ SERVICE_CONTROL_POWEREVENT, "POWER EVENT" },
		{ SERVICE_CONTROL_SESSIONCHANGE, "SESSION CHANGE" }
	};

	static const QMap<DWORD, const char *> sessionChangeEventTypes{
		{ WTS_CONSOLE_CONNECT, "CONSOLE CONNECT" },
		{ WTS_CONSOLE_DISCONNECT, "CONSOLE DISCONNECT" },
		{ WTS_REMOTE_CONNECT, "REMOTE CONNECT" },
		{ WTS_REMOTE_DISCONNECT, "REMOTE DISCONNECT" },
		{ WTS_SESSION_LOGON, "LOGON" },
		{ WTS_SESSION_LOGOFF, "LOGOFF" },
		{ WTS_SESSION_LOCK, "LOCK" },
		{ WTS_SESSION_UNLOCK, "UNLOCK" },
		{ WTS_SESSION_REMOTE_CONTROL, "REMOTE CONTROL" },
		{ WTS_SESSION_CREATE, "CREATE" },
		{ WTS_SESSION_TERMINATE, "TERMINATE" }
	};

	if( ctrlCode != SERVICE_CONTROL_SESSIONCHANGE &&
		controlMessages.contains( ctrlCode ) )
	{
		vDebug() << "control code:" << controlMessages[ctrlCode] << "event type:" << eventType;
	}

	// What control code have we been sent?
	switch( ctrlCode )
	{
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		m_serviceStopRequested = 1;
		reportStatus(SERVICE_STOP_PENDING,NO_ERROR,30000);
		{QMutexLocker lock(&m_eventMutex);if(m_stopServiceEvent) SetEvent(m_stopServiceEvent);}
		break;

	case SERVICE_CONTROL_INTERROGATE:
		// Service control manager just wants to know our state
		break;

	case SERVICE_CONTROL_SESSIONCHANGE:
		if( sessionChangeEventTypes.contains( eventType ) )
		{
			const auto notification = reinterpret_cast<WTSSESSION_NOTIFICATION *>( eventData );
			vDebug() << "session change event:" << sessionChangeEventTypes[eventType]
					 << "for session" << ( notification ? notification->dwSessionId : -1 );
		}
		switch( eventType )
		{
		case WTS_SESSION_LOGON:
		case WTS_SESSION_LOGOFF:
		case WTS_CONSOLE_CONNECT:
		case WTS_CONSOLE_DISCONNECT:
		case WTS_REMOTE_CONNECT:
		case WTS_REMOTE_DISCONNECT:
			m_sessionChanged = 1;
			{QMutexLocker lock(&m_eventMutex);if(m_sessionChangeEvent) SetEvent(m_sessionChangeEvent);}
			break;
		}
		break;

	default:
		// Control code not recognised
		break;
	}

	// Tell the control manager what we're up to.
	if(ctrlCode==SERVICE_CONTROL_INTERROGATE) {
		QMutexLocker lock(&m_statusMutex);
		if(!m_terminalStatus) SetServiceStatus(m_statusHandle,&m_status);
	}

	return NO_ERROR;
}



// Service manager status reporting
bool WindowsServiceCore::reportStatus( DWORD state, DWORD exitCode, DWORD waitHint )
{
	QMutexLocker lock(&m_statusMutex);
	if(m_terminalStatus) return state==SERVICE_STOPPED;
	if(state==SERVICE_RUNNING && m_serviceStopRequested.loadAcquire()) {SetLastError(ERROR_OPERATION_ABORTED);return false;}
	bool result = true;

	// If we're in the start state then we don't want the control manager
	// sending us control messages because they'll confuse us.
	m_status.dwControlsAccepted = WindowsServicePolicy::acceptedControls(state);

	// Save the new status we've been given
	m_status.dwCurrentState = state;
	m_status.dwWin32ExitCode = exitCode;
	m_status.dwWaitHint = waitHint;

	// Update the checkpoint variable to let the SCM know that we
	// haven't died if requests take a long time
	if( ( state == SERVICE_RUNNING ) || ( state == SERVICE_STOPPED ) )
	{
		m_status.dwCheckPoint = 0;
	}
	else
	{
		m_status.dwCheckPoint = ++m_checkpoint;
	}

	// Tell the SCM our new status
	if( !( result = SetServiceStatus( m_statusHandle, &m_status ) ) )
	{
		const auto error=GetLastError();vCritical() << "SetServiceStatus failed; state"<<state<<"Windows error"<<error;SetLastError(error);
	}
	else if(state==SERVICE_STOPPED) m_terminalStatus=true;

	return result;
}
