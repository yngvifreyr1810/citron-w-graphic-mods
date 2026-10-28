// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QAbstractItemView>
#include <QComboBox>
#include <QWheelEvent>

namespace GraphicPacks {
// Closed settings controls must not consume a wheel gesture meant for the panel.
// Keyboard navigation and scrolling an explicitly opened popup remain available.
class ScrollSafeComboBox final : public QComboBox {
public:
    explicit ScrollSafeComboBox(QWidget* parent = nullptr) : QComboBox(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }
protected:
    void wheelEvent(QWheelEvent* event) override {
        if (view()->isVisible()) QComboBox::wheelEvent(event);
        else event->ignore();
    }
};
} // namespace GraphicPacks
