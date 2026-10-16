/*
 * WindowsKeyboardShortcutTrapper.cpp - class for trapping shortcuts on Windows
 *
 * Copyright (c) 2006-2022 Tobias Junghans <tobydox@veyon.io>
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

#include <windows.h>

#include "WindowsKeyboardShortcutTrapper.h"
#include <QMap>
#include <QPointer>

// clazy:excludeall=non-pod-global-static

static QMutex __trappedShortcutsMutex;
static QList<WindowsKeyboardShortcutTrapper::Shortcut> __trappedShortcuts;
static QMap<DWORD,KeyboardShortcutTrapper::Shortcut> __pressedShortcutKeys;

QMutex WindowsKeyboardShortcutTrapper::s_refCntMutex;
int WindowsKeyboardShortcutTrapper::s_refCnt = 0;


static HHOOK __lowLevelKeyboardHookHandle = nullptr; // hook handle

KeyboardShortcutTrapper::Shortcut WindowsKeyboardShortcutTrapper::classifyShortcut(quint32 key,bool control,bool shift,bool alt)
{
    if(key==VK_ESCAPE && control) return shift?CtrlShiftEsc:CtrlEsc;
    if(key==VK_TAB && alt) return AltTab;
    if(key==VK_ESCAPE && alt) return AltEsc;
    if(key==VK_SPACE && alt) return AltSpace;
    if(key==VK_F4 && alt) return AltF4;
    if(key==VK_LWIN || key==VK_RWIN) return SuperKeyDown;
    return NoShortcut; // especially Ctrl+Alt+Del: never synthesize or claim it
}


LRESULT CALLBACK TaskKeyHookLL( int nCode, WPARAM wp, LPARAM lp )
{
    if(nCode != HC_ACTION) return CallNextHookEx(__lowLevelKeyboardHookHandle,nCode,wp,lp);
    const auto key=reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
    const bool down=wp==WM_KEYDOWN || wp==WM_SYSKEYDOWN;
    const bool up=wp==WM_KEYUP || wp==WM_SYSKEYUP;
    if(!down && !up) return CallNextHookEx(__lowLevelKeyboardHookHandle,nCode,wp,lp);
    const bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
    QMutexLocker locker(&__trappedShortcutsMutex);
    if(up) {
        const auto found=__pressedShortcutKeys.find(key->vkCode);
        if(found==__pressedShortcutKeys.end()) {locker.unlock();return CallNextHookEx(__lowLevelKeyboardHookHandle,nCode,wp,lp);}
        const auto shortcut=found.value();__pressedShortcutKeys.erase(found);
        if(shortcut==KeyboardShortcutTrapper::SuperKeyDown && __trappedShortcuts.size()<128)
            __trappedShortcuts.append(KeyboardShortcutTrapper::SuperKeyUp);
        return 1;
    }
    if(__pressedShortcutKeys.contains(key->vkCode)) return 1; // suppress auto-repeat, not duplicate events
    const auto shortcut=WindowsKeyboardShortcutTrapper::classifyShortcut(key->vkCode,ctrl,
        (GetAsyncKeyState(VK_SHIFT)&0x8000)!=0,(key->flags&LLKHF_ALTDOWN)!=0);
    if(shortcut!=KeyboardShortcutTrapper::NoShortcut) {
        __pressedShortcutKeys.insert(key->vkCode,shortcut);
        if(__trappedShortcuts.size()<128) __trappedShortcuts.append(shortcut);
        return 1;
    }
    locker.unlock();
    return CallNextHookEx(__lowLevelKeyboardHookHandle,nCode,wp,lp);
}


WindowsKeyboardShortcutTrapper::WindowsKeyboardShortcutTrapper( QObject* parent ) :
	KeyboardShortcutTrapper( parent ),
	m_pollTimer( this )
{
	connect( &m_pollTimer, &QTimer::timeout,
				this, &WindowsKeyboardShortcutTrapper::forwardTrappedShortcuts );
}



WindowsKeyboardShortcutTrapper::~WindowsKeyboardShortcutTrapper()
{
	setEnabled( false );
}



void WindowsKeyboardShortcutTrapper::setEnabled( bool on )
{
	if( on == m_enabled )
	{
		return;
	}

	s_refCntMutex.lock();

	m_enabled = on;
	if( on )
	{
		if( !s_refCnt )
		{
			if( __lowLevelKeyboardHookHandle == nullptr )
			{
				{QMutexLocker pending(&__trappedShortcutsMutex);__trappedShortcuts.clear();__pressedShortcutKeys.clear();}
				HINSTANCE hAppInstance = GetModuleHandle( nullptr );
				// set lowlevel-keyboard-hook
				__lowLevelKeyboardHookHandle = SetWindowsHookEx( WH_KEYBOARD_LL, TaskKeyHookLL, hAppInstance, 0 );
				if(!__lowLevelKeyboardHookHandle) {
					qWarning("NetLink keyboard shortcut hook could not be installed: Windows error %lu",GetLastError());
					m_enabled=false;
					s_refCntMutex.unlock();
					Q_EMIT shortcutMonitoringFailed();
					return;
				}
			}
		}

		m_pollTimer.start( PollTimerInterval );
		++s_refCnt;
	}
	else
	{
		m_pollTimer.stop();
		--s_refCnt;
		if( !s_refCnt )
		{
			UnhookWindowsHookEx( __lowLevelKeyboardHookHandle );
			__lowLevelKeyboardHookHandle = nullptr;
			QMutexLocker pending(&__trappedShortcutsMutex);__trappedShortcuts.clear();__pressedShortcutKeys.clear();
		}
	}
	s_refCntMutex.unlock();
}



void WindowsKeyboardShortcutTrapper::forwardTrappedShortcuts()
{
	QList<Shortcut> pending;
    {QMutexLocker locker(&__trappedShortcutsMutex);pending.swap(__trappedShortcuts);}
    // Callbacks may disable/re-arm the trapper: never emit while holding its mutex.
    QPointer<WindowsKeyboardShortcutTrapper> live(this);
    for(const auto shortcut:pending) {
        if(!live || !live->m_enabled) break;
        Q_EMIT shortcutTrapped(shortcut);
    }
}
