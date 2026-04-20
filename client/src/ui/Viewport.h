#pragma once

#include <QOpenGLWidget>
#include <QString>

// Streamed-frame viewport. See planning/ui-design/04-client.md §6.
// MVP behaviour: renders a neutral background + placeholder label until frames
// start arriving. Frame upload + resize throttling land in the Phase 3 mailbox
// wiring (planning/UI.md Phase 3 last bullet).

namespace griz::ui {

class Viewport : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit Viewport(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_placeholderText;
};

} // namespace griz::ui
