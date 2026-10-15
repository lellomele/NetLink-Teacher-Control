/*
 * AuthenticationProxy.h - declaration of class AuthenticationProxy
 *
 * Copyright (c) 2020-2022 Tobias Junghans <tobydox@veyon.io>
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

#pragma once

#include <QMutexLocker>
#include <QWaitCondition>
#include <QDeadlineTimer>

#include "AuthenticationCredentials.h"
#include "RfbVeyonAuth.h"

class AuthenticationProxy
{
public:
	using AuthenticationMethod = RfbVeyonAuth::Type;
	using AuthenticationMethods = QList<AuthenticationMethod>;

	virtual ~AuthenticationProxy() = default;

	void notifyProtocolError()
	{
		lock();
		m_protocolErrorOccurred = true;
		unlock();

		m_authenticationMethodsAvailable.wakeAll();
	}

	bool protocolErrorOccurred() const
	{
		return m_protocolErrorOccurred;
	}

	void setAuthenticationTypes( const AuthenticationMethods& authenticationTypes )
	{
		lock();
		m_authenticationTypes = authenticationTypes;
		unlock();

		m_authenticationMethodsAvailable.wakeAll();
	}

	AuthenticationMethods authenticationTypes()
	{
		QMutexLocker l( &m_mutex );
		return m_authenticationTypes;
	}

	bool waitForAuthenticationMethods( int timeout )
	{
		const auto notified = waitForCondition(m_authenticationMethodsAvailable, timeout, [this]() {
			return !m_authenticationTypes.isEmpty() || m_protocolErrorOccurred;
		});
		return notified && !protocolErrorOccurred() && !authenticationTypes().isEmpty();
	}

	AuthenticationCredentials credentials()
	{
		QMutexLocker l( &m_mutex );
		return m_credentials;
	}

	void setCredentials( const AuthenticationCredentials& credentials )
	{
		QMutexLocker l( &m_mutex );
		m_credentials = credentials;
	}

	virtual RfbVeyonAuth::Type initCredentials() = 0;

protected:
	// Check the predicate and enter the wait under the same mutex used by the
	// publisher. An unrelated local mutex loses notifications in that window.
	// Recheck after every wake: an empty/spurious update is not readiness.
	template<class Predicate>
	bool waitForCondition(QWaitCondition& condition, int timeout, Predicate ready)
	{
		const QDeadlineTimer deadline(qBound(0, timeout, 60000));
		QMutexLocker locker(&m_mutex);
		while (!ready())
		{
			if (!condition.wait(&m_mutex, deadline)) return ready();
		}
		return true;
	}

	void lock()
	{
		m_mutex.lock();
	}

	void unlock()
	{
		m_mutex.unlock();
	}

	QMutex* mutex()
	{
		return &m_mutex;
	}

private:
	QMutex m_mutex;
	QAtomicInteger<bool> m_protocolErrorOccurred{false};
	AuthenticationCredentials m_credentials;
	AuthenticationMethods m_authenticationTypes;
	QWaitCondition m_authenticationMethodsAvailable;

};
