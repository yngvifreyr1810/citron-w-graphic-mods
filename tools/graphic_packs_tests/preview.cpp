// SPDX-License-Identifier: GPL-2.0-or-later
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPalette>
#include "dialog.h"

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(45, 48, 50));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(35, 38, 40));
    palette.setColor(QPalette::AlternateBase, QColor(45, 48, 50));
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(55, 58, 60));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor(45, 115, 180));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    application.setPalette(palette);
    const auto args = application.arguments();
    if (args.size() < 4) return 2;
    QFile file(args[1]);
    if (!file.open(QIODevice::ReadOnly)) return 3;
    QVector<GraphicPacks::Game> games;
    for (const auto& value : QJsonDocument::fromJson(file.readAll()).array()) {
        const auto object = value.toObject();
        GraphicPacks::Game game;
        game.title_id = object.value(QStringLiteral("title_id")).toString();
        game.name = object.value(QStringLiteral("name")).toString();
        game.version = object.value(QStringLiteral("version")).toString();
        game.build_id = object.value(QStringLiteral("build_id")).toString();
        for (const auto& mod : object.value(QStringLiteral("disabled_mods")).toArray()) game.disabled_mods << mod.toString();
        games << game;
    }
    GraphicPacks::Dialog dialog(games, args[2], args[3], true);
    dialog.show();
    return application.exec();
}
