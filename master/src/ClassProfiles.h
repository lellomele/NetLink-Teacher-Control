// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "SchoolClassProfile.h"
#include "ComputerControlInterface.h"
#include <QDialog>
#include <QPointer>
#include <QTimer>

class VeyonMasterInterface;
class ComputerManager;

// Instantiated only by the Master, never by the service or a student client.
class ClassProfiles : public QObject {
    Q_OBJECT
public:
    explicit ClassProfiles(VeyonMasterInterface&, QObject* parent = nullptr);
    ~ClassProfiles() override;
    void show(const QString& operation = {});
    void activate(const SchoolClassProfile&);
    void synchronize();
    const SchoolClassProfile& profile() const { return m_profile; }
Q_SIGNALS:
    void assignmentsChanged();
private:
    void addSelectedComputers();
    void changed();
    QPointer<VeyonMasterInterface> m_master;
    QPointer<QDialog> m_dialog;
    SchoolClassProfile m_profile;
    QList<QPointer<ComputerControlInterface>> m_seen;
    QTimer m_timer{this};
    QString m_path;
    bool m_dirty = false;
    friend class ClassProfileTests;
};
