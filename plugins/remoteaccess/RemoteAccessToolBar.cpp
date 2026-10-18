// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "RemoteAccessToolBar.h"
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"
#include <QApplication>
#include <QEvent>
#include <QContextMenuEvent>
#include <QGridLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScopedValueRollback>
#include <QRegion>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <QVariantAnimation>

namespace {
constexpr int ButtonExtent = 44;
constexpr int Spacing = 4;
constexpr int PanelMargin = 6;
constexpr int EdgeMargin = 8;

// Fade this 44x44 control only, not the remote view or a backing overlay.
// Native style drawing keeps the Console's palette, icon set and focus ring.
class RemoteAccessPinButton final : public QToolButton
{
public:
    explicit RemoteAccessPinButton(QWidget* parent) : QToolButton(parent), m_fade(this) {
        m_fade.setDuration(220);
        connect(&m_fade,&QVariantAnimation::valueChanged,this,[this](const QVariant& value) {
            m_opacity=value.toReal();update();
        });
    }
    void setIdle(bool idle) {
        idle=idle && !qApp->property("netlinkHighContrast").toBool();
        if(property("netlinkPinIdle").toBool()==idle && (!idle || m_opacity<1.)) return;
        setProperty("netlinkPinIdle",idle);m_fade.stop();
        if(idle) {m_fade.setStartValue(m_opacity);m_fade.setEndValue(.38);m_fade.start();}
        else {m_opacity=1.;update();}
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOptionToolButton option;initStyleOption(&option);
        QPainter painter(this);painter.setOpacity(m_opacity);
        style()->drawComplexControl(QStyle::CC_ToolButton,&option,&painter,this);
    }
private:
    QVariantAnimation m_fade;
    qreal m_opacity=1.;
};
}

