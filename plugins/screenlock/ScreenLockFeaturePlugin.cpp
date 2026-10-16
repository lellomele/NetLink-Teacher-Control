/*
 * ScreenLockFeaturePlugin.cpp - implementation of ScreenLockFeaturePlugin class
 *
 * Copyright (c) 2017-2022 Tobias Junghans <tobydox@veyon.io>
 * NetLink lock event reporting: Copyright (C) 2026 prof. Ing. Raffaele Mele
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

#include <QCoreApplication>

#include "ScreenLockFeaturePlugin.h"
#include "FeatureWorkerManager.h"
#include "LockWidget.h"
#include "LockActivity.h"
#include "NetLinkUi.h"
#include "VeyonMasterInterface.h"
#include "VeyonWorkerInterface.h"
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QUuid>
#include "PlatformCoreFunctions.h"
#include "PlatformSessionFunctions.h"
#include "VeyonServerInterface.h"


ScreenLockFeaturePlugin::ScreenLockFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_screenLockFeature( QStringLiteral( "ScreenLock" ),
						 Feature::Flag::Mode | Feature::Flag::AllComponents,
						 Feature::Uid( "ccb535a2-1d24-4cc1-a709-8b47d2b2ac79" ),
						 Feature::Uid(),
						 tr( "Lock" ), tr( "Unlock" ),
						 NetLinkUi::text("Applies or removes the software screen cover on selected computers. Ordinary shortcuts are filtered during lock. Windows security screens remain accessible: their exact triggering shortcut cannot be identified. Lock events require updated NetLink on both PCs."),
						 QStringLiteral(":/screenlock/system-lock-screen.png") ),
	m_lockInputDevicesFeature( QStringLiteral( "InputDevicesLock" ),
							   Feature::Flag::Mode | Feature::Flag::AllComponents | Feature::Flag::Meta,
							   Feature::Uid( "e4a77879-e544-4fec-bc18-e534f33b934c" ),
							   {},
							   tr( "Lock input devices" ), tr( "Unlock input devices" ),
							   tr( "To reclaim all user's full attention you can lock "
								   "their computers using this button. "
								   "In this mode all input devices are locked while the desktop is still visible." ),
							   QStringLiteral(":/screenlock/system-lock-screen.png") ),
	m_features( { m_screenLockFeature, m_lockInputDevicesFeature } ),
	m_lockWidget( nullptr )
{
	m_activityTimer.setInterval(2000); // aggregate repeated shortcuts, no modal flood
}



ScreenLockFeaturePlugin::~ScreenLockFeaturePlugin()
{
	delete m_lockWidget;
	delete m_eventsWindow;
}



bool ScreenLockFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											 const QVariantMap& arguments,
											 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( hasFeature( featureUid ) == false )
	{
		return false;
	}

	if( operation == Operation::Start )
	{
		for(const auto& client:computerControlInterfaces) {
			const auto token=QUuid::createUuid().toString(QUuid::WithoutBraces);
			client->setProperty("netlinkLockToken",token);
			client->setProperty("netlinkLockSequence",qulonglong(0));
			client->sendFeatureMessage(FeatureMessage{featureUid,StartLockCommand}.addArgument(100,token));
		}

		return true;
	}

	if( operation == Operation::Stop )
	{
		for(const auto& client:computerControlInterfaces) client->setProperty("netlinkLockToken",QVariant{});
		sendFeatureMessage( FeatureMessage{ featureUid, StopLockCommand }, computerControlInterfaces );

		return true;
	}

	return false;
}

bool ScreenLockFeaturePlugin::startFeature(VeyonMasterInterface& master,const Feature& feature,const ComputerControlInterfaceList& clients)
{
	m_masterWindow=master.mainWindow();
	return controlFeature(feature.uid(),Operation::Start,{},clients);
}

bool ScreenLockFeaturePlugin::handleFeatureMessage(ComputerControlInterface::Pointer client,const FeatureMessage& message)
{
	if(!hasFeature(message.featureUid()) || message.command()!=LockEventsCommand) return false;
	const auto token=client->property("netlinkLockToken").toString();
	const auto sequence=message.argument(1).toULongLong();
	const auto batch=message.argument(2).toMap();
	if(token.isEmpty() || message.argument(0).toString()!=token || sequence<=client->property("netlinkLockSequence").toULongLong() || !LockActivity::validBatch(batch)) return true;
	client->setProperty("netlinkLockSequence",sequence);
	displayActivity(client,batch);
	return true;
}

void ScreenLockFeaturePlugin::displayActivity(ComputerControlInterface::Pointer client,const QVariantMap& batch)
{
	if(!m_masterWindow) return;
	if(!m_eventsWindow) {
		auto dialog=new QDialog(m_masterWindow,Qt::Tool);
		dialog->setObjectName(QStringLiteral("netlinkLockEvents"));dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->setAttribute(Qt::WA_ShowWithoutActivating);
		dialog->setWindowTitle(NetLinkUi::windowTitle(NetLinkUi::text("Lock events")));
		dialog->resize(680,320);auto layout=new QVBoxLayout(dialog);
		auto note=new QLabel(NetLinkUi::text("Live events during NetLink lock only. Exact protected shortcuts cannot be identified. No typed text is collected; disconnected periods are not recorded. Last 200 lines, in memory only."),dialog);
		note->setWordWrap(true);layout->addWidget(note);
		m_eventsText=new QPlainTextEdit(dialog);m_eventsText->setReadOnly(true);m_eventsText->setMaximumBlockCount(200);layout->addWidget(m_eventsText);
		auto buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);layout->addWidget(buttons);
		connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);
		m_eventsWindow=dialog;dialog->show(); // no exec(), raise() or focus stealing
	}
	for(auto i=batch.cbegin();i!=batch.cend();++i) {
		const QString identityRaw=client->computer().name()+QStringLiteral(" [")+client->computer().hostAddress()+QStringLiteral("]");
		const auto identity=identityRaw.simplified().left(240);
		const QString line=QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))+QStringLiteral(" — ")+identity+QStringLiteral(" — ")+LockActivity::description(i.key())+QStringLiteral(" ×%1").arg(i.value().toInt());
		m_eventsText->appendPlainText(line);
		if(auto main=qobject_cast<QMainWindow*>(m_masterWindow.data())) main->statusBar()->showMessage(line,15000);
	}
}

void ScreenLockFeaturePlugin::flushActivity(VeyonWorkerInterface& worker)
{
	const auto batch=m_activity.take();
	if(batch.isEmpty() || m_workerToken.isEmpty()) return;
	if(!worker.sendFeatureMessageReply(FeatureMessage(m_workerFeature,LockEventsCommand).addArgument(0,m_workerToken).addArgument(1,++m_workerSequence).addArgument(2,batch)))
		qWarning("NetLink lock events: authenticated worker channel is unavailable; batch not delivered");
}



bool ScreenLockFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
													const MessageContext& messageContext,
													const FeatureMessage& message )
{
	if( message.featureUid() == m_screenLockFeature.uid() ||
		message.featureUid() == m_lockInputDevicesFeature.uid() )
	{
		if(message.command()==LockEventsCommand) {
			// The IPC nonce was checked by FeatureWorkerManager. A remote Master
			// must never be able to inject an event or cause a reply loop.
			const auto sequence=message.argument(1).toULongLong();
			if(messageContext.connection() || !messageContext.ioDevice() || !messageContext.ioDevice()->property("workerAuthenticated").toBool() ||
			   m_monitorToken.isEmpty() || message.featureUid()!=m_monitoredFeature || message.argument(0).toString()!=m_monitorToken ||
			   sequence<=m_monitorSequence || !LockActivity::validBatch(message.argument(2).toMap())) return true;
			if(m_receiveLimit.isValid() && m_receiveLimit.elapsed()<750) return true;
			m_receiveLimit.start();m_monitorSequence=sequence;
			if(m_monitorContext.ioDevice()) server.sendFeatureMessageReply(m_monitorContext,message);
			return true;
		}
		if(!messageContext.connection() || !messageContext.ioDevice() || (message.command()!=StartLockCommand && message.command()!=StopLockCommand)) return true;
		if(message.command()==StopLockCommand) {
			m_monitorToken.clear();m_monitorContext=MessageContext{};
		} else {
			const auto token=message.argument(100).toString();
			// Legacy clients omit this additive opt-in: lock/unlock stay compatible.
			m_monitorToken=token.size()==36 && !QUuid(token).isNull()?token:QString{};
			m_monitorContext=m_monitorToken.isEmpty()?MessageContext{}:messageContext;
			m_monitoredFeature=message.featureUid();m_monitorSequence=0;m_receiveLimit.invalidate();
		}
		if( server.featureWorkerManager().isWorkerRunning( message.featureUid() ) == false &&
			message.command() == StopLockCommand )
		{
			return true;
		}

		if( VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false )
		{
			vDebug() << "not locking screen since not running in a user session";
			return true;
		}

		// forward message to worker
		server.featureWorkerManager().sendMessageToManagedSystemWorker( message );

		return true;
	}

	return false;
}



bool ScreenLockFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() == m_screenLockFeature.uid() ||
		message.featureUid() == m_lockInputDevicesFeature.uid() )
	{
		switch( message.command() )
		{
		case StartLockCommand:
			m_activity.take();m_workerSequence=0;m_workerFeature=message.featureUid();
			m_workerToken=message.argument(100).toString();
			if(m_workerToken.size()!=36 || QUuid(m_workerToken).isNull()) m_workerToken.clear();
			if( m_lockWidget == nullptr )
			{
				VeyonCore::platform().coreFunctions().disableScreenSaver();

				auto mode = LockWidget::BackgroundPixmap;
				if( message.featureUid() == m_lockInputDevicesFeature.uid() )
				{
					mode = LockWidget::DesktopVisible;
				}

				m_lockWidget = new LockWidget( mode,
											   QPixmap( QStringLiteral(":/screenlock/locked-screen-background.png" ) ) );
				connect(m_lockWidget,&LockWidget::activityObserved,this,[this](const QString& code) {if(!m_workerToken.isEmpty()) m_activity.record(code);});
			}
			m_activityTimer.stop();QObject::disconnect(&m_activityTimer,nullptr,nullptr,nullptr);
			if(auto live=dynamic_cast<QObject*>(&worker)) {
				connect(&m_activityTimer,&QTimer::timeout,live,[this,&worker] {flushActivity(worker);});
				if(!m_workerToken.isEmpty()) m_activityTimer.start();
			}
			return true;

		case StopLockCommand:
			m_activityTimer.stop();m_workerToken.clear();m_activity.take();
			delete m_lockWidget;
			m_lockWidget = nullptr;

			VeyonCore::platform().coreFunctions().restoreScreenSaverSettings();

			QCoreApplication::quit();

			return true;

		default:
			break;
		}
	}

	return false;
}
