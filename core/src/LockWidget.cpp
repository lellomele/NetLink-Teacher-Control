/*
 *  LockWidget.cpp - widget for locking a client
 *
 *  Copyright (c) 2006-2022 Tobias Junghans <tobydox@veyon.io>
 *
 *  This file is part of Veyon - https://veyon.io
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include "LockWidget.h"
#include "PlatformCoreFunctions.h"
#include "PlatformInputDeviceFunctions.h"
#include "KeyboardShortcutTrapper.h"

#include <QApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wtsapi32.h>
#endif


LockWidget::LockWidget( Mode mode, const QPixmap& background, QWidget* parent ) :
    LockWidget(mode,background,parent,true)
{
}

LockWidget::LockWidget(Mode mode,const QPixmap& background,QWidget* parent,bool initializeRuntime) :
	QWidget( parent, Qt::X11BypassWindowManagerHint ),
	m_initializeRuntime(initializeRuntime),
	m_background( background ),
	m_mode( mode )
{
	// Only explicit teacher Unlock destroys this widget. Ordinary window close
	// must not quit its worker. Do not intercept Windows' secure attention keys.
	setAttribute(Qt::WA_QuitOnClose,false);
#ifdef Q_OS_WIN
	setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
#endif
	auto leftMostScreen = QGuiApplication::primaryScreen();
	int minimumX = 0;
	const auto screens = QGuiApplication::screens();
	for (auto* screen : screens)
	{
		if (screen->geometry().x() < minimumX)
		{
			minimumX = screen->geometry().x();
			leftMostScreen = screen;
		}
	}

	if (mode == DesktopVisible && initializeRuntime && leftMostScreen)
	{
		m_background = leftMostScreen->grabWindow(0);
	}

	if(initializeRuntime) {
		VeyonCore::platform().coreFunctions().setSystemUiState( false );
		VeyonCore::platform().inputDeviceFunctions().disableInputDevices();
#ifdef Q_OS_WIN
		m_shortcutTrapper=VeyonCore::platform().inputDeviceFunctions().createKeyboardShortcutTrapper(this);
		if(m_shortcutTrapper) {
			connect(m_shortcutTrapper,&KeyboardShortcutTrapper::shortcutTrapped,this,[this](KeyboardShortcutTrapper::Shortcut shortcut) {
				QString code;
				switch(shortcut) {
				case KeyboardShortcutTrapper::AltTab: code=QStringLiteral("alt-tab");break;
				case KeyboardShortcutTrapper::AltEsc: code=QStringLiteral("alt-esc");break;
				case KeyboardShortcutTrapper::AltSpace: code=QStringLiteral("alt-space");break;
				case KeyboardShortcutTrapper::AltF4: code=QStringLiteral("alt-f4");break;
				case KeyboardShortcutTrapper::CtrlEsc: code=QStringLiteral("ctrl-esc");break;
				case KeyboardShortcutTrapper::CtrlShiftEsc: code=QStringLiteral("ctrl-shift-esc");break;
				case KeyboardShortcutTrapper::SuperKeyDown: code=QStringLiteral("windows-key");break;
				default: break;
				}
				if(!code.isEmpty()) Q_EMIT activityObserved(code);
			});
			connect(m_shortcutTrapper,&KeyboardShortcutTrapper::shortcutMonitoringFailed,this,[this] {
				Q_EMIT activityObserved(QStringLiteral("hook-unavailable"));
			},Qt::QueuedConnection);
			m_shortcutTrapper->setEnabled(true);
		}
#endif
	}

	setWindowTitle( {} );

	if(!leftMostScreen) return;
	move(leftMostScreen->geometry().topLeft());
	showFullScreen();
	windowHandle()->setScreen(leftMostScreen);
	setFixedSize(leftMostScreen->virtualSize());

	if(initializeRuntime) VeyonCore::platform().coreFunctions().raiseWindow(this, true);

	setFocusPolicy( Qt::StrongFocus );
	setFocus();
	if(initializeRuntime) {grabMouse();grabKeyboard();}
	setCursor( Qt::BlankCursor );
	if(initializeRuntime) QGuiApplication::setOverrideCursor( Qt::BlankCursor );

	if(initializeRuntime) QCursor::setPos( mapToGlobal( QPoint( 0, 0 ) ) );
	connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state) {
		if(state==Qt::ApplicationActive) scheduleRestore();
	});
#ifdef Q_OS_WIN
	if(initializeRuntime) {
		m_sessionNotifications=WTSRegisterSessionNotification(reinterpret_cast<HWND>(winId()),NOTIFY_FOR_THIS_SESSION);
		if(!m_sessionNotifications) {
			qWarning("NetLink lock: Windows session notification registration failed: %lu",GetLastError());
			QTimer::singleShot(0,this,[this] {Q_EMIT activityObserved(QStringLiteral("monitor-unavailable"));});
		}
		QTimer::singleShot(0,this,&LockWidget::inspectInputDesktop);
		auto monitor=new QTimer(this);monitor->setInterval(500);
		connect(monitor,&QTimer::timeout,this,&LockWidget::inspectInputDesktop);monitor->start();
	}
#endif
}



LockWidget::~LockWidget()
{
	if(!m_initializeRuntime) return;
#ifdef Q_OS_WIN
	if(m_sessionNotifications) WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
#endif
	if(m_shortcutTrapper) m_shortcutTrapper->setEnabled(false);
	VeyonCore::platform().inputDeviceFunctions().enableInputDevices();
	VeyonCore::platform().coreFunctions().setSystemUiState( true );

	QGuiApplication::restoreOverrideCursor();
}

void LockWidget::closeEvent(QCloseEvent* event)
{
	event->ignore();
	Q_EMIT activityObserved(QStringLiteral("close-window"));
	if(m_initializeRuntime && !m_closeWarningLogged) {
		m_closeWarningLogged=true;
		qWarning("NetLink software lock: window close ignored; teacher Unlock is required. Windows security screens remain accessible.");
	}
}

void LockWidget::observeDesktopState(int state)
{
	if(state==m_inputDesktopState) return;
	const bool hadBaseline=m_inputDesktopState!=-1;
	m_inputDesktopState=state;
	if(!hadBaseline && state==0) return;
	if(state==0) {
		restoreWindow();
		Q_EMIT activityObserved(QStringLiteral("desktop-returned"));
	} else Q_EMIT activityObserved(state==1?QStringLiteral("secure-desktop"):
		state==2?QStringLiteral("other-desktop"):QStringLiteral("desktop-unavailable"));
}

void LockWidget::inspectInputDesktop()
{
#ifdef Q_OS_WIN
	// Read only the desktop identity. No screenshots, key polling, desktop switch
	// or privilege escalation. A failed query is NOT evidence of Ctrl+Alt+Del.
	wchar_t inputName[256]={},ownName[256]={};DWORD length=0;
	const auto input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
	const bool readInput=input && GetUserObjectInformationW(input,UOI_NAME,inputName,sizeof(inputName),&length);
	if(input) CloseDesktop(input);
	const bool readOwn=GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,ownName,sizeof(ownName),&length);
	if(!readInput || !readOwn) {observeDesktopState(3);return;}
	const auto current=QString::fromWCharArray(inputName);
	observeDesktopState(current.compare(QString::fromWCharArray(ownName),Qt::CaseInsensitive)==0?0:
		current.compare(QStringLiteral("Winlogon"),Qt::CaseInsensitive)==0?1:2);
#endif
}

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
bool LockWidget::nativeEvent(const QByteArray& eventType,void* message,qintptr* result)
#else
bool LockWidget::nativeEvent(const QByteArray& eventType,void* message,long* result)
#endif
{
	const auto native=static_cast<MSG*>(message);
	if(m_initializeRuntime && native && native->message==WM_WTSSESSION_CHANGE) {
		if(native->wParam==WTS_SESSION_LOCK) Q_EMIT activityObserved(QStringLiteral("session-locked"));
		else if(native->wParam==WTS_SESSION_UNLOCK) Q_EMIT activityObserved(QStringLiteral("session-unlocked"));
		QTimer::singleShot(0,this,&LockWidget::inspectInputDesktop);
	}
	return QWidget::nativeEvent(eventType,message,result);
}
#endif

void LockWidget::changeEvent(QEvent* event)
{
	QWidget::changeEvent(event);
	if(event->type()==QEvent::WindowStateChange && isMinimized()) scheduleRestore();
}

void LockWidget::scheduleRestore()
{
	if(m_restoreQueued) return;
	m_restoreQueued=true;
	QTimer::singleShot(0,this,[this] {
		m_restoreQueued=false;
		if(QGuiApplication::applicationState()==Qt::ApplicationActive) restoreWindow();
	});
}

void LockWidget::restoreWindow()
{
	// Re-cover the ordinary desktop after unlock/return. Never switch desktops,
	// synthesize SAS, modify policies or take over the Winlogon/UAC desktop.
	if(!isVisible() || isMinimized()) showFullScreen();
	if(m_initializeRuntime) {
		// A Windows desktop transition may have cancelled input grabs/hooks.
		// Re-arm only on return to this application's ordinary desktop.
		if(m_shortcutTrapper) {m_shortcutTrapper->setEnabled(false);m_shortcutTrapper->setEnabled(true);}
		VeyonCore::platform().coreFunctions().raiseWindow(this,true);
		grabMouse();grabKeyboard();
	}
}



void LockWidget::paintEvent( QPaintEvent* event )
{
	Q_UNUSED(event);

	QPainter p( this );
	switch( m_mode )
	{
	case DesktopVisible:
		p.drawPixmap( 0, 0, m_background );
		break;

	case BackgroundPixmap:
		p.fillRect( rect(), QColor( 64, 64, 64 ) );
		p.drawPixmap( ( width() - m_background.width() ) / 2,
					  ( height() - m_background.height() ) / 2,
					  m_background );
		break;

	default:
		break;
	}
}
