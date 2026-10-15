/*
 * FeatureWorkerManager.cpp - class for managing feature worker instances
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

#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QTimer>
#include <QtEndian>

#include "FeatureManager.h"
#include "FeatureWorkerManager.h"
#include "VariantArrayMessage.h"
#include "Filesystem.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "PlatformCoreFunctions.h"
#include "PlatformUserFunctions.h"

// clazy:excludeall=detaching-member

FeatureWorkerManager::FeatureWorkerManager( VeyonServerInterface& server, QObject* parent ) :
	FeatureWorkerManager(server,parent,true)
{
}

FeatureWorkerManager::FeatureWorkerManager(VeyonServerInterface& server, QObject* parent, bool initializeRuntime) :
	QObject( parent ),
	m_server( server ),
	m_tcpServer( this )
{
	if(!initializeRuntime) return; // isolated lifecycle fixtures, no live configuration/listener
	connect( &m_tcpServer, &QTcpServer::newConnection,
			 this, &FeatureWorkerManager::acceptConnection );

	if( !m_tcpServer.listen( QHostAddress::LocalHost,
							 static_cast<quint16>( VeyonCore::config().featureWorkerManagerPort() + VeyonCore::sessionId() ) ) )
	{
		vCritical() << "can't listen on localhost!";
	}

	auto pendingMessagesTimer = new QTimer( this );
	connect( pendingMessagesTimer, &QTimer::timeout, this, &FeatureWorkerManager::sendPendingMessages );

	pendingMessagesTimer->start( 100 );
}



FeatureWorkerManager::~FeatureWorkerManager()
{
	m_tcpServer.close();
	// Disconnect pending sockets before member destruction can trigger callbacks.
	for(auto socket:qAsConst(m_pendingHandshakes)) {
		socket->disconnect(this);
		socket->abort();
		socket->deleteLater();
	}
	m_pendingHandshakes.clear();

	// properly shutdown all worker processes
	while( m_workers.isEmpty() == false )
	{
		stopWorker( m_workers.firstKey() );
	}
}



bool FeatureWorkerManager::startManagedSystemWorker( Feature::Uid featureUid )
{
	if( thread() != QThread::currentThread() )
	{
		vCritical() << "thread mismatch for feature" << featureUid;
		return false;
	}

	stopWorker( featureUid );

	Worker worker;
	worker.token = QUuid::createUuid().toString() + QUuid::createUuid().toString();

	worker.process = new QProcess;
	worker.process->setProcessChannelMode( QProcess::ForwardedChannels );

	connect( worker.process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
			 worker.process, &QProcess::deleteLater );

	vDebug() << "Starting managed system worker for feature" << VeyonCore::featureManager().feature(featureUid).name();
	if( qEnvironmentVariableIsSet("VEYON_VALGRIND_WORKERS") )
	{
		worker.process->start( QStringLiteral("valgrind"),
							   { QStringLiteral("--error-limit=no"),
								 QStringLiteral("--leak-check=full"),
								 QStringLiteral("--show-leak-kinds=all"),
								 QStringLiteral("--log-file=valgrind-%1.log").arg(VeyonCore::formattedUuid(featureUid)),
								 VeyonCore::filesystem().workerFilePath(), featureUid.toString(), worker.token } );
	}
	else
	{
		worker.process->start( VeyonCore::filesystem().workerFilePath(), { featureUid.toString(), worker.token } );
	}

	m_workersMutex.lock();
	m_workers[featureUid] = worker;
	m_workersMutex.unlock();
	watchStartup(featureUid,worker.token);

	return true;
}



bool FeatureWorkerManager::startUnmanagedSessionWorker( Feature::Uid featureUid )
{
	if( thread() != QThread::currentThread() )
	{
		vCritical() << "thread mismatch for feature" << featureUid;
		return false;
	}

	stopWorker( featureUid );

	Worker worker;
	worker.token = QUuid::createUuid().toString() + QUuid::createUuid().toString();

	vDebug() << "Starting worker (unmanaged session process) for feature" << featureUid;

	const auto currentUser = VeyonCore::platform().userFunctions().currentUser();
	if( currentUser.isEmpty() )
	{
		vDebug() << "could not determine current user - probably a console session with logon screen";
		return false;
	}

	const auto ret = VeyonCore::platform().coreFunctions().
					 runProgramAsUser( VeyonCore::filesystem().workerFilePath(), { featureUid.toString(), worker.token },
									   currentUser,
									   VeyonCore::platform().coreFunctions().activeDesktopName() );
	if( ret == false )
	{
		vWarning() << "failed to start worker for feature" << featureUid;
		return false;
	}

	m_workersMutex.lock();
	m_workers[featureUid] = worker;
	m_workersMutex.unlock();
	watchStartup(featureUid,worker.token);

	return true;
}

void FeatureWorkerManager::watchStartup(Feature::Uid featureUid, const QString& token)
{
	QTimer::singleShot(10000,this,[this,featureUid,token] {expireStartup(featureUid,token);});
}

void FeatureWorkerManager::expireStartup(Feature::Uid featureUid, const QString& token)
{
	QMutexLocker locker(&m_workersMutex);
	const auto it=m_workers.constFind(featureUid);
	// A launch API succeeding does not mean the process loaded its DLLs or
	// authenticated its IPC connection. Never leave a phantom worker forever.
	if(it==m_workers.cend() || it->token!=token || it->socket) return;
	vWarning() << "Worker startup deadline expired before authenticated IPC; feature" << featureUid
		<< "queued messages" << it->pendingMessages.size();
	stopWorker(featureUid);
}



bool FeatureWorkerManager::stopWorker( Feature::Uid featureUid )
{
	if( thread() != QThread::currentThread() )
	{
		vCritical() << "thread mismatch for feature" << featureUid;
		return false;
	}
	m_retryGenerations.remove(featureUid);

	QMutexLocker locker( &m_workersMutex );

	if( m_workers.contains( featureUid ) )
	{
		vDebug() << "Stopping worker for feature" << featureUid;

		auto& worker = m_workers[featureUid];

		if( worker.socket )
		{
			worker.socket->disconnect( this );
			disconnect( worker.socket );

			worker.socket->close();
			worker.socket->deleteLater();
		}

		if( worker.process )
		{
			if(worker.process->state()==QProcess::NotRunning) {
				worker.process->deleteLater();
			} else {
			auto killTimer = new QTimer( worker.process );
			killTimer->setSingleShot( true );
			connect( killTimer, &QTimer::timeout, worker.process, &QProcess::terminate );
			connect( killTimer, &QTimer::timeout, worker.process, &QProcess::kill );
			connect( killTimer, &QTimer::timeout, killTimer, &QTimer::deleteLater );
			killTimer->start( 5000 );
			}
		}

		m_workers.remove( featureUid );
		return true;
	}

	return false;
}



void FeatureWorkerManager::sendMessageToManagedSystemWorker( const FeatureMessage& message )
{
	if( isWorkerRunning( message.featureUid() ) == false &&
		startManagedSystemWorker( message.featureUid() ) == false )
	{
		vCritical() << "could not start managed system worker";
		return;
	}

	sendMessage( message );

}



void FeatureWorkerManager::sendMessageToUnmanagedSessionWorker( const FeatureMessage& message )
{
	const auto generation=++m_retryGeneration;
	m_retryGenerations[message.featureUid()]=generation;
	retrySessionMessage(message, 6, generation);
}

void FeatureWorkerManager::retrySessionMessage(const FeatureMessage& message, int remaining, quint64 generation)
{
	if(m_retryGenerations.value(message.featureUid())!=generation) return;
	if( isWorkerRunning( message.featureUid() ) == false &&
		startUnmanagedSessionWorker( message.featureUid() ) == false )
	{
		// startWorker calls stopWorker; restore the retry generation only for this attempt.
		m_retryGenerations[message.featureUid()]=generation;
		if(remaining<=0) {m_retryGenerations.remove(message.featureUid());return;}
		QTimer::singleShot( UnmanagedSessionProcessRetryInterval, this,
							[=]() { retrySessionMessage(message, remaining-1, generation); } );
		return;
	}
	m_retryGenerations.remove(message.featureUid());
	sendMessage( message );
}



bool FeatureWorkerManager::isWorkerRunning( Feature::Uid featureUid )
{
	QMutexLocker locker( &m_workersMutex );
	return m_workers.contains( featureUid );
}



void FeatureWorkerManager::acceptConnection()
{
	vDebug() << "accepting connection";

	QTcpSocket* socket = m_tcpServer.nextPendingConnection();
	if( !socket ) return;
	if(m_pendingHandshakes.size() >= 16) {socket->abort();socket->deleteLater();return;}
	m_pendingHandshakes.insert(socket);
	// An unauthenticated local process must not allocate a full 32 MiB frame.
	socket->setReadBufferSize(16*1024 + sizeof(VariantArrayMessage::MessageSize));
	QTimer::singleShot( 5000, socket, [socket] {
		if( !socket->property("workerAuthenticated").toBool() ) socket->abort();
	} );

	// connect to readyRead() signal of new connection
	connect( socket, &QTcpSocket::readyRead,
			 this, [=] () { processConnection( socket ); } );

	connect( socket, &QTcpSocket::disconnected,
			 this, [=] () { closeConnection( socket ); } );
}



void FeatureWorkerManager::processConnection( QTcpSocket* socket )
{
	if(!socket->property("workerAuthenticated").toBool()) {
		quint32 size=0;
		if(socket->peek(reinterpret_cast<char*>(&size),sizeof(size))==sizeof(size) &&
			qFromBigEndian(size)>16*1024) {socket->abort();return;}
	}
	FeatureMessage message;
	// Drain complete frames only: readyRead may contain a fragment or several frames.
	int budget = 32;
	while( budget-- && message.isReadyForReceive( socket ) )
	{
		if( !message.receive( socket ) ) { socket->abort(); return; }
		bool dispatch = false;
		bool reject = false;
		{
			QMutexLocker locker( &m_workersMutex );
			auto it = m_workers.find( message.featureUid() );
			if( it == m_workers.end() ) reject = true;
			else if( it.value().socket.isNull() )
			{
				auto& worker = it.value();
				if( message.command() != FeatureMessage::InitCommand ||
					message.argument(0).toString() != worker.token ) reject = true;
				else {
					worker.socket = socket;
					worker.token.clear(); // nonce is single-use
					socket->setProperty("workerAuthenticated", true);
					m_pendingHandshakes.remove(socket);
					// An authenticated worker can carry a complete permitted frame.
					socket->setReadBufferSize(VariantArrayMessage::MaxMessageSize + sizeof(VariantArrayMessage::MessageSize));
				}
			}
			else if( it.value().socket != socket ) reject = true;
			else dispatch = message.command() >= 0;
		}
		// abort can synchronously emit disconnected and erase the worker-map entry.
		if(reject) { socket->abort(); return; }
		if( dispatch ) VeyonCore::featureManager().handleFeatureMessage( m_server, MessageContext( socket ), message );
		if( !socket->isOpen() ) return;
	}
	sendPendingMessages();
	if( budget < 0 && socket->bytesAvailable() )
		QTimer::singleShot( 0, socket, [this, socket] { processConnection(socket); } );
}



void FeatureWorkerManager::closeConnection( QTcpSocket* socket )
{
	m_pendingHandshakes.remove(socket);
	m_workersMutex.lock();

	for( auto it = m_workers.begin(); it != m_workers.end(); )
	{
		if( it.value().socket == socket )
		{
			vDebug() << "removing worker after socket has been closed";
			it = m_workers.erase( it );
		}
		else
		{
			++it;
		}
	}

	m_workersMutex.unlock();

	socket->deleteLater();
}



void FeatureWorkerManager::sendMessage( const FeatureMessage& message )
{
	m_workersMutex.lock();

	if( m_workers.contains( message.featureUid() ) )
	{
		auto& messages = m_workers[message.featureUid()].pendingMessages;
		if( messages.size() < 128 ) messages.append( message );
		else vWarning() << "worker message queue limit exceeded";
	}
	else
	{
		vWarning() << "worker does not exist for feature" << message.featureUid();
	}

	m_workersMutex.unlock();
}



void FeatureWorkerManager::sendPendingMessages()
{
	QList<QPair<QPointer<QTcpSocket>, QList<FeatureMessage>>> batches;
	{
		QMutexLocker locker( &m_workersMutex );
		for( auto& worker : m_workers )
		{
			if( worker.socket && worker.socket->bytesToWrite() < 1024 * 1024 && !worker.pendingMessages.isEmpty() )
			{
				batches.append( qMakePair(worker.socket, worker.pendingMessages) );
				worker.pendingMessages.clear();
			}
		}
	}
	// Socket callbacks must never invalidate a worker-map iterator under this loop.
	for( const auto& batch : batches ) for( const auto& message : batch.second )
	{
		if( !batch.first ) break;
		if( !message.send(batch.first) ) { batch.first->abort(); break; }
	}
}
