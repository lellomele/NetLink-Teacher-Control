/*
 * VncConnection.cpp - implementation of VncConnection class
 *
 * Copyright (c) 2008-2022 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * code partly taken from KRDC / vncclientthread.cpp:
 * Copyright (C) 2007-2008 Urs Wolfer <uwolfer @ kde.org>
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

#include <rfb/rfbclient.h>

#include <QBitmap>
#include <QHostAddress>
#include <QMutexLocker>
#include <QPixmap>
#include <QRegularExpression>
#include <QTime>
#include <memory>
#include <new>
#include <limits>

#include "PlatformNetworkFunctions.h"
#include "VeyonConfiguration.h"
#include "VncConnection.h"
#include "SocketDevice.h"
#include "VncEvents.h"


rfbBool VncConnection::hookInitFrameBuffer( rfbClient* client )
{
	auto connection = static_cast<VncConnection *>( clientData( client, VncConnectionTag ) );
	if (connection && connection->m_client == client)
	{
		return connection->initFrameBuffer();
	}

	return false;
}




void VncConnection::hookUpdateFB( rfbClient* client, int x, int y, int w, int h )
{
	auto connection = static_cast<VncConnection *>( clientData( client, VncConnectionTag ) );
	if( connection )
	{
		if (x >= 0 && y >= 0 && x < client->width && y < client->height &&
			w > 0 && h > 0 && w <= client->width-x && h <= client->height-y)
		{
			connection->m_pendingImageUpdate |= QRect(x, y, w, h);
		}
	}
}




void VncConnection::hookFinishFrameBufferUpdate( rfbClient* client )
{
	auto connection = static_cast<VncConnection *>( clientData( client, VncConnectionTag ) );
	if( connection )
	{
		connection->finishFrameBufferUpdate();
	}
}




rfbBool VncConnection::hookHandleCursorPos( rfbClient* client, int x, int y )
{
	auto connection = static_cast<VncConnection *>( clientData( client, VncConnectionTag ) );
	if( connection )
	{
		Q_EMIT connection->cursorPosChanged( x, y );
	}

	return true;
}




void VncConnection::hookCursorShape( rfbClient* client, int xh, int yh, int w, int h, int bpp )
{
	if( bpp != 4 || w <= 0 || h <= 0 || w > 2048 || h > 2048 ||
		client->rcSource == nullptr || client->rcMask == nullptr )
	{
		vWarning() << "Invalid remote cursor dimensions or storage" << w << h << bpp;
		return;
	}

	// QPixmap/QBitmap are GUI resources. Decode to an owning QImage in the worker
	// and create the pixmap only in the connection object's GUI thread.
	auto image = QImage(client->rcSource, w, h, QImage::Format_RGB32).convertToFormat(QImage::Format_ARGB32);
	if (image.isNull()) return;
	for (int y = 0; y < h; ++y)
	{
		auto pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
		for (int x = 0; x < w; ++x)
		{
			pixels[x] = (pixels[x] & 0x00ffffffU) | (client->rcMask[y*w+x] ? 0xff000000U : 0U);
		}
	}

	auto connection = static_cast<VncConnection *>( clientData( client, VncConnectionTag ) );
	if( connection )
	{
		QTimer::singleShot(0, connection, [connection, image, xh, yh]() {
			if (!connection->isControlFlagSet(ControlFlag::TerminateThread))
			{
				Q_EMIT connection->cursorShapeUpdated(QPixmap::fromImage(image), xh, yh);
			}
		});
	}
}



void VncConnection::hookCutText( rfbClient* client, const char* text, int textlen )
{
	auto connection = static_cast<VncConnection *>( clientData( client, VncConnectionTag ) );
	const auto cutText = QString::fromUtf8( text, textlen );

	if( connection && cutText.isEmpty() == false  )
	{
		Q_EMIT connection->gotCut( cutText );
	}
}



void VncConnection::rfbClientLogDebug( const char* format, ... )
{
	va_list args;
	va_start( args, format );

	static constexpr int MaxMessageLength = 256;
	char message[MaxMessageLength];

	vsnprintf( message, sizeof(message), format, args );
	message[MaxMessageLength-1] = 0;

	va_end(args);

	vDebug() << QThread::currentThreadId() << message;
}




void VncConnection::rfbClientLogNone( const char* format, ... )
{
	Q_UNUSED(format);
}



void VncConnection::framebufferCleanup( void* framebuffer )
{
	delete[] static_cast<RfbPixel *>( framebuffer );
}




VncConnection::VncConnection(QObject* parent) : VncConnection(VeyonCore::config(), parent)
{
}


VncConnection::VncConnection(const VeyonConfiguration& configuration, QObject* parent) :
	QThread( parent ),
	m_state( State::Disconnected ),
	m_framebufferState( FramebufferState::Invalid ),
	m_controlFlags(),
	m_client( nullptr ),
	m_quality( Quality::Default ),
	m_host(),
	m_port( -1 ),
	m_defaultPort( configuration.veyonServerPort() ),
	m_globalMutex(),
	m_eventQueueMutex(),
	m_updateIntervalSleeper(),
	m_framebufferUpdateInterval( 0 )
{
	if( configuration.useCustomVncConnectionSettings() )
	{
		m_threadTerminationTimeout = configuration.vncConnectionThreadTerminationTimeout();
		m_connectTimeout = configuration.vncConnectionConnectTimeout();
		m_readTimeout = configuration.vncConnectionReadTimeout();
		m_connectionRetryInterval = configuration.vncConnectionRetryInterval();
		m_messageWaitTimeout = configuration.vncConnectionMessageWaitTimeout();
		m_fastFramebufferUpdateInterval = configuration.vncConnectionFastFramebufferUpdateInterval();
		m_framebufferUpdateWatchdogTimeout = configuration.vncConnectionFramebufferUpdateWatchdogTimeout();
		m_socketKeepaliveIdleTime = configuration.vncConnectionSocketKeepaliveIdleTime();
		m_socketKeepaliveInterval = configuration.vncConnectionSocketKeepaliveInterval();
		m_socketKeepaliveCount = configuration.vncConnectionSocketKeepaliveCount();
	}
	// Invalid configured values must not become infinite reads or unsigned waits.
	m_connectTimeout = qBound(1000, m_connectTimeout, DefaultConnectTimeout);
	m_readTimeout = qBound(1000, m_readTimeout, DefaultReadTimeout);
	m_threadTerminationTimeout = qBound(qMax(m_connectTimeout, m_readTimeout) + 1000,
										m_threadTerminationTimeout, 60000);
	m_connectionRetryInterval = qBound(100, m_connectionRetryInterval, 60000);
	m_messageWaitTimeout = qBound(100, m_messageWaitTimeout, 10000);
	m_framebufferUpdateWatchdogTimeout = qBound(1000, m_framebufferUpdateWatchdogTimeout, 60000);
	connect(this, &QThread::finished, this, [this]() {
		if (isControlFlagSet(ControlFlag::DeleteAfterFinished)) deleteLaterInMainThread();
	});
}



VncConnection::~VncConnection()
{
	stop();
	if( isRunning() )
	{
		vWarning() << "Waiting for VNC connection thread to finish.";
		wait( m_threadTerminationTimeout );
	}

	if( isRunning() )
	{
		vWarning() << "Terminating hanging VNC connection thread!";

		terminate();
		wait();
	}
	discardEvents();
}



void VncConnection::initLogging( bool debug )
{
	if( debug )
	{
		rfbClientLog = rfbClientLogDebug;
		rfbClientErr = rfbClientLogDebug;
	}
	else
	{
		rfbClientLog = rfbClientLogNone;
		rfbClientErr = rfbClientLogNone;
	}
}



QImage VncConnection::image()
{
	QReadLocker locker( &m_imgLock );
	return m_image;
}



void VncConnection::restart()
{
	QMutexLocker lock(&m_waitMutex);
	setControlFlag( ControlFlag::RestartConnection, true );
	m_updateIntervalSleeper.wakeAll();
}



void VncConnection::stop()
{
	{
		QMutexLocker lock(&m_waitMutex);
		setControlFlag(ControlFlag::TerminateThread, true);
		m_updateIntervalSleeper.wakeAll();
	}
	discardEvents();
}



void VncConnection::stopAndDeleteLater()
{
	setControlFlag(ControlFlag::DeleteAfterFinished, true);
	stop();
	if (!isRunning())
	{
		deleteLaterInMainThread();
	}
}



void VncConnection::setHost( const QString& host )
{
	QMutexLocker locker( &m_globalMutex );
	m_host = host;

	QRegularExpressionMatch match;
	if(
		// if IPv6-mapped IPv4 address use plain IPv4 address as libvncclient cannot handle IPv6-mapped IPv4 addresses on Windows properly
		( match = QRegularExpression( QStringLiteral("^::[fF]{4}:(\\d+.\\d+.\\d+.\\d+)$") ).match( m_host ) ).hasMatch() ||
		( match = QRegularExpression( QStringLiteral("^::[fF]{4}:(\\d+.\\d+.\\d+.\\d+):(\\d+)$") ).match( m_host ) ).hasMatch() ||
		( match = QRegularExpression( QStringLiteral("^\\[::[fF]{4}:(\\d+.\\d+.\\d+.\\d+)\\]:(\\d+)$") ).match( m_host ) ).hasMatch() ||
		// any other IPv6 address with port number
		( match = QRegularExpression( QStringLiteral("^\\[([0-9a-fA-F:]+)\\]:(\\d+)$") ).match( m_host ) ).hasMatch() ||
		// irregular IPv6 address + port number specification where port number can be identified if > 9999
		( match = QRegularExpression( QStringLiteral("^([0-9a-fA-F:]+):(\\d{5})$"), QRegularExpression::InvertedGreedinessOption ).match( m_host ) ).hasMatch() ||
		// any other notation with trailing port number
		( match = QRegularExpression( QStringLiteral("^([^:]+):(\\d+)$") ).match( m_host ) ).hasMatch()
		)
	{
		const auto matchedHost = match.captured( 1 );
		if( matchedHost.isEmpty() == false )
		{
			m_host = matchedHost;
		}

		const auto port = match.captured( 2 ).toInt();
		if( port > 0 )
		{
			m_port = port;
		}
	}
}



void VncConnection::setPort( int port )
{
	if( port >= 0 && port <= 65535 )
	{
		QMutexLocker locker( &m_globalMutex );
		m_port = port;
	}
}



void VncConnection::setUseRemoteCursor( bool enabled )
{
	QMutexLocker lock(&m_waitMutex);
	m_useRemoteCursor.store(enabled);
	setControlFlag(ControlFlag::UpdateFormatAndEncodings, true);
	m_updateIntervalSleeper.wakeAll();
}


QString VncConnection::host() const
{
	QMutexLocker locker(&m_globalMutex);
	return m_host;
}



void VncConnection::setServerReachable()
{
	setControlFlag( ControlFlag::ServerReachable, true );
}



void VncConnection::setScaledSize( QSize s )
{
	QMutexLocker globalLock( &m_globalMutex );

	if( m_scaledSize != s )
	{
		m_scaledSize = s;
		setControlFlag( ControlFlag::ScaledFramebufferNeedsUpdate, true );
	}
}



QImage VncConnection::scaledFramebuffer()
{
	rescaleFramebuffer();
	return m_scaledFramebuffer;
}



void VncConnection::setFramebufferUpdateInterval( int interval )
{
	QMutexLocker lock(&m_waitMutex);
	m_framebufferUpdateInterval = interval;

	if (m_framebufferUpdateInterval <= 0)
	{
		setControlFlag(ControlFlag::TriggerFramebufferUpdate, true);
	}

	m_updateIntervalSleeper.wakeAll();
}



void VncConnection::rescaleFramebuffer()
{
	if( hasValidFramebuffer() == false || m_scaledSize.isNull() )
	{
		m_scaledFramebuffer = {};
		return;
	}

	if( isControlFlagSet( ControlFlag::ScaledFramebufferNeedsUpdate ) == false )
	{
		return;
	}

	QReadLocker locker( &m_imgLock );

	if( m_image.size().isValid() == false )
	{
		return;
	}

	m_scaledFramebuffer = m_image.scaled( m_scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

	setControlFlag( ControlFlag::ScaledFramebufferNeedsUpdate, false );
}



void* VncConnection::clientData( rfbClient* client, int tag )
{
	if( client )
	{
		return rfbClientGetClientData( client, reinterpret_cast<void *>( tag ) );
	}

	return nullptr;
}



void VncConnection::setClientData( int tag, void* data )
{
	// The client and its metadata belong exclusively to the VNC worker.
	// In particular, stop() must not race rfbInitClient()/rfbClientCleanup().
	if (QThread::currentThread() != this)
	{
		vWarning() << "Rejected VNC client metadata mutation outside its worker";
		return;
	}
	if( m_client )
	{
		rfbClientSetClientData( m_client, reinterpret_cast<void *>( tag ), data );
	}
}



void VncConnection::run()
{
	while( isControlFlagSet( ControlFlag::TerminateThread ) == false )
	{
		establishConnection();
		handleConnection();
		closeConnection();
	}

}



void VncConnection::establishConnection()
{
	setState( State::Connecting );
	setControlFlag( ControlFlag::RestartConnection, false );

	m_framebufferState = FramebufferState::Invalid;

	while( isControlFlagSet( ControlFlag::TerminateThread ) == false &&
		   state() != State::Connected ) // try to connect as long as the server allows
	{
		m_client = rfbGetClient( RfbBitsPerSample, RfbSamplesPerPixel, RfbBytesPerPixel );
		if (m_client == nullptr)
		{
			vCritical() << "Could not allocate VNC client";
			setState(State::ConnectionFailed);
			setControlFlag(ControlFlag::TerminateThread, true);
			return;
		}
		m_client->MallocFrameBuffer = hookInitFrameBuffer;
		m_client->canHandleNewFBSize = true;
		m_client->GotFrameBufferUpdate = hookUpdateFB;
		m_client->FinishedFrameBufferUpdate = hookFinishFrameBufferUpdate;
		m_client->HandleCursorPos = hookHandleCursorPos;
		m_client->GotCursorShape = hookCursorShape;
		m_client->GotXCutText = hookCutText;
		m_client->connectTimeout = m_connectTimeout / 1000;
		m_client->readTimeout = m_readTimeout / 1000;
		setClientData( VncConnectionTag, this );

		Q_EMIT connectionPrepared();

		m_globalMutex.lock();

		if( m_port < 0 ) // use default port?
		{
			m_client->serverPort = m_defaultPort;
		}
		else
		{
			m_client->serverPort = m_port;
		}

		free( m_client->serverHost );
		m_client->serverHost = strdup( m_host.toUtf8().constData() );

		m_globalMutex.unlock();

		setControlFlag( ControlFlag::ServerReachable, false );

		const auto clientInitialized = rfbInitClient( m_client, nullptr, nullptr );
		if( clientInitialized == FALSE )
		{
			// rfbInitClient() calls rfbClientCleanup() when failed
			m_client = nullptr;
		}

		// do not continue/sleep when already requested to stop
		if( isControlFlagSet( ControlFlag::TerminateThread ) )
		{
			return;
		}

		if( clientInitialized )
		{
			m_framebufferUpdateWatchdog.restart();

			VeyonCore::platform().networkFunctions().
					configureSocketKeepalive( static_cast<PlatformNetworkFunctions::Socket>( m_client->sock ), true,
											  m_socketKeepaliveIdleTime, m_socketKeepaliveInterval, m_socketKeepaliveCount );

			setState( State::Connected );
		}
		else
		{
			// guess reason why connection failed
			if( isControlFlagSet( ControlFlag::ServerReachable ) == false )
			{
				if( isControlFlagSet( ControlFlag::SkipHostPing ) ||
					VeyonCore::platform().networkFunctions().ping( host() ) == false )
				{
					setState( State::HostOffline );
				}
				else
				{
					setState( State::ServerNotRunning );
				}
			}
			else if( m_framebufferState == FramebufferState::Invalid )
			{
				setState( State::AuthenticationFailed );
			}
			else
			{
				// failed for an unknown reason
				setState( State::ConnectionFailed );
			}

			// wait a bit until next connect
			QMutexLocker sleepLock(&m_waitMutex);
			if (isControlFlagSet(ControlFlag::TerminateThread)) return;
			if( m_framebufferUpdateInterval > 0 )
			{
				m_updateIntervalSleeper.wait( &m_waitMutex, m_framebufferUpdateInterval );
			}
			else
			{
				// default: retry every second
				m_updateIntervalSleeper.wait( &m_waitMutex, m_connectionRetryInterval );
			}
		}
	}
}



void VncConnection::handleConnection()
{
	QElapsedTimer loopTimer;

	while( state() == State::Connected &&
		   isControlFlagSet( ControlFlag::TerminateThread ) == false &&
		   isControlFlagSet( ControlFlag::RestartConnection ) == false )
	{
		loopTimer.start();

		const int i = WaitForMessage(m_client, m_framebufferUpdateInterval > 0 ?
													m_messageWaitTimeout * 100 : m_messageWaitTimeout);
		if( isControlFlagSet( ControlFlag::TerminateThread ) || i < 0 )
		{
			break;
		}
		else if( i )
		{
			// A busy/malicious peer must not monopolize the worker and starve
			// outgoing feature commands, reconnection or cancellation.
			bool handledOkay = true;
			int handledMessages = 0;
			QElapsedTimer batchTimer;
			batchTimer.start();
			do {
				handledOkay &= HandleRFBServerMessage( m_client );
				++handledMessages;
			} while( handledOkay && handledMessages < 64 && batchTimer.elapsed() < 10 &&
					 !isControlFlagSet(ControlFlag::TerminateThread) &&
					 !isControlFlagSet(ControlFlag::RestartConnection) && WaitForMessage( m_client, 0 ) > 0 );
			if (batchTimer.elapsed() > 1000 && !m_slowMessageReported)
			{
				m_slowMessageReported = true;
				vWarning() << "Slow remote message decoding/read" << batchTimer.elapsed()
						   << "ms; further warnings suppressed until reconnect";
			}

			if( handledOkay == false )
			{
				break;
			}
		}
		else if (m_framebufferUpdateWatchdog.elapsed() >=
				 qMax<qint64>(2*m_framebufferUpdateInterval, m_framebufferUpdateWatchdogTimeout))
		{
			SendFramebufferUpdateRequest(m_client, 0, 0, m_client->width, m_client->height, false);
			m_framebufferUpdateWatchdog.restart();
		}
		else if (m_framebufferUpdateInterval > 0 && m_framebufferUpdateWatchdog.elapsed() > m_framebufferUpdateInterval)
		{
			SendIncrementalFramebufferUpdateRequest(m_client);
			m_framebufferUpdateWatchdog.restart();
		}
		else if (isControlFlagSet(ControlFlag::TriggerFramebufferUpdate))
		{
			setControlFlag(ControlFlag::TriggerFramebufferUpdate, false);
			SendIncrementalFramebufferUpdateRequest(m_client);
		}

		const auto remainingUpdateInterval = m_framebufferUpdateInterval - loopTimer.elapsed();

		// compat with Veyon Server < 4.7
		if (remainingUpdateInterval > 0 &&
			isControlFlagSet(ControlFlag::RequiresManualUpdateRateControl) &&
			isControlFlagSet(ControlFlag::TerminateThread) == false)
		{
			QMutexLocker sleepLock(&m_waitMutex);
			if (!isControlFlagSet(ControlFlag::TerminateThread) &&
				!isControlFlagSet(ControlFlag::RestartConnection) && isEventQueueEmpty())
			{
				m_updateIntervalSleeper.wait(&m_waitMutex, remainingUpdateInterval);
			}
		}

		sendEvents();
	}
}



void VncConnection::closeConnection()
{
	setState( State::Disconnected );
	discardEvents();
	if( m_client )
	{
		rfbClientCleanup( m_client );
		m_client = nullptr;
	}

	m_receivingImage = {};
	m_pendingImageUpdate = {};
	m_slowMessageReported = false;
}



void VncConnection::setState( State state )
{
	if( m_state.exchange( state ) != state )
	{
		Q_EMIT stateChanged();
	}
}



void VncConnection::setControlFlag( VncConnection::ControlFlag flag, bool on )
{
	if( on )
	{
		m_controlFlags |= static_cast<int>( flag );
	}
	else
	{
		m_controlFlags &= ~static_cast<int>( flag );
	}
}



bool VncConnection::isControlFlagSet( VncConnection::ControlFlag flag )
{
	return m_controlFlags & static_cast<int>( flag );
}




bool VncConnection::initFrameBuffer()
{
	if (m_client->format.bitsPerPixel != RfbBitsPerSample * RfbBytesPerPixel)
	{
		vCritical() << "Bits per pixel does not match" << m_client->format.bitsPerPixel;
		return false;
	}

	if (m_client->width <= 0 || m_client->height <= 0 ||
		m_client->width > 16384 || m_client->height > 16384 ||
		quint64(m_client->width) * quint64(m_client->height) > MaximumFramebufferBytes / RfbBytesPerPixel)
	{
		vWarning() << "Rejected oversized or invalid remote framebuffer" << m_client->width << m_client->height;
		return false;
	}
	const auto pixelCount = quint64(m_client->width) * quint64(m_client->height);
	auto framebuffer = new (std::nothrow) RfbPixel[pixelCount]{};
	if (framebuffer == nullptr)
	{
		vWarning() << "Remote framebuffer allocation failed" << pixelCount * RfbBytesPerPixel;
		return false;
	}
	m_client->frameBuffer = reinterpret_cast<uint8_t *>(framebuffer);

	// initialize framebuffer image which just wraps the allocated memory and ensures cleanup after last
	// image copy using the framebuffer gets destroyed
	m_imgLock.lockForWrite();
	m_receivingImage = QImage(m_client->frameBuffer, m_client->width, m_client->height, QImage::Format_RGB32,
					 framebufferCleanup, m_client->frameBuffer);
	m_image = m_receivingImage.copy();
	m_pendingImageUpdate = QRect(0, 0, m_client->width, m_client->height);
	m_imgLock.unlock();
	if (m_image.isNull()) return false;

	// set up pixel format according to QImage
	m_client->format.redShift = 16;
	m_client->format.greenShift = 8;
	m_client->format.blueShift = 0;
	m_client->format.redMax = 0xff;
	m_client->format.greenMax = 0xff;
	m_client->format.blueMax = 0xff;

	m_client->appData.encodingsString = "zrle ultra copyrect hextile zlib corre rre raw";
	m_client->appData.useRemoteCursor = m_useRemoteCursor.load() ? TRUE : FALSE;
	m_client->appData.compressLevel = 0;
	m_client->appData.useBGR233 = false;
	m_client->appData.qualityLevel = 9;
	m_client->appData.enableJPEG = false;

	switch( m_quality.load() )
	{
	case Quality::Screenshot:
		// make sure to use lossless raw encoding
		m_client->appData.encodingsString = "raw";
		break;
	case Quality::Thumbnail:
		m_client->appData.compressLevel = 9;
		m_client->appData.qualityLevel = 5;
		m_client->appData.enableJPEG = true;
		break;
	default:
		break;
	}

	m_framebufferState = FramebufferState::Initialized;

	Q_EMIT framebufferSizeChanged(m_client->width, m_client->height);

	return true;
}



void VncConnection::finishFrameBufferUpdate()
{
	m_framebufferUpdateWatchdog.restart();
	if (m_pendingImageUpdate.isEmpty())
	{
		// An empty RFB heartbeat still resets monitoring/watchdogs, but must not
		// copy an entire desktop or enqueue another repaint for every client.
		Q_EMIT framebufferUpdateComplete();
		return;
	}
	// Publish a complete immutable snapshot. A shallow wrapper would let the next
	// decoder update overwrite pixels while the GUI is painting/scaling them.
	const auto snapshot = m_receivingImage.copy();
	if (snapshot.isNull())
	{
		vWarning() << "Could not publish remote framebuffer snapshot; reconnecting";
		setControlFlag(ControlFlag::RestartConnection, true);
		return;
	}
	{
		QWriteLocker locker(&m_imgLock);
		m_image = snapshot;
		setControlFlag(ControlFlag::ScaledFramebufferNeedsUpdate, true);
	}

	m_framebufferState = FramebufferState::Valid;
	const auto update = m_pendingImageUpdate;
	m_pendingImageUpdate = {};
	if (!update.isEmpty()) Q_EMIT imageUpdated(update.x(), update.y(), update.width(), update.height());

	Q_EMIT framebufferUpdateComplete();
}



void VncConnection::sendEvents()
{
	if (isControlFlagSet(ControlFlag::UpdateFormatAndEncodings))
	{
		setControlFlag(ControlFlag::UpdateFormatAndEncodings, false);
		m_client->appData.useRemoteCursor = m_useRemoteCursor.load() ? TRUE : FALSE;
		SetFormatAndEncodings(m_client);
	}
	m_eventQueueMutex.lock();

	int sent = 0;
	while( m_eventQueue.isEmpty() == false && sent++ < 64 )
	{
		auto event = m_eventQueue.dequeue();

		// unlock the queue mutex during the runtime of ClientEvent::fire()
		m_eventQueueMutex.unlock();

		if( isControlFlagSet( ControlFlag::TerminateThread ) == false &&
			isControlFlagSet( ControlFlag::RestartConnection ) == false )
		{
			event->fire( m_client );
		}

		delete event;

		// and lock it again
		m_eventQueueMutex.lock();
	}

	m_eventQueueMutex.unlock();
}



void VncConnection::deleteLaterInMainThread()
{
	if (!m_deleteScheduled.exchange(true)) deleteLater();
}



void VncConnection::enqueueEvent(VncEvent* event)
{
	std::unique_ptr<VncEvent> ownedEvent(event);
	QMutexLocker lock(&m_eventQueueMutex);
	if( event == nullptr || state() != State::Connected ||
		isControlFlagSet(ControlFlag::TerminateThread) || isControlFlagSet(ControlFlag::RestartConnection) )
	{
		return;
	}

	if (m_eventQueue.size() >= MaximumQueuedEvents)
	{
		if (!m_eventOverflowReported)
		{
			vWarning() << "VNC outgoing queue limit reached; reconnecting instead of replaying stale commands" << MaximumQueuedEvents;
			m_eventOverflowReported = true;
		}
		setControlFlag(ControlFlag::RestartConnection, true);
		return;
	}
	m_eventQueue.enqueue(ownedEvent.release());
	lock.unlock();

	QMutexLocker waitLock(&m_waitMutex);
	m_updateIntervalSleeper.wakeAll();
}


void VncConnection::discardEvents()
{
	QMutexLocker lock(&m_eventQueueMutex);
	qDeleteAll(m_eventQueue);
	m_eventQueue.clear();
	m_eventOverflowReported = false;
}



bool VncConnection::isEventQueueEmpty()
{
	QMutexLocker lock( &m_eventQueueMutex );
	return m_eventQueue.isEmpty();
}



void VncConnection::mouseEvent( int x, int y, int buttonMask )
{
	enqueueEvent(new VncPointerEvent(x, y, buttonMask));
}



void VncConnection::keyEvent( unsigned int key, bool pressed )
{
	enqueueEvent(new VncKeyEvent(key, pressed));
}



void VncConnection::clientCut( const QString& text )
{
	enqueueEvent(new VncClientCutEvent(text));
}



qint64 VncConnection::libvncClientDispatcher( char* buffer, const qint64 bytes,
											  SocketDevice::SocketOperation operation, void* user )
{
	auto client = static_cast<rfbClient *>( user );
	if (client == nullptr || bytes < 0 || quint64(bytes) > std::numeric_limits<unsigned int>::max()) return 0;
	switch( operation )
	{
	case SocketDevice::SocketOpRead:
		return ReadFromRFBServer( client, buffer, static_cast<unsigned int>( bytes ) ) ? bytes : 0;

	case SocketDevice::SocketOpWrite:
		return WriteToRFBServer( client, buffer, static_cast<unsigned int>( bytes ) ) ? bytes : 0;
	}

	return 0;
}
