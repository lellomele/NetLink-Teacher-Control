/*
 * VncConnection.h - declaration of VncConnection class
 *
 * Copyright (c) 2008-2022 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * code partly taken from KRDC / vncclientthread.h:
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

#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QReadWriteLock>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include "VeyonCore.h"
#include "SocketDevice.h"

using rfbClient = struct _rfbClient;

class VncEvent;
class VeyonConfiguration;

class VEYON_CORE_EXPORT VncConnection : public QThread
{
	Q_OBJECT
public:
	// intervals and timeouts
	static constexpr int DefaultThreadTerminationTimeout = 30000;
	static constexpr int DefaultConnectTimeout = 10000;
	static constexpr int DefaultReadTimeout = 30000;
	static constexpr int DefaultConnectionRetryInterval = 1000;
	static constexpr int DefaultMessageWaitTimeout = 500;
	static constexpr int DefaultFastFramebufferUpdateInterval = 100;
	static constexpr int DefaultFramebufferUpdateWatchdogTimeout = 10000;
	static constexpr int DefaultSocketKeepaliveIdleTime = 1000;
	static constexpr int DefaultSocketKeepaliveInterval = 500;
	static constexpr int DefaultSocketKeepaliveCount = 5;
	static constexpr int MaximumQueuedEvents = 1024;
	static constexpr quint64 MaximumFramebufferBytes = 128ULL * 1024 * 1024;

	enum class Quality
	{
		Thumbnail,
		Screenshot,
		RemoteControl,
		Default
	} ;

	enum class FramebufferState
	{
		Invalid,
		Initialized,
		Valid
	} ;

	enum class State
	{
		None,
		Disconnected,
		Connecting,
		HostOffline,
		ServerNotRunning,
		AuthenticationFailed,
		ConnectionFailed,
		Connected
	} ;
	Q_ENUM(State)

	explicit VncConnection( QObject *parent = nullptr );

	static void initLogging( bool debug );

	QImage image();

	void restart();
	void stop();
	void stopAndDeleteLater();

	void setHost( const QString& host );
	void setPort( int port );

	State state() const
	{
		return m_state;
	}

	bool isConnected() const
	{
		return state() == State::Connected && isRunning();
	}

	QString host() const;

	void setQuality( Quality quality )
	{
		m_quality.store(quality);
	}

	void setUseRemoteCursor( bool enabled );

	void setServerReachable();

	void enqueueEvent(VncEvent* event);
	bool isEventQueueEmpty();

	/** \brief Returns whether framebuffer data is valid, i.e. at least one full FB update received */
	bool hasValidFramebuffer() const
	{
		return m_framebufferState == FramebufferState::Valid;
	}

	void setScaledSize( QSize s );

	QImage scaledFramebuffer();

	void setFramebufferUpdateInterval( int interval );

	void setSkipHostPing( bool on )
	{
		setControlFlag( ControlFlag::SkipHostPing, on );
	}

	void setRequiresManualUpdateRateControl(bool on)
	{
		setControlFlag(ControlFlag::RequiresManualUpdateRateControl, on);
	}

	void rescaleFramebuffer();

	static constexpr int VncConnectionTag = 0x590123;

	static void* clientData( rfbClient* client, int tag );
	void setClientData( int tag, void* data );

	static qint64 libvncClientDispatcher( char * buffer, const qint64 bytes,
										  SocketDevice::SocketOperation operation, void * user );

	void mouseEvent( int x, int y, int buttonMask );
	void keyEvent( unsigned int key, bool pressed );
	void clientCut( const QString& text );

Q_SIGNALS:
	void connectionPrepared();
	void imageUpdated( int x, int y, int w, int h );
	void framebufferUpdateComplete();
	void framebufferSizeChanged( int w, int h );
	void cursorPosChanged( int x, int y );
	void cursorShapeUpdated( const QPixmap& cursorShape, int xh, int yh );
	void gotCut( const QString& text );
	void stateChanged();

protected:
	void run() override;

