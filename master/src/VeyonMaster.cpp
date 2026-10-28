/*
 * VeyonMaster.cpp - management of application-global instances
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

#include <QHostAddress>

#include "VeyonMaster.h"
#include "ClassProfiles.h"
#include "BuiltinFeatures.h"
#include "SchoolFeatures.h"
#include "ComputerControlListModel.h"
#include "ComputerMonitoringModel.h"
#include "FeatureManager.h"
#include "VncConnection.h"
#include "VeyonConfiguration.h"
#include "VeyonConnection.h"
#include "MainWindow.h"
#include "ComputerManager.h"
#include "ComputerSelectPanel.h"
#include "MonitoringMode.h"
#include "UserConfig.h"
#include "PluginManager.h"
#include "SelectedFeatureControl.h"
#include "NetLinkUi.h"
#include <QMessageBox>
#include <QInputDialog>


VeyonMaster::VeyonMaster( QObject* parent ) :
	VeyonMasterInterface( parent ),
	m_userConfig( new UserConfig( Configuration::Store::JsonFile ) ),
	m_features( featureList() ),
	m_computerManager( new ComputerManager( *m_userConfig, this ) ),
	m_computerControlListModel( new ComputerControlListModel( this, this ) ),
	m_computerMonitoringModel( new ComputerMonitoringModel( this ) ),
	m_localSessionControlInterface( Computer( NetworkObject::Uid::createUuid(),
											  QStringLiteral("localhost"),
											  QStringLiteral("%1").
											  arg( QHostAddress( QHostAddress::LocalHost ).toString() ) ),
									VeyonCore::config().veyonServerPort() + VeyonCore::sessionId(),
									this ),
	m_mainWindow( nullptr ),
	m_currentMode( VeyonCore::builtinFeatures().monitoringMode().feature().uid() )
{
	if( VeyonCore::config().enforceSelectedModeForClients() )
	{
		connect( m_computerControlListModel, &ComputerControlListModel::activeFeaturesChanged,
				 this, &VeyonMaster::enforceDesignatedMode );
	}

	m_localSessionControlInterface.start({}, ComputerControlInterface::UpdateMode::Disabled);

	// attach computer list model to proxy model
	m_computerMonitoringModel->setSourceModel( m_computerControlListModel );
	m_computerMonitoringModel->setSortRole( Qt::InitialSortOrderRole );
	m_computerMonitoringModel->setStateRole( ComputerControlListModel::StateRole );
	m_computerMonitoringModel->sort( 0 );

	m_mainWindow = new MainWindow( *this );
}



VeyonMaster::~VeyonMaster()
{
	shutdown();

	delete m_mainWindow;

	delete m_computerManager;

	m_userConfig->flushStore();
	delete m_userConfig;
}



FeatureList VeyonMaster::subFeatures( Feature::Uid parentFeatureUid ) const
{
	const auto disabledFeatures = VeyonCore::config().disabledFeatures();
	if( disabledFeatures.contains( parentFeatureUid.toString() ) )
	{
		return {};
	}

	const auto& relatedFeatures = VeyonCore::featureManager().relatedFeatures( parentFeatureUid );

	FeatureList subFeatures;
	subFeatures.reserve( relatedFeatures.size() );

	for( const auto& relatedFeature : relatedFeatures )
	{
		if( relatedFeature.testFlag( Feature::Flag::Master ) &&
			relatedFeature.parentUid() == parentFeatureUid &&
			disabledFeatures.contains( relatedFeature.uid().toString() ) == false )
		{
			subFeatures += relatedFeature;
		}
	}

	return subFeatures;
}



FeatureList VeyonMaster::allFeatures() const
{
	FeatureList featureList;

	for( const auto& feature : qAsConst( features() ) )
	{
		featureList.append(feature); // clazy:exclude=reserve-candidates
		const auto modeSubFeatures = subFeatures( feature.uid() );
		for( const auto& subFeature : modeSubFeatures )
		{
			featureList.append( subFeature );
		}
	}

	return featureList;
}



QWidget* VeyonMaster::mainWindow()
{
	return m_mainWindow;
}



Configuration::Object* VeyonMaster::userConfigurationObject()
{
	return m_userConfig;
}



void VeyonMaster::reloadSubFeatures()
{
	m_mainWindow->reloadSubFeatures();
}



const ComputerControlInterfaceList& VeyonMaster::allComputerControlInterfaces() const
{
	return m_computerControlListModel->computerControlInterfaces();
}



ComputerControlInterfaceList VeyonMaster::selectedComputerControlInterfaces() const
{
	return m_mainWindow->selectedComputerControlInterfaces();
}



ComputerControlInterfaceList VeyonMaster::filteredComputerControlInterfaces() const
{
	const auto rowCount = m_computerMonitoringModel->rowCount();

	ComputerControlInterfaceList computerControlInterfaces;
	computerControlInterfaces.reserve( rowCount );

	for( int i = 0; i < rowCount; ++i )
	{
		const auto index = m_computerMonitoringModel->index( i, 0 );
		const auto sourceIndex = m_computerMonitoringModel->mapToSource( index );
		computerControlInterfaces.append( m_computerControlListModel->computerControlInterface( sourceIndex ) );
	}

	return computerControlInterfaces;
}



QUuid VeyonMaster::highlightedLocationUid() const
{
	if(!m_mainWindow) return {};
	const auto panel=m_mainWindow->findChild<ComputerSelectPanel*>();
	return panel?panel->highlightedProfileGroup():QUuid{};
}

void VeyonMaster::showClassProfiles(const QString& operation)
{
	if (!m_classProfiles)
	{
		m_classProfiles = new ClassProfiles(*this, this);
		m_classProfiles->setDirectoryContext(m_computerManager->networkObjectModel(),
			qobject_cast<CheckableItemProxyModel*>(m_computerManager->computerTreeModel()),
			[this] {return highlightedLocationUid();},[this] {
				if(VeyonCore::builtinFeatures().schoolFeatures().activeInternetPolicies()>0) return false;
				const auto monitoring=VeyonCore::builtinFeatures().monitoringMode().feature().uid();
				for(const auto& ci:allComputerControlInterfaces()) if(ci) {
					if(!ci->designatedModeFeature().isNull() && ci->designatedModeFeature()!=monitoring) return false;
					for(const auto& feature:allFeatures()) if(SelectedFeatureControl::isMode(feature) && ci->activeFeatures().contains(feature.uid())) return false;
					if(ci->property("schoolPolicyVerifiedState").toString()==QStringLiteral("blocked")) return false;
				}
				return true;
			});
		connect(m_classProfiles, &ClassProfiles::assignmentsChanged, this, [this] {
			QHash<QUuid, QString> names;
			for (const auto& seat : m_classProfiles->profile().assignments) names.insert(seat.computerId, seat.student);
			m_computerManager->setClassProfileNames(names);
			for (const auto& ci : allComputerControlInterfaces()) if (ci) m_computerManager->updateUser(ci);
		});
	}
	m_classProfiles->show(operation);
}

void VeyonMaster::runFeature( const Feature& feature )
{
	const auto computerControlInterfaces = SelectedFeatureControl::applies(feature) ?
		SelectedFeatureControl::targets(selectedComputerControlInterfaces()) : filteredComputerControlInterfaces();
	if(SelectedFeatureControl::applies(feature) && computerControlInterfaces.isEmpty()) {
		auto notice=new QMessageBox(QMessageBox::Information,feature.displayName(),
			NetLinkUi::text("Select at least one computer thumbnail before sending this command."),QMessageBox::Ok,mainWindow());
		notice->setAttribute(Qt::WA_DeleteOnClose);notice->open();return;
	}
	if(SelectedFeatureControl::isMode(feature)) {
		auto& manager=VeyonCore::featureManager();
		FeatureUidList family;
		for(const auto& related:manager.relatedFeatures(feature.uid())) family.append(related.uid());
		if(!family.contains(feature.uid())) family.append(feature.uid());
		auto plan=SelectedFeatureControl::plan(feature,computerControlInterfaces,family,allFeatures());
		if(plan.start && (feature.name()==QStringLiteral("ShareUserScreenWindow") || feature.name()==QStringLiteral("ShareUserScreenFullScreen"))) {
			if(plan.clients.size()<2) {
				auto notice=new QMessageBox(QMessageBox::Information,feature.displayName(),
					NetLinkUi::text("Select the source computer and at least one recipient thumbnail before sharing a student's screen."),QMessageBox::Ok,mainWindow());
				notice->setAttribute(Qt::WA_DeleteOnClose);notice->open();return;
			}
			QStringList choices;
			for(int i=0;i<plan.clients.size();++i) choices.append(QStringLiteral("%1. %2 (%3)").arg(i+1)
				.arg(plan.clients[i]->computer().name(),plan.clients[i]->computer().hostAddress()));
			bool accepted=false;
			const auto source=QInputDialog::getItem(mainWindow(),feature.displayName(),NetLinkUi::text("Choose the source screen. Only the other selected computers will receive it."),choices,0,false,&accepted);
			// Cancelling does not stop an existing mode. Reject a changed selection
			// rather than sending commands to a stale group after the dialog.
			if(!accepted || computerControlInterfaces!=SelectedFeatureControl::targets(selectedComputerControlInterfaces())) return;
			const auto index=choices.indexOf(source);if(index<0) return;
			plan.clients.prepend(plan.clients.takeAt(index));
		}
		if(plan.start && (feature.name().startsWith(QStringLiteral("ShareOwnScreen")) || feature.name().startsWith(QStringLiteral("ShareUserScreen")))) {
			const auto origin=feature.name().startsWith(QStringLiteral("ShareOwnScreen")) ? QStringLiteral("local") : plan.clients.first()->computer().networkObjectUid().toString();
			if(SelectedFeatureControl::conflictingDemoSource(allComputerControlInterfaces(),plan.clients,origin)) {
				auto notice=new QMessageBox(QMessageBox::Information,feature.displayName(),
					NetLinkUi::text("Other computers are receiving a different demonstration. Stop that demonstration before changing its source; no commands were sent."),QMessageBox::Ok,mainWindow());
				notice->setAttribute(Qt::WA_DeleteOnClose);notice->open();return;
			}
		}
		for(const auto& mode:plan.stop) manager.stopFeature(*this,mode,plan.clients);
		const auto& next=plan.start ? feature : VeyonCore::builtinFeatures().monitoringMode().feature();
		manager.startFeature(*this,next,plan.clients);
		m_currentMode=next.uid();
		return;
	}

	if( feature.testFlag( Feature::Flag::Mode ) )
	{
		stopAllFeatures( computerControlInterfaces );

		if( m_currentMode == feature.uid() ||
			subFeatures( feature.uid() ).contains( VeyonCore::featureManager().feature( m_currentMode ) ) )
		{
			const Feature& monitoringModeFeature = VeyonCore::builtinFeatures().monitoringMode().feature();

			VeyonCore::featureManager().startFeature( *this, monitoringModeFeature, computerControlInterfaces );
			m_currentMode = monitoringModeFeature.uid();
		}
		else
		{
			VeyonCore::featureManager().startFeature( *this, feature, computerControlInterfaces );
			m_currentMode = feature.uid();
		}
	}
	else
	{
		VeyonCore::featureManager().startFeature( *this, feature, computerControlInterfaces );
	}
}



void VeyonMaster::enforceDesignatedMode( const QModelIndex& index )
{
	auto controlInterface = m_computerControlListModel->computerControlInterface( index );
	if( controlInterface )
	{
		const auto designatedModeFeature = controlInterface->designatedModeFeature();

		if( designatedModeFeature.isNull() == false &&
			designatedModeFeature != VeyonCore::builtinFeatures().monitoringMode().feature().uid() &&
			controlInterface->activeFeatures().contains( designatedModeFeature ) == false &&
			controlInterface->activeFeatures().contains( VeyonCore::featureManager().metaFeatureUid(designatedModeFeature) ) == false )
		{
			VeyonCore::featureManager().startFeature( *this, VeyonCore::featureManager().feature(designatedModeFeature),
													  { controlInterface } );
		}
	}
}



void VeyonMaster::stopAllFeatures( const ComputerControlInterfaceList& computerControlInterfaces )
{
	const auto features = allFeatures();

	for( const auto& feature : features )
	{
		VeyonCore::featureManager().stopFeature( *this, feature, computerControlInterfaces );
	}
}



void VeyonMaster::shutdown()
{
	// Do not let the periodic lease renewal race with shutdown releases.
	VeyonCore::builtinFeatures().schoolFeatures().stopPolicyRenewals();
	stopAllFeatures( m_computerControlListModel->computerControlInterfaces() );

	const auto allMessageQueuesEmpty = [&]() {
		for( const auto& computerControlInterface : m_computerControlListModel->computerControlInterfaces() )
		{
			if( computerControlInterface->state() == ComputerControlInterface::State::Connected &&
				computerControlInterface->isMessageQueueEmpty() == false )
			{
				return false;
			}
		}

		return true;
	};

	// Offline clients cannot acknowledge outgoing messages. Never stall the UI
	// for a minute; InternetGuard's native 90-second lease is the fallback.
	static constexpr auto MessageQueueWaitTimeout = 2000;
	static constexpr auto MessageQueuePollInterval = 10;

	QElapsedTimer messageQueueWaitTimer;
	messageQueueWaitTimer.start();

	while( allMessageQueuesEmpty() == false &&
		   messageQueueWaitTimer.elapsed() < MessageQueueWaitTimeout )
	{
		QThread::msleep(MessageQueuePollInterval);
	}

	vDebug() << "finished in" << messageQueueWaitTimer.elapsed() << "ms";
}



FeatureList VeyonMaster::featureList() const
{
	FeatureList features;

	const auto disabledFeatures = VeyonCore::config().disabledFeatures();
	auto pluginUids = VeyonCore::pluginManager().pluginUids();

	std::sort( pluginUids.begin(), pluginUids.end() );

	const auto addFeatures = [&]( const std::function<bool(const Feature&)>& extraFilter )
	{
		for(const auto& pluginUid : qAsConst(pluginUids))
		{
			for( const auto& feature : VeyonCore::featureManager().features( pluginUid ) )
			{
				if( feature.testFlag( Feature::Flag::Master ) &&
					feature.testFlag( Feature::Flag::Meta ) == false &&
					feature.parentUid().isNull() &&
					disabledFeatures.contains( feature.uid().toString() ) == false &&
					extraFilter( feature ) )
				{
					features += feature;
				}
			}
		}
	};

	addFeatures( []( const Feature& feature ) { return feature.testFlag( Feature::Flag::Mode ); } );
	addFeatures( []( const Feature& feature ) { return feature.testFlag( Feature::Flag::Mode ) == false; } );

	return features;
}
