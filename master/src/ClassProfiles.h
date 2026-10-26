// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#pragma once
#include "SchoolClassProfile.h"
#include "ComputerControlInterface.h"
#include <QDialog>
#include <QPointer>
#include <QTimer>
#include <functional>

class QAbstractItemModel;
class CheckableItemProxyModel;

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
    void setDirectoryContext(QAbstractItemModel*, CheckableItemProxyModel*,
                             std::function<QUuid()> highlightedGroup,
                             std::function<bool()> canChangeSelection);
    const SchoolClassProfile& profile() const { return m_profile; }
Q_SIGNALS:
    void assignmentsChanged();
private:
    void addSelectedComputers();
    void changed();
    bool groupSnapshot(SchoolClassProfile&, QString* error) const;
    void confirmLoad(const SchoolClassProfile&, const QString& path, const std::function<void()>& finished);
    QPointer<VeyonMasterInterface> m_master;
    QPointer<QDialog> m_dialog;
    SchoolClassProfile m_profile;
    QList<QPointer<ComputerControlInterface>> m_seen;
    QTimer m_timer{this};
    QString m_path;
    bool m_dirty = false;
    QPointer<QAbstractItemModel> m_directory;
    QPointer<CheckableItemProxyModel> m_checked;
    std::function<QUuid()> m_highlightedGroup;
    std::function<bool()> m_canChangeSelection;
    friend class ClassProfileTests;
};
