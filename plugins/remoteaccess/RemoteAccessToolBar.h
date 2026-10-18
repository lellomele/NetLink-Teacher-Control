// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once

#include <QTimer>
#include <QWidget>

class QGridLayout;
class QMenu;
class QToolButton;

// Presentation only: no connection, key store, service or global input hooks.
// All sizes are device-independent Qt pixels. Only the compact panel receives
// input/paint events, never an overlay covering the remote desktop.
class RemoteAccessToolBar : public QWidget
{
    Q_OBJECT
public:
    explicit RemoteAccessToolBar(QWidget* parent, bool viewOnly, bool showViewOnlyToggle);
    void appear();
    void disappear();
    void requestRevealAt(const QPoint& parentPosition);
    void cancelReveal();
    void updateControls(bool viewOnly);
    void setConnected(bool connected);
    void setPinned(bool pinned);
    bool isPinned() const { return m_pinned; }
    bool isExpanded() const { return m_expanded; }
    Qt::Edge dockEdge() const { return m_edge; }
    void setDockEdge(Qt::Edge edge);
    void cycleDockEdge();
    void updatePosition();

Q_SIGNALS:
    void viewOnlyChanged(bool viewOnly);
    void fullScreenChanged(bool fullScreen);
    void screenshotRequested();
    void disconnectRequested();
    void returnFocusToView();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void setScreenSelectionVisible(bool visible);
    QToolButton* m_viewOnlyButton;
    QToolButton* m_selectScreenButton;
    QToolButton* m_sendShortcutButton;
    QToolButton* m_screenshotButton;
    QToolButton* m_fullScreenButton;
    QToolButton* m_exitButton;

private:
    void setExpanded(bool expanded);
    bool interactionActive() const;
    void refreshAppearance();
    void updatePinDescription();
    void wakePin();
    void schedulePinFade();
    QWidget* m_panel;
    QGridLayout* m_layout;
    QToolButton* m_pinButton;
    QTimer m_revealTimer;
    QTimer m_hideTimer;
    QTimer m_pinFadeTimer;
    Qt::Edge m_edge = Qt::TopEdge;
    bool m_pinned = false;
    bool m_expanded = true;
    bool m_pointerInside = false;
    bool m_positioning = false;
    bool m_rightClickPressed = false;
};
