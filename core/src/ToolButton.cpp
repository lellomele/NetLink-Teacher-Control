/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original ToolButton: Copyright (c) 2006-2022 Tobias Junghans
 * NetLink restyling: (C) 2026 prof. Ing. Raffaele Mele
 */
#include "ToolButton.h"
#include <QEvent>
#include <QToolBar>
#include <QMenu>
#include <QWidgetAction>
#include <QScopedValueRollback>

namespace {
// Unlike addWidget()/defaultWidget, a factory-backed action can also appear
// as a normal actionable menu item when Qt switches overflow to popup mode.
class FeatureButtonAction final : public QWidgetAction {
public:
    FeatureButtonAction(ToolButton* button,QObject* parent):QWidgetAction(parent),m_button(button) {}
protected:
    QWidget* createWidget(QWidget* parent) override {
        if(m_button && qobject_cast<QToolBar*>(parent)) {m_button->setParent(parent);return m_button;}
        if(auto popup=qobject_cast<QMenu*>(parent)) {
            if(m_popup!=popup) {
                m_popup=popup;
                connect(popup,&QMenu::aboutToShow,this,[this] {if(m_button) m_button->updateMinimumSize();});
            }
        }
        return nullptr; // native menu item uses the synchronized QAction below
    }
private:
    QPointer<ToolButton> m_button;
    QPointer<QMenu> m_popup;
};
}
bool ToolButton::s_toolTipsDisabled=false;
bool ToolButton::s_iconOnlyMode=false;

ToolButton::ToolButton(const QIcon& icon,const QString& label,const QString& altLabel,
    const QString& description,const QKeySequence& shortcut) {
    setProperty("netlinkFeatureButton",true);
    setIcon(icon);setIconSize(QSize(36,36));setText(label);setAccessibleName(label);
    setAccessibleDescription(description);setShortcut(shortcut);
    setToolButtonStyle(s_iconOnlyMode?Qt::ToolButtonIconOnly:Qt::ToolButtonTextUnderIcon);
    setFocusPolicy(Qt::StrongFocus);setMinimumHeight(44);setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
    auto tip=QStringLiteral("<b>%1</b><p>%2</p>").arg(label.toHtmlEscaped(),description.toHtmlEscaped().replace(QLatin1Char('\n'),QStringLiteral("<br>")));
    if(!shortcut.isEmpty()) tip+=QStringLiteral("<p>%1</p>").arg(shortcut.toString(QKeySequence::NativeText).toHtmlEscaped());
    setToolTip(tip);
    connect(this,&QToolButton::toggled,this,[this,label,altLabel](bool checked){setText(checked && !altLabel.isEmpty()?altLabel:label);setAccessibleName(text());updateMinimumSize();});
    updateMinimumSize();
}
QSize ToolButton::minimumSizeHint() const {
    // Keep complete labels; use the native toolbar overflow instead of elision.
    return sizeHint().expandedTo(QSize(44,44));
}
void ToolButton::updateMinimumSize() {
    // QWidgetAction/toolbar layout can prefer the explicit minimum over the hint.
    // Recompute after polish/font/style changes, never from a fixed pixel width.
    const auto width=minimumSizeHint().width();
    if(minimumWidth()!=width) setMinimumWidth(width);
    syncToolbarAction();
}
void ToolButton::setIconOnlyMode(QWidget* window,bool enabled) {
    s_iconOnlyMode=enabled;
    for(auto button:window->findChildren<ToolButton*>()) {button->setToolButtonStyle(enabled?Qt::ToolButtonIconOnly:Qt::ToolButtonTextUnderIcon);button->updateMinimumSize();}
}
void ToolButton::addTo(QToolBar* toolBar) {
    auto action=new FeatureButtonAction(this,toolBar);m_toolbarAction=action;syncToolbarAction();
    connect(action,&QAction::triggered,this,[this] {click();});
    toolBar->addAction(action);
}
void ToolButton::syncToolbarAction() {
    if(!m_toolbarAction || m_syncingToolbarAction) return;
    QScopedValueRollback<bool> guard(m_syncingToolbarAction,true);
    // QWidgetAction propagates enabled state on *every* ActionChanged. Set it
    // first or a label/icon refresh can re-enable a deliberately disabled button.
    m_toolbarAction->setEnabled(!testAttribute(Qt::WA_ForceDisabled));
    m_toolbarAction->setText(text());m_toolbarAction->setIcon(icon());m_toolbarAction->setToolTip(toolTip());
    m_toolbarAction->setCheckable(isCheckable());m_toolbarAction->setChecked(isChecked());
    m_toolbarAction->setMenu(menu());
}
bool ToolButton::event(QEvent* event) {
    if(event->type()==QEvent::ToolTip && s_toolTipsDisabled) return true;
    if(event->type()==QEvent::Leave || event->type()==QEvent::MouseButtonPress) Q_EMIT mouseLeftButton();
    const auto result=QToolButton::event(event);
    if(event->type()==QEvent::Polish || event->type()==QEvent::FontChange || event->type()==QEvent::StyleChange || event->type()==QEvent::EnabledChange || event->type()==QEvent::LayoutRequest)
        updateMinimumSize();
    return result;
}