RemoteAccessToolBar::RemoteAccessToolBar(QWidget* parent, bool viewOnly, bool showViewOnlyToggle) :
    QWidget(parent),
    m_viewOnlyButton(showViewOnlyToggle ? new QToolButton : nullptr),
    m_selectScreenButton(new QToolButton),
    m_sendShortcutButton(new QToolButton),
    m_screenshotButton(new QToolButton),
    m_fullScreenButton(new QToolButton),
    m_exitButton(new QToolButton),
    m_panel(new QWidget(this)), m_layout(new QGridLayout(m_panel)),
    m_pinButton(new RemoteAccessPinButton(this))
{
    setObjectName(QStringLiteral("netlinkRemoteToolbar"));
    setAccessibleName(NetLinkUi::text("Remote access controls"));
    setAttribute(Qt::WA_NoMousePropagation);
    installEventFilter(this);
    m_panel->setObjectName(QStringLiteral("remoteToolbarPanel"));
    m_panel->installEventFilter(this);
    const auto setup = [this](QToolButton* button, const char* name, const char* label, const char* description) {
        button->setParent(button==m_pinButton ? this : m_panel);
        button->setObjectName(QString::fromLatin1(name));
        button->setProperty("netlinkCompactButton", true);
        button->setText(NetLinkUi::text(label));
        button->setAccessibleName(button->text());
        button->setToolTip(NetLinkUi::text(description));
        button->setAccessibleDescription(button->toolTip());
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(true);
        button->setFixedSize(ButtonExtent, ButtonExtent);
        button->setIconSize(QSize(28, 28));
        button->setFocusPolicy(Qt::StrongFocus);
        button->installEventFilter(this);
        button->show();
        connect(button, &QToolButton::clicked, this, [this] {
            // Keep keyboard input local while interacting, then return it to
            // the VNC view. A menu owns focus until it has actually closed.
            QTimer::singleShot(0, this, [this] { Q_EMIT returnFocusToView(); disappear(); });
        });
    };
    setup(m_selectScreenButton, "remoteScreens", "Select screen", "Choose a remote monitor or show all remote monitors.");
    setup(m_sendShortcutButton, "remoteShortcuts", "Send shortcut", "Send a key combination to the remote computer, not this computer.");
    if(m_viewOnlyButton) {
        setup(m_viewOnlyButton, "remoteViewOnly", "View only", "Switch between viewing only and controlling the remote mouse and keyboard.");
        m_viewOnlyButton->setCheckable(true);
        m_viewOnlyButton->setChecked(viewOnly);
        connect(m_viewOnlyButton, &QToolButton::toggled, this, [this](bool checked) {
            updateControls(checked); Q_EMIT viewOnlyChanged(checked);
        });
    }
    setup(m_screenshotButton, "remoteScreenshot", "Screenshot", "Save a screenshot of the remote computer.");
    setup(m_fullScreenButton, "remoteFullscreen", "Fullscreen", "Switch this remote session between fullscreen and windowed mode.");
    setup(m_pinButton, "remotePin", "Pin toolbar", "Hover to show controls. Left-click to pin or unpin. Right-click to move the pin and toolbar to the next edge.");
    setup(m_exitButton, "remoteDisconnect", "Disconnect", "End this remote session. The remote computer and its applications remain running.");
    m_exitButton->setProperty("netlinkDisconnectButton", true);
    m_fullScreenButton->setCheckable(true);
    m_pinButton->setCheckable(true);
    connect(m_fullScreenButton, &QToolButton::toggled, this, &RemoteAccessToolBar::fullScreenChanged);
    connect(m_screenshotButton, &QToolButton::clicked, this, &RemoteAccessToolBar::screenshotRequested);
    connect(m_exitButton, &QToolButton::clicked, this, &RemoteAccessToolBar::disconnectRequested);
    connect(m_pinButton, &QToolButton::toggled, this, &RemoteAccessToolBar::setPinned);

    for(auto button : {m_selectScreenButton, m_sendShortcutButton}) {
        auto menu = new QMenu(button);
        button->setMenu(menu);
        button->setPopupMode(QToolButton::InstantPopup);
        connect(menu, &QMenu::aboutToShow, this, [this] { m_hideTimer.stop(); cancelReveal(); wakePin(); });
        connect(menu, &QMenu::aboutToHide, this, [this] {
            QTimer::singleShot(0, this, [this] { Q_EMIT returnFocusToView(); disappear(); });
        });
    }
    m_revealTimer.setSingleShot(true);
    m_revealTimer.setInterval(350);
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(650);
    connect(&m_revealTimer, &QTimer::timeout, this, [this] {
        if(!m_pinned) appear();
    });
    connect(&m_hideTimer, &QTimer::timeout, this, [this] {
        if(!m_pinned && !interactionActive()) setExpanded(false);
    });
    m_pinFadeTimer.setSingleShot(true);
    m_pinFadeTimer.setInterval(1800);
    connect(&m_pinFadeTimer,&QTimer::timeout,this,[this] {
        if(!interactionActive()) static_cast<RemoteAccessPinButton*>(m_pinButton)->setIdle(true);
    });
    m_layout->setContentsMargins(PanelMargin, PanelMargin, PanelMargin, PanelMargin);
    m_layout->setSpacing(Spacing);
    parent->installEventFilter(this);
    // Use the existing icon preferences; no new runtime or external icon set.
    if(auto appearance = qApp->findChild<NetLinkAppearance*>(QStringLiteral("netLinkAppearanceManager")))
        connect(appearance, &NetLinkAppearance::changed, this, &RemoteAccessToolBar::refreshAppearance);
    updateControls(viewOnly);
    refreshAppearance();
    updatePinDescription();
    show();
    updatePosition();
    disappear();
}

void RemoteAccessToolBar::appear()
{
    cancelReveal(); m_hideTimer.stop(); wakePin(); setExpanded(true);
    // If the pointer just touched an edge and stays on the desktop, do not
    // leave a transient toolbar open forever. Entering it cancels this timer.
    disappear();
}

void RemoteAccessToolBar::disappear()
{
    if(!m_pinned && m_expanded && !interactionActive() && !m_hideTimer.isActive()) m_hideTimer.start();
    schedulePinFade();
}

