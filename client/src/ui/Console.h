#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

// Command console per planning/ui-design/04-client.md §5.
// MVP: prompt + output pane, in-session Up/Down history, menu interop via
// execute(). Network dispatch and error-code badge formatting land with Worker.

namespace griz::ui {

class Console : public QWidget {
    Q_OBJECT
public:
    explicit Console(QWidget *parent = nullptr);

public slots:
    void execute(const QString &cmd);
    void appendOutput(const QString &text);
    void clearOutput();

signals:
    void commandEntered(QString cmd);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onReturnPressed();

private:
    void navigateHistory(int direction);

    QPlainTextEdit *m_output = nullptr;
    QLineEdit      *m_prompt = nullptr;
    QStringList     m_history;
    int             m_historyPos = 0;
};

} // namespace griz::ui
