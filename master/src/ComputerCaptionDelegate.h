// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once

#include <QStyledItemDelegate>
#include "ComputerControlInterface.h"
#include "ComputerListModel.h"

// Plain model text remains searchable/accessible. Attendance and the confirmed
// InternetGuard state are formatted independently, never as client-supplied HTML.
class ComputerCaptionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static bool webLocked(const ComputerControlInterface& client) {
        return client.property("schoolPolicyVerifiedState").toString() == QStringLiteral("blocked");
    }
    static QColor webLockedBackground() { return QColor(QStringLiteral("#a52a32")); }
    static QString captionText(const ComputerControlInterface& client,
                               ComputerListModel::DisplayRoleContent content);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};
