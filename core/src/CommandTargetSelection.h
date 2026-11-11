// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ComputerControlInterface.h"
#include "NetLinkUi.h"
#include <QMessageBox>
#include <functional>

// Resolve a fresh selection for each user request. Background mode recovery and
// policy renewals deliberately do not enter this interactive boundary.
namespace CommandTargetSelection {
using Clients = ComputerControlInterfaceList;
using Selection = std::function<Clients()>;
using Action = std::function<void(const Clients&)>;
inline Clients unique(const Clients& clients) {
    Clients result;
    for(const auto& client:clients) if(client && !result.contains(client)) result.append(client);
    return result;
}
inline void request(QWidget* parent, QObject* context, const QString& command,
                    Selection selected, Selection available, Action action,
                    std::function<bool()> forceWarning = [] {return false;}) {
    const auto chosen=unique(selected()), visible=unique(available());
    const auto targets=chosen.isEmpty()?visible:chosen;
    if(targets.isEmpty()) return;
    const bool forced=forceWarning();
    if(!chosen.isEmpty() && !forced) {action(targets);return;}
    if(!context->property("netlinkTargetConfirmationPending").toString().isEmpty()) return;
    const auto token=QUuid::createUuid().toString();
    context->setProperty("netlinkTargetConfirmationPending",token);
    auto box=new QMessageBox(QMessageBox::Warning,command,
        NetLinkUi::text("No client is selected in the Console or command panel. Apply '%1' to all %2 computers in the current target list?\nOther groups and hidden computers will not be included.").arg(command).arg(targets.size()),
        QMessageBox::Yes|QMessageBox::No,parent);
    box->setObjectName(QStringLiteral("netlinkCommandTargetConfirmation"));
    box->setTextFormat(Qt::PlainText);box->setDefaultButton(QMessageBox::No);
    box->setEscapeButton(QMessageBox::No);box->setAttribute(Qt::WA_DeleteOnClose);
    QStringList details;
    for(const auto& client:targets) details.append(client->computer().name()+QStringLiteral(" — ")+client->computer().hostAddress());
    box->setDetailedText(details.join(QLatin1Char('\n')));
    QObject::connect(box,&QObject::destroyed,context,[context,token] {
        if(context->property("netlinkTargetConfirmationPending").toString()==token) context->setProperty("netlinkTargetConfirmationPending",QString{});
    });
    QObject::connect(box,&QMessageBox::finished,context,
        [=](int answer) {
            if(context->property("netlinkTargetConfirmationPending").toString()==token) context->setProperty("netlinkTargetConfirmationPending",QString{});
            if(answer!=QMessageBox::Yes) return;
            // Never broaden or silently retarget an already displayed warning.
            if(unique(selected())!=chosen || unique(available())!=visible || forceWarning()!=forced) {
                auto notice=new QMessageBox(QMessageBox::Information,command,
                    NetLinkUi::text("The target selection changed. No command was sent. Repeat the command with the new selection."),QMessageBox::Ok,parent);
                notice->setTextFormat(Qt::PlainText);notice->setAttribute(Qt::WA_DeleteOnClose);notice->open();return;
            }
            action(targets);
        });
    box->open();
}
}
