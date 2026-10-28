// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

namespace GameListPresentation {
// The complete list remains in ToolTipRole / Properties. Keep the library row
// to at most two lines instead of clipping or scrolling a list of patch files.
inline QString SummarizeAddons(const QString& details) {
    QStringList updates;
    int dlc = 0;
    int mods = 0;
    int disabled_mods = 0;
    const QRegularExpression update(QStringLiteral("^(?:.*/)?Update\\s+v([^\\s(]+)"), QRegularExpression::CaseInsensitiveOption);
    for (auto line : details.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        const bool disabled = line.startsWith(QStringLiteral("[D] "));
        if (disabled) line.remove(0, 4);
        const auto version = update.match(line);
        if (version.hasMatch()) {
            if (!disabled) updates << version.captured(1);
        } else if (line.startsWith(QStringLiteral("DLC"), Qt::CaseInsensitive) ||
                   line.contains(QStringLiteral("Add-On"), Qt::CaseInsensitive)) {
            if (!disabled) ++dlc;
        } else if (disabled) ++disabled_mods;
        else ++mods;
    }
    updates.removeDuplicates();
    QStringList rows;
    if (updates.size() == 1) rows << QCoreApplication::translate("GameList", "Update v%1").arg(updates.front());
    else if (updates.size() > 1) rows << QCoreApplication::translate("GameList", "%1 updates").arg(updates.size());
    QStringList extras;
    if (dlc) extras << QCoreApplication::translate("GameList", "DLC (%1)").arg(dlc);
    if (mods == 1) extras << QCoreApplication::translate("GameList", "1 mod");
    else if (mods > 1) extras << QCoreApplication::translate("GameList", "%1 mods").arg(mods);
    else if (disabled_mods) extras << QCoreApplication::translate("GameList", "Mods disabled");
    if (!extras.isEmpty()) rows << extras.join(QStringLiteral(" · "));
    return rows.join(QLatin1Char('\n'));
}
} // namespace GameListPresentation
