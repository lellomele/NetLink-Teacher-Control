/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original ToolButton: Copyright (c) 2006-2022 Tobias Junghans
 * NetLink restyling: (C) 2026 prof. Ing. Raffaele Mele
 */
#include "ToolButton.h"
#include <QEvent>
#include <QToolBar>
bool ToolButton::s_toolTipsDisabled=false;
bool ToolButton::s_iconOnlyMode=false;

ToolButton::ToolButton(const QIcon& icon,const QString& label,const QString& altLabel,
    const QString& description,const QKeySequence& shortcut) {
    setProperty("netlinkFeatureButton",true);
    setIcon(icon);setIconSize(QSize(36,36));setText(label);setAccessibleName(label);
    setAccessibleDescription(description);setShortcut(shortcut);
    setToolButtonStyle(s_iconOnlyMode?Qt::ToolButtonIconOnly:Qt::ToolButtonTextUnderIcon);
    setFocusPolicy(Qt::StrongFocus);setMinimumSize(44,44);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    auto tip=QStringLiteral("<b>%1</b><p>%2</p>").arg(label.toHtmlEscaped(),description.toHtmlEscaped().replace(QLatin1Char('\n'),QStringLiteral("<br>")));
    if(!shortcut.isEmpty()) tip+=QStringLiteral("<p>%1</p>").arg(shortcut.toString(QKeySequence::NativeText).toHtmlEscaped());
    setToolTip(tip);
    connect(this,&QToolButton::toggled,this,[this,label,altLabel](bool checked){setText(checked && !altLabel.isEmpty()?altLabel:label);});
}
void ToolButton::setIconOnlyMode(QWidget* window,bool enabled) {
    s_iconOnlyMode=enabled;
    for(auto button:window->findChildren<ToolButton*>()) button->setToolButtonStyle(enabled?Qt::ToolButtonIconOnly:Qt::ToolButtonTextUnderIcon);
}
void ToolButton::addTo(QToolBar* toolBar) {
    auto action=toolBar->addWidget(this);action->setText(text());action->setIcon(icon());action->setToolTip(toolTip());
}
bool ToolButton::event(QEvent* event) {
    if(event->type()==QEvent::ToolTip && s_toolTipsDisabled) return true;
    if(event->type()==QEvent::Leave || event->type()==QEvent::MouseButtonPress) Q_EMIT mouseLeftButton();
    return QToolButton::event(event);
}
