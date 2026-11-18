/*
 * LogoffEventFilter.cpp - implementation of LogoffEventFilter class
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

#include <QCoreApplication>

#include "VeyonCore.h"
#include "LogoffEventFilter.h"

LogoffEventFilter::LogoffEventFilter()
{
	if(!m_stop.open(qEnvironmentVariable(WindowsSessionStop::variable()),SYNCHRONIZE|EVENT_MODIFY_STATE))
		vCritical()<<"Cannot open VNC session stop event"<<GetLastError();

	QCoreApplication::instance()->installNativeEventFilter( this );
}



bool LogoffEventFilter::nativeEventFilter( const QByteArray& eventType, void* message, long* result )
{
	Q_UNUSED(eventType);
	Q_UNUSED(result);

	if (!message)
	{
		return false;
	}

	const auto winMsg = reinterpret_cast<MSG *>( message )->message;

	if (winMsg == WM_ENDSESSION && reinterpret_cast<MSG*>(message)->wParam)
	{
		vInfo() << "Got WM_ENDSESSION - initiating server shutdown";

		// tell UltraVNC server to quit
		if(!m_stop.signal()) vWarning()<<"Could not signal VNC session logoff"<<GetLastError();
	}

	return false;
}
