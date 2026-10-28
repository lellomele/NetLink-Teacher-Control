/*
 * Configuration/LocalStore.h - LocalStore class
 *
 * Copyright (c) 2009-2022 Tobias Junghans <tobydox@veyon.io>
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

#include "Configuration/Store.h"

class QSettings;

namespace Configuration
{

// clazy:excludeall=copyable-polymorphic

class VEYON_CORE_EXPORT LocalStore : public Store
{
public:
	explicit LocalStore( Scope scope );

	void load( Object *obj ) override;
	void flush( const Object *obj ) override;
	// Update one value without clearing or rewriting unrelated configuration.
	bool writeValue(const QString& key, const QVariant& value, const QString& parentKey = {});
	// Checked optimistic save: merges disjoint edits, rejects conflicts and reports I/O errors.
	bool save(Object& object, QString& error);
	bool compareAndWriteValue(const QString& key,const QVariant& expected,const QVariant& value,
	                         const QString& parentKey,QString& error);
	bool clearChecked(QString& error);
	QString errorString() const {return m_error;}
	bool isWritable() const override;
	void clear() override;

	virtual QSettings *createSettingsObject() const;

private:
	bool saveSnapshot(const Object& object,Object* updated,QString& error);
	QString m_error;

} ;

}
