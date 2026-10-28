/*
 * MainWindow.cpp - implementation of MainWindow class
 *
 * Copyright (c) 2004-2022 Tobias Junghans <tobydox@veyon.io>
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

#include <QCloseEvent>
#include <QKeyEvent>
#include <QHostAddress>
#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QStyle>

#include "AboutDialog.h"
#include "AccessControlProvider.h"
#include "MainWindow.h"
#include "BuiltinFeatures.h"
#include "SchoolFeatures.h"
#include "AuthenticationCredentials.h"
#include "ComputerControlListModel.h"
#include "ComputerManager.h"
#include "ComputerSelectPanel.h"
#include "ScreenshotManagementPanel.h"
#include "FeatureManager.h"
#include "MonitoringMode.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "SlideshowPanel.h"
#include "SpotlightPanel.h"
#include "ToolButton.h"
#include "VeyonConfiguration.h"
#include "VeyonMaster.h"
#include "UserConfig.h"
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"
#include "SelectedFeatureControl.h"

#include "ui_MainWindow.h"


MainWindow::MainWindow( VeyonMaster &masterCore, QWidget* parent ) :
	QMainWindow( parent ),
	ui( new Ui::MainWindow ),
	m_master( masterCore ),
	m_modeGroup( new QButtonGroup( this ) )
{
	ui->setupUi( this );
	auto appearance = NetLinkAppearance::install();
	NetLinkUi::addInformationMenu(this);
	auto appearanceButton = new QToolButton(this);
	appearanceButton->setObjectName(QStringLiteral("netlinkAppearanceButton"));
	appearanceButton->setText(NetLinkUi::text("Appearance"));
	appearanceButton->setToolTip(NetLinkUi::text("Choose a theme, colors, icon set and button size. Does not change keys, clients or the service."));
	appearanceButton->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
	appearanceButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(appearanceButton, &QToolButton::clicked, this, [this, appearance] { appearance->showSettings(this); });
	connect(appearance, &NetLinkAppearance::changed, ui->computerMonitoringWidget, &ComputerMonitoringWidget::refreshAppearance);
	connect(appearance, &NetLinkAppearance::changed, this, &MainWindow::refreshFeatureAppearance);
	ui->computerMonitoringWidget->refreshAppearance();

	// Qt appends the display name unless already at the end: include it once.
	setWindowTitle(VeyonCore::applicationName()+QStringLiteral(" — ")+NetLinkUi::text("Teacher Console"));

	restoreState( QByteArray::fromBase64( m_master.userConfig().windowState().toUtf8() ) );
	restoreGeometry( QByteArray::fromBase64( m_master.userConfig().windowGeometry().toUtf8() ) );

	// add widgets to status bar
	ui->statusBar->addWidget( ui->panelButtons );
	ui->statusBar->addWidget( ui->spacerLabel1 );
	ui->statusBar->addWidget( ui->filterLineEdit, 2 );
	ui->statusBar->addWidget( ui->filterPoweredOnComputersButton );
	ui->statusBar->addWidget( ui->filterComputersWithLoggedOnUsersButton );
	ui->statusBar->addWidget( ui->spacerLabel2, 1 );
	ui->statusBar->addWidget( ui->gridSizeSlider, 2 );
	ui->statusBar->addWidget( ui->autoAdjustComputerIconSizeButton );
	ui->statusBar->addWidget( ui->spacerLabel3 );
	ui->statusBar->addWidget( ui->useCustomComputerArrangementButton );
	ui->statusBar->addWidget( ui->alignComputersButton );
	ui->statusBar->addWidget( ui->spacerLabel4 );
	ui->statusBar->addWidget( ui->aboutButton );
	ui->statusBar->addPermanentWidget(appearanceButton);

	// create all views
	auto mainSplitter = new QSplitter( Qt::Horizontal, ui->centralWidget );
	mainSplitter->setChildrenCollapsible( false );
	mainSplitter->setObjectName( QStringLiteral("MainSplitter") );

	auto monitoringSplitter = new QSplitter( Qt::Vertical, mainSplitter );
	monitoringSplitter->setChildrenCollapsible( false );
	monitoringSplitter->setObjectName( QStringLiteral("MonitoringSplitter") );

	auto slideshowSpotlightSplitter = new QSplitter( Qt::Horizontal, monitoringSplitter );
	slideshowSpotlightSplitter->setChildrenCollapsible( false );
	slideshowSpotlightSplitter->setObjectName( QStringLiteral("SlideshowSpotlightSplitter") );

	auto computerSelectPanel = new ComputerSelectPanel( m_master.computerManager() );
	connect(computerSelectPanel, &ComputerSelectPanel::classProfileRequested, &m_master, &VeyonMaster::showClassProfiles);
	auto screenshotManagementPanel = new ScreenshotManagementPanel();
	auto slideshowPanel = new SlideshowPanel( m_master.userConfig(), ui->computerMonitoringWidget );
	auto spotlightPanel = new SpotlightPanel( m_master.userConfig(), ui->computerMonitoringWidget );

	slideshowSpotlightSplitter->addWidget( slideshowPanel );
	slideshowSpotlightSplitter->addWidget( spotlightPanel );
	slideshowSpotlightSplitter->setStretchFactor( slideshowSpotlightSplitter->indexOf(slideshowPanel), 1 );
	slideshowSpotlightSplitter->setStretchFactor( slideshowSpotlightSplitter->indexOf(spotlightPanel), 1 );

	monitoringSplitter->addWidget( slideshowSpotlightSplitter );
	monitoringSplitter->addWidget( ui->computerMonitoringWidget );
	monitoringSplitter->setStretchFactor( monitoringSplitter->indexOf(slideshowSpotlightSplitter), 1 );
	monitoringSplitter->setStretchFactor( monitoringSplitter->indexOf(ui->computerMonitoringWidget), 1 );

	mainSplitter->addWidget( computerSelectPanel );
	mainSplitter->addWidget( screenshotManagementPanel );
	mainSplitter->addWidget( monitoringSplitter );

	mainSplitter->setStretchFactor( mainSplitter->indexOf(monitoringSplitter), 1 );


	const QHash<QWidget *, QAbstractButton *> panelButtons{
		{ computerSelectPanel, ui->computerSelectPanelButton },
		{ screenshotManagementPanel, ui->screenshotManagementPanelButton },
		{ slideshowPanel, ui->slideshowPanelButton },
		{ spotlightPanel, ui->spotlightPanelButton }
	};

	for( auto it = panelButtons.constBegin(), end = panelButtons.constEnd(); it != end; ++it )
	{
		it.key()->hide();
		it.key()->installEventFilter( this );
		connect( *it, &QAbstractButton::toggled, it.key(), &QWidget::setVisible );
	}

	QList<int> splitterSizes;
	for( auto* splitter : { slideshowSpotlightSplitter, monitoringSplitter, mainSplitter } )
	{
		splitter->setHandleWidth( 7 );
		splitter->setStyleSheet( QStringLiteral("QSplitter::handle:hover{background-color:palette(highlight);}") );

		splitter->installEventFilter( this );

		int index = 0;

		const auto splitterStates = m_master.userConfig().splitterStates()[splitter->objectName()].toArray();
		splitterSizes.clear();
		splitterSizes.reserve( splitterStates.size() );

		for( const auto& sizeObject : splitterStates )
		{
			auto size = sizeObject.toInt();
			const auto widget = splitter->widget( index );
			const auto button = panelButtons.value( widget );

			if( widget )
			{
				if( button )
				{
					widget->setVisible( size > 0 );
					button->setChecked( size > 0 );
				}
				size = qAbs( size );
				if( splitter->orientation() == Qt::Horizontal )
				{
					widget->resize( size, widget->height() );
				}
				else
				{
					widget->resize( widget->width(), size );
				}

				widget->setProperty( originalSizePropertyName(), widget->size() );
			}
			splitterSizes.append( size );
			++index;
		}
		splitter->setSizes( splitterSizes );
	}

	const auto SplitterContentBaseSize = 500;

	if( spotlightPanel->property( originalSizePropertyName() ).isNull() ||
		slideshowPanel->property( originalSizePropertyName() ).isNull() )
	{
		slideshowSpotlightSplitter->setSizes( { SplitterContentBaseSize, SplitterContentBaseSize } );
	}

	if( slideshowSpotlightSplitter->property( originalSizePropertyName() ).isNull() ||
		ui->computerMonitoringWidget->property( originalSizePropertyName() ).isNull() )
	{
		monitoringSplitter->setSizes( { SplitterContentBaseSize, SplitterContentBaseSize } );
	}

	ui->centralLayout->addWidget( mainSplitter );

	if( VeyonCore::config().autoOpenComputerSelectPanel() )
	{
		ui->computerSelectPanelButton->setChecked( true );
	}

	// initialize search filter
	ui->filterPoweredOnComputersButton->setChecked( m_master.userConfig().filterPoweredOnComputers() );
	ui->filterComputersWithLoggedOnUsersButton->setChecked( m_master.userConfig().filterComputersWithLoggedOnUsers() );
	connect( ui->filterLineEdit, &QLineEdit::textChanged,
			 this, [this]( const QString& filter ) { ui->computerMonitoringWidget->setSearchFilter( filter ); } );
	connect( ui->filterPoweredOnComputersButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) { ui->computerMonitoringWidget->setFilterPoweredOnComputers( enabled ); } );
	connect( ui->filterComputersWithLoggedOnUsersButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) { ui->computerMonitoringWidget->setFilterComputersWithLoggedOnUsers( enabled ); } );

	// initialize monitoring screen size slider
	ui->gridSizeSlider->setMinimum( ComputerMonitoringWidget::MinimumComputerScreenSize );
	ui->gridSizeSlider->setMaximum( ComputerMonitoringWidget::MaximumComputerScreenSize );

	ui->autoAdjustComputerIconSizeButton->setChecked( ui->computerMonitoringWidget->autoAdjustIconSize() );

	connect( ui->gridSizeSlider, &QSlider::valueChanged,
			 this, [this]( int size ) { ui->computerMonitoringWidget->setComputerScreenSize( size ); } );
	connect( ui->computerMonitoringWidget, &ComputerMonitoringWidget::computerScreenSizeAdjusted,
			 ui->gridSizeSlider, &QSlider::setValue );
	connect( ui->autoAdjustComputerIconSizeButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) {
				 ui->computerMonitoringWidget->setAutoAdjustIconSize( enabled );
				 m_master.userConfig().setAutoAdjustMonitoringIconSize( enabled );
			 } );

	int size = ComputerMonitoringWidget::DefaultComputerScreenSize;
	if( m_master.userConfig().monitoringScreenSize() >= ComputerMonitoringWidget::MinimumComputerScreenSize )
	{
		size = m_master.userConfig().monitoringScreenSize();
	}

	ui->gridSizeSlider->setValue( size );
	ui->computerMonitoringWidget->setComputerScreenSize( size );

	// initialize computer placement controls
	ui->useCustomComputerArrangementButton->setChecked( m_master.userConfig().useCustomComputerPositions() );
	connect( ui->useCustomComputerArrangementButton, &QToolButton::toggled,
			 ui->computerMonitoringWidget, &ComputerMonitoringWidget::setUseCustomComputerPositions );
	connect( ui->alignComputersButton, &QToolButton::clicked,
			 ui->computerMonitoringWidget, &ComputerMonitoringWidget::alignComputers );


	// create the main toolbar
	ui->toolBar->layout()->setSpacing( 2 );
	ui->toolBar->toggleViewAction()->setEnabled( false );

	addToolBar( Qt::TopToolBarArea, ui->toolBar );

	addFeaturesToToolBar();
	refreshFeatureAppearance();
	reloadSubFeatures();

	m_modeGroup->button(int(qHash(VeyonCore::builtinFeatures().monitoringMode().feature().uid())))->setChecked(true); // clazy:exclude=qt6-qhash-signature
	connect(ui->computerMonitoringWidget->selectionModel(),&QItemSelectionModel::selectionChanged,this,[this] {
		updateModeButtonGroup();reloadSubFeatures();
	});
	connect(&m_master.computerControlListModel(),&ComputerControlListModel::activeFeaturesChanged,this,[this] {
		updateModeButtonGroup();
	});

	VeyonCore::enforceBranding( this );
}



MainWindow::~MainWindow()
{
	ui->computerMonitoringWidget->saveConfiguration();

	delete ui;
}



bool MainWindow::initAuthentication()
{
	if( VeyonCore::instance()->initAuthentication() )
	{
		return true;
	}

	if( VeyonCore::config().authenticationMethod() == VeyonCore::AuthenticationMethod::KeyFileAuthentication )
	{
		if(!VeyonCore::config().masterAuthenticationKey().isEmpty()) {
			QMessageBox::warning(nullptr,NetLinkUi::text("Master authentication key"),NetLinkUi::text("The configured default private key is missing, unreadable or invalid. Check the Master key selection and file permissions in Configurator; do not regenerate working keys."));return false;
		}
		QMessageBox::information( nullptr,
				tr( "Authentication impossible" ),
				tr(	"No authentication key files were found or your current ones "
					"are outdated. Please create new key files using the %1 "
					"Configurator. Alternatively set up logon authentication "
					"using the %1 Configurator. Otherwise you won't be "
					"able to access computers using %1." ).arg( VeyonCore::applicationName() ) );

	}

	return false;
}



bool MainWindow::initAccessControl()
{
	if( VeyonCore::config().accessControlForMasterEnabled() &&
			VeyonCore::authenticationCredentials().hasCredentials( AuthenticationCredentials::Type::UserLogon ) )
	{
		const auto accessControlResult =
				AccessControlProvider().checkAccess( VeyonCore::authenticationCredentials().logonUsername(),
													 QHostAddress( QHostAddress::LocalHost ).toString(),
													 QStringList() );
		if( accessControlResult == AccessControlProvider::Access::Deny )
		{
			vWarning() << "user" << VeyonCore::authenticationCredentials().logonUsername()
					   << "is not allowed to access computers";
			QMessageBox::critical( nullptr, tr( "Access denied" ),
								   tr( "According to the local configuration you're not allowed "
									   "to access computers in the network. Please log in with a different "
									   "account or let your system administrator check the local configuration." ) );
			return false;
		}
	}

	return true;
}



void MainWindow::reloadSubFeatures()
{
	for( const auto& feature : m_master.features() )
	{
		auto button = ui->toolBar->findChild<QToolButton *>( feature.name() );
		if( button )
		{
			addSubFeaturesToToolButton( button, feature );
		}
	}
}



ComputerControlInterfaceList MainWindow::selectedComputerControlInterfaces() const
{
	return ui->computerMonitoringWidget->selectedComputerControlInterfaces();
}



void MainWindow::closeEvent( QCloseEvent* event )
{
	Feature activeFeature;
	const auto monitoringUid=VeyonCore::builtinFeatures().monitoringMode().feature().uid();
	for(const auto& ci:m_master.allComputerControlInterfaces()) if(ci && !ci->designatedModeFeature().isNull() && ci->designatedModeFeature()!=monitoringUid) {
		activeFeature=VeyonCore::featureManager().feature(ci->designatedModeFeature());break;
	}
	if( activeFeature.isValid() )
	{
		if(QMessageBox::question(this,tr("Feature active"),
			NetLinkUi::text("Stop the active feature (%1) and close the Console?").arg(activeFeature.displayName()),
			QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) {
			event->ignore();return;
		}
	}
	if(VeyonCore::builtinFeatures().schoolFeatures().activeInternetPolicies()>0 &&
		QMessageBox::question(this,QStringLiteral("InternetGuard"),NetLinkUi::text("InternetGuard has requested blocks or unconfirmed unblocks. Close and send unblock commands? Unconfirmed clients normally release filters after 90 seconds without renewal; Windows errors may delay cleanup."),
			QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) {
		event->ignore();return;
	}

	QJsonObject splitterStates;
	const auto splitters = findChildren<QSplitter *>();
	for( const auto* splitter : splitters )
	{
		const auto sizes = splitter->sizes();
		QJsonArray splitterSizes;
		int i = 0;
		int hiddenSize = 0;
		for( auto size : sizes )
		{
			auto widget = splitter->widget(i);
			const auto originalSize = widget->property( originalSizePropertyName() ).toSize();
			if( widget->size().isEmpty() && originalSize.isEmpty() == false )
			{
				size = splitter->orientation() == Qt::Horizontal ? -originalSize.width() : -originalSize.height();
				hiddenSize += qAbs(size);
			}
			else
			{
				size -= hiddenSize;
			}

			splitterSizes.append( size );
			++i;
		}
		splitterStates[splitter->objectName()] = splitterSizes;
	}

	m_master.userConfig().setSplitterStates( splitterStates );

	m_master.userConfig().setWindowState( QString::fromLatin1( saveState().toBase64() ) );
	m_master.userConfig().setWindowGeometry( QString::fromLatin1( saveGeometry().toBase64() ) );

	QMainWindow::closeEvent( event );
}



bool MainWindow::eventFilter( QObject* object, QEvent* event )
{
	if( event->type() == QEvent::Resize )
	{
		const auto widget = qobject_cast<QWidget *>( object );
		const auto resizeEvent = static_cast<QResizeEvent *>( event );

		if( resizeEvent->oldSize().isEmpty() == false )
		{
			widget->setProperty( originalSizePropertyName(), resizeEvent->oldSize() );
		}
	}

	return QMainWindow::eventFilter( object, event );
}



void MainWindow::keyPressEvent( QKeyEvent* event )
{
	switch( event->key() )
	{
	case Qt::Key_F5:
		VeyonCore::networkObjectDirectoryManager().configuredDirectory()->update();
		m_master.computerControlListModel().reload();
		event->accept();
		break;
	case Qt::Key_F11:
		QWidget::setWindowState( QWidget::windowState() ^ Qt::WindowFullScreen );
		event->accept();
		break;
	default:
		QMainWindow::keyPressEvent( event );
		break;
	}
}



void MainWindow::showAboutDialog()
{
	AboutDialog( this ).exec();
}



void MainWindow::addFeaturesToToolBar()
{
	for( const auto& feature : m_master.features() )
	{
		if( feature.testFlag( Feature::Flag::Meta ) )
		{
			continue;
		}

		auto btn = new ToolButton( QIcon( feature.iconUrl() ),
										  feature.displayName(),
										  feature.displayNameActive(),
										  NetLinkUi::featureDescription(feature),
										  feature.shortcut() );
		connect( btn, &QToolButton::clicked, this, [=] () {
			m_master.runFeature( feature );
			updateModeButtonGroup();
			if( feature.testFlag( Feature::Flag::Mode ) )
			{
				reloadSubFeatures();
			}
		} );
		btn->setObjectName( feature.name() );
		btn->addTo( ui->toolBar );

		if( feature.testFlag( Feature::Flag::Mode ) )
		{
			btn->setCheckable( true );
			m_modeGroup->addButton( btn, buttonId( feature ) );
		}
	}
}



void MainWindow::addSubFeaturesToToolButton( QToolButton* button, const Feature& parentFeature )
{
	if( button->menu() )
	{
		button->menu()->close();
		button->menu()->deleteLater();
		button->setMenu( nullptr );
	}

	const auto parentFeatureIsMode = parentFeature.testFlag( Feature::Flag::Mode );
	const auto subFeatures = m_master.subFeatures( parentFeature.uid() );

	if( subFeatures.isEmpty() ||
		( parentFeatureIsMode && button->isChecked() ) )
	{
		return;
	}

	auto menu = new QMenu( button );
	menu->setObjectName( parentFeature.name() );
	menu->setToolTipsVisible( true );

	for( const auto& subFeature : subFeatures )
	{
#if QT_VERSION < 0x050600
#warning Building legacy compat code for unsupported version of Qt
		auto action = menu->addAction( QIcon( subFeature.iconUrl() ), subFeature.displayName() );
		action->setShortcut( subFeature.shortcut() );
		connect( action, &QAction::triggered, this,
			[=]() {
				m_master.runFeature( subFeature );
				updateModeButtonGroup();
				if( parentFeatureIsMode )
				{
					reloadSubFeatures();
				}
			}
		);
#else
		auto action = menu->addAction( QIcon( subFeature.iconUrl() ), subFeature.displayName(), this,
			[=]() {
				m_master.runFeature( subFeature );
				updateModeButtonGroup();
				if( parentFeatureIsMode )
				{
					reloadSubFeatures();
				}
			},
			subFeature.shortcut() );
#endif
		const auto preferences=NetLinkAppearance::install()->preferences();
		action->setIcon(NetLinkUi::icon(subFeature.name(),preferences.iconSet,preferences.accent,QIcon(subFeature.iconUrl())));
		action->setToolTip( NetLinkUi::featureDescription(subFeature) );
		action->setObjectName( subFeature.uid().toString() );

		if( subFeature.testFlag( Feature::Flag::Option ) )
		{
			action->setCheckable( true );
			action->setChecked( subFeature.testFlag( Feature::Flag::Checked ) );
		}
	}

	button->setMenu( menu );
	button->setPopupMode( ToolButton::InstantPopup );
}



void MainWindow::updateModeButtonGroup()
{
	const auto& monitoringMode = VeyonCore::builtinFeatures().monitoringMode().feature();
	const auto selected=SelectedFeatureControl::targets(m_master.selectedComputerControlInterfaces());
	bool any=false,changed=false;
	m_modeGroup->setExclusive(false);
	for(const auto& feature:m_master.features()) if(feature.testFlag(Feature::Flag::Mode) && feature.uid()!=monitoringMode.uid()) {
		auto button=m_modeGroup->button(buttonId(feature));if(!button) continue;
		FeatureUidList family;
		for(const auto& related:VeyonCore::featureManager().relatedFeatures(feature.uid())) family.append(related.uid());
		if(!family.contains(feature.uid())) family.append(feature.uid());
		const bool active=SelectedFeatureControl::allActive(selected,family);
		changed|=button->isChecked()!=active;button->setChecked(active);any|=active;
	}
	if(auto button=m_modeGroup->button(buttonId(monitoringMode))) {changed|=button->isChecked()==any;button->setChecked(!any);}
	m_modeGroup->setExclusive(true);
	if(changed) reloadSubFeatures();
}

void MainWindow::refreshFeatureAppearance()
{
	const auto preferences=NetLinkAppearance::install()->preferences();
	ui->toolBar->setIconSize(QSize(preferences.toolbarIconSize,preferences.toolbarIconSize));
	const QHash<QAbstractButton*,QString> utilityIcons{
		{ui->computerSelectPanelButton,QStringLiteral("group")},{ui->screenshotManagementPanelButton,QStringLiteral("Screenshot")},
		{ui->slideshowPanelButton,QStringLiteral("Demo")},{ui->spotlightPanelButton,QStringLiteral("RemoteView")},
		{ui->filterPoweredOnComputersButton,QStringLiteral("PowerOn")},{ui->filterComputersWithLoggedOnUsersButton,QStringLiteral("Appello")},
		{ui->autoAdjustComputerIconSizeButton,QStringLiteral("monitor")},{ui->useCustomComputerArrangementButton,QStringLiteral("StartApp")},
		{ui->alignComputersButton,QStringLiteral("monitor")},{ui->aboutButton,QStringLiteral("info")},
		{findChild<QToolButton*>(QStringLiteral("netlinkAppearanceButton")),QStringLiteral("settings")}};
	for(auto it=utilityIcons.constBegin();it!=utilityIcons.constEnd();++it) {
		auto button=it.key();if(!button) continue;
		if(!button->property("netlinkClassicIcon").isValid()) button->setProperty("netlinkClassicIcon",QVariant::fromValue(button->icon()));
		button->setIcon(NetLinkUi::icon(it.value(),preferences.iconSet,preferences.accent,button->property("netlinkClassicIcon").value<QIcon>()));
		button->setIconSize(QSize(20,20));
	}
	ui->computerSelectPanelButton->setToolTip(NetLinkUi::text("Shows or hides groups and computers. Select the computers targeted by Console commands here."));
	ui->screenshotManagementPanelButton->setToolTip(NetLinkUi::text("Shows saved screenshots. Images may contain personal data: check where they are stored."));
	ui->slideshowPanelButton->setToolTip(NetLinkUi::text("Shows or hides the rotating desktop slideshow panel."));
	ui->spotlightPanelButton->setToolTip(NetLinkUi::text("Shows or hides the enlarged observation panel for selected computers."));
	ui->filterPoweredOnComputersButton->setToolTip(NetLinkUi::text("Shows only connected computers. Does not power computers on or off."));
	ui->filterComputersWithLoggedOnUsersButton->setToolTip(NetLinkUi::text("Shows only computers with a detected user session. Windows accounts are separate from self-declared attendance names."));
	ui->autoAdjustComputerIconSizeButton->setToolTip(NetLinkUi::text("Automatically fits thumbnails to the available Console space."));
	ui->useCustomComputerArrangementButton->setToolTip(NetLinkUi::text("Uses a custom computer layout. Does not change groups or network addresses."));
	ui->alignComputersButton->setToolTip(NetLinkUi::text("Aligns thumbnails in the current layout without sending commands to clients."));
	ui->aboutButton->setToolTip(NetLinkUi::text("NetLink version, copyright and original component licenses."));
	for(const auto& feature:m_master.features()) {
		auto button=ui->toolBar->findChild<QToolButton*>(feature.name());
		if(!button) continue;
		button->setIcon(NetLinkUi::icon(feature.name(),preferences.iconSet,preferences.accent,QIcon(feature.iconUrl())));
		button->setIconSize(QSize(preferences.toolbarIconSize,preferences.toolbarIconSize));
		if(auto featureButton=qobject_cast<ToolButton*>(button)) featureButton->updateMinimumSize();
		for(auto action:ui->toolBar->actions()) if(ui->toolBar->widgetForAction(action)==button) action->setIcon(button->icon());
	}
	reloadSubFeatures();
}
