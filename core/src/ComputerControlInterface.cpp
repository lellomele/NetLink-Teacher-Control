/*
 * ComputerControlInterface.cpp - interface class for controlling a computer
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

#include "BuiltinFeatures.h"
#include "ComputerControlInterface.h"
#include "Computer.h"
#include "FeatureManager.h"
#include "MonitoringMode.h"
#include "NetLinkUi.h"
#include "VeyonConfiguration.h"
#include "VeyonConnection.h"
#include "VncConnection.h"


ComputerControlInterface::ComputerControlInterface( const Computer& computer, int port, QObject* parent ) :
	QObject( parent ),
	m_computer( computer ),
	m_port( port )
{
	m_serviceDiagnosticsClock.start();
	m_serviceDiagnosticsTimer.setInterval(2000);
	connect(&m_serviceDiagnosticsTimer,&QTimer::timeout,this,&ComputerControlInterface::updateServiceDiagnostics);
	m_pingTimer.setInterval(ConnectionWatchdogPingDelay);
	m_pingTimer.setSingleShot(true);
	connect(&m_pingTimer, &QTimer::timeout, this, &ComputerControlInterface::ping);

	m_connectionWatchdogTimer.setInterval( ConnectionWatchdogTimeout );
	m_connectionWatchdogTimer.setSingleShot( true );
	connect( &m_connectionWatchdogTimer, &QTimer::timeout, this, &ComputerControlInterface::restartConnection );

	m_serverVersionQueryTimer.setInterval(ServerVersionQueryTimeout);
	m_serverVersionQueryTimer.setSingleShot(true);
	connect( &m_serverVersionQueryTimer, &QTimer::timeout, this, [this]() {
		setServerVersion(VeyonCore::ApplicationVersion::Unknown);
	});

	connect(&m_statePollingTimer, &QTimer::timeout, this, [this]() {
		updateUser();
		updateActiveFeatures();
	});
}



ComputerControlInterface::~ComputerControlInterface()
{
	// Owners/models may already be tearing down: never publish a name reset
	// from an object whose shared-pointer lifetime has just ended.
	blockSignals(true);
	stop();
}



void ComputerControlInterface::start( QSize scaledFramebufferSize, UpdateMode updateMode, AuthenticationProxy* authenticationProxy )
{
	// make sure we do not leak
	stop();

	m_scaledFramebufferSize = scaledFramebufferSize;

	if( m_computer.hostAddress().isEmpty() == false )
	{
		m_connection = new VeyonConnection;
		m_connection->setAuthenticationProxy( authenticationProxy );

		auto vncConnection = m_connection->vncConnection();
		vncConnection->setHost( m_computer.hostAddress() );
		if( m_port > 0 )
		{
			vncConnection->setPort( m_port );
		}
		vncConnection->setQuality( VncConnection::Quality::Thumbnail );
		vncConnection->setScaledSize( m_scaledFramebufferSize );

		// Disconnect does not retract already queued Qt signals. The generation
		// also rejects callbacks from an old connection after stop()/start().
		const auto generation = m_connectionGeneration;
		connectFramebufferSignals(vncConnection);
		connect(vncConnection, &VncConnection::framebufferSizeChanged, this, [this, generation]() {
			if (generation == m_connectionGeneration) Q_EMIT framebufferSizeChanged();
		});
		connect(vncConnection, &VncConnection::stateChanged, this, [this, generation]() {
			if (generation != m_connectionGeneration) return;
			updateState();
			setMinimumFramebufferUpdateInterval();
			updateServerVersion();
			updateUser();
			updateActiveFeatures();
			updateScreens();
			Q_EMIT stateChanged();
		});
		connect(m_connection, &VeyonConnection::featureMessageReceived, this, [this, generation](const FeatureMessage& message) {
			if (generation != m_connectionGeneration) return;
			handleFeatureMessage(message);
			resetWatchdog();
		});

		setUpdateMode( updateMode );

		vncConnection->start();
	}
	else
	{
		vWarning() << "computer host address is empty!";
	}
}



void ComputerControlInterface::connectFramebufferSignals(VncConnection* transport)
{
	const auto generation = m_connectionGeneration;
	connect(transport, &VncConnection::framebufferUpdateComplete, this, [this, generation]() {
		if (generation == m_connectionGeneration) resetWatchdog();
	});
	connect(transport, &VncConnection::imageUpdated, this, [this, generation]() {
		if (generation == m_connectionGeneration) Q_EMIT framebufferUpdated();
	});
}


void ComputerControlInterface::stop()
{
	++m_connectionGeneration;
	if( m_connection )
	{
		if (auto oldTransport = m_connection->vncConnection())
		{
			disconnect(oldTransport, nullptr, this, nullptr);
		}
		disconnect(m_connection, nullptr, this, nullptr);
		m_connection->stopAndDeleteLater();
		m_connection = nullptr;
	}

	m_pingTimer.stop();
	m_connectionWatchdogTimer.stop();
	m_serverVersionQueryTimer.stop();
	m_statePollingTimer.stop();
	m_serviceDiagnosticsTimer.stop();m_serviceDiagnostics.reset();

	m_state = State::Disconnected;
	clearStudentDeclaration();
	invalidatePolicyConfirmation();
	m_serverVersion = VeyonCore::ApplicationVersion::Unknown;
	Q_EMIT serviceDiagnosticsChanged();
}

void ComputerControlInterface::invalidatePolicyConfirmation()
{
	if(m_networkPolicyStatus.isEmpty()) return;
	setProperty("schoolPolicyVerifiedState",QStringLiteral("unknown"));
	setProperty("schoolPolicyRequest",QString{});setProperty("schoolPolicyDeadline",0);
	if(auto deadline=findChild<QTimer*>(QStringLiteral("schoolPolicyDeadlineTimer"),Qt::FindDirectChildrenOnly)) deadline->stop();
	setNetworkPolicyStatus(NetLinkUi::text("not connected"));
}

void ComputerControlInterface::clearStudentDeclaration()
{
	setProperty("schoolAttendanceRequest", QString{});
	if (auto deadline = findChild<QTimer*>(QStringLiteral("schoolAttendanceDeadlineTimer"), Qt::FindDirectChildrenOnly)) deadline->stop();
	setStudentName({});
}

void ComputerControlInterface::setExpectedStudentName(const QString& name)
{
	if (name == m_expectedStudentName) return;
	m_expectedStudentName = name;
	Q_EMIT userChanged();
}

QString ComputerControlInterface::attendanceStatus() const
{
	if (!m_studentName.isEmpty())
	{
		return !m_expectedStudentName.isEmpty() && m_expectedStudentName != m_studentName
			? NetLinkUi::text("Declared by client — different from assignment") : NetLinkUi::text("Declared by client");
	}
	return m_expectedStudentName.isEmpty() ? NetLinkUi::text("No name") : NetLinkUi::text("Preloaded name");
}

QString ComputerControlInterface::attendanceLabel() const
{
	return attendanceName().isEmpty() ? QString{} : attendanceName() + QStringLiteral(" [") + attendanceStatus() + QLatin1Char(']');
}



bool ComputerControlInterface::hasValidFramebuffer() const
{
	return vncConnection() && vncConnection()->hasValidFramebuffer();
}



QSize ComputerControlInterface::screenSize() const
{
	if( vncConnection() )
	{
		return vncConnection()->image().size();
	}

	return {};
}



void ComputerControlInterface::setScaledFramebufferSize( QSize scaledFramebufferSize )
{
	m_scaledFramebufferSize = scaledFramebufferSize;

	if( vncConnection() )
	{
		vncConnection()->setScaledSize( m_scaledFramebufferSize );
	}
}



QImage ComputerControlInterface::scaledFramebuffer() const
{
	if( vncConnection() && vncConnection()->isConnected() )
	{
		return vncConnection()->scaledFramebuffer();
	}

	return {};
}



QImage ComputerControlInterface::framebuffer() const
{
	if( vncConnection() && vncConnection()->isConnected() )
	{
		return vncConnection()->image();
	}

	return {};
}



void ComputerControlInterface::setServerVersion(VeyonCore::ApplicationVersion version)
{
	m_serverVersionQueryTimer.stop();

	m_serverVersion = version;

	const auto statePollingInterval = VeyonCore::config().computerStatePollingInterval();

	if (m_serverVersion >= VeyonCore::ApplicationVersion::Version_4_7 &&
		statePollingInterval <= 0)
	{
		m_statePollingTimer.stop();
		if (vncConnection()) vncConnection()->setRequiresManualUpdateRateControl(false);

		updateScreens();
		setMinimumFramebufferUpdateInterval();
	}
	else
	{
		if (vncConnection())
		{
			vncConnection()->setRequiresManualUpdateRateControl(true);
		}

		m_statePollingTimer.start(statePollingInterval > 0 ? statePollingInterval :
															 VeyonCore::config().computerMonitoringUpdateInterval());
	}
}



void ComputerControlInterface::setUserInformation( const QString& userLoginName, const QString& userFullName, int sessionId )
{
	if( userLoginName != m_userLoginName ||
		userFullName != m_userFullName ||
		sessionId != m_userSessionId )
	{
		m_userLoginName = userLoginName;
		m_userFullName = userFullName;
		m_userSessionId = sessionId;

		Q_EMIT userChanged();
	}
}



void ComputerControlInterface::setScreens(const ScreenList& screens)
{
	if(screens != m_screens)
	{
		m_screens = screens;

		Q_EMIT screensChanged();
	}
}



void ComputerControlInterface::setActiveFeatures( const FeatureUidList& activeFeatures )
{
	if( activeFeatures != m_activeFeatures )
	{
		m_activeFeatures = activeFeatures;

		Q_EMIT activeFeaturesChanged();
	}
}



void ComputerControlInterface::sendFeatureMessage(const FeatureMessage& featureMessage)
{
	if( m_connection && m_connection->isConnected() )
	{
		m_connection->sendFeatureMessage(featureMessage);
	}
}



bool ComputerControlInterface::isMessageQueueEmpty()
{
	if( vncConnection() && vncConnection()->isConnected() )
	{
		return vncConnection()->isEventQueueEmpty();
	}

	return true;
}



void ComputerControlInterface::setUpdateMode( UpdateMode updateMode )
{
	m_updateMode = updateMode;

	setMinimumFramebufferUpdateInterval();

	if (vncConnection())
	{
		vncConnection()->setSkipHostPing(m_updateMode == UpdateMode::Basic);
	}
}



ComputerControlInterface::Pointer ComputerControlInterface::weakPointer()
{
	return Pointer( this, []( ComputerControlInterface* ) { } );
}



void ComputerControlInterface::ping()
{
	if (m_serverVersion >= VeyonCore::ApplicationVersion::Version_4_7)
	{
		VeyonCore::builtinFeatures().monitoringMode().ping({weakPointer()});
	}
}



void ComputerControlInterface::setMinimumFramebufferUpdateInterval()
{
	auto updateInterval = -1;

	switch (m_updateMode)
	{
	case UpdateMode::Disabled:
		updateInterval = UpdateIntervalDisabled;
		break;

	case UpdateMode::Basic:
	case UpdateMode::Monitoring:
		updateInterval = VeyonCore::config().computerMonitoringUpdateInterval();
		break;

	case UpdateMode::Live:
		break;
	}

	if (vncConnection())
	{
		vncConnection()->setFramebufferUpdateInterval(updateInterval);
	}

	if (m_serverVersion >= VeyonCore::ApplicationVersion::Version_4_7)
	{
		VeyonCore::builtinFeatures().monitoringMode().setMinimumFramebufferUpdateInterval({weakPointer()}, updateInterval);
	}
}



void ComputerControlInterface::resetWatchdog()
{
	if( state() == State::Connected )
	{
		m_pingTimer.start();
		m_connectionWatchdogTimer.start();
	}
}



void ComputerControlInterface::restartConnection()
{
	if( vncConnection() )
	{
		vDebug();
		vncConnection()->restart();

		m_connectionWatchdogTimer.stop();
	}
}



void ComputerControlInterface::updateState()
{
	lock();

	if( vncConnection() )
	{
		switch( vncConnection()->state() )
		{
		case VncConnection::State::Disconnected: m_state = State::Disconnected; break;
		case VncConnection::State::Connecting: m_state = State::Connecting; break;
		case VncConnection::State::Connected: m_state = State::Connected; break;
		case VncConnection::State::HostOffline: m_state = State::HostOffline; break;
		case VncConnection::State::ServerNotRunning: m_state = State::ServerNotRunning; break;
		case VncConnection::State::AuthenticationFailed: m_state = State::AuthenticationFailed; break;
		default: m_state = VncConnection::State::Disconnected; break;
		}
	}
	else
	{
		m_state = State::Disconnected;
	}

	unlock();
	// A declaration belongs to this connection, never to a later Windows session.
	// Keep only the expected seat assignment across disconnect/reconnect.
	if (m_state != State::Connected) {
		clearStudentDeclaration();invalidatePolicyConfirmation();
		m_serviceDiagnosticsTimer.stop();m_serviceDiagnostics.reset();Q_EMIT serviceDiagnosticsChanged();
	} else {
		// A queued state notification can observe Connected again after a fast
		// transport reconnect. Always replace its diagnostic request nonce,
		// even when the intermediate Connecting state was not observed here.
		m_serviceDiagnostics.reset(true);Q_EMIT serviceDiagnosticsChanged();
		updateServiceDiagnostics();m_serviceDiagnosticsTimer.start();
	}
}

void ComputerControlInterface::updateServiceDiagnostics()
{
	if(m_state!=State::Connected) return;
	const auto now=m_serviceDiagnosticsClock.elapsed();
	if(m_serviceDiagnostics.expire(now)) Q_EMIT serviceDiagnosticsChanged();
	const auto id=m_serviceDiagnostics.request(now);
	if(!id.isEmpty()) sendFeatureMessage(FeatureMessage(ServiceDiagnostics::uid(),ServiceDiagnostics::Query).addArgument(0,id).addArgument(1,1));
}



void ComputerControlInterface::updateServerVersion()
{
	lock();

	if (vncConnection() && vncConnection()->isConnected())
	{
		VeyonCore::builtinFeatures().monitoringMode().queryApplicationVersion({weakPointer()});
		m_serverVersionQueryTimer.start();
	}

	unlock();
}



void ComputerControlInterface::updateActiveFeatures()
{
	lock();

	if (vncConnection() && state() == State::Connected)
	{
		VeyonCore::builtinFeatures().monitoringMode().queryActiveFeatures({weakPointer()});
	}
	else
	{
		setActiveFeatures({});
	}

	unlock();
}



void ComputerControlInterface::updateUser()
{
	lock();

	if( vncConnection() && state() == State::Connected )
	{
		if( userLoginName().isEmpty() )
		{
			VeyonCore::builtinFeatures().monitoringMode().queryLoggedOnUserInfo( { weakPointer() } );
		}
	}
	else
	{
		setUserInformation( {}, {}, -1 );
	}

	unlock();
}



void ComputerControlInterface::updateScreens()
{
	lock();

	if (vncConnection() && state() == State::Connected &&
		m_serverVersion >= VeyonCore::ApplicationVersion::Version_4_7)
	{
		VeyonCore::builtinFeatures().monitoringMode().queryScreens({weakPointer()});
	}
	else
	{
		setScreens({});
	}

	unlock();
}



void ComputerControlInterface::handleFeatureMessage( const FeatureMessage& message )
{
	lock();
	if(message.featureUid()==ServiceDiagnostics::uid()) {
		if(m_state==State::Connected && m_serviceDiagnostics.receive(message,m_serviceDiagnosticsClock.elapsed())) Q_EMIT serviceDiagnosticsChanged();
		unlock();return;
	}
	VeyonCore::featureManager().handleFeatureMessage( weakPointer(), message );
	unlock();
}



QDebug operator<<(QDebug stream, ComputerControlInterface::Pointer computerControlInterface)
{
	if (computerControlInterface.isNull() == false)
	{
		stream << qUtf8Printable(computerControlInterface->computer().hostAddress());
	}
	return stream;
}



QDebug operator<<(QDebug stream, const ComputerControlInterfaceList& computerControlInterfaces)
{
	QStringList hostAddresses;
	hostAddresses.reserve(computerControlInterfaces.size());
	for(const auto& computerControlInterface : computerControlInterfaces)
	{
		if (computerControlInterface.isNull() == false)
		{
			hostAddresses.append(computerControlInterface->computer().hostAddress());
		}
	}

	stream << QStringLiteral("[%1]").arg(hostAddresses.join(QLatin1Char(','))).toUtf8().constData();

	return stream;
}
