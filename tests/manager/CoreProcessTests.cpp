#include "CoreProcess.h"
#include "system/WindowsSupport.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class CoreProcessTests : public QObject {
    Q_OBJECT
    QString probe() const {
        return QCoreApplication::applicationDirPath() + "/aetherwall_core_launch_probe.exe";
    }
  private slots:
    void missingExecutableAndEarlyExitHaveDistinctResults() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CoreProcess process;
        QVERIFY(!process.start(probe() + ".missing", false, directory.path()).isEmpty());
        QFile exit(directory.filePath("exit"));
        QVERIFY(exit.open(QIODevice::WriteOnly));
        exit.close();
        QVERIFY(process.start(probe(), false, directory.path()).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(process.exitCode().has_value(), 5000);
        QCOMPARE(*process.exitCode(), quint32(23));
    }
    void observationDoesNotOwnChildLifetime() {
        QTemporaryDir directory(QDir::tempPath() +
                                QStringLiteral("/AetherWall запуск space-XXXXXX"));
        QVERIFY(directory.isValid());
        DWORD pid = 0;
        {
            CoreProcess process;
            QVERIFY(process.start(probe(), false, directory.path()).isEmpty());
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory.filePath("pid")), 5000);
            QFile file(directory.filePath("pid"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QTRY_VERIFY_WITH_TIMEOUT(file.size() > 0, 2000);
            pid = file.readAll().toUInt();
            QVERIFY(pid != 0);
            QVERIFY(!process.start(probe(), false, directory.path()).isEmpty());
        }
        aetherwall::UniqueHandle child(
            OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        QVERIFY(child);
        QCOMPARE(WaitForSingleObject(child.get(), 0), DWORD(WAIT_TIMEOUT));
        QFile stop(directory.filePath("stop"));
        QVERIFY(stop.open(QIODevice::WriteOnly));
        stop.close();
        QCOMPARE(WaitForSingleObject(child.get(), 5000), DWORD(WAIT_OBJECT_0));
    }
};
QTEST_GUILESS_MAIN(CoreProcessTests)
#include "CoreProcessTests.moc"
