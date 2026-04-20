#include "Console.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace griz::ui {

Console::Console(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("Command output will appear here."));

    m_prompt = new QLineEdit(this);
    m_prompt->setPlaceholderText(QStringLiteral("griz> "));
    m_prompt->installEventFilter(this);

    layout->addWidget(m_output, 1);
    layout->addWidget(m_prompt, 0);

    connect(m_prompt, &QLineEdit::returnPressed, this, &Console::onReturnPressed);
}

void Console::execute(const QString &cmd) {
    if (cmd.isEmpty()) {
        return;
    }
    m_output->appendPlainText(QStringLiteral("> %1").arg(cmd));
    m_history.append(cmd);
    m_historyPos = m_history.size();
    emit commandEntered(cmd);
}

void Console::appendOutput(const QString &text) {
    if (text.isEmpty()) {
        return;
    }
    m_output->appendPlainText(text);
}

void Console::onReturnPressed() {
    const QString cmd = m_prompt->text().trimmed();
    m_prompt->clear();
    execute(cmd);
}

bool Console::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_prompt && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Up) {
            navigateHistory(-1);
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            navigateHistory(+1);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void Console::navigateHistory(int direction) {
    if (m_history.isEmpty()) {
        return;
    }
    const int next = qBound(0, m_historyPos + direction, m_history.size());
    m_historyPos = next;
    m_prompt->setText(next == m_history.size() ? QString() : m_history.at(next));
}

} // namespace griz::ui
