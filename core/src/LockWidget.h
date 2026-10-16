/*
 *  LockWidget.h - widget for locking a client
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

#pragma once

#include <QWidget>
#include <QPixmap>

#include "VeyonCore.h"
class KeyboardShortcutTrapper;


class VEYON_CORE_EXPORT LockWidget : public QWidget
{
	Q_OBJECT
public:
	enum Mode
	{
		DesktopVisible,
		BackgroundPixmap,
		NoBackground
	} ;

	explicit LockWidget( Mode mode, const QPixmap& background = QPixmap(), QWidget* parent = nullptr );
	~LockWidget() override;

Q_SIGNALS:
	void activityObserved(const QString& code);

private:
	friend class NetLinkUiTests;
	friend class LockNotificationTests;
	LockWidget(Mode mode, const QPixmap& background, QWidget* parent, bool initializeRuntime);
	void paintEvent( QPaintEvent * ) override;
	void closeEvent(QCloseEvent*) override;
	void changeEvent(QEvent*) override;
	void scheduleRestore();
	void restoreWindow();
	void inspectInputDesktop();
	void observeDesktopState(int state);
#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
	bool nativeEvent(const QByteArray& eventType,void* message,qintptr* result) override;
#else
	bool nativeEvent(const QByteArray& eventType,void* message,long* result) override;
#endif
#endif
	bool m_initializeRuntime=true;
	bool m_restoreQueued=false;
	bool m_closeWarningLogged=false;
	KeyboardShortcutTrapper* m_shortcutTrapper=nullptr;
	bool m_sessionNotifications=false;
	int m_inputDesktopState=-1;

	QPixmap m_background;
	Mode m_mode;

} ;
