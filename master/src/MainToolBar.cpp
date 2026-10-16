/*
 *  MainToolBar.cpp - MainToolBar for MainWindow
 *
 *  Copyright (c) 2007-2022 Tobias Junghans <tobydox@veyon.io>
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

#include "NetLinkUi.h"
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>

#include "MainToolBar.h"
#include "MainWindow.h"
#include "VeyonMaster.h"
#include "ToolButton.h"
#include "UserConfig.h"
#include "NetLinkAppearance.h"


MainToolBar::MainToolBar( QWidget* parent ) :
	QToolBar( tr( "Configuration" ), parent ),
	m_mainWindow( dynamic_cast<MainWindow *>( parent ) )
{
	ToolButton::setToolTipsDisabled( m_mainWindow->masterCore().userConfig().noToolTips() );
	ToolButton::setIconOnlyMode( m_mainWindow, m_mainWindow->masterCore().userConfig().toolButtonIconOnlyMode() );
}



void MainToolBar::contextMenuEvent( QContextMenuEvent* event )
{
	QMenu menu( this );

#if QT_VERSION < 0x050600
#warning Building legacy compat code for unsupported version of Qt
	auto toolTipAction = menu.addAction( NetLinkUi::text("Hide button tooltips") );
	connect( toolTipAction, &QAction::triggered, this, &MainToolBar::toggleToolTips );
#else
	auto toolTipAction = menu.addAction( NetLinkUi::text("Hide button tooltips"), this, &MainToolBar::toggleToolTips );
#endif
	toolTipAction->setCheckable( true );
	toolTipAction->setChecked( m_mainWindow->masterCore().userConfig().noToolTips() );

#if QT_VERSION < 0x050600
#warning Building legacy compat code for unsupported version of Qt
	auto iconModeAction = menu.addAction( tr( "Show icons only" ) );
	connect( iconModeAction, &QAction::triggered, this, &MainToolBar::toggleIconMode );
#else
	auto iconModeAction = menu.addAction( tr( "Show icons only" ), this, &MainToolBar::toggleIconMode );
#endif
	iconModeAction->setCheckable( true );
	iconModeAction->setChecked( m_mainWindow->masterCore().userConfig().toolButtonIconOnlyMode() );
	menu.addSeparator();
	auto appearance=menu.addAction(NetLinkUi::text("Appearance, colors and icons…"));
	connect(appearance,&QAction::triggered,this,[this]{NetLinkAppearance::install()->showSettings(m_mainWindow);});

	menu.exec( event->globalPos() );
}



void MainToolBar::paintEvent( QPaintEvent* event )
{
	QPainter p( this );
	p.setPen( palette().color(QPalette::Mid) );
	p.fillRect( event->rect(), palette().brush( QPalette::Window ) );
	p.drawLine( 0, 0, width(), 0 );
	p.drawLine( 0, height()-1, width(), height()-1 );
}



void MainToolBar::toggleToolTips()
{
	bool newToolTipState = !m_mainWindow->masterCore().userConfig().noToolTips();

	ToolButton::setToolTipsDisabled( newToolTipState );
	m_mainWindow->masterCore().userConfig().setNoToolTips( newToolTipState );
}



void MainToolBar::toggleIconMode()
{
	bool newToolButtonIconMode = !m_mainWindow->masterCore().userConfig().toolButtonIconOnlyMode();

	ToolButton::setIconOnlyMode( m_mainWindow, newToolButtonIconMode );
	m_mainWindow->masterCore().userConfig().setToolButtonIconOnlyMode( newToolButtonIconMode );
}