private:
	// Shared construction path; tests supply an isolated configuration store.
	explicit VncConnection(const VeyonConfiguration& configuration, QObject* parent = nullptr);
	// RFB parameters
	using RfbPixel = uint32_t;
	static constexpr int RfbBitsPerSample = 8;
	static constexpr int RfbSamplesPerPixel = 3;
	static constexpr int RfbBytesPerPixel = sizeof(RfbPixel);

	enum class ControlFlag {
		ScaledFramebufferNeedsUpdate = 0x01,
		ServerReachable = 0x02,
		TerminateThread = 0x04,
		RestartConnection = 0x08,
		DeleteAfterFinished = 0x10,
		SkipHostPing = 0x20,
		RequiresManualUpdateRateControl = 0x40,
		TriggerFramebufferUpdate = 0x80,
		UpdateFormatAndEncodings = 0x100
	};

	~VncConnection() override;

	void establishConnection();
	void handleConnection();
	void closeConnection();

	void setState( State state );

	void setControlFlag( ControlFlag flag, bool on );
	bool isControlFlagSet( ControlFlag flag );

	bool initFrameBuffer();
	void finishFrameBufferUpdate();

	void sendEvents();
	void discardEvents();

	void deleteLaterInMainThread();

	// hooks for LibVNCClient
	static int8_t hookInitFrameBuffer( rfbClient* client );
	static void hookUpdateFB( rfbClient* client, int x, int y, int w, int h );
	static void hookFinishFrameBufferUpdate( rfbClient* client );
	static int8_t hookHandleCursorPos( rfbClient* client, int x, int y );
	static void hookCursorShape( rfbClient* client, int xh, int yh, int w, int h, int bpp );
	static void hookCutText( rfbClient* client, const char *text, int textlen );
	static void rfbClientLogDebug( const char* format, ... );
	static void rfbClientLogNone( const char* format, ... );
	static void framebufferCleanup( void* framebuffer );

	// intervals and timeouts
	int m_threadTerminationTimeout{DefaultThreadTerminationTimeout};
	int m_connectTimeout{DefaultConnectTimeout};
	int m_readTimeout{DefaultReadTimeout};
	int m_connectionRetryInterval{DefaultConnectionRetryInterval};
	int m_messageWaitTimeout{DefaultMessageWaitTimeout};
	int m_fastFramebufferUpdateInterval{DefaultFastFramebufferUpdateInterval};
	int m_framebufferUpdateWatchdogTimeout{DefaultFramebufferUpdateWatchdogTimeout};
	int m_socketKeepaliveIdleTime{DefaultSocketKeepaliveIdleTime};
	int m_socketKeepaliveInterval{DefaultSocketKeepaliveInterval};
	int m_socketKeepaliveCount{DefaultSocketKeepaliveCount};

	// states and flags
	std::atomic<State> m_state;
	std::atomic<FramebufferState> m_framebufferState;
	std::atomic_bool m_deleteScheduled{false};
	QAtomicInt m_controlFlags;

	// connection parameters and data
	rfbClient* m_client;
	std::atomic<Quality> m_quality;
	QString m_host;
	int m_port;
	int m_defaultPort{-1};
	std::atomic_bool m_useRemoteCursor{false};

	// thread and timing control
	mutable QMutex m_globalMutex;
	QMutex m_eventQueueMutex;
	QMutex m_waitMutex;
	QWaitCondition m_updateIntervalSleeper;
	QAtomicInt m_framebufferUpdateInterval;
	QElapsedTimer m_framebufferUpdateWatchdog;

	// queue for RFB and custom events
	QQueue<VncEvent *> m_eventQueue;
	bool m_eventOverflowReported{false}; // guarded by m_eventQueueMutex
	bool m_slowMessageReported{false}; // VNC worker only, at most once per connection

	// framebuffer data and thread synchronization objects
	// Only the VNC worker accesses the decoder buffer. Published images never share
	// storage with it, so GUI readers can retain a snapshot without a data race.
	QImage m_receivingImage{};
	QRect m_pendingImageUpdate{}; // accumulated by the VNC worker until publication
	QImage m_image{};
	QImage m_scaledFramebuffer{};
	QSize m_scaledSize{};
	QReadWriteLock m_imgLock{};
	friend class ConnectionReliabilityTests;

} ;
