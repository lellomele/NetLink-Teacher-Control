// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
#include "ComputerCaptionDelegate.h"
#include "NetLinkAppearance.h"
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextLayout>

QString ComputerCaptionDelegate::captionText(const ComputerControlInterface& client,
                                            ComputerListModel::DisplayRoleContent content)
{
    const auto suffix = client.networkPolicyStatus().isEmpty() ? QString{} :
        QStringLiteral(" [IG: %1]").arg(client.networkPolicyStatus());
    if (!client.attendanceName().isEmpty())
    {
        QStringList parts{QStringLiteral("[%1]").arg(client.attendanceName())};
        // The Windows login is an independent identity, not the student's name
        // or the Windows account's optional full name.
        if (!client.userLoginName().isEmpty()) parts.append(client.userLoginName());
        if (!client.computer().name().isEmpty()) parts.append(client.computer().name());
        return parts.join(QStringLiteral(" - ")) + suffix;
    }
    if (content != ComputerListModel::DisplayRoleContent::ComputerName &&
        client.state() == ComputerControlInterface::State::Connected && !client.userLoginName().isEmpty())
    {
        const auto user = client.userFullName().isEmpty() ? client.userLoginName() : client.userFullName();
        return (content == ComputerListModel::DisplayRoleContent::UserName ? user :
            QStringLiteral("%1 - %2").arg(user, client.computer().name())) + suffix;
    }
    return (content == ComputerListModel::DisplayRoleContent::UserName ?
        QCoreApplication::translate("ComputerControlListModel", "[no user]") : client.computer().name()) + suffix;
}

void ComputerCaptionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    const auto attendance = index.data(ComputerListModel::AttendanceNameRole).toString();
    if (attendance.isEmpty())
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QStyleOptionViewItem item(option);
    initStyleOption(&item, index);
    const auto prefix = QStringLiteral("[%1]").arg(attendance);
    if (!item.text.startsWith(prefix))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    const auto style = item.widget ? item.widget->style() : QApplication::style();
    const auto textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &item, item.widget);
    auto text = item.text;
    // Keep the native thumbnail, selection, focus and geometry. Render literal
    // text, never HTML received from a client or read from a class profile.
    auto chrome = item;
    // Keep the original text's geometry for native selection/focus painting;
    // only suppress its glyphs before painting the formatted version below.
    chrome.palette.setColor(QPalette::Text, Qt::transparent);
    chrome.palette.setColor(QPalette::HighlightedText, Qt::transparent);
    style->drawControl(QStyle::CE_ItemViewItem, &chrome, painter, item.widget);
    if (textRect.isEmpty()) return;

    const bool selected = item.state.testFlag(QStyle::State_Selected);
    const bool preloaded = index.data(ComputerListModel::AttendancePreloadedRole).toBool();
    const auto group = !(item.state & QStyle::State_Enabled) ? QPalette::Disabled :
        (item.state & QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
    const auto background = item.palette.color(group, selected ? QPalette::Base :
        preloaded ? QPalette::Midlight : QPalette::Highlight);
    QTextCharFormat badge;
    badge.setBackground(background);
    badge.setForeground(NetLinkAppearance::contrastingText(background));
    badge.setFontItalic(preloaded);
    QTextLayout::FormatRange format;
    format.start = 0;
    format.length = prefix.size();
    format.format = badge;

    QTextOption textOption;
    textOption.setTextDirection(item.direction);
    textOption.setAlignment(item.displayAlignment & Qt::AlignHorizontal_Mask);
    const bool wrap = item.features.testFlag(QStyleOptionViewItem::WrapText);
    textOption.setWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    if (!wrap) text = item.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width());
    QTextLayout layout(text, item.font, painter->device());
    layout.setTextOption(textOption);
    layout.setFormats({format});
    // Limit wrapped captions to the native text area and elide the last line,
    // so even long names cannot paint over another computer's thumbnail.
    if (wrap)
    {
        layout.beginLayout();
        qreal height = 0;
        int lastLineStart = 0;
        while (true)
        {
            auto line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(textRect.width());
            if (height + line.height() > textRect.height() && height > 0)
            {
                text = text.left(lastLineStart) + item.fontMetrics.elidedText(
                    text.mid(lastLineStart), Qt::ElideRight, textRect.width());
                break;
            }
            lastLineStart = line.textStart();
            height += line.height();
        }
        layout.endLayout();
        layout.setText(text);
    }
    format.length = qMin(prefix.size(), text.size());
    layout.setFormats({format});
    layout.beginLayout();
    qreal height = 0;
    while (true)
    {
        auto line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(textRect.width());
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    qreal top = textRect.top();
    if (item.displayAlignment & Qt::AlignVCenter) top += qMax(qreal(0), (textRect.height() - height) / 2);
    else if (item.displayAlignment & Qt::AlignBottom) top += qMax(qreal(0), textRect.height() - height);
    painter->save();
    painter->setClipRect(textRect, Qt::IntersectClip);
    painter->setPen(item.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
    layout.draw(painter, QPointF(textRect.left(), top));
    painter->restore();
}
