#include "Viewport.h"

#include <QPainter>
#include <QPaintEvent>

namespace griz::ui {

Viewport::Viewport(QWidget *parent)
    : QOpenGLWidget(parent),
      m_placeholderText(QStringLiteral("No connection — launch griz-server to begin.")) {
    setMinimumSize(480, 360);
}

void Viewport::setPlaceholderText(const QString &text) {
    m_placeholderText = text;
    update();
}

void Viewport::initializeGL() {}

void Viewport::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void Viewport::paintGL() {}

void Viewport::paintEvent(QPaintEvent *event) {
    QOpenGLWidget::paintEvent(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(32, 32, 36));
    painter.setPen(QColor(180, 180, 186));
    painter.drawText(rect(), Qt::AlignCenter, m_placeholderText);
}

} // namespace griz::ui
