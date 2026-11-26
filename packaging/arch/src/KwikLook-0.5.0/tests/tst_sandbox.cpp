// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// kwiklook-sandbox: the probe (sandboxprobe.cpp) runs inside it and tries what a
// converter must and must not be able to do.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

class TstSandbox : public QObject
{
    Q_OBJECT

    QTemporaryDir m_readable; // given with --read
    QTemporaryDir m_writable; // given with --write
    QTemporaryDir m_other; // not given

    // Runs the probe in the sandbox; returns what it printed (trimmed).
    QString probe(const QStringList &probeArgs, const QStringList &extra = {})
    {
        QStringList args{u"--read"_s,
                         QStringLiteral(PROBE_BINARY), // the probe itself must be readable
                         u"--read"_s,
                         m_readable.path(),
                         u"--write"_s,
                         m_writable.path()};
        args << extra << u"--"_s << QStringLiteral(PROBE_BINARY) << probeArgs;
        QProcess process;
        process.start(QStringLiteral(SANDBOX_BINARY), args);
        if (!process.waitForFinished(10000)) {
            return u"(timeout)"_s;
        }
        const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (process.exitCode() != 0) {
            return u"(exit %1: %2)"_s.arg(process.exitCode()).arg(err);
        }
        return QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    }

    static QString write(const QString &path)
    {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("secret\n");
        }
        return path;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QProcess process;
        process.start(QStringLiteral(SANDBOX_BINARY), {u"--"_s, u"true"_s});
        QVERIFY(process.waitForFinished(5000));
        if (process.exitCode() == 77) {
            QSKIP("this kernel's Landlock can't lock a program in (ABI 6 and RESOLVE_UNIX needed)");
        }
        QCOMPARE(process.exitCode(), 0);
    }

    void readsOnlyWhatItIsGiven()
    {
        QCOMPARE(probe({u"read"_s, write(m_readable.filePath(u"in.docx"_s))}), u"ok"_s);
        QCOMPARE(probe({u"read"_s, write(m_writable.filePath(u"work.txt"_s))}), u"ok"_s);
        QCOMPARE(probe({u"list"_s, u"/usr/share/fonts"_s}), u"ok"_s); // system folders stay readable
        QCOMPARE(probe({u"read"_s, write(m_other.filePath(u"private.txt"_s))}), u"denied 13"_s);
        QCOMPARE(probe({u"list"_s, QDir::homePath()}), u"denied 13"_s);
    }

    void writesOnlyWhereItMay()
    {
        QCOMPARE(probe({u"write"_s, m_writable.filePath(u"out.pdf"_s)}), u"ok"_s);
        QVERIFY(QFile::exists(m_writable.filePath(u"out.pdf"_s)));
        QCOMPARE(probe({u"write"_s, m_readable.filePath(u"changed.txt"_s)}), u"denied 13"_s);
        QCOMPARE(probe({u"write"_s, m_other.filePath(u"dropped.txt"_s)}), u"denied 13"_s);
        QCOMPARE(probe({u"write"_s, u"/tmp/kwiklook-sandbox-probe"_s}), u"denied 13"_s);
        QVERIFY(!QFile::exists(u"/tmp/kwiklook-sandbox-probe"_s));
    }

    void noSocketsOfAnyKind()
    {
        QCOMPARE(probe({u"tcp"_s}), u"denied 13"_s);
        QCOMPARE(probe({u"udp"_s}), u"denied 13"_s);
        // A local service listening in a readable folder (like the session bus).
        const QByteArray path = QFile::encodeName(m_readable.filePath(u"service.sock"_s));
        const int server = socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(server >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        qstrncpy(addr.sun_path, path.constData(), sizeof(addr.sun_path));
        QCOMPARE(bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
        QCOMPARE(listen(server, 1), 0);
        QCOMPARE(probe({u"unix"_s, QString::fromLocal8Bit(path)}), u"denied 13"_s);
        close(server);
        // An abstract socket listening outside (like X11's @/tmp/.X11-unix/X0).
        const int abstractServer = socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(abstractServer >= 0);
        sockaddr_un abstractAddr{};
        abstractAddr.sun_family = AF_UNIX;
        qstrcpy(abstractAddr.sun_path + 1, "kwiklook-probe"); // the name the probe connects to
        QCOMPARE(bind(abstractServer, reinterpret_cast<sockaddr *>(&abstractAddr), sizeof(abstractAddr)), 0);
        QCOMPARE(listen(abstractServer, 1), 0);
        QCOMPARE(probe({u"abstract"_s}), u"denied 1"_s); // EPERM for abstract sockets (Landlock scope)
        close(abstractServer);
        // Its own sockets work (LibreOffice talks to itself through one).
        QCOMPARE(probe({u"selfsocket"_s, m_writable.filePath(u"own.sock"_s)}), u"ok"_s);
    }

    void socketDirOnlyTakesSockets()
    {
        // As /tmp for LibreOffice's pipe: sockets can be made and removed there, files
        // can't be read, written or made.
        const QStringList dir{u"--socket-dir"_s, m_other.path()};
        QCOMPARE(probe({u"selfsocket"_s, m_other.filePath(u"pipe.sock"_s)}, dir), u"ok"_s);
        QCOMPARE(probe({u"read"_s, write(m_other.filePath(u"theirs.txt"_s))}, dir), u"denied 13"_s);
        QCOMPARE(probe({u"write"_s, m_other.filePath(u"new.txt"_s)}, dir), u"denied 13"_s);
    }

    void startsWithACleanEnvironment()
    {
        QCOMPARE(probe({u"env"_s, u"HOME"_s}), u"(unset)"_s);
        QCOMPARE(probe({u"env"_s, u"DBUS_SESSION_BUS_ADDRESS"_s}), u"(unset)"_s);
        QCOMPARE(probe({u"env"_s, u"WAYLAND_DISPLAY"_s}), u"(unset)"_s);
        QCOMPARE(probe({u"env"_s, u"HOME"_s}, {u"--env"_s, u"HOME=%1"_s.arg(m_writable.path())}), m_writable.path());
        QCOMPARE(probe({u"fds"_s}), u"0"_s);
    }

    void badUsageIsAnError()
    {
        QProcess process;
        process.start(QStringLiteral(SANDBOX_BINARY), {u"--read"_s});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 78);
    }
};

QTEST_GUILESS_MAIN(TstSandbox)
#include "tst_sandbox.moc"
