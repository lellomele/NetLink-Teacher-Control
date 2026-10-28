/*
 * ComputerSelectPanel.cpp - provides a view for a network object tree
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

#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>

#include "ComputerSelectPanel.h"
#include "ComputerManager.h"
#include "NetworkObjectModel.h"
#include "RecursiveFilterProxyModel.h"
#include "LocationDialog.h"
#include "VeyonConfiguration.h"
#include "NetLinkUi.h"
#include "ClassProfileDirectory.h"
#include "NetLinkIdentity.h"
#include "PlatformCoreFunctions.h"
#include <QMenu>

#include "ui_ComputerSelectPanel.h"


ComputerSelectPanel::ComputerSelectPanel( ComputerManager& computerManager, QWidget *parent ) :
	QWidget(parent),
	ui(new Ui::ComputerSelectPanel),
	m_computerManager( computerManager ),
	m_filterProxyModel( new RecursiveFilterProxyModel( this ) )
{
	m_filterProxyModel->setSourceModel( computerManager.computerTreeModel() );
	m_filterProxyModel->setFilterCaseSensitivity( Qt::CaseInsensitive );
	m_filterProxyModel->setFilterKeyColumn( -1 ); // filter all columns instead of first one only
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	m_filterProxyModel->setRecursiveFilteringEnabled(true);
#endif

	ui->setupUi(this);
	ui->profilesButton->setText(NetLinkUi::text("Class–room profiles…"));
	ui->loadProfileButton->setText(NetLinkUi::text("Load JSON…"));
	ui->saveProfileButton->setText(NetLinkUi::text("Save JSON…"));
	ui->saveListButton->setText(NetLinkUi::text("Export CSV…"));
	ui->saveProfileButton->setToolTip(NetLinkUi::text("Saves every configured PC in the highlighted row's group, including unchecked PCs. Other groups are not saved."));
	ui->saveListButton->setToolTip(ui->saveProfileButton->toolTip());
	ui->loadProfileButton->setToolTip(NetLinkUi::text("Loads names and checks the saved PCs. Existing groups, computers, addresses and keys are never replaced."));
	ui->profilesButton->setToolTip(NetLinkUi::text("Edit the student assigned to each computer. Profiles do not change computer names, addresses, keys or permissions."));
	connect(ui->profilesButton, &QPushButton::clicked, this, [this] {Q_EMIT classProfileRequested(QString{});});
	connect(ui->loadProfileButton, &QPushButton::clicked, this, [this] {Q_EMIT classProfileRequested(QStringLiteral("load"));});
	connect(ui->saveProfileButton, &QPushButton::clicked, this, [this] {Q_EMIT classProfileRequested(QStringLiteral("save"));});
	connect(ui->saveListButton, &QPushButton::clicked, this, [this] {Q_EMIT classProfileRequested(QStringLiteral("csv"));});

	// capture keyboard events for tree view
	ui->treeView->installEventFilter( this );

	// set computer tree model as data model
	ui->treeView->setModel( m_filterProxyModel );
	ui->treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->treeView,&QTreeView::customContextMenuRequested,this,[this](const QPoint& point){
		QMenu menu(this);auto manage=menu.addAction(NetLinkUi::text("Manage groups and move computers…"));
		if(menu.exec(ui->treeView->viewport()->mapToGlobal(point))!=manage) return;
		if(QMessageBox::question(this,NetLinkUi::text("Open Configurator"),NetLinkUi::text("Open Configurator as administrator to manage groups and move PCs. End Lock, Demo and InternetGuard before applying changes; restart the Console afterwards. No client key will be changed."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)
			if(!VeyonCore::platform().coreFunctions().runProgramAsAdmin(NetLinkIdentity::executablePath(NetLinkIdentity::Application::Configurator),{}))
				QMessageBox::warning(this,NetLinkUi::text("Open Configurator"),NetLinkUi::text("Configurator could not be opened or administrator approval was cancelled."));
	});

	// set default sort order
	ui->treeView->sortByColumn( 0, Qt::AscendingOrder );

	ui->addLocationButton->setVisible( VeyonCore::config().showCurrentLocationOnly() &&
									   VeyonCore::config().allowAddingHiddenLocations() );

	ui->filterLineEdit->setHidden( VeyonCore::config().hideComputerFilter() );

	connect( ui->filterLineEdit, &QLineEdit::textChanged,
			 this, &ComputerSelectPanel::updateFilter );
}



ComputerSelectPanel::~ComputerSelectPanel()
{
	delete ui;
}

QUuid ComputerSelectPanel::highlightedProfileGroup() const
{
	const auto selection=ui->treeView->selectionModel();
	// Current index alone can survive a cleared selection: require a highlighted row.
	if(!selection || !selection->hasSelection() || !selection->isRowSelected(selection->currentIndex().row(),selection->currentIndex().parent())) return {};
	return ClassProfileDirectory::highlightedGroup(selection->currentIndex());
}



bool ComputerSelectPanel::eventFilter( QObject *watched, QEvent *event )
{
	if( watched == ui->treeView &&
		event->type() == QEvent::KeyPress &&
		dynamic_cast<QKeyEvent*>(event) != nullptr &&
		dynamic_cast<QKeyEvent*>(event)->key() == Qt::Key_Delete )
	{
		removeLocation();
		return true;
	}

	return QWidget::eventFilter( watched, event );
}



void ComputerSelectPanel::addLocation()
{
	LocationDialog dialog( m_computerManager.networkObjectModel(), this );
	if( dialog.exec() && dialog.selectedLocation().isEmpty() == false )
	{
		m_computerManager.addLocation( dialog.selectedLocation() );
	}
}



void ComputerSelectPanel::removeLocation()
{
	auto model = m_computerManager.computerTreeModel();
	const auto index = ui->treeView->selectionModel()->currentIndex();

	if( index.isValid() &&
		QVariantHelper<NetworkObject::Type>::value( model->data( index, NetworkObjectModel::TypeRole ) ) == NetworkObject::Type::Location )
	{
		m_computerManager.removeLocation( model->data( index, NetworkObjectModel::NameRole ).toString() );
	}
}



void ComputerSelectPanel::saveList()
{
	QString fileName = QFileDialog::getSaveFileName( this, tr( "Select output filename" ),
													 QDir::homePath(), tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() == false )
	{
		if( m_computerManager.saveComputerAndUsersList( fileName ) == false )
		{
			QMessageBox::critical( this, tr( "File error"),
								   tr( "Could not write the computer and users list to %1! "
									   "Please check the file access permissions." ).arg( fileName ) );
		}
	}
}



void ComputerSelectPanel::updateFilter()
{
	const auto filter = ui->filterLineEdit->text();
	auto model = ui->treeView->model();

	if( filter.isEmpty() )
	{
		m_filterProxyModel->setFilterWildcard( filter );

		for( int i = 0; i < model->rowCount(); ++i )
		{
			const auto index = model->index( i, 0 );
			ui->treeView->setExpanded( index, m_expandedGroups.contains( index ) );
		}

		m_previousFilter.clear();
	}
	else
	{
		if( m_previousFilter.isEmpty() )
		{
			m_expandedGroups.clear();

			for( int i = 0; i < model->rowCount(); ++i )
			{
				const auto index = model->index( i, 0 );
				if( ui->treeView->isExpanded( index ) )
				{
					m_expandedGroups.append( index );
				}
			}
		}

		m_previousFilter = filter;

		m_filterProxyModel->setFilterWildcard( filter );
		ui->treeView->expandAll();
	}
}
