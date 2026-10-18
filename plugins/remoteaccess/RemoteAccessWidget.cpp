/*
 *  RemoteAccessWidget.cpp - widget containing a VNC-view and controls for it
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

#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>

#include "rfb/keysym.h"

#include "RemoteAccessWidget.h"
#include "RemoteAccessFeaturePlugin.h"
#include "VncViewWidget.h"
#include "VeyonConfiguration.h"
#include "VeyonConnection.h"
#include "VeyonMasterInterface.h"
#include "Computer.h"
#include "ComputerControlInterface.h"
#include "PlatformCoreFunctions.h"
#include "Screenshot.h"


// toolbar for remote-control-widget
RemoteAccessWidgetToolBar::RemoteAccessWidgetToolBar( RemoteAccessWidget* parent,
													  bool startViewOnly, bool showViewOnlyToggleButton ) :
	RemoteAccessToolBar( parent, startViewOnly, showViewOnlyToggleButton ),
	m_parent( parent ),
	m_screenSelectActions( new QActionGroup(this) )
{
	connect( this, &RemoteAccessToolBar::viewOnlyChanged, parent, &RemoteAccessWidget::setViewOnly );
	connect( this, &RemoteAccessToolBar::fullScreenChanged, parent, &RemoteAccessWidget::toggleFullScreen );
	connect( this, &RemoteAccessToolBar::screenshotRequested, parent, &RemoteAccessWidget::takeScreenshot );
	connect( this, &RemoteAccessToolBar::disconnectRequested, parent, &QWidget::close );

	auto vncView = parent->vncView();
	connect( vncView->connection(), &VncConnection::stateChanged, this, &RemoteAccessWidgetToolBar::updateConnectionState );

	updateScreens();

	auto shortcutMenu = m_sendShortcutButton->menu();
#if QT_VERSION < 0x050600
#warning Building legacy compat code for unsupported version of Qt
	connect( shortcutMenu->addAction( tr( "Ctrl+Alt+Del" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutCtrlAltDel ); } );
	connect( shortcutMenu->addAction( tr( "Ctrl+Esc" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutCtrlEscape ); } );
	connect( shortcutMenu->addAction( tr( "Alt+Tab" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutAltTab ); } );
	connect( shortcutMenu->addAction( tr( "Alt+F4" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutAltF4 ); } );
	connect( shortcutMenu->addAction( tr( "Win+Tab" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutWinTab ); } );
	connect( shortcutMenu->addAction( tr( "Win" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutWin ); } );
	connect( shortcutMenu->addAction( tr( "Menu" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutMenu ); } );
	connect( shortcutMenu->addAction( tr( "Alt+Ctrl+F1" ) ), &QAction::triggered,
			 this, [=]() { vncView->sendShortcut( VncView::ShortcutAltCtrlF1 ); } );
#else
	shortcutMenu->addAction( tr( "Ctrl+Alt+Del" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutCtrlAltDel ); }  );
	shortcutMenu->addAction( tr( "Ctrl+Esc" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutCtrlEscape ); }  );
	shortcutMenu->addAction( tr( "Alt+Tab" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutAltTab ); }  );
	shortcutMenu->addAction( tr( "Alt+F4" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutAltF4 ); }  );
	shortcutMenu->addAction( tr( "Win+Tab" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutWinTab ); }  );
	shortcutMenu->addAction( tr( "Win" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutWin ); }  );
	shortcutMenu->addAction( tr( "Menu" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutMenu ); }  );
	shortcutMenu->addAction( tr( "Alt+Ctrl+F1" ), this, [=]() { vncView->sendShortcut( VncView::ShortcutAltCtrlF1 ); }  );
#endif

	connect( vncView->computerControlInterface().data(), &ComputerControlInterface::screensChanged,
			 this, &RemoteAccessWidgetToolBar::updateScreens );

	connect( m_parent, &RemoteAccessWidget::screenChangedInRemoteAccessWidget,
			 this, &RemoteAccessWidgetToolBar::updateScreenSelectActions );
}



void RemoteAccessWidgetToolBar::updateScreenSelectActions( int newScreen )
{
	const auto screens = m_parent->vncView()->computerControlInterface()->screens();
	if( newScreen < -1 || newScreen >= screens.size() ) return;
	const auto m_screenSelectActions = m_selectScreenButton->menu()->actions();
	for (const auto& screenSelectAction : m_screenSelectActions)
	{
		if ( newScreen == -1 )
		{
			screenSelectAction->setChecked(true);
			break;
		}
		if ( screenSelectAction->text() == screens[newScreen].name )
		{
			screenSelectAction->setChecked(true);
		}
	}
}



void RemoteAccessWidgetToolBar::updateConnectionState()
{
	setConnected( m_parent->vncView()->connection()->state() == VncConnection::State::Connected );
}



void RemoteAccessWidgetToolBar::updateScreens()
{
	const auto checkedScreenName = m_screenSelectActions->checkedAction() ?
										m_screenSelectActions->checkedAction()->text() : QString{};

	const auto screens = m_parent->vncView()->computerControlInterface()->screens();
	setScreenSelectionVisible(screens.size() > 1);

	const auto menu = m_selectScreenButton->menu();
	menu->clear();

	if(screens.size() > 1)
	{
		const auto showAllScreens = menu->addAction( tr( "All screens" ), this, [=]() {
			m_parent->vncView()->setViewport({});
		});

		m_screenSelectActions->addAction(showAllScreens);
		showAllScreens->setCheckable(true);
		showAllScreens->setChecked(checkedScreenName.isEmpty() ||
									 checkedScreenName == showAllScreens->text());

		menu->addSeparator();

		QPoint minimumScreenPosition{};
		for (const auto& screen : screens)
		{
			minimumScreenPosition.setX(qMin(minimumScreenPosition.x(), screen.geometry.x()));
			minimumScreenPosition.setY(qMin(minimumScreenPosition.y(), screen.geometry.y()));
		}

		for (const auto& screen : screens)
		{
			const auto action = menu->addAction(screen.name, this, [=]() {
				m_parent->vncView()->setViewport(screen.geometry.translated(-minimumScreenPosition));
			});
			action->setCheckable(true);
			if(action->text() == checkedScreenName)
			{
				action->setChecked(true);
			}
			m_screenSelectActions->addAction(action);
		}

		if(m_screenSelectActions->checkedAction())
		{
			m_screenSelectActions->checkedAction()->trigger();
		}
		else
		{
			showAllScreens->setChecked(true);
			showAllScreens->trigger();
		}
	}
	else
	{
		m_parent->vncView()->setViewport({});
	}
}



RemoteAccessWidget::RemoteAccessWidget( const ComputerControlInterface::Pointer& computerControlInterface,
										bool startViewOnly, bool showViewOnlyToggleButton ) :
	QWidget( nullptr ),
	m_computerControlInterface( computerControlInterface ),
	m_vncView( new VncViewWidget( computerControlInterface, {}, this ) ),
	m_toolBar( new RemoteAccessWidgetToolBar( this, startViewOnly, showViewOnlyToggleButton ) )
{
	const auto openOnMasterScreen = VeyonCore::config().showFeatureWindowsOnSameScreen();
	const auto master = VeyonCore::instance()->findChild<VeyonMasterInterface *>();
	if( master && openOnMasterScreen )
	{
		const auto masterWindow = master->mainWindow();
		move( masterWindow->x(), masterWindow->y() );
	} else {
		move( 0, 0 );
	}

	updateRemoteAccessTitle();
	connect( m_computerControlInterface.data(), &ComputerControlInterface::userChanged, this, &RemoteAccessWidget::updateRemoteAccessTitle );

	setWindowIcon( QPixmap( QStringLiteral(":/remoteaccess/kmag.png") ) );
	setAttribute( Qt::WA_DeleteOnClose, true );
	setMinimumSize(240,180);

	m_vncView->move( 0, 0 );
	m_vncView->installEventFilter( this );
	connect( m_toolBar, &RemoteAccessToolBar::returnFocusToView, this, [this] {
		if(isActiveWindow() && !QApplication::activePopupWidget()) m_vncView->setFocus(Qt::OtherFocusReason);
	} );
	connect( m_vncView, &VncViewWidget::sizeHintChanged, this, &RemoteAccessWidget::updateSize );

	showMaximized();
	VeyonCore::platform().coreFunctions().raiseWindow( this, false );

	showNormal();

	updateSize();

	setViewOnly( startViewOnly );
}



RemoteAccessWidget::~RemoteAccessWidget()
{
	delete m_vncView;
}



VncView* RemoteAccessWidget::vncView() const
{
	return m_vncView;
}



bool RemoteAccessWidget::eventFilter( QObject* object, QEvent* event )
{
	if( object == m_vncView )
	{
		if( event->type() == QEvent::MouseMove )
		{
			const auto mouse = static_cast<QMouseEvent*>(event);
			if(mouse->buttons() == Qt::NoButton) m_toolBar->requestRevealAt(mouse->pos());
			else m_toolBar->cancelReveal();
		}
		else if( event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress )
			m_toolBar->cancelReveal();
	}
	if( event->type() == QEvent::KeyRelease &&
		dynamic_cast<QKeyEvent *>( event )->key() == Qt::Key_Escape &&
		m_vncView->connection()->isConnected() == false )
	{
		close();
		return true;
	}

	if( object == m_vncView && event->type() == QEvent::FocusOut )
	{
		m_toolBar->disappear();
	}

	if( event->type() == QEvent::KeyPress && m_vncView->viewOnly() )
	{
		const auto screens = m_vncView->computerControlInterface()->screens();
		const auto key = static_cast<QKeyEvent *>( event )->key();
		if ( screens.size() > 1 && ( key == Qt::Key_Tab || key == Qt::Key_Backtab ) )
		{
			if( key == Qt::Key_Tab )
			{
				if ( m_currentScreen < screens.size() - 1 )
				{
					m_currentScreen++;
				} else
				{
					m_currentScreen = -1;
				}
			}

			if( key == Qt::Key_Backtab )
			{
				if ( m_currentScreen == -1 )
				{
					m_currentScreen = screens.size()-1;
				} else if ( m_currentScreen > 0 )
				{
					m_currentScreen--;
				} else
				{
					m_currentScreen = -1;
				}
			}

			if ( m_currentScreen == -1)
			{
				m_vncView->setViewport( {} );
			}
			else
			{
				m_vncView->setViewport(screens[m_currentScreen].geometry);
			}
			Q_EMIT screenChangedInRemoteAccessWidget(m_currentScreen);
			return true;
		}

		return false;
	}

	return QWidget::eventFilter( object, event );
}



#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void RemoteAccessWidget::enterEvent( QEnterEvent* event )
#else
void RemoteAccessWidget::enterEvent( QEvent* event )
#endif
{
	m_toolBar->disappear();

	QWidget::enterEvent( event );
}



void RemoteAccessWidget::leaveEvent( QEvent* event )
{
	m_toolBar->cancelReveal();
	m_toolBar->disappear();

	QWidget::leaveEvent( event );
}



void RemoteAccessWidget::resizeEvent( QResizeEvent* event )
{
	m_vncView->setFixedSize(size());
	m_toolBar->updatePosition();

	QWidget::resizeEvent( event );
}



void RemoteAccessWidget::updateSize()
{
	if (!(windowState() & Qt::WindowFullScreen))
	{
		resize( m_vncView->sizeHint() );
	}
}



void RemoteAccessWidget::toggleFullScreen( bool _on )
{
	if( _on )
	{
		setWindowState( windowState() | Qt::WindowFullScreen );
	}
	else
	{
		setWindowState( windowState() & ~Qt::WindowFullScreen );
	}
}



void RemoteAccessWidget::setViewOnly( bool viewOnly )
{
	// Restore the view's pre-focus-out state first, then apply the explicit
	// user's mode. Otherwise focusInEvent can undo the toolbar toggle.
	m_vncView->setFocus(Qt::OtherFocusReason);
	m_vncView->setViewOnly( viewOnly );
	m_toolBar->updateControls( viewOnly );
	m_toolBar->update();
}



void RemoteAccessWidget::takeScreenshot()
{
	Screenshot().take( m_computerControlInterface );
}



void RemoteAccessWidget::updateRemoteAccessTitle()
{
	const auto username = m_computerControlInterface->userFullName().isEmpty() ?
							  m_computerControlInterface->userLoginName() : m_computerControlInterface->userFullName();

	if (username.isEmpty() )
	{
		setWindowTitle( tr( "%1 - %2 Remote Access" ).arg( m_computerControlInterface->computer().name(),
														   VeyonCore::applicationName() ) );
	}
	else
	{
		setWindowTitle( tr( "%1 - %2 - %3 Remote Access" ).arg( username,
																m_computerControlInterface->computer().name(),
																VeyonCore::applicationName() ) );
	}
}
