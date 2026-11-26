/*
 * FileTransferPlugin.cpp - implementation of FileTransferPlugin class
 *
 * Copyright (c) 2018-2022 Tobias Junghans <tobydox@veyon.io>
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

#include <QDesktopServices>
#include <QTimer>
#include <QLabel>
#include <QLayout>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <unistd.h>
#endif
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include "BuiltinFeatures.h"
#include "Filesystem.h"
#include "FileTransferConfigurationPage.h"
#include "FileTransferController.h"
#include "FileTransferDialog.h"
#include "FileTransferPlugin.h"
#include "FileTransferUserConfiguration.h"
#include "FeatureWorkerManager.h"
#include "PlatformFilesystemFunctions.h"
#include "SystemTrayIcon.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


FileTransferPlugin::FileTransferPlugin( QObject* parent ) :
	QObject( parent ),
	m_fileTransferFeature( QStringLiteral( "FileTransfer" ),
						   Feature::Flag::Action | Feature::Flag::AllComponents,
						   Feature::Uid( "4a70bd5a-fab2-4a4b-a92a-a1e81d2b75ed" ),
						   Feature::Uid(),
						   tr( "File transfer" ), {},
						   tr( "Click this button to transfer files from your computer to all computers." ),
						   QStringLiteral(":/filetransfer/applications-office.png") ),
	m_features( { m_fileTransferFeature } ),
	m_configuration( &VeyonCore::config() )
{
	m_receiveIdle.setSingleShot(true);
	m_receiveIdle.setInterval(120000);
	connect(&m_receiveIdle,&QTimer::timeout,this,[this]{m_currentFile.reset();m_currentTransferId=QUuid();});
}



FileTransferPlugin::~FileTransferPlugin()
{
	delete m_fileTransferController;
}



bool FileTransferPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( hasFeature( featureUid ) == false )
	{
		return false;
	}

	if( operation == Operation::Initialize )
	{
		auto files = arguments.value( argToString(Argument::Files) ).toStringList();
		if( files.isEmpty() )
		{
			return false;
		}

		for( auto& file : files )
		{
			QFileInfo fileInfo( file );
			if( fileInfo.dir() == QDir::current() )
			{
				file = fileInfo.fileName();
			}
		}

		if( m_fileTransferController == nullptr )
		{
			m_fileTransferController = new FileTransferController( this );
		}

		m_fileTransferController->setFiles( files );
		m_fileTransferController->setInterfaces( computerControlInterfaces );

		return true;
	}

	if( operation == Operation::Start )
	{
		if( m_fileTransferController )
		{
			m_fileTransferController->start();
		}

		return true;
	}

	if( operation == Operation::Stop )
	{
		if( m_fileTransferController )
		{
			m_fileTransferController->stop();
		}

		return true;
	}

	return false;
}



bool FileTransferPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature == m_fileTransferFeature )
	{
		FileTransferUserConfiguration config( master.userConfigurationObject() );

		m_lastFileTransferSourceDirectory = config.lastFileTransferSourceDirectory();
		if( m_configuration.rememberLastFileTransferSourceDirectory() == false ||
			QDir( m_lastFileTransferSourceDirectory ).exists() == false )
		{
			m_lastFileTransferSourceDirectory = VeyonCore::filesystem().expandPath( m_configuration.fileTransferDefaultSourceDirectory() );
		}
		const auto files = QFileDialog::getOpenFileNames( master.mainWindow(),
														  tr( "Select one or more files to transfer" ),
														  m_lastFileTransferSourceDirectory );

		if( files.isEmpty() == false )
		{
			config.setLastFileTransferSourceDirectory( QFileInfo( files.first() ).absolutePath() );

			if( controlFeature( feature.uid(), Operation::Initialize,
								{ { argToString(Argument::Files), files } }, computerControlInterfaces ) )
			{
				auto dialog = new FileTransferDialog( m_fileTransferController, master.mainWindow() );
				auto report=new QLabel(tr("Progress indicates sending; only a client receipt confirms saving."),dialog);
				report->setTextFormat(Qt::PlainText);report->setWordWrap(true);dialog->layout()->addWidget(report);
				connect(this,&FileTransferPlugin::deliveryStatus,report,[report](const QString& line){report->setText(QString(report->text()+QLatin1Char('\n')+line).right(8192));});
				connect( dialog, &QDialog::finished, dialog, &QDialog::deleteLater );
				dialog->exec();
			}
		}

		return true;
	}

	return false;
}



bool FileTransferPlugin::handleFeatureMessage( VeyonServerInterface& server,
											   const MessageContext& messageContext,
											   const FeatureMessage& message )
{
	Q_UNUSED(messageContext)

	if( m_fileTransferFeature.uid() == message.featureUid() )
	{
		const auto id=message.argument(Argument::TransferId).toUuid();
		if(message.command()==Receipt) {
			if(messageContext.connection() || !messageContext.ioDevice() || !messageContext.ioDevice()->property("workerAuthenticated").toBool()) return true;
			auto origin=m_origins.take(id);if(origin.connection() && origin.ioDevice()) server.sendFeatureMessageReply(origin,message);
			return true;
		}
		if(!messageContext.connection()) return true;
		if(message.command()!=OpenTransferFolder) {
			if(id.isNull()) return true;
			if(message.command()==FileTransferStartCommand) {
				for(auto i=m_origins.begin();i!=m_origins.end();) {if(!i->connection()) i=m_origins.erase(i);else ++i;}
				if(m_origins.size()>=256 || m_origins.contains(id))return true;
				m_origins.insert(id,messageContext);
			} else if(m_origins.value(id).connection()!=messageContext.connection())return true;
			if(message.command()==FileTransferCancelCommand)m_origins.remove(id);
		}
		// forward message to worker
		server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

		return true;
	}

	return false;
}



bool FileTransferPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( m_fileTransferFeature.uid() == message.featureUid() )
	{
		switch( message.command() )
		{
		case FileTransferStartCommand: {
			const auto id=message.argument(Argument::TransferId).toUuid();const auto name=message.argument(Argument::Filename).toString();
			auto refuse=[&]{worker.sendFeatureMessageReply(FeatureMessage(m_fileTransferFeature.uid(),Receipt).addArgument(Argument::TransferId,id).addArgument(Argument::Committed,false));};
			static const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:[.]|$)"),QRegularExpression::CaseInsensitiveOption);
			if(m_currentFile || id.isNull() || name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char(':')) || name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')) || reserved.match(name).hasMatch()){refuse();return true;}
			m_overwrite=message.argument(Argument::OverwriteExistingFile).toBool();const auto dir=destinationDirectory();m_currentFileName=QDir(dir).filePath(name);
			if(!m_overwrite && QFileInfo::exists(m_currentFileName)){refuse();return true;}
			m_currentFile.reset(new QTemporaryFile(QDir(dir).filePath(QStringLiteral(".netlink-XXXXXX"))));
			if(!m_currentFile->open()){m_currentFile.reset();refuse();return true;}
			bool ok=false;m_expectedSize=message.argument(Argument::ExpectedSize).toLongLong(&ok);if(!ok)m_expectedSize=-1;
			m_currentTransferId=id;m_receiveIdle.start();return true;
		}
		case FileTransferContinueCommand:
		case FileTransferCancelCommand:
		case FileTransferFinishCommand: {
			const auto id=message.argument(Argument::TransferId).toUuid();if(!m_currentFile || id!=m_currentTransferId)return true;
			bool committed=false;
			if(message.command()==FileTransferContinueCommand){const auto chunk=message.argument(Argument::DataChunk).toByteArray();if((m_expectedSize<0 || chunk.size()<=m_expectedSize-m_currentFile->pos()) && m_currentFile->write(chunk)==chunk.size()){m_receiveIdle.start();return true;}}
			else if(message.command()==FileTransferFinishCommand && (m_expectedSize<0 || m_expectedSize==m_currentFile->pos()) && m_currentFile->flush()){
				const auto tmp=m_currentFile->fileName();m_currentFile->setAutoRemove(false);m_currentFile.reset();
#ifdef Q_OS_WIN
				committed=MoveFileExW(reinterpret_cast<LPCWSTR>(tmp.utf16()),reinterpret_cast<LPCWSTR>(m_currentFileName.utf16()),MOVEFILE_WRITE_THROUGH|(m_overwrite?MOVEFILE_REPLACE_EXISTING:0));
#else
				committed=m_overwrite ? ::rename(QFile::encodeName(tmp),QFile::encodeName(m_currentFileName))==0 : ::link(QFile::encodeName(tmp),QFile::encodeName(m_currentFileName))==0;
				if(committed && !m_overwrite)QFile::remove(tmp);
#endif
				if(!committed)QFile::remove(tmp);
			}
			m_receiveIdle.stop();m_currentFile.reset();m_currentTransferId=QUuid();
			if(message.command()!=FileTransferCancelCommand)worker.sendFeatureMessageReply(FeatureMessage(m_fileTransferFeature.uid(),Receipt).addArgument(Argument::TransferId,id).addArgument(Argument::Committed,committed));
			if(committed && message.argument(Argument::OpenFileInApplication).toBool())QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentFileName));
			return true;
		}
		case OpenTransferFolder:
			QDesktopServices::openUrl( QUrl::fromLocalFile( destinationDirectory() ) );
			return true;

		default:
			break;
		}
	}

	return false;
}



void FileTransferPlugin::sendStartMessage( QUuid transferId, const QString& fileName,
										   bool overwriteExistingFile, const ComputerControlInterfaceList& interfaces, qint64 size )
{
	m_pending.insert(transferId,interfaces);
	sendFeatureMessage( FeatureMessage( m_fileTransferFeature.uid(), FileTransferStartCommand ).
						addArgument(Argument::ExpectedSize,size).
						addArgument( Argument::TransferId, transferId ).
						addArgument( Argument::Filename, fileName ).
						addArgument( Argument::OverwriteExistingFile, overwriteExistingFile ),
						interfaces );
}



void FileTransferPlugin::sendDataMessage( QUuid transferId, const QByteArray& data,
										  const ComputerControlInterfaceList& interfaces )
{
	sendFeatureMessage( FeatureMessage( m_fileTransferFeature.uid(), FileTransferContinueCommand ).
						addArgument( Argument::TransferId, transferId ).
						addArgument( Argument::DataChunk, data ),
						interfaces );
}



void FileTransferPlugin::sendCancelMessage( QUuid transferId,
											const ComputerControlInterfaceList& interfaces )
{
	m_pending.remove(transferId);
	sendFeatureMessage( FeatureMessage( m_fileTransferFeature.uid(), FileTransferCancelCommand ).
						addArgument( Argument::TransferId, transferId ), interfaces );
}



void FileTransferPlugin::sendFinishMessage( QUuid transferId, const QString& fileName,
											bool openFileInApplication, const ComputerControlInterfaceList& interfaces )
{
	QTimer::singleShot(15000,this,[this,transferId,fileName]{for(const auto& ci:m_pending.take(transferId)) if(ci) Q_EMIT deliveryStatus(tr("%1 — %2: sent, not confirmed").arg(ci->computer().name(),fileName));});
	sendFeatureMessage( FeatureMessage( m_fileTransferFeature.uid(), FileTransferFinishCommand ).
						addArgument( Argument::TransferId, transferId ).
						addArgument( Argument::Filename, fileName ).
						addArgument( Argument::OpenFileInApplication, openFileInApplication ), interfaces );
}



void FileTransferPlugin::sendOpenTransferFolderMessage( const ComputerControlInterfaceList& interfaces )
{
	sendFeatureMessage( FeatureMessage( m_fileTransferFeature.uid(), OpenTransferFolder ), interfaces );
}



QString FileTransferPlugin::destinationDirectory() const
{
	auto dir = VeyonCore::filesystem().expandPath( m_configuration.fileTransferDestinationDirectory() );
	if( QDir( dir ).exists() == false &&
		m_configuration.fileTransferCreateDestinationDirectory() &&
		VeyonCore::filesystem().ensurePathExists( dir ) == false )
	{
		return QDir::homePath();
	}

	return dir;
}



ConfigurationPage* FileTransferPlugin::createConfigurationPage()
{
	return new FileTransferConfigurationPage( m_configuration );
}


bool FileTransferPlugin::handleFeatureMessage(ComputerControlInterface::Pointer client,const FeatureMessage& message) {
	if(message.featureUid()!=m_fileTransferFeature.uid() || message.command()!=Receipt || !client)return false;
	auto it=m_pending.find(message.argument(Argument::TransferId).toUuid());
	if(it!=m_pending.end() && it.value().removeOne(client)){if(it.value().isEmpty())m_pending.erase(it);Q_EMIT deliveryStatus(client->computer().name()+QStringLiteral(": ")+(message.argument(Argument::Committed).toBool()?tr("file saved"):tr("file not saved")));}return true;
}
IMPLEMENT_CONFIG_PROXY(FileTransferConfiguration)
