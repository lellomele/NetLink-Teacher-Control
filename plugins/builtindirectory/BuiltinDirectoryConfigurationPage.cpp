/*
 * BuiltinDirectoryConfigurationPage.cpp - implementation of BuiltinDirectoryConfigurationPage
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

#include <QJsonObject>
#include <QHeaderView>
#include <QMessageBox>
#include <QSignalBlocker>
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"

#include "BuiltinDirectoryConfiguration.h"
#include "BuiltinDirectoryConfigurationPage.h"
#include "Configuration/UiMapping.h"
#include "NetworkObjectModel.h"
#include "ObjectManager.h"

#include "ui_BuiltinDirectoryConfigurationPage.h"

BuiltinDirectoryConfigurationPage::BuiltinDirectoryConfigurationPage( BuiltinDirectoryConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui(new Ui::BuiltinDirectoryConfigurationPage),
	m_configuration( configuration )
{
	ui->setupUi(this);
	// Keep explicit actions above the tables, including in a non-maximized window.
	auto grid=ui->gridLayout_2;
	grid->removeItem(ui->horizontalLayout);grid->removeItem(ui->horizontalLayout_2);
	grid->removeWidget(ui->computerTableWidget);grid->removeWidget(ui->locationTableWidget);
	grid->addLayout(ui->horizontalLayout_2,1,0);grid->addLayout(ui->horizontalLayout,1,1);
	grid->addWidget(ui->locationTableWidget,2,0);grid->addWidget(ui->computerTableWidget,2,1);
	grid->setRowStretch(1,0);grid->setRowStretch(2,1);grid->setColumnStretch(0,1);grid->setColumnStretch(1,3);
	ui->label->setText(NetLinkUi::text("Groups / classrooms"));ui->label_2->setText(NetLinkUi::text("Computers in selected group"));
	ui->groupBox->setTitle(NetLinkUi::text("Groups and computers"));
	ui->addLocationButton->setText(NetLinkUi::text("Add"));ui->removeLocationButton->setText(NetLinkUi::text("Remove"));
	ui->addComputerButton->setText(NetLinkUi::text("Add computer"));ui->removeComputerButton->setText(NetLinkUi::text("Remove computer"));
	ui->addLocationButton->setToolTip(NetLinkUi::text("Adds a group/classroom. Double-click its cell to edit the name."));
	ui->removeLocationButton->setToolTip(NetLinkUi::text("Removes the selected group and its computers after confirmation. Does not uninstall NetLink from computers."));
	ui->addComputerButton->setToolTip(NetLinkUi::text("Adds a row to the selected group. Enter the client's name and IP/hostname. A MAC is needed only for Wake-on-LAN."));
	ui->removeComputerButton->setToolTip(NetLinkUi::text("Removes the selected computer row from configuration, not an individual cell. Does not change the remote computer."));
	ui->label_3->setText(NetLinkUi::text("Select a group on the left. Double-click a cell to edit. Name and IP/hostname identify a computer. MAC is optional, for Wake-on-LAN. Use Apply to save."));
	auto helpFont=font();helpFont.setItalic(false);ui->label_3->setFont(helpFont);
	ui->computerTableWidget->setHorizontalHeaderLabels({NetLinkUi::text("Computer name"),NetLinkUi::text("IP / hostname"),NetLinkUi::text("MAC (optional)")});
	for(auto table:{ui->locationTableWidget,ui->computerTableWidget}) {
		table->setAlternatingRowColors(true);table->setSelectionBehavior(QAbstractItemView::SelectRows);
		table->setSelectionMode(QAbstractItemView::SingleSelection);table->setMinimumSize(120,120);
		table->horizontalHeader()->setMinimumSectionSize(75);
	}
	ui->computerTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui->computerTableWidget->setColumnWidth(0,135);ui->computerTableWidget->setColumnWidth(1,155);
	ui->computerTableWidget->setColumnWidth(2,145);
	const auto refreshIcons=[this]{const auto p=NetLinkAppearance::install()->preferences();
		for(auto button:{ui->addLocationButton,ui->addComputerButton}) button->setIcon(NetLinkUi::icon(QStringLiteral("add"),p.iconSet,p.accent));
		for(auto button:{ui->removeLocationButton,ui->removeComputerButton}) button->setIcon(NetLinkUi::icon(QStringLiteral("remove"),p.iconSet,p.accent));
	};
	refreshIcons();connect(NetLinkAppearance::install(),&NetLinkAppearance::changed,this,refreshIcons);

	populateLocations();

	connect( ui->locationTableWidget, &QTableWidget::currentItemChanged,
			 this, &BuiltinDirectoryConfigurationPage::populateComputers );
	const auto selectionChanged=[this]{ui->removeLocationButton->setEnabled(!currentLocationObject().uid().isNull());
		ui->addComputerButton->setEnabled(!currentLocationObject().uid().isNull());
		ui->removeComputerButton->setEnabled(!currentComputerObject().uid().isNull());};
	connect(ui->locationTableWidget,&QTableWidget::itemSelectionChanged,this,selectionChanged);
	connect(ui->computerTableWidget,&QTableWidget::itemSelectionChanged,this,selectionChanged);
	selectionChanged();
}



BuiltinDirectoryConfigurationPage::~BuiltinDirectoryConfigurationPage()
{
	delete ui;
}



void BuiltinDirectoryConfigurationPage::resetWidgets()
{
	populateLocations();

	ui->locationTableWidget->setCurrentCell( 0, 0 );
}



void BuiltinDirectoryConfigurationPage::connectWidgetsToProperties()
{
}



void BuiltinDirectoryConfigurationPage::applyConfiguration()
{
}



void BuiltinDirectoryConfigurationPage::addLocation()
{
	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.add( NetworkObject( NetworkObject::Type::Location, tr( "New location" ),
									  {}, {}, {}, QUuid::createUuid() ) );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();

	ui->locationTableWidget->setCurrentCell( ui->locationTableWidget->rowCount()-1, 0 );
}



void BuiltinDirectoryConfigurationPage::updateLocation()
{
	auto currentLocationIndex = ui->locationTableWidget->currentIndex();
	if( currentLocationIndex.isValid() == false )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.update( currentLocationObject() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	// The edited item is already current. Rebuilding while cellChanged is being
	// delivered can recurse and invalidate the selection/editor.
}



void BuiltinDirectoryConfigurationPage::removeLocation()
{
	const auto uid=currentLocationObject().uid();
	if(uid.isNull()) return;
	if(QMessageBox::question(this,NetLinkUi::text("Remove group?"),NetLinkUi::text("The selected group and all its computers will be removed from configuration. Programs on those computers are not uninstalled. Continue?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.remove( uid, true );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();
}



void BuiltinDirectoryConfigurationPage::addComputer()
{
	auto currentLocationUid = currentLocationObject().uid();
	if( currentLocationUid.isNull() )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.add( NetworkObject( NetworkObject::Type::Host, tr( "New computer" ),
									  {}, {}, {},
									  QUuid::createUuid(),
									  currentLocationUid ) );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();

	ui->computerTableWidget->setCurrentCell( ui->computerTableWidget->rowCount()-1, 0 );
}



void BuiltinDirectoryConfigurationPage::updateComputer()
{
	auto currentComputerIndex = ui->computerTableWidget->currentIndex();
	if( currentComputerIndex.isValid() == false )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.update( currentComputerObject() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	// Do not rebuild the table inside cellChanged.
}



void BuiltinDirectoryConfigurationPage::removeComputer()
{
	if(currentComputerObject().uid().isNull()) return;
	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.remove( currentComputerObject().uid() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();
}



void BuiltinDirectoryConfigurationPage::populateLocations()
{
	const auto previousUid=currentLocationObject().uid();
	const QSignalBlocker blocker(ui->locationTableWidget);
	ui->locationTableWidget->setUpdatesEnabled( false );
	ui->locationTableWidget->setRowCount(0);

	int rowCount = 0;

	const auto networkObjects = m_configuration.networkObjects();
	for( const auto& networkObjectValue : networkObjects )
	{
		const NetworkObject networkObject( networkObjectValue.toObject() );
		if( networkObject.type() == NetworkObject::Type::Location )
		{
			auto item = new QTableWidgetItem( networkObject.name() );
			item->setData( NetworkObjectModel::UidRole, networkObject.uid() );
			ui->locationTableWidget->setRowCount( ++rowCount );
			ui->locationTableWidget->setItem( rowCount-1, 0, item );
		}
	}

	ui->locationTableWidget->setUpdatesEnabled( true );
	int selected=0;
	for(int row=0;row<rowCount;++row) if(ui->locationTableWidget->item(row,0)->data(NetworkObjectModel::UidRole).toUuid()==previousUid) selected=row;
	if(rowCount>0) ui->locationTableWidget->setCurrentCell(selected,0);
	populateComputers();
	ui->removeLocationButton->setEnabled(rowCount>0);ui->addComputerButton->setEnabled(rowCount>0);
}



void BuiltinDirectoryConfigurationPage::populateComputers()
{
	const QSignalBlocker blocker(ui->computerTableWidget);
	auto parentUid = currentLocationObject().uid();

	ui->computerTableWidget->setUpdatesEnabled( false );
	ui->computerTableWidget->setRowCount( 0 );

	int rowCount = 0;

	const auto networkObjects = m_configuration.networkObjects();
	for( const auto& networkObjectValue : networkObjects )
	{
		const NetworkObject networkObject( networkObjectValue.toObject() );

		if( networkObject.type() == NetworkObject::Type::Host &&
			networkObject.parentUid() == parentUid )
		{
			auto nameItem = new QTableWidgetItem( networkObject.name() );
			nameItem->setData( NetworkObjectModel::UidRole, networkObject.uid() );
			nameItem->setData( NetworkObjectModel::ParentUidRole, networkObject.parentUid() );

			ui->computerTableWidget->setRowCount( rowCount+1 );
			ui->computerTableWidget->setItem( rowCount, 0, nameItem );
			ui->computerTableWidget->setItem( rowCount, 1, new QTableWidgetItem( networkObject.hostAddress() ) );
			ui->computerTableWidget->setItem( rowCount, 2, new QTableWidgetItem( networkObject.macAddress() ) );
			++rowCount;
		}
	}

	ui->computerTableWidget->setUpdatesEnabled( true );
	ui->removeComputerButton->setEnabled(false);
}



NetworkObject BuiltinDirectoryConfigurationPage::currentLocationObject() const
{
	const auto selectedLocation = ui->locationTableWidget->currentItem();
	if( selectedLocation )
	{
		return NetworkObject( NetworkObject::Type::Location,
							  selectedLocation->text(),
							  {},
							  {},
							  {},
							  selectedLocation->data( NetworkObjectModel::UidRole ).toUuid(),
							  selectedLocation->data( NetworkObjectModel::ParentUidRole ).toUuid() );
	}

	return NetworkObject();
}



NetworkObject BuiltinDirectoryConfigurationPage::currentComputerObject() const
{
	const int row = ui->computerTableWidget->currentRow();
	if( row >= 0 )
	{
		auto nameItem = ui->computerTableWidget->item( row, 0 );
		auto hostAddressItem = ui->computerTableWidget->item( row, 1 );
		auto macAddressItem = ui->computerTableWidget->item( row, 2 );
		if(!nameItem || !hostAddressItem || !macAddressItem) return NetworkObject();

		return NetworkObject( NetworkObject::Type::Host,
							  nameItem->text(),
							  hostAddressItem->text().trimmed(),
							  macAddressItem->text().trimmed(),
							  {},
							  nameItem->data( NetworkObjectModel::UidRole ).toUuid(),
							  nameItem->data( NetworkObjectModel::ParentUidRole ).toUuid() );
	}

	return NetworkObject();
}
