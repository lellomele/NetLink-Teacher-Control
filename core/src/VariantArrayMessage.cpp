/*
 * VariantArrayMessage.cpp - class for sending/receiving a variant array as message
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

#include "VeyonCore.h"

#include "VariantArrayMessage.h"
#include "VariantStream.h"
#include "SocketDevice.h"


VariantArrayMessage::VariantArrayMessage( QIODevice* ioDevice ) :
	m_buffer(),
	m_stream( &m_buffer ),
	m_ioDevice( ioDevice )
{
	Q_ASSERT( m_ioDevice != nullptr );

	m_buffer.open( QBuffer::ReadWrite ); // Flawfinder: ignore
}



bool VariantArrayMessage::send()
{
	if (!m_ioDevice || m_buffer.size() > MaxMessageSize)
		return false;
	const auto messageSize = qToBigEndian<MessageSize>( static_cast<MessageSize>( m_buffer.size() ) );
	QByteArray frame(reinterpret_cast<const char*>(&messageSize), sizeof(messageSize));
	frame.append(m_buffer.data());
	return m_ioDevice->write(frame) == frame.size();
}



bool VariantArrayMessage::isReadyForReceive()
{
	if (!m_ioDevice || !m_ioDevice->isOpen()) return false;
	MessageSize messageSize = 0;

	if( m_ioDevice->peek( reinterpret_cast<char *>( &messageSize ), sizeof(messageSize) ) == sizeof(messageSize) )
	{
		messageSize = qFromBigEndian(messageSize);
		if (messageSize > MaxMessageSize) {
			m_ioDevice->close();
			return false;
		}
		return m_ioDevice->bytesAvailable() >= qint64(sizeof(messageSize)) + messageSize;
	}

	return false;
}



bool VariantArrayMessage::receive()
{
	if (!m_ioDevice || !m_ioDevice->isOpen()) return false;
	// LibVNCClient's adapter performs bounded, exact-length blocking reads and
	// cannot report pending wire bytes through QIODevice::bytesAvailable(). Do
	// not peek on it: that would consume the header into Qt's unrelated buffer.
	// Event-driven sockets and buffers must still retain incomplete frames.
	if (!qobject_cast<SocketDevice*>(m_ioDevice) && !isReadyForReceive()) return false;
	MessageSize messageSize = 0;

	if( m_ioDevice->read( reinterpret_cast<char *>( &messageSize ), sizeof(messageSize) ) != sizeof(messageSize) ) // Flawfinder: ignore
	{
		vDebug() << "could not read message size!";
		return false;
	}

	messageSize = qFromBigEndian(messageSize);
	if( messageSize > MaxMessageSize )
	{
		vDebug() << "invalid message size" << messageSize;
		return false;
	}

	const auto data = m_ioDevice->read( messageSize ); // Flawfinder: ignore
	if( data.size() != static_cast<qint64>( messageSize ) )
	{
		vDebug() << "could not read message data!";
		return false;
	}

	m_buffer.close();
	m_buffer.setData( data );
	m_buffer.open( QBuffer::ReadOnly ); // Flawfinder: ignore

	return true;
}



QVariant VariantArrayMessage::read() // Flawfinder: ignore
{
	return m_stream.read(); // Flawfinder: ignore
}



VariantArrayMessage& VariantArrayMessage::write( const QVariant& v )
{
	m_stream.write( v );

	return *this;
}