void RemoteAccessToolBar::cancelReveal() { m_revealTimer.stop(); }

void RemoteAccessToolBar::requestRevealAt(const QPoint& position)
{
    if(m_pinned || m_expanded || !parentWidget()->rect().contains(position)) { cancelReveal(); return; }
    const auto area = parentWidget()->rect();
    const bool activeEdge = m_edge==Qt::TopEdge ? position.y()<=3 :
        m_edge==Qt::BottomEdge ? position.y()>=area.bottom()-3 :
        m_edge==Qt::LeftEdge ? position.x()<=3 : position.x()>=area.right()-3;
    if(!activeEdge) {cancelReveal();return;}
    if(!m_revealTimer.isActive()) m_revealTimer.start();
}

void RemoteAccessToolBar::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_panel->setVisible(expanded);
    m_pinButton->show();
    updatePosition();
    raise();
    update();
}

void RemoteAccessToolBar::setPinned(bool pinned)
{
    m_pinned = pinned;
    const QSignalBlocker blocker(m_pinButton);
    m_pinButton->setChecked(pinned);
    updatePinDescription();
    if(pinned) appear(); else disappear();
}

void RemoteAccessToolBar::updatePinDescription()
{
    m_pinButton->setToolTip(m_pinned ? NetLinkUi::text("Toolbar pinned. Left-click to enable automatic hiding. Right-click to move to the next edge.") :
        NetLinkUi::text("Hover to show controls. Left-click to pin or unpin. Right-click to move the pin and toolbar to the next edge."));
    m_pinButton->setAccessibleDescription(m_pinButton->toolTip());
}

void RemoteAccessToolBar::updateControls(bool viewOnly)
{
    m_sendShortcutButton->setVisible(!viewOnly);
    if(m_viewOnlyButton) { const QSignalBlocker blocker(m_viewOnlyButton); m_viewOnlyButton->setChecked(viewOnly); }
    refreshAppearance();
    updatePosition();
}

void RemoteAccessToolBar::setScreenSelectionVisible(bool visible)
{
    m_selectScreenButton->setVisible(visible); updatePosition();
}

void RemoteAccessToolBar::setConnected(bool connected)
{
    setToolTip(connected ? NetLinkUi::text("Connected.") : NetLinkUi::text("Connecting..."));
    if(!connected) appear();
}

bool RemoteAccessToolBar::interactionActive() const
{
    const auto focus = QApplication::focusWidget();
    return m_pointerInside || (focus && (focus == this || isAncestorOf(focus))) ||
        m_selectScreenButton->menu()->isVisible() || m_sendShortcutButton->menu()->isVisible();
}

void RemoteAccessToolBar::setDockEdge(Qt::Edge edge)
{
    if(edge != Qt::TopEdge && edge != Qt::BottomEdge && edge != Qt::LeftEdge && edge != Qt::RightEdge) return;
    cancelReveal();m_edge = edge; updatePosition();
}

void RemoteAccessToolBar::cycleDockEdge()
{
    setDockEdge(m_edge==Qt::TopEdge ? Qt::LeftEdge : m_edge==Qt::LeftEdge ? Qt::BottomEdge :
        m_edge==Qt::BottomEdge ? Qt::RightEdge : Qt::TopEdge);
    m_pointerInside=false;wakePin();
    QTimer::singleShot(0,this,[this] {Q_EMIT returnFocusToView();disappear();});
}

void RemoteAccessToolBar::wakePin()
{
    m_pinFadeTimer.stop();static_cast<RemoteAccessPinButton*>(m_pinButton)->setIdle(false);
}

void RemoteAccessToolBar::schedulePinFade()
{
    if(!interactionActive() && !m_pinFadeTimer.isActive()) m_pinFadeTimer.start();
}

