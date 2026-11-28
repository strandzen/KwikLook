// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Office previews: LibreOffice converts in kwiklook-sandbox, results are cached.
// Needs LibreOffice; skipped without it.

#include "officeconverter.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{
// A tiny OpenDocument text in the flat XML format (one file, no zip).
QString writeDocument(const QString &path, const QString &text)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
 office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text">
 <office:body><office:text><text:p>)");
        f.write(text.toUtf8());
        f.write("</text:p></office:text></office:body></office:document>\n");
    }
    return path;
}

// Processes whose command line mentions `text` (LibreOffice left running?).
int processesMentioning(const QString &text)
{
    int found = 0;
    const QStringList pids = QDir(u"/proc"_s).entryList(QDir::Dirs);
    for (const QString &pid : pids) {
        QFile cmdline(u"/proc/%1/cmdline"_s.arg(pid));
        if (pid.toInt() > 0 && cmdline.open(QIODevice::ReadOnly) && cmdline.readAll().contains(text.toUtf8())) {
            ++found;
        }
    }
    return found;
}
} // namespace

class TstOfficeConverter : public QObject
{
    Q_OBJECT

    QTemporaryDir m_files;
    QTemporaryDir m_cache;

private Q_SLOTS:
    void initTestCase()
    {
        if (!OfficeConverter::available()) {
            QSKIP("LibreOffice or kwiklook-sandbox not found");
        }
        if (!OfficeConverter::sandboxSupported()) {
            QSKIP("this kernel's Landlock can't lock LibreOffice in");
        }
    }

    void convertsAndCaches()
    {
        OfficeConverter converter;
        converter.setCacheDir(m_cache.path());
        QSignalSpy converted(&converter, &OfficeConverter::converted);
        QSignalSpy failed(&converter, &OfficeConverter::failed);
        const QString doc = writeDocument(m_files.filePath(u"letter.fodt"_s), u"Hello from KwikLook"_s);
        converter.convert(doc);
        QVERIFY(converted.wait(60000));
        QCOMPARE(failed.size(), 0);
        QCOMPARE(converted.at(0).at(0).toString(), doc);
        const QString pdf = converted.at(0).at(1).toUrl().toLocalFile();
        QVERIFY(pdf.startsWith(m_cache.path()));
        QFile file(pdf);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(5) == "%PDF-");
        // The scratch folder is gone.
        QCOMPARE(QDir(m_cache.path()).entryList({u"work-*"_s}, QDir::Dirs).size(), 0);

        // Again: from the cache, at once.
        converter.convert(doc);
        QCOMPARE(converted.size(), 2);
        QCOMPARE(converted.at(1).at(1).toUrl().toLocalFile(), pdf);

        // Changed: converted again, to a new entry.
        QTest::qWait(10); // a different modification time
        writeDocument(doc, u"Changed text, longer than before"_s);
        converter.convert(doc);
        QVERIFY(converted.wait(60000));
        QVERIFY(converted.at(2).at(1).toUrl().toLocalFile() != pdf);
    }

    void brokenDocumentFails()
    {
        OfficeConverter converter;
        converter.setCacheDir(m_cache.path());
        QSignalSpy failed(&converter, &OfficeConverter::failed);
        QFile broken(m_files.filePath(u"broken.docx"_s));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("PK\x03\x04 this is not really a document");
        broken.close();
        converter.convert(broken.fileName());
        QVERIFY(failed.wait(60000));
        QCOMPARE(failed.at(0).at(1).value<OfficeConverter::Failure>(), OfficeConverter::Failure::Failed);
    }

    void cancelStopsLibreOffice()
    {
        OfficeConverter converter;
        converter.setCacheDir(m_cache.path());
        QSignalSpy converted(&converter, &OfficeConverter::converted);
        converter.convert(writeDocument(m_files.filePath(u"cancelled.fodt"_s), u"Never shown"_s));
        const QString work = converter.workDir();
        QVERIFY(!work.isEmpty());
        // The fresh profile has macros and link updates turned off.
        QFile settings(work + u"/profile/user/registrymodifications.xcu"_s);
        QVERIFY(settings.open(QIODevice::ReadOnly));
        const QByteArray xcu = settings.readAll();
        QVERIFY(xcu.contains("DisableMacrosExecution"));
        QVERIFY(xcu.contains("oor:finalized=\"true\"><value>true</value>"));
        QTRY_VERIFY_WITH_TIMEOUT(processesMentioning(work) > 0, 5000); // LibreOffice runs
        converter.cancel();
        QCOMPARE(processesMentioning(work), 0); // all of it, not only the first process
        QVERIFY(!QDir(work).exists());
        QTest::qWait(200);
        QCOMPARE(converted.size(), 0);
    }

    void staleScratchFoldersGo()
    {
        // Left by a viewer that was killed mid-conversion.
        QTemporaryDir cache;
        QVERIFY(QDir().mkpath(cache.filePath(u"work-1/profile/user"_s)));
        OfficeConverter converter;
        converter.setCacheDir(cache.path());
        QSignalSpy converted(&converter, &OfficeConverter::converted);
        converter.convert(writeDocument(m_files.filePath(u"stale.fodt"_s), u"Stale"_s));
        QVERIFY(!QDir(cache.filePath(u"work-1"_s)).exists());
        QVERIFY(converted.wait(60000));
    }

    void cacheIsKeptSmall()
    {
        OfficeConverter converter;
        QTemporaryDir cache;
        converter.setCacheDir(cache.path());
        // Old entries: 3 × 100 KB, oldest first.
        for (int i = 0; i < 3; ++i) {
            QFile old(cache.filePath(u"old%1.pdf"_s.arg(i)));
            QVERIFY(old.open(QIODevice::WriteOnly));
            old.write(QByteArray(100 * 1024, 'x'));
            old.setFileTime(QDateTime::currentDateTime().addDays(-10 + i), QFileDevice::FileModificationTime);
        }
        converter.setCacheLimit(250 * 1024);
        QSignalSpy converted(&converter, &OfficeConverter::converted);
        converter.convert(writeDocument(m_files.filePath(u"new.fodt"_s), u"New"_s));
        QVERIFY(converted.wait(60000));
        // The new PDF and the newest old ones that fit stay; the oldest went.
        QVERIFY(QFile::exists(converted.at(0).at(1).toUrl().toLocalFile()));
        QVERIFY(!QFile::exists(cache.filePath(u"old0.pdf"_s)));
        QVERIFY(QFile::exists(cache.filePath(u"old2.pdf"_s)));
    }
};

QTEST_GUILESS_MAIN(TstOfficeConverter)
#include "tst_officeconverter.moc"
