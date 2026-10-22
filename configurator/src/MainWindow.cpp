/*
 * MainWindow.cpp - implementation of MainWindow class
 *
 * Copyright (c) 2010-2022 Tobias Junghans <tobydox@veyon.io>
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

#include <QActionGroup>
#include <QDir>
#include <QProcess>
#include <QCloseEvent>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QComboBox>
#include <QLabel>
#include <QScreen>
#include <QToolBar>
#include <QSignalBlocker>

#include "Configuration/JsonStore.h"
#include "Configuration/UiMapping.h"

#include "AboutDialog.h"
#include "ConfigurationPagePluginInterface.h"
#include "ConfigurationManager.h"
#include "VeyonConfiguration.h"
#include "MainWindow.h"
#include "PluginManager.h"
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"
#include <QMenuBar>

#include "ui_MainWindow.h"


MainWindow::MainWindow( QWidget* parent ) :
	QMainWindow( parent ),
	ui( new Ui::MainWindow ),
	m_configChanged( false )
{
	ui->setupUi( this );
	auto appearance = NetLinkAppearance::install();
	Q_UNUSED(appearance)
	NetLinkUi::addInformationMenu(this);
	ui->actionAboutVeyon->setText(NetLinkUi::text("About NetLink"));

	setWindowTitle(VeyonCore::applicationName()+QStringLiteral(" Configurator"));

	loadConfigurationPagePlugins();

	// reset all widget's values to current configuration
	reset( true );

	// if local configuration is incomplete, re-enable the apply button
	if( VeyonConfiguration().data().size() < VeyonCore::config().data().size() )
	{
		configurationChanged();
	}

	const auto pages = findChildren<ConfigurationPage *>();
	for( auto page : pages )
	{
		page->connectWidgetsToProperties();
		connect( page, &ConfigurationPage::widgetsChanged, this, &MainWindow::updateView );
	}

	connect( ui->buttonBox, &QDialogButtonBox::clicked, this, &MainWindow::resetOrApply );

	connect( ui->actionLoadSettings, &QAction::triggered, this, &MainWindow::loadSettingsFromFile );
	connect( ui->actionSaveSettings, &QAction::triggered, this, &MainWindow::saveSettingsToFile );

	auto viewModeGroup = new QActionGroup( this );
	m_viewModeEasy=new QAction(NetLinkUi::text("Easy — keys and connection"),this);m_viewModeEasy->setCheckable(true);
	m_viewModeEasy->setObjectName(QStringLiteral("viewModeEasy"));
	ui->menu_View->insertAction(ui->viewModeStandard,m_viewModeEasy);
	viewModeGroup->addAction(m_viewModeEasy);
	viewModeGroup->addAction( ui->viewModeStandard );
	viewModeGroup->addAction( ui->viewModeAdvanced );
	viewModeGroup->setExclusive( true );
	ui->viewModeStandard->setText(NetLinkUi::text("Normal"));ui->viewModeAdvanced->setText(NetLinkUi::text("Advanced"));
	QSettings viewSettings(QSettings::NativeFormat,QSettings::UserScope,QStringLiteral("Veyon"),QStringLiteral("NetLinkAppearance"));
	const auto savedMode=viewSettings.value(QStringLiteral("ConfiguratorView"),QStringLiteral("Easy")).toString();
	(savedMode==QStringLiteral("Advanced")?ui->viewModeAdvanced:savedMode==QStringLiteral("Standard")?ui->viewModeStandard:m_viewModeEasy)->setChecked(true);
	auto viewBar=addToolBar(NetLinkUi::text("Configuration mode"));viewBar->setObjectName(QStringLiteral("netlinkConfiguratorViewBar"));viewBar->setMovable(false);
	viewBar->addWidget(new QLabel(NetLinkUi::text("Mode: "),viewBar));
	m_viewModeSelector=new QComboBox(viewBar);m_viewModeSelector->setObjectName(QStringLiteral("netlinkConfiguratorView"));
	m_viewModeSelector->addItem(NetLinkUi::text("Easy"));m_viewModeSelector->addItem(NetLinkUi::text("Normal"));m_viewModeSelector->addItem(NetLinkUi::text("Advanced"));
	m_viewModeSelector->setToolTip(NetLinkUi::text("Easy: keys, authentication, essential service settings, groups/computers and discovery. Normal and Advanced show more options. Switching views does not change or clear settings."));
	viewBar->addWidget(m_viewModeSelector);
	connect(m_viewModeSelector,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){
		(index==2?ui->viewModeAdvanced:index==1?ui->viewModeStandard:m_viewModeEasy)->setChecked(true);updateView();
	});

	connect( viewModeGroup, &QActionGroup::triggered, this, &MainWindow::updateView );

	connect( ui->actionAboutQt, &QAction::triggered, QApplication::instance(), &QApplication::aboutQt );

	connect( &VeyonCore::config(), &VeyonConfiguration::configurationChanged, this, &MainWindow::configurationChanged );

	connect( ui->configPages, &QStackedWidget::currentChanged, this, &MainWindow::updateSizes );
	connect(ui->configPages,&QStackedWidget::currentChanged,this,[this]{ui->scrollArea->verticalScrollBar()->setValue(0);});
	ui->pageSelector->setIconSize(QSize(32,32));
	ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	ui->configPages->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);

	const auto available=QGuiApplication::primaryScreen()?QGuiApplication::primaryScreen()->availableGeometry().size():QSize(1100,750);
	resize(qMin(1040,available.width()-60),qMin(680,available.height()-80));

	updateView();

	VeyonCore::enforceBranding( this );
}



MainWindow::~MainWindow()
{
	delete ui;
}



void MainWindow::reset( bool onlyUI )
{
	if( onlyUI == false )
	{
		VeyonCore::config().clear();
		VeyonCore::config().reloadFromStore();
	}

	const auto pages = findChildren<ConfigurationPage *>();
	for( auto page : pages )
	{
		page->resetWidgets();
	}

	ui->buttonBox->setEnabled( false );
	m_configChanged = false;
}




void MainWindow::apply()
{
	const auto pages = findChildren<ConfigurationPage *>();
	// Reject invalid staged values before writing settings or changing service/firewall state.
	for( auto page : pages )
	{
		if( page->validateConfiguration() == false ) return;
	}
	if( applyConfiguration() )
	{
		for( auto page : pages )
		{
			page->applyConfiguration();
		}

		ui->buttonBox->setEnabled( false );
		m_configChanged = false;
	}
}



void MainWindow::resizeEvent( QResizeEvent* event )
{
	QMainWindow::resizeEvent( event );

	updateSizes();
}




void MainWindow::configurationChanged()
{
	ui->buttonBox->setEnabled( true );
	m_configChanged = true;
}




void MainWindow::resetOrApply( QAbstractButton *btn )
{
	if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Apply )
	{
		apply();
	}
	else if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Reset )
	{
		reset();
	}
}



void MainWindow::loadSettingsFromFile()
{
	QString fileName = QFileDialog::getOpenFileName( this, tr( "Load settings from file" ),
													 QDir::homePath(), tr( "JSON files (*.json)" ) );
	if( !fileName.isEmpty() )
	{
		// write current configuration to output file
		Configuration::JsonStore( Configuration::JsonStore::System, fileName ).load( &VeyonCore::config() );
		reset( true );
		configurationChanged();	// give user a chance to apply possible changes
	}
}




void MainWindow::saveSettingsToFile()
{
	QString fileName = QFileDialog::getSaveFileName( this, tr( "Save settings to file" ),
													 QDir::homePath(), tr( "JSON files (*.json)" ) );
	if( !fileName.isEmpty() )
	{
		if( !fileName.endsWith( QStringLiteral(".json"), Qt::CaseInsensitive ) )
		{
			fileName += QStringLiteral(".json");
		}

		bool configChangedPrevious = m_configChanged;

		// write current configuration to output file
		Configuration::JsonStore( Configuration::JsonStore::System, fileName ).flush( &VeyonCore::config() );

		m_configChanged = configChangedPrevious;
		ui->buttonBox->setEnabled( m_configChanged );
	}
}



void MainWindow::resetConfiguration()
{
	if( QMessageBox::warning( this, tr( "Reset configuration" ),
							  tr( "Do you really want to reset the local configuration and revert "
								  "all settings to their defaults?" ), QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) ==
		QMessageBox::Yes )
	{
		ConfigurationManager().clearConfiguration();
		reset( false );
	}
}



void MainWindow::aboutVeyon()
{
	AboutDialog( this ).exec();
}



void MainWindow::updateSizes()
{
	if(!ui->configPages->currentWidget()) return;
	ui->configPages->setMinimumSize(0,ui->configPages->currentWidget()->minimumSizeHint().height());
}



void MainWindow::updateView()
{
	for(int i=0;i<ui->configPages->count();++i) NetLinkUi::setEasyPage(ui->configPages->widget(i),false);
	for(int i=0;i<ui->pageSelector->count();++i) ui->pageSelector->setRowHidden(i,false);
	if( ui->viewModeAdvanced->isChecked() )
	{
		switchToAdvancedView();
	}
	else
	{
		switchToStandardView();
	}
	const bool easy=m_viewModeEasy && m_viewModeEasy->isChecked();
	for(int i=0;i<ui->pageSelector->count();++i) {
		auto page=ui->configPages->widget(i);
		if(easy) {
			ui->pageSelector->setRowHidden(i,!NetLinkUi::easyPage(QString::fromLatin1(page->metaObject()->className())));
			NetLinkUi::setEasyPage(page,true);
		}
	}
	if(ui->pageSelector->isRowHidden(ui->pageSelector->currentRow())) ui->pageSelector->setCurrentRow(0);
	if(m_viewModeSelector) {
		const QSignalBlocker blocker(m_viewModeSelector);
		m_viewModeSelector->setCurrentIndex(easy?0:ui->viewModeAdvanced->isChecked()?2:1);
		QSettings settings(QSettings::NativeFormat,QSettings::UserScope,QStringLiteral("Veyon"),QStringLiteral("NetLinkAppearance"));
		const auto mode=easy?QStringLiteral("Easy"):ui->viewModeAdvanced->isChecked()?QStringLiteral("Advanced"):QStringLiteral("Standard");
		if(settings.value(QStringLiteral("ConfiguratorView")).toString()!=mode) settings.setValue(QStringLiteral("ConfiguratorView"),mode);
	}

	QTimer::singleShot( 0, this, &MainWindow::updateSizes );
}



void MainWindow::switchToStandardView()
{
	const auto widgets = findChildren<QWidget *>();

	// hide widgets with advanced property flag
	for( auto widget : widgets )
	{
		const auto flags = Configuration::UiMapping::flags( widget );
		if( flags & Configuration::Property::Flag::Hidden )
		{
			widget->hide();
		}
		else if( flags & Configuration::Property::Flag::Standard )
		{
			widget->show();
		}
		else if( flags & Configuration::Property::Flag::Advanced )
		{
			widget->hide();
		}
	}

	// hide page selector items for advanced pages
	for( int i = 0; i < ui->pageSelector->count(); ++i )
	{
		auto item = ui->pageSelector->item( i );
		auto page = item->data( Qt::UserRole ).value<ConfigurationPage *>();
		if( page )
		{
			const auto flags = Configuration::UiMapping::flags( page );

			ui->pageSelector->setRowHidden( i, flags & Configuration::Property::Flag::Advanced );
			ui->configPages->widget( i )->setVisible( i == ui->pageSelector->currentRow() );
			if( flags & Configuration::Property::Flag::Advanced &&
				i == ui->pageSelector->currentRow() )
			{
				ui->pageSelector->setCurrentRow( 0 );
			}
		}
	}
}



void MainWindow::switchToAdvancedView()
{
	const auto widgets = findChildren<QWidget *>();
	for( auto widget : widgets )
	{
		const auto flags = Configuration::UiMapping::flags( widget );
		if( flags & Configuration::Property::Flag::Advanced )
		{
			widget->show();
		}
	}

	for( int i = 0; i < ui->pageSelector->count(); ++i )
	{
		ui->pageSelector->setRowHidden( i, false );
		ui->configPages->widget( i )->setVisible( i == ui->pageSelector->currentRow() );
	}
}



bool MainWindow::applyConfiguration()
{
	ConfigurationManager configurationManager;

	if( configurationManager.saveConfiguration() == false ||
		configurationManager.applyConfiguration() == false )
	{
		vCritical() << configurationManager.errorString().toUtf8().constData();

		QMessageBox::critical( nullptr,
							   tr( "%1 Configurator" ).arg( VeyonCore::applicationName() ),
							   configurationManager.errorString() );
		return false;
	}

	return true;
}



void MainWindow::loadConfigurationPagePlugins()
{
	for( auto pluginObject : qAsConst( VeyonCore::pluginManager().pluginObjects() ) )
	{
		auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );
		auto configurationPagePluginInterface = qobject_cast<ConfigurationPagePluginInterface *>( pluginObject );

		if( pluginInterface && configurationPagePluginInterface )
		{
			auto page = configurationPagePluginInterface->createConfigurationPage();
			if( page )
			{
				ui->configPages->addWidget( page );

				auto item = new QListWidgetItem( page->windowIcon(), page->windowTitle() );
				item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
				item->setData( Qt::UserRole, QVariant::fromValue( page ) );
				ui->pageSelector->addItem( item );
			}
		}
	}

	// adjust minimum size
	ui->pageSelector->setMinimumSize( ui->pageSelector->sizeHintForColumn(0) + 3 * ui->pageSelector->spacing(),
									  ui->pageSelector->minimumHeight() );
}



void MainWindow::closeEvent( QCloseEvent *closeEvent )
{
	if( m_configChanged &&
		QMessageBox::question( this, tr( "Unsaved settings" ),
							   tr( "There are unsaved settings. "
								   "Quit anyway?" ),
							   QMessageBox::Yes | QMessageBox::No ) !=
		QMessageBox::Yes )
	{
		closeEvent->ignore();
		return;
	}

	closeEvent->accept();
	QMainWindow::closeEvent( closeEvent );
}
