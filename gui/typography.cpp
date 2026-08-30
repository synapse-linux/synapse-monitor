// SPDX-License-Identifier: MIT
#include "typography.h"

#include <QString>

QFont MonitorTypography::interfaceFont(const QFont &base) {
    QFont font(base);
    font.setFamily(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace, QFont::PreferMatch);
    font.setFixedPitch(true);
    return font;
}
