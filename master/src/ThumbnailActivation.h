// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "Feature.h"
#include <QItemSelectionModel>

namespace ThumbnailActivation {
inline Feature::Uid feature(const Feature::Uid& configured,Qt::KeyboardModifiers modifiers) {
    // Existing RemoteControl protocol identity: do not create a new feature or
    // change the configured normal double-click action.
    return modifiers.testFlag(Qt::ControlModifier)?
        Feature::Uid(QStringLiteral("ca00ad68-1709-4abe-85e2-48dff6ccf8a2")):configured;
}
inline void selectTarget(QItemSelectionModel* selection,const QModelIndex& index,Qt::KeyboardModifiers modifiers) {
    if(!selection || !index.isValid()) return;
    if(modifiers.testFlag(Qt::ControlModifier))
        selection->setCurrentIndex(index,QItemSelectionModel::ClearAndSelect);
    else selection->select(index,QItemSelectionModel::SelectCurrent);
}
}
