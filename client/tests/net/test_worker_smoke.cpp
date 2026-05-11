// End-to-end smoke for net/Worker. Spawns a live griz-server --transport=rpc,
// performs the handshake, and exchanges one q_state over the wire. Skips (as a
// Qt Test SKIP) if GRIZ_BIN / the sample database can't be located.

#include "net/Worker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>
#include <QtTest>

namespace {

QString findServerBinary() {
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString override = env.value(QStringLiteral("GRIZ_BIN"));
    if (!override.isEmpty() && QFileInfo(override).isExecutable()) {
        return override;
    }

    const QDir repoRoot(QStringLiteral(GRIZ_REPO_ROOT));
    QFileInfoList candidates;
    const QDir srcDir(repoRoot.filePath(QStringLiteral("Src")));
    const QStringList buildDirs = srcDir.entryList({QStringLiteral("GRIZ4-*")}, QDir::Dirs);
    for (const QString &entry : buildDirs) {
        const QString binPath = srcDir.filePath(entry
                                                + QStringLiteral("/bin_server_opt/griz-server"));
        QFileInfo info(binPath);
        if (info.isExecutable()) {
            candidates.append(info);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() > b.lastModified();
    });
    if (!candidates.isEmpty()) {
        return candidates.first().absoluteFilePath();
    }
    return {};
}

QString findSampleDatabase() {
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString override = env.value(QStringLiteral("GRIZ_TEST_DB"));
    if (!override.isEmpty() && QFileInfo(override).exists()) {
        return override;
    }
    const QDir repoRoot(QStringLiteral(GRIZ_REPO_ROOT));
    const QString db = repoRoot.filePath(QStringLiteral("Src/test/image/bar71/bar71.pltA"));
    return QFileInfo(db).exists() ? db : QString();
}

} // namespace

class WorkerSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void launchAndConnectSucceeds();
    void qStateReturnsObject();
    void cleanupTestCase();

private:
    QString           m_serverBinary;
    QString           m_databasePath;
    griz::net::Worker m_worker;
    bool              m_sessionUp = false;
};

void WorkerSmokeTest::initTestCase() {
    m_serverBinary = findServerBinary();
    if (m_serverBinary.isEmpty()) {
        QSKIP("griz-server not found; set GRIZ_BIN or run ./build.sh server");
    }
    m_databasePath = findSampleDatabase();
    if (m_databasePath.isEmpty()) {
        QSKIP("sample database (Src/test/image/bar71/bar71.pltA) not present");
    }
}

void WorkerSmokeTest::launchAndConnectSucceeds() {
    const bool ok = m_worker.launchAndConnect(m_serverBinary, m_databasePath, 256, 256, 30000);
    if (!ok) {
        qWarning() << "launchAndConnect failed:" << m_worker.errorString();
    }
    QVERIFY(ok);
    QVERIFY(m_worker.isConnected());

    const QJsonObject info = m_worker.serverInfo();
    QCOMPARE(info.value(QStringLiteral("version")).toString(), QStringLiteral("1.0"));
    QCOMPARE(info.value(QStringLiteral("server")).toString(),  QStringLiteral("griz-server"));
    QCOMPARE(info.value(QStringLiteral("compatible")).toBool(), true);

    m_sessionUp = true;
}

void WorkerSmokeTest::qStateReturnsObject() {
    if (!m_sessionUp) {
        QSKIP("previous launchAndConnect failed");
    }
    griz::net::Response response;
    const bool ok = m_worker.runCommand(QStringLiteral("q_state"), 10000, &response);
    if (!ok) {
        qWarning() << "q_state failed:" << m_worker.errorString();
    }
    QVERIFY(ok);
    QVERIFY(response.ok);
    QVERIFY(!response.requestId.isEmpty());
    QVERIFY(!response.data.isEmpty());
    QVERIFY(response.data.contains(QStringLiteral("database"))
            || response.data.contains(QStringLiteral("time"))
            || response.data.contains(QStringLiteral("view")));
}

void WorkerSmokeTest::cleanupTestCase() {
    m_worker.shutdown(5000);
}

QTEST_GUILESS_MAIN(WorkerSmokeTest)
#include "test_worker_smoke.moc"
