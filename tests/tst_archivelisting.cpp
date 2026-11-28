// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "archivelisting.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using ArchiveListing::Kind;

class TstArchiveListing : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_bsdtar;

    // Runs kwiklook-archive (without the sandbox: that has its own test).
    static ArchiveListing::Listing list(const QString &archive, const QStringList &extra = {})
    {
        QProcess p;
        p.start(QStringLiteral(ARCHIVE_HELPER), QStringList{archive} + extra);
        p.waitForFinished(20000);
        return ArchiveListing::parse(p.readAllStandardOutput());
    }
    void write(const QString &relative, const QByteArray &data)
    {
        const QString path = m_dir.filePath(u"tree/"_s + relative);
        QDir().mkpath(QFileInfo(path).path());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    void pack(const QString &name, const QStringList &options)
    {
        QProcess p;
        p.setWorkingDirectory(m_dir.filePath(u"tree"_s));
        p.start(m_bsdtar, options + QStringList{u"-cf"_s, m_dir.filePath(name), u"."_s});
        QVERIFY(p.waitForFinished(20000));
        QCOMPARE(p.exitCode(), 0);
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_bsdtar = QStandardPaths::findExecutable(u"bsdtar"_s);
        write(u"docs/readme.md"_s, "# hello\n");
        write(u"docs/deep/a/b/file.txt"_s, QByteArray(1000, 'x'));
        write(u"Ünïcödé 日本語.txt"_s, "unicode\n");
        write(u"tab\there.txt"_s, "tab in the name\n");
        QDir().mkpath(m_dir.filePath(u"tree/empty folder"_s));
    }

    void parsesLines()
    {
        const auto l = ArchiveListing::parse("e\td\t0\t100\t0\tdocs/\n"
                                             "e\tf\t42\t-1\t1\tdocs/a\\tb\\\\c.txt\n"
                                             "e\tl\t0\t5\t0\tlink\n"
                                             "end\t7\t42\tok\t\n");
        QCOMPARE(l.entries.size(), 3);
        QCOMPARE(l.entries.at(0).kind, Kind::Folder);
        QCOMPARE(l.entries.at(1).path, u"docs/a\tb\\c.txt"_s);
        QCOMPARE(l.entries.at(1).size, 42);
        QCOMPARE(l.entries.at(1).modified, -1);
        QVERIFY(l.entries.at(1).encrypted);
        QCOMPARE(l.entries.at(2).kind, Kind::Link);
        QCOMPARE(l.total, 7);
        QCOMPARE(l.unpacked, 42);
        QCOMPARE(l.status, u"ok"_s);
        QVERIFY(ArchiveListing::parse("garbage\n").status.isEmpty());
    }

    void splitsPaths()
    {
        QCOMPARE(ArchiveListing::parts(u"./docs/a.txt"_s), (QStringList{u"docs"_s, u"a.txt"_s}));
        QCOMPARE(ArchiveListing::parts(u"docs/sub/"_s), (QStringList{u"docs"_s, u"sub"_s}));
        QCOMPARE(ArchiveListing::parts(u"./"_s), QStringList());
        QCOMPARE(ArchiveListing::parts(u"a/./b"_s), (QStringList{u"a"_s, u"b"_s}));
        // Names that try to leave stay whole.
        QCOMPARE(ArchiveListing::parts(u"../outside.txt"_s), QStringList{u"../outside.txt"_s});
        QCOMPARE(ArchiveListing::parts(u"/etc/passwd"_s), QStringList{u"/etc/passwd"_s});
        QCOMPARE(ArchiveListing::parts(u"a/../../b"_s), QStringList{u"a/../../b"_s});
    }

    void knowsArchiveTypes()
    {
        QVERIFY(ArchiveListing::isArchive(u"application/zip"_s));
        QVERIFY(ArchiveListing::isArchive(u"application/x-compressed-tar"_s));
        QVERIFY(ArchiveListing::isArchive(u"application/gzip"_s));
        // Zip files underneath, with views of their own.
        QVERIFY(!ArchiveListing::isArchive(u"application/vnd.openxmlformats-officedocument.wordprocessingml.document"_s));
        QVERIFY(!ArchiveListing::isArchive(u"application/vnd.oasis.opendocument.text"_s));
        QVERIFY(!ArchiveListing::isArchive(u"application/epub+zip"_s));
    }

    void helperListsFormats_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QStringList>("options");
        QTest::newRow("zip") << u"a.zip"_s << QStringList{u"--format"_s, u"zip"_s};
        QTest::newRow("7z") << u"a.7z"_s << QStringList{u"--format"_s, u"7zip"_s};
        QTest::newRow("tar.gz") << u"a.tar.gz"_s << QStringList{u"-z"_s};
        QTest::newRow("tar.zst") << u"a.tar.zst"_s << QStringList{u"--zstd"_s};
    }
    void helperListsFormats()
    {
        if (m_bsdtar.isEmpty()) {
            QSKIP("bsdtar is needed to make test archives");
        }
        QFETCH(QString, name);
        QFETCH(QStringList, options);
        pack(name, options);
        const auto l = list(m_dir.filePath(name));
        QCOMPARE(l.status, u"ok"_s);
        QStringList paths;
        for (const auto &e : l.entries) {
            const QStringList parts = ArchiveListing::parts(e.path);
            if (!parts.isEmpty()) {
                paths << parts.join(u'/');
            }
        }
        QVERIFY2(paths.contains(u"Ünïcödé 日本語.txt"_s), qPrintable(paths.join(u", "_s))); // names in UTF-8 without a locale
        QVERIFY(paths.contains(u"tab\there.txt"_s));
        QVERIFY(paths.contains(u"docs/deep/a/b/file.txt"_s));
        QVERIFY(paths.contains(u"empty folder"_s));
        QCOMPARE(l.unpacked, 8 + 1000 + 8 + 16);
    }

    void helperLimitsAndCounts()
    {
        if (m_bsdtar.isEmpty()) {
            QSKIP("bsdtar is needed to make test archives");
        }
        pack(u"limit.zip"_s, {u"--format"_s, u"zip"_s});
        const auto l = list(m_dir.filePath(u"limit.zip"_s), {u"3"_s});
        QCOMPARE(l.entries.size(), 3);
        QVERIFY(l.total > 3); // counted on
        QCOMPARE(l.status, u"ok"_s);
    }

    void helperSingleCompressedFile()
    {
        if (m_bsdtar.isEmpty()) {
            QSKIP("bsdtar is needed to make test archives");
        }
        QProcess gzip;
        gzip.start(u"gzip"_s, {u"-k"_s, m_dir.filePath(u"tree/docs/readme.md"_s)});
        if (!gzip.waitForFinished(10000) || gzip.exitCode() != 0) {
            QSKIP("gzip is needed");
        }
        const auto l = list(m_dir.filePath(u"tree/docs/readme.md.gz"_s));
        QCOMPARE(l.entries.size(), 1);
        QCOMPARE(l.entries.at(0).path, u"readme.md"_s);
        QCOMPARE(l.entries.at(0).size, 8);
    }

    void helperDamagedAndNotAnArchive()
    {
        if (m_bsdtar.isEmpty()) {
            QSKIP("bsdtar is needed to make test archives");
        }
        pack(u"whole.zip"_s, {u"--format"_s, u"zip"_s});
        QFile whole(m_dir.filePath(u"whole.zip"_s));
        QVERIFY(whole.open(QIODevice::ReadOnly));
        QFile half(m_dir.filePath(u"half.zip"_s));
        QVERIFY(half.open(QIODevice::WriteOnly));
        half.write(whole.read(whole.size() / 2));
        half.close();
        const auto damaged = list(half.fileName());
        QVERIFY(damaged.status == u"error"_s || damaged.status == u"ok"_s); // never a crash or hang
        const auto missing = list(m_dir.filePath(u"nothing-here.zip"_s));
        QCOMPARE(missing.status, u"error"_s);
        QVERIFY(missing.entries.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstArchiveListing)
#include "tst_archivelisting.moc"
