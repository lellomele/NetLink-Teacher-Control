/*
 * ScreenLockFeaturePlugin.h - declaration of ScreenLockFeaturePlugin class
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

#pragma once

#include "FeatureProviderInterface.h"
#include "LockActivity.h"
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>

class QDialog;
class QPlainTextEdit;

class LockWidget;

class ScreenLockFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.ScreenLock")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit ScreenLockFeaturePlugin( QObject* parent = nullptr );
	~ScreenLockFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("2ad98ccb-e9a5-43ef-8c4c-876ac5efbcb1") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 1 );
	}

	QString name() const override
	{
		return QStringLiteral("ScreenLock");
	}

	QString description() const override
	{
		return tr( "Lock screen and input devices of a computer" );
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon Community");
	}

	QString copyright() const override
	{
		return QStringLiteral("Tobias Junghans");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces ) override;
	bool startFeature(VeyonMasterInterface& master,const Feature& feature,const ComputerControlInterfaceList& clients) override;
	bool handleFeatureMessage(ComputerControlInterface::Pointer client,const FeatureMessage& message) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

private:
	friend class LockNotificationTests;
	void flushActivity(VeyonWorkerInterface& worker);
	void displayActivity(ComputerControlInterface::Pointer client,const QVariantMap& batch);
	enum Commands
	{
		StartLockCommand,
		StopLockCommand,
		LockEventsCommand,
		CommandCount
	};

	const Feature m_screenLockFeature;
	const Feature m_lockInputDevicesFeature;
	const FeatureList m_features;

	LockWidget* m_lockWidget;
	QTimer m_activityTimer{this};
	LockActivity m_activity;
	QString m_workerToken,m_monitorToken;
	quint64 m_workerSequence=0,m_monitorSequence=0;
	Feature::Uid m_workerFeature,m_monitoredFeature;
	MessageContext m_monitorContext;
	QElapsedTimer m_receiveLimit;
	QPointer<QWidget> m_masterWindow;
	QPointer<QDialog> m_eventsWindow;
	QPointer<QPlainTextEdit> m_eventsText;

};