void RemoteAccessToolBar::updatePosition()
{
    if(m_positioning || !parentWidget()) return;
    QScopedValueRollback<bool> guard(m_positioning, true);
    const bool horizontal = m_edge == Qt::TopEdge || m_edge == Qt::BottomEdge;
    QSize extent, panelExtent;
    if(m_expanded) {
        while(auto item = m_layout->takeAt(0)) delete item;
        QList<QToolButton*> buttons{m_selectScreenButton, m_sendShortcutButton};
        if(m_viewOnlyButton) buttons.append(m_viewOnlyButton);
        buttons << m_screenshotButton << m_fullScreenButton << m_exitButton;
        const int available = horizontal ? parentWidget()->width() : parentWidget()->height();
        const int perLine = qMax(1, (available-2*EdgeMargin-2*PanelMargin+Spacing)/(ButtonExtent+Spacing));
        int count = 0;
        for(auto button : buttons) if(!button->isHidden()) {
            m_layout->addWidget(button, horizontal ? count/perLine : count%perLine,
                horizontal ? count%perLine : count/perLine);
            ++count;
        }
        // In normal windows this is a single row/column. Narrow windows wrap
        // instead of shrinking the click targets or clipping the exit button.
        const int major = qMin(count, perLine), minor = (count+perLine-1)/perLine;
        const QSize grid(2*PanelMargin+major*ButtonExtent+qMax(0,major-1)*Spacing,
            2*PanelMargin+minor*ButtonExtent+qMax(0,minor-1)*Spacing);
        panelExtent = horizontal ? grid : QSize(grid.height(), grid.width());
        extent = horizontal ? QSize(qMax(ButtonExtent,panelExtent.width()),ButtonExtent+Spacing+panelExtent.height()) :
            QSize(ButtonExtent+Spacing+panelExtent.width(),qMax(ButtonExtent,panelExtent.height()));
    } else extent = QSize(ButtonExtent,ButtonExtent);
    extent = extent.boundedTo(parentWidget()->size());
    resize(extent);
    if(horizontal) {
        m_pinButton->move((width()-ButtonExtent)/2,m_edge==Qt::TopEdge ? 0 : height()-ButtonExtent);
        m_panel->setGeometry((width()-panelExtent.width())/2,m_edge==Qt::TopEdge ? ButtonExtent+Spacing : 0,panelExtent.width(),panelExtent.height());
    } else {
        m_pinButton->move(m_edge==Qt::LeftEdge ? 0 : width()-ButtonExtent,(height()-ButtonExtent)/2);
        m_panel->setGeometry(m_edge==Qt::LeftEdge ? ButtonExtent+Spacing : 0,(height()-panelExtent.height())/2,panelExtent.width(),panelExtent.height());
    }
    m_layout->activate();
    // Transparent space beside the permanent pin must remain clickable on
    // the remote desktop, not become an invisible input-catching overlay.
    auto inputRegion=QRegion(m_pinButton->geometry());
    if(m_expanded) inputRegion=inputRegion.united(QRegion(m_panel->geometry()));
    setMask(inputRegion);
    const int x = horizontal ? (parentWidget()->width()-width())/2 :
        m_edge == Qt::LeftEdge ? 0 : parentWidget()->width()-width();
    const int y = !horizontal ? (parentWidget()->height()-height())/2 :
        m_edge == Qt::TopEdge ? 0 : parentWidget()->height()-height();
    move(qBound(0,x,qMax(0,parentWidget()->width()-width())), qBound(0,y,qMax(0,parentWidget()->height()-height())));
    raise();
}

