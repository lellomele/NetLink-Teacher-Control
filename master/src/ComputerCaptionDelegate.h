// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once

#include <QStyledItemDelegate>
#include "ComputerControlInterface.h"
#include "ComputerListModel.h"

// Plain model text remains searchable/accessible; only the attendance prefix
// receives additional formatting when the monitoring view paints it.
class ComputerCaptionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static QString captionText(const ComputerControlInterface& client,
                               ComputerListModel::DisplayRoleContent content);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};
