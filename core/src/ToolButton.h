/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original ToolButton: Copyright (c) 2006-2022 Tobias Junghans
 * NetLink restyling: (C) 2026 prof. Ing. Raffaele Mele
 */
#pragma once
#include <QToolButton>
#include "VeyonCore.h"
class QToolBar;

class VEYON_CORE_EXPORT ToolButton : public QToolButton {
    Q_OBJECT
public:
    ToolButton(const QIcon& icon,const QString& label,const QString& altLabel={},
               const QString& description={},const QKeySequence& shortcut={});
    static void setIconOnlyMode(QWidget* mainWindow,bool enabled);
    static bool iconOnlyMode() { return s_iconOnlyMode; }
    static void setToolTipsDisabled(bool disabled) { s_toolTipsDisabled=disabled; }
    static bool toolTipsDisabled() { return s_toolTipsDisabled; }
    void addTo(QToolBar* toolBar);
protected:
    bool event(QEvent* event) override;
Q_SIGNALS:
    void mouseLeftButton();
private:
    static bool s_toolTipsDisabled;
    static bool s_iconOnlyMode;
};