bool RemoteAccessToolBar::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type()==QEvent::UngrabMouse || event->type()==QEvent::WindowDeactivate) m_rightClickPressed=false;
    if(watched!=parentWidget()) {
        if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonDblClick) {
            if(static_cast<QMouseEvent*>(event)->button()==Qt::RightButton) {
                m_rightClickPressed=true;m_hideTimer.stop();cancelReveal();wakePin();return true;
            }
        }
        if(event->type()==QEvent::MouseButtonRelease && static_cast<QMouseEvent*>(event)->button()==Qt::RightButton && m_rightClickPressed) {
            m_rightClickPressed=false;cycleDockEdge();return true;
        }
        if(event->type()==QEvent::ContextMenu) {
            if(static_cast<QContextMenuEvent*>(event)->reason()==QContextMenuEvent::Keyboard) cycleDockEdge();
            return true;
        }
    }
    if(watched == parentWidget() && event->type() == QEvent::Resize) updatePosition();
    if(watched == m_pinButton) {
        if(event->type() == QEvent::Enter) { wakePin();if(!m_expanded) m_revealTimer.start(); }
        if(event->type() == QEvent::Leave) cancelReveal();
    }
    if(watched != parentWidget()) {
        if(event->type() == QEvent::FocusIn) {m_hideTimer.stop();wakePin();}
        if(event->type() == QEvent::FocusOut)
            QTimer::singleShot(0,this,[this] { disappear(); });
    }
    return QWidget::eventFilter(watched,event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void RemoteAccessToolBar::enterEvent(QEnterEvent* event)
#else
void RemoteAccessToolBar::enterEvent(QEvent* event)
#endif
{
    m_pointerInside = true; m_hideTimer.stop();wakePin(); QWidget::enterEvent(event);
}
void RemoteAccessToolBar::leaveEvent(QEvent* event)
{
    m_pointerInside = false; cancelReveal(); disappear(); QWidget::leaveEvent(event);
}

void RemoteAccessToolBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    // The disconnect command has its own outline and breathing space; no
    // ambiguous power/shutdown icon and no whole-desktop dimming layer.
    if(m_expanded) {
        painter.setPen(palette().color(QPalette::Mid));
        painter.setBrush(palette().color(QPalette::Window));
        painter.drawRoundedRect(QRectF(m_panel->geometry()).adjusted(.5,.5,-.5,-.5),8,8);
        const auto box = QRect(m_exitButton->mapTo(this,QPoint()),m_exitButton->size()).adjusted(-1,-1,1,1);
        painter.setPen(palette().color(QPalette::Highlight)); painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box,6,6);
    }
}

void RemoteAccessToolBar::changeEvent(QEvent* event)
{
    if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) refreshAppearance();
    QWidget::changeEvent(event);
}

void RemoteAccessToolBar::refreshAppearance()
{
    const auto appearance = qApp->findChild<NetLinkAppearance*>(QStringLiteral("netLinkAppearanceManager"));
    const auto set = appearance ? appearance->preferences().iconSet : QStringLiteral("Organic");
    const auto color = appearance ? appearance->preferences().accent : palette().color(QPalette::Highlight);
    const auto classic=[](const char* file) { return QIcon(QStringLiteral(":/remoteaccess/")+QString::fromLatin1(file)); };
    m_selectScreenButton->setIcon(NetLinkUi::icon(QStringLiteral("monitor"),set,color,classic("preferences-system-windows-effect-desktopgrid.png")));
    m_sendShortcutButton->setIcon(NetLinkUi::icon(QStringLiteral("keyboard"),set,color,classic("preferences-desktop-keyboard.png")));
    if(m_viewOnlyButton) m_viewOnlyButton->setIcon(NetLinkUi::icon(m_viewOnlyButton->isChecked() ? QStringLiteral("RemoteView") :
        QStringLiteral("RemoteControl"),set,color,classic("kmag.png")));
    m_screenshotButton->setIcon(NetLinkUi::icon(QStringLiteral("Screenshot"),set,color,classic("camera-photo.png")));
    m_fullScreenButton->setIcon(NetLinkUi::icon(QStringLiteral("fullscreen"),set,color,classic("view-fullscreen.png")));
    m_pinButton->setIcon(NetLinkUi::icon(QStringLiteral("pin"),set,color));
    if(qApp->property("netlinkHighContrast").toBool()) static_cast<RemoteAccessPinButton*>(m_pinButton)->setIdle(false);
    m_exitButton->setIcon(NetLinkUi::icon(QStringLiteral("UserLogoff"),set,color,classic("application-exit.png")));
    update();
}
