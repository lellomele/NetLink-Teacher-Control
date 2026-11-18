// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <QString>
#include <QUuid>

// One sticky, non-inheritable event per child launch, not per machine (nor per
// logical session number, which can be reused). A new child cannot consume a
// predecessor's stop. CreateProcessAsUser does not inherit service handles.
namespace WindowsSessionStop {
inline const char* variable() {return "NETLINK_SESSION_STOP_EVENT";}
inline QString prefix() {return QStringLiteral("Global\\NetLink.SessionStop.");}
class Event {
public:
    Event()=default;
    ~Event(){close();}
    Event(const Event&)=delete;
    Event& operator=(const Event&)=delete;
    HANDLE handle() const {return m_handle;}
    QString name() const {return m_name;}
    void close(){if(m_handle) CloseHandle(m_handle);m_handle=nullptr;m_name.clear();}
    bool create() {
        close();
        m_name=prefix()+QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Service and child both run under LocalSystem. Use the creator token's
        // default DACL; do not grant other users stop permissions.
        m_handle=CreateEventW(nullptr,TRUE,FALSE,reinterpret_cast<LPCWSTR>(m_name.utf16()));
        if(m_handle && GetLastError()==ERROR_ALREADY_EXISTS) {close();SetLastError(ERROR_ALREADY_EXISTS);}
        return m_handle!=nullptr;
    }
    bool open(const QString& name,DWORD access) {
        close();
        if(name.isEmpty()) { // manually started test/server: local to this process
            m_handle=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        } else {
            const auto guid=QUuid(name.mid(prefix().size()));
            if(guid.isNull() || name!=prefix()+guid.toString(QUuid::WithoutBraces)) {
                SetLastError(ERROR_INVALID_NAME);return false;
            }
            m_handle=OpenEventW(access,FALSE,reinterpret_cast<LPCWSTR>(name.utf16()));
        }
        return m_handle!=nullptr;
    }
    bool signal() const {return m_handle && SetEvent(m_handle);}
private:
    HANDLE m_handle=nullptr;
    QString m_name;
};
}
