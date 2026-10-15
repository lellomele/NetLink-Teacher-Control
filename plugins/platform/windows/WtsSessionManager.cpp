/*
 * WtsSessionManager.cpp - implementation of WtsSessionManager class
 *
 * Copyright (c) 2018-2022 Tobias Junghans <tobydox@veyon.io>
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
#include <wtsapi32.h>
#include <array>
#include <memory>
#include <vector>

#include "Logger.h"
#include "WtsSessionManager.h"


WtsSessionManager::SessionId WtsSessionManager::currentSession()
{
	auto sessionId = InvalidSession;
	if( ProcessIdToSessionId( GetCurrentProcessId(), &sessionId ) == 0 )
	{
		vWarning() << GetLastError();
		return InvalidSession;
	}

	return sessionId;
}



WtsSessionManager::SessionId WtsSessionManager::activeConsoleSession()
{
	return WTSGetActiveConsoleSessionId();
}



WtsSessionManager::SessionList WtsSessionManager::activeSessions()
{
	PWTS_SESSION_INFO sessions;
	DWORD sessionCount = 0;

	const auto result = WTSEnumerateSessions( WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &sessionCount );
	if( result == false )
	{
		return {};
	}

	SessionList sessionList;
	sessionList.reserve( sessionCount );

	for( DWORD sessionIndex = 0; sessionIndex < sessionCount; ++sessionIndex )
	{
		const auto session = &sessions[sessionIndex];
		if( session->State == WTSActive ||
			QString::fromWCharArray(session->pWinStationName)
				.compare( QLatin1String("multiseat"), Qt::CaseInsensitive ) == 0 )
		{
			sessionList.append( session->SessionId );
		}
	}

	WTSFreeMemory( sessions );

	return sessionList;
}



QString WtsSessionManager::querySessionInformation( SessionId sessionId, SessionInfo sessionInfo )
{
	if( sessionId == InvalidSession )
	{
		vCritical() << "called with invalid session ID";
		return {};
	}

	WTS_INFO_CLASS infoClass = WTSInitialProgram;

	switch( sessionInfo )
	{
	case SessionInfo::UserName: infoClass = WTSUserName; break;
	case SessionInfo::DomainName: infoClass = WTSDomainName; break;
	default:
		vCritical() << "invalid session info" << sessionInfo << "requested";
		return {};
	}

	QString result;
	LPWSTR pBuffer = nullptr;
	DWORD dwBufferLen;

	if( WTSQuerySessionInformation( WTS_CURRENT_SERVER_HANDLE, sessionId, infoClass,
									&pBuffer, &dwBufferLen ) )
	{
		result = QString::fromWCharArray( pBuffer );
	}
	else
	{
		const auto lastError = GetLastError();
		vCritical() << lastError;
	}

	WTSFreeMemory( pBuffer );

	vDebug() << sessionId << sessionInfo << result;

	return result;
}



WtsSessionManager::ProcessId WtsSessionManager::findWinlogonProcessId( SessionId sessionId )
{
	if( sessionId == InvalidSession )
	{
		vCritical() << "called with invalid session ID";
		return InvalidProcess;
	}

	PWTS_PROCESS_INFO processInfo = nullptr;
	DWORD processCount = 0;

	if( WTSEnumerateProcesses( WTS_CURRENT_SERVER_HANDLE, 0, 1, &processInfo, &processCount ) == false )
	{
		return InvalidProcess;
	}

	auto pid = InvalidProcess;
	const auto processName = QStringLiteral("winlogon.exe");

	for( DWORD proc = 0; proc < processCount; ++proc )
	{
		if( processInfo[proc].ProcessId == 0 )
		{
			continue;
		}

		if( processName.compare( QString::fromWCharArray( processInfo[proc].pProcessName ), Qt::CaseInsensitive ) == 0 &&
			sessionId == processInfo[proc].SessionId )
		{
			pid = processInfo[proc].ProcessId;
			break;
		}
	}

	WTSFreeMemory( processInfo );

	return pid;
}



namespace {
struct TokenHandleCloser {
	void operator()(HANDLE handle) const {if(handle) CloseHandle(handle);}
};
using TokenHandle = std::unique_ptr<void,TokenHandleCloser>;
}

HANDLE WtsSessionManager::copyMatchingUserToken(HANDLE token, PSID expectedSid, SessionId expectedSession)
{
	if(!token || !expectedSid || !IsValidSid(expectedSid) || expectedSession==InvalidSession) return nullptr;
	DWORD size=0, session=0;
	if(!GetTokenInformation(token,TokenSessionId,&session,sizeof(session),&size) || session!=expectedSession)
		return nullptr;
	GetTokenInformation(token,TokenUser,nullptr,0,&size);
	if(size<sizeof(TOKEN_USER) || size>64*1024) return nullptr;
	std::vector<BYTE> user(size);
	if(!GetTokenInformation(token,TokenUser,user.data(),size,&size) ||
		!EqualSid(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,expectedSid)) return nullptr;
	DWORD appContainer=0;
	if(!GetTokenInformation(token,TokenIsAppContainer,&appContainer,sizeof(appContainer),&size) ||
		appContainer || IsTokenRestricted(token)) return nullptr;
	HANDLE primary=nullptr;
	if(!DuplicateTokenEx(token,WtsSessionManager::UserTokenAccess,nullptr,
		SecurityImpersonation,TokenPrimary,&primary)) return nullptr;
	return primary;
}

HANDLE WtsSessionManager::queryUserToken( const QString& userName, SessionId sessionId )
{
	if(userName.isEmpty() || sessionId==InvalidSession) return nullptr;
	DWORD sidLen = SECURITY_MAX_SID_SIZE; // Flawfinder: ignore
	std::array<char, SECURITY_MAX_SID_SIZE> userSID{};
	std::array<wchar_t, DOMAIN_LENGTH> domainName{};
	DWORD domainLen = domainName.size();
	SID_NAME_USE sidNameUse;

	if( LookupAccountName( nullptr,		// system name
						   reinterpret_cast<const wchar_t*>( userName.utf16() ),
						   userSID.data(),
						   &sidLen,
						   domainName.data(),
						   &domainLen,
						   &sidNameUse ) == false )
	{
		vCritical() << "could not look up SID structure";
		return nullptr;
	}

	// The service can obtain the session's actual logon token without touching
	// an arbitrary user process (which might be protected, such as antimalware).
	HANDLE sessionToken=nullptr;
	if(WTSQueryUserToken(sessionId,&sessionToken)) {
		TokenHandle scopedToken(sessionToken);
		if(auto primary=copyMatchingUserToken(sessionToken,userSID.data(),sessionId)) {
			vDebug() << "User token source WTS; session" << sessionId;
			return primary;
		}
		vWarning() << "WTS user token unusable for requested session; trying verified processes" << sessionId;
	} else {
		const auto error=GetLastError();
		vDebug() << "WTSQueryUserToken() failed; session" << sessionId << "error" << error;
	}

	PWTS_PROCESS_INFO processInfo=nullptr;
	DWORD processCount = 0;

	if( WTSEnumerateProcesses( WTS_CURRENT_SERVER_HANDLE, 0, 1, &processInfo, &processCount ) == false )
	{
		vWarning() << "WTSEnumerateProcesses() failed; session" << sessionId << "error" << GetLastError();
		return nullptr;
	}

	HANDLE result=nullptr;

	// Prefer the interactive shell over an arbitrary (possibly elevated) app.
	// This is a preference, not a security exception: every token is still
	// checked for matching SID/session and usable rights. Custom shells retain
	// the same verified fallback on the second pass.
	for(int pass=0;pass<2 && !result;++pass)
	{
		for(DWORD proc=0;proc<processCount;++proc)
		{
			const bool shell=processInfo[proc].pProcessName &&
				QString::fromWCharArray(processInfo[proc].pProcessName)
					.compare(QStringLiteral("explorer.exe"),Qt::CaseInsensitive)==0;
			if((pass==0)!=shell) continue;
			if(processInfo[proc].ProcessId>0 && processInfo[proc].SessionId==sessionId &&
				processInfo[proc].pUserSid && EqualSid(processInfo[proc].pUserSid,userSID.data()))
			{
				TokenHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,false,processInfo[proc].ProcessId));
				if(!process) continue;
				HANDLE processToken=nullptr;
				if(!OpenProcessToken(process.get(),TOKEN_QUERY|TOKEN_DUPLICATE,&processToken)) continue;
				TokenHandle scopedToken(processToken);
				// Recheck the opened token: an enumerated PID may have exited
				// or been reused. Skip unusable tokens and keep trying.
				result=copyMatchingUserToken(processToken,userSID.data(),sessionId);
				if(result) {
					vDebug() << "User token source verified process; session" << sessionId
						<< "PID" << processInfo[proc].ProcessId << "shell preference" << shell;
					break;
				}
			}
		}
	}

	WTSFreeMemory( processInfo );

	if(!result) vWarning() << "No usable user token in WTS session" << sessionId;
	return result;
}



WtsSessionManager::ProcessId WtsSessionManager::findProcessId( const QString& processName )
{
	PWTS_PROCESS_INFO processInfo = nullptr;
	DWORD processCount = 0;

	if( WTSEnumerateProcesses( WTS_CURRENT_SERVER_HANDLE, 0, 1, &processInfo, &processCount ) == false )
	{
		return InvalidProcess;
	}

	auto pid = InvalidProcess;

	for( DWORD proc = 0; proc < processCount; ++proc )
	{
		if( processInfo[proc].ProcessId == 0 )
		{
			continue;
		}

		if( processName.compare( QString::fromWCharArray( processInfo[proc].pProcessName ), Qt::CaseInsensitive ) == 0 )
		{
			pid = processInfo[proc].ProcessId;
			break;
		}
	}

	WTSFreeMemory( processInfo );

	return pid;
}
