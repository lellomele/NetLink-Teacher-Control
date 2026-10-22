/*
 * ComputerControlServer.cpp - implementation of ComputerControlServer
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

#include <QCoreApplication>
#include <QSysInfo>
#include <QTimer>
#include <QNetworkInterface>
#include <QTcpSocket>
#include "NetLinkUi.h"

#include "AccessControlProvider.h"
#include "BuiltinFeatures.h"
#include "ComputerControlClient.h"
#include "ComputerControlServer.h"
#include "FeatureManager.h"
#include "FeatureMessage.h"
#include "HostAddress.h"
#include "VeyonConfiguration.h"
#include "SystemTrayIcon.h"


ComputerControlServer::ComputerControlServer( QObject* parent ) :
	VeyonServerInterface(parent),
	m_allowedIPs(),
	m_failedAuthHosts(),
	m_featureWorkerManager( *this ),
	m_serverAuthenticationManager( this ),
	m_serverAccessControlManager( m_featureWorkerManager, VeyonCore::builtinFeatures().desktopAccessDialog(), this ),
	m_vncServer(),
	m_vncProxyServer( VeyonCore::config().localConnectOnly() || AccessControlProvider().isAccessToLocalComputerDenied() ?
						  QHostAddress::LocalHost : QHostAddress::Any,
					  VeyonCore::config().veyonServerPort() + VeyonCore::sessionId(),
					  this,
					  this )
{
	updateTrayIconToolTip();

	// make app terminate once the VNC server thread has finished
	connect( &m_vncServer, &VncServer::finished, QCoreApplication::instance(), &QCoreApplication::quit );

	connect( &m_serverAuthenticationManager, &ServerAuthenticationManager::finished,
			 this, &ComputerControlServer::showAuthenticationMessage );

	connect( &m_serverAccessControlManager, &ServerAccessControlManager::finished,
			 this, &ComputerControlServer::showAccessControlMessage );

	connect(&m_vncProxyServer, &VncProxyServer::serverMessageProcessed,
			 this, &ComputerControlServer::sendAsyncFeatureMessages, Qt::DirectConnection);
	connect( &m_vncProxyServer, &VncProxyServer::connectionClosed, this, &ComputerControlServer::updateTrayIconToolTip );
	auto trayTimer=new QTimer(this);trayTimer->setInterval(15000);
	connect(trayTimer,&QTimer::timeout,this,&ComputerControlServer::updateTrayIconToolTip);trayTimer->start();
}



ComputerControlServer::~ComputerControlServer()
{
	vDebug();

	m_vncProxyServer.stop();
	for(auto watcher:findChildren<QFutureWatcher<void>*>()) watcher->waitForFinished();
}



bool ComputerControlServer::start()
{
	if( m_vncProxyServer.start( m_vncServer.serverPort(), m_vncServer.password() ) == false )
	{
		return false;
	}

	m_vncServer.prepare();
	m_backendProbeClock.start();
	m_vncServer.start();
	updateTrayIconToolTip();
	QTimer::singleShot(1000,this,[this] {m_backendProbeClock.invalidate();updateTrayIconToolTip();});

	return true;
}



VncProxyConnection* ComputerControlServer::createVncProxyConnection( QTcpSocket* clientSocket,
																	 int vncServerPort,
																	 const Password& vncServerPassword,
																	 QObject* parent )
{
	auto client = new ComputerControlClient( this, clientSocket, vncServerPort, vncServerPassword, parent );

	connect( client, &ComputerControlClient::serverConnectionClosed, this,
		[=]() { checkForIncompleteAuthentication( client->serverClient() ); },
		Qt::DirectConnection );

	return client;
}



bool ComputerControlServer::handleFeatureMessage(ComputerControlClient* client)
{
	auto socket = client->proxyClientSocket();
	if(client->serverClient()->authState()!=VncServerClient::AuthState::Successful ||
		client->serverClient()->accessControlState()!=VncServerClient::AccessControlState::Successful) {
		socket->close();return false;
	}

	char messageType;
	if( socket->getChar( &messageType ) == false )
	{
		vWarning() << "could not read feature message!";
		return false;
	}

	// receive message
	FeatureMessage featureMessage;
	if( featureMessage.isReadyForReceive( socket ) == false )
	{
		socket->ungetChar( messageType );
		return false;
	}

	if (featureMessage.receive(socket) == false)
	{
		return false;
	}

	VeyonCore::featureManager().handleFeatureMessage( *this, MessageContext{socket, client}, featureMessage );

	return true;
}



bool ComputerControlServer::sendFeatureMessageReply( const MessageContext& context, const FeatureMessage& reply )
{
	vDebug() << reply;

	if( context.ioDevice() )
	{
		char rfbMessageType = FeatureMessage::RfbMessageType;
		context.ioDevice()->write( &rfbMessageType, sizeof(rfbMessageType) );

		return reply.send( context.ioDevice() );
	}

	return false;
}



void ComputerControlServer::setMinimumFramebufferUpdateInterval(const MessageContext& context, int interval)
{
	auto client = qobject_cast<ComputerControlClient *>(context.connection());
	if (client)
	{
		client->setMinimumFramebufferUpdateInterval(interval);
	}
}



void ComputerControlServer::checkForIncompleteAuthentication( VncServerClient* client )
{
	// connection to client closed during authentication?
	if( client->protocolState() == VncServerProtocol::AuthenticationTypes ||
		client->protocolState() == VncServerProtocol::Authenticating )
	{
		// then mark as failed authentication and report it
		client->setAuthState( VncServerClient::AuthState::Failed );

		showAuthenticationMessage( client );
	}
}



void ComputerControlServer::showAuthenticationMessage( VncServerClient* client )
{
	if( client->authState() == VncServerClient::AuthState::Failed )
	{
		vWarning() << "Authentication failed for" << client->hostAddress() << client->username();

		if( VeyonCore::config().failedAuthenticationNotificationsEnabled() )
		{
			QMutexLocker l( &m_dataMutex );

			if( m_failedAuthHosts.contains( client->hostAddress() ) == false )
			{
				m_failedAuthHosts += client->hostAddress();

				const auto fqdn = HostAddress( client->hostAddress() ).tryConvert( HostAddress::Type::FullyQualifiedDomainName );

				VeyonCore::builtinFeatures().systemTrayIcon().showMessage(
							tr( "Authentication error" ),
							tr( "User \"%1\" at host \"%2\" attempted to access this computer "
								"but could not authenticate successfully." ).arg( client->username(), fqdn ),
							m_featureWorkerManager );
			}
		}
	}
}



void ComputerControlServer::showAccessControlMessage( VncServerClient* client )
{
	if( client->accessControlState() == VncServerClient::AccessControlState::Successful &&
		client->protocolState() == VncServerProtocol::State::AccessControl )
	{
		vInfo() << "Access control successful for" << client->hostAddress() << client->username();
		m_trayAuthorizedClients.insert(client);

		if( VeyonCore::config().remoteConnectionNotificationsEnabled() )
		{
			const auto fqdn = HostAddress( client->hostAddress() ).tryConvert( HostAddress::Type::FullyQualifiedDomainName );

			VeyonCore::builtinFeatures().systemTrayIcon().showMessage(
						tr( "Remote access" ),
						tr( "User \"%1\" at host \"%2\" is now accessing this computer." ).
						arg( client->username(), fqdn ),
						m_featureWorkerManager );
		}

		updateTrayIconToolTip();
	}
	else if( client->accessControlState() == VncServerClient::AccessControlState::Failed )
	{
		vWarning() << "Access control failed for" << client->hostAddress() << client->username();

		if( VeyonCore::config().failedAuthenticationNotificationsEnabled() )
		{
			QMutexLocker l( &m_dataMutex );

			if( m_failedAccessControlHosts.contains( client->hostAddress() ) == false )
			{
				m_failedAccessControlHosts += client->hostAddress();

				const auto fqdn = HostAddress( client->hostAddress() ).tryConvert( HostAddress::Type::FullyQualifiedDomainName );

				VeyonCore::builtinFeatures().systemTrayIcon().showMessage(
							tr( "Access control error" ),
							tr( "User \"%1\" at host \"%2\" attempted to access this computer "
								"but has been blocked due to access control settings." ).
							arg( client->username(), fqdn ),
							m_featureWorkerManager );
			}
		}
	}
}



QFutureWatcher<void>* ComputerControlServer::resolveFQDNs( const QStringList& hosts )
{
	auto watcher = new QFutureWatcher<void>(this);

	watcher->setFuture( QtConcurrent::run( [this, hosts]() {
		for( const auto& host : hosts )
		{
			const auto fqdn = HostAddress( host ).tryConvert( HostAddress::Type::FullyQualifiedDomainName );

			m_dataMutex.lock();
			m_resolvedHostNames[host] = fqdn;
			m_dataMutex.unlock();
		}
	} ) );

	connect( watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater );

	return watcher;
}



void ComputerControlServer::sendAsyncFeatureMessages(VncProxyConnection* connection)
{
	VeyonCore::featureManager().sendAsyncFeatureMessages(*this, MessageContext{connection->proxyClientSocket()});
}



void ComputerControlServer::updateTrayIconToolTip()
{
	// No DNS, native service call or policy-pipe request on the server event loop.
	QSet<const VncServerClient*> current;
	for(auto connection:m_vncProxyServer.clients())
		if(auto client=qobject_cast<ComputerControlClient*>(connection)) current.insert(client->serverClient());
	m_trayAuthorizedClients.intersect(current);
	NetLinkUi::ServiceSnapshot snapshot;
	snapshot.host=QSysInfo::machineHostName();snapshot.port=VeyonCore::config().veyonServerPort()+VeyonCore::sessionId();
	snapshot.bindAddress=m_vncProxyServer.listenAddress().toString();snapshot.listening=m_vncProxyServer.isListening();
	snapshot.plugin=m_vncServer.pluginName();snapshot.external=m_vncServer.externalBackend();snapshot.backendPort=m_vncServer.serverPort();
	snapshot.authorized=m_trayAuthorizedClients.size();snapshot.pending=current.size()-snapshot.authorized;
	snapshot.discovery=VeyonCore::config().schoolDiscoveryEnabled();
	bool backendConnecting=false,backendConnected=false;
	const auto addressText=[](const QHostAddress& address) {bool v4=false;const auto value=address.toIPv4Address(&v4);return v4?QHostAddress(value).toString():address.toString();};
	for(auto connection:m_vncProxyServer.clients()) {
		snapshot.backendReady|=connection->backendReady();
		backendConnecting|=connection->vncServerSocket()->state()!=QAbstractSocket::UnconnectedState;
		backendConnected|=connection->vncServerSocket()->state()==QAbstractSocket::ConnectedState;
		const auto age=connection->videoFrameAge();
		if(age>=0 && (snapshot.frameAgeMs<0 || age<snapshot.frameAgeMs)) snapshot.frameAgeMs=age;
		const auto local=connection->proxyClientSocket()->localAddress();
		if(!local.isNull() && !local.isLoopback() && !snapshot.localAddresses.contains(addressText(local))) snapshot.localAddresses.append(addressText(local));
	}
	for(const auto& interface:QNetworkInterface::allInterfaces()) {
		if(!interface.flags().testFlag(QNetworkInterface::IsUp) || !interface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
		for(const auto& entry:interface.addressEntries()) {
			const auto ip=entry.ip();
			if(ip.isNull() || (!m_vncProxyServer.listensLocallyOnly() && ip.isLoopback())) continue;
			if(m_vncProxyServer.listensLocallyOnly() && !ip.isLoopback()) continue;
			if(snapshot.localAddresses.size()<16 && !snapshot.localAddresses.contains(addressText(ip))) snapshot.localAddresses.append(addressText(ip));
		}
	}
	snapshot.backendChecked=m_backendChecked || backendConnected;snapshot.backendAvailable=m_backendAvailable || backendConnected;
	// Loopback-only, bounded and asynchronous: this checks TCP, not VNC auth.
	if(!backendConnecting && m_vncServer.isRunning() && (!m_backendProbeClock.isValid() || m_backendProbeClock.elapsed()>=15000)) {
		m_backendProbeClock.start();
		auto socket=new QTcpSocket(this);auto deadline=new QTimer(socket);deadline->setSingleShot(true);
		auto finish=[this,socket,deadline](bool available) {
			if(socket->property("netlinkProbeFinished").toBool()) return;
			socket->setProperty("netlinkProbeFinished",true);deadline->stop();socket->abort();socket->deleteLater();
			m_backendChecked=true;m_backendAvailable=available;updateTrayIconToolTip();
		};
		connect(socket,&QTcpSocket::connected,this,[finish]{finish(true);});
		connect(socket,&QTcpSocket::errorOccurred,this,[finish](QAbstractSocket::SocketError){finish(false);});
		connect(deadline,&QTimer::timeout,this,[finish]{finish(false);});deadline->start(1000);
		socket->connectToHost(QHostAddress::LocalHost,snapshot.backendPort);
	}
	auto toolTip=NetLinkUi::serviceSnapshot(snapshot);
	toolTip+=QLatin1Char('\n')+NetLinkUi::text("Authentication: %1").arg(
		VeyonCore::config().authenticationMethod()==VeyonCore::AuthenticationMethod::KeyFileAuthentication?NetLinkUi::text("Key"):QStringLiteral("Account"));
	QStringList peers;
	for(auto connection:m_vncProxyServer.clients()) {
		if(peers.size()>=8) {peers.append(NetLinkUi::text("Additional connections not shown"));break;}
		if(auto client=qobject_cast<ComputerControlClient*>(connection))
			peers.append(client->proxyClientSocket()->peerAddress().toString().left(64)+
				QStringLiteral(" — ")+(m_trayAuthorizedClients.contains(client->serverClient())?NetLinkUi::text("authorized"):NetLinkUi::text("other TCP"))+
				QStringLiteral(" — ")+(connection->backendReady()?NetLinkUi::text("VNC ready"):NetLinkUi::text("waiting for VNC"))+
				QStringLiteral(" — ")+NetLinkUi::text("Video: %1").arg(connection->videoFrameAge()>=0?QString::number(connection->videoFrameAge()/1000)+QStringLiteral(" s"):NetLinkUi::text("waiting for frame")));
	}
	if(!peers.isEmpty()) toolTip+=QStringLiteral("\n\n")+NetLinkUi::text("Observed TCP connections:")+QLatin1Char('\n')+peers.join(QLatin1Char('\n'));
	VeyonCore::builtinFeatures().systemTrayIcon().setToolTip(toolTip,m_featureWorkerManager);
}
