// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/imageeditor.h"

#include <exiv2/exiv2.hpp>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;

// Colours that survive JPEG well enough to compare.
static bool near(QRgb a, QColor b)
{
    return std::abs(qRed(a) - b.red()) < 40 && std::abs(qGreen(a) - b.green()) < 40 && std::abs(qBlue(a) - b.blue()) < 40;
}

class TstImageEditor : public QObject
{
    Q_OBJECT

    QString m_dir; // in the build folder: the same disk as the scratch Trash

    // 40×20: left half red, right half blue, a green 4×4 square in the top-right corner.
    static QImage picture()
    {
        QImage image(40, 20, QImage::Format_RGB32);
        image.fill(Qt::red);
        for (int y = 0; y < 20; ++y) {
            for (int x = 20; x < 40; ++x) {
                image.setPixelColor(x, y, Qt::blue);
            }
        }
        for (int y = 0; y < 4; ++y) {
            for (int x = 36; x < 40; ++x) {
                image.setPixelColor(x, y, Qt::green);
            }
        }
        return image;
    }
    QString write(const QString &name, int orientation = 0)
    {
        const QString path = m_dir + u'/' + name;
        QFile::remove(path);
        QImage image = picture();
        if (orientation == 6) {
            // Stored turned left; the Exif flag says “turn right to show”: shown as picture().
            image = image.transformed(QTransform().rotate(-90));
        }
        if (!image.save(path)) {
            return QString();
        }
        if (name.endsWith(u".jpg"_s)) {
            auto file = Exiv2::ImageFactory::open(QFile::encodeName(path).toStdString());
            Exiv2::ExifData exif;
            exif["Exif.Image.Make"] = "KwikLook Camera";
            exif["Exif.Photo.DateTimeOriginal"] = "2026:11:22 12:34:56";
            exif["Exif.GPSInfo.GPSLatitudeRef"] = "N";
            if (orientation) {
                exif["Exif.Image.Orientation"] = uint16_t(orientation);
            }
            file->setExifData(exif);
            file->writeMetadata();
        }
        return path;
    }
    static QString exif(const QString &path, const char *key)
    {
        auto file = Exiv2::ImageFactory::open(QFile::encodeName(path).toStdString());
        file->readMetadata();
        const auto it = file->exifData().findKey(Exiv2::ExifKey(key));
        return it == file->exifData().end() ? QString() : QString::fromStdString(it->toString());
    }
    static QByteArray hash(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1) : QByteArray();
    }
    // Runs a save, returns the file written (empty on failure).
    static QString save(const QString &path, QRectF crop, int turn, bool replace, QString *failure = nullptr)
    {
        ImageEditor editor;
        editor.setTrashFunction([](const QString &file, QObject *, std::function<void(const QString &)> done) {
            const bool moved = QFile::rename(file, trashDir() + u'/' + QFileInfo(file).fileName());
            QMetaObject::invokeMethod(qApp, [done, moved] { done(moved ? QString() : u"could not move"_s); }, Qt::QueuedConnection);
        });
        QSignalSpy saved(&editor, &ImageEditor::saved);
        QSignalSpy failed(&editor, &ImageEditor::failed);
        editor.save(QUrl::fromLocalFile(path), crop, turn, replace);
        if (!QTest::qWaitFor([&] { return saved.size() + failed.size() > 0; }, 15000)) {
            return QString();
        }
        if (!failed.isEmpty()) {
            if (failure) {
                *failure = failed.first().first().toString();
            }
            return QString();
        }
        return saved.first().first().toUrl().toLocalFile();
    }
    static QStringList hiddenLeftovers(const QString &dir)
    {
        return QDir(dir).entryList({u"*.kwiklook-edit.*"_s}, QDir::Files | QDir::Hidden);
    }
    // Replace's “Trash” in these tests: a scratch folder (KIO's real Trash put files
    // of this disk into the user's disk trash, .Trash-1000, whatever XDG_DATA_HOME said).
    static QString trashDir()
    {
        return QStringLiteral(TEST_SCRATCH) + u"/trash"_s;
    }
    static QStringList trashed()
    {
        return QDir(trashDir()).entryList(QDir::Files);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStringLiteral(TEST_SCRATCH)).removeRecursively();
        m_dir = QStringLiteral(TEST_SCRATCH) + u"/pictures"_s;
        QVERIFY(QDir().mkpath(m_dir));
        QVERIFY(QDir().mkpath(trashDir()));
    }

    void cropsACopyAndKeepsMetadata()
    {
        const QString path = write(u"photo.jpg"_s);
        QVERIFY(!path.isEmpty());
        const QByteArray before = hash(path);
        // The left half: 20×20, red.
        const QString copy = save(path, QRectF(0, 0, 0.5, 1), 0, false);
        QCOMPARE(copy, m_dir + u"/photo (edited).jpg"_s);
        const QImage result(copy);
        QCOMPARE(result.size(), QSize(20, 20));
        QVERIFY(near(result.pixel(10, 10), Qt::red));
        // Metadata kept, size and orientation set, the original untouched.
        QCOMPARE(exif(copy, "Exif.Image.Make"), u"KwikLook Camera"_s);
        QCOMPARE(exif(copy, "Exif.Photo.DateTimeOriginal"), u"2026:11:22 12:34:56"_s);
        QCOMPARE(exif(copy, "Exif.GPSInfo.GPSLatitudeRef"), u"N"_s);
        QCOMPARE(exif(copy, "Exif.Image.Orientation"), u"1"_s);
        QCOMPARE(exif(copy, "Exif.Photo.PixelXDimension"), u"20"_s);
        QCOMPARE(hash(path), before);
        // Again: a new name.
        QCOMPARE(save(path, QRectF(0, 0, 0.5, 1), 0, false), m_dir + u"/photo (edited) (1).jpg"_s);
        QVERIFY(hiddenLeftovers(m_dir).isEmpty());
    }

    void turnsCounterclockwise()
    {
        const QString path = write(u"turn.png"_s);
        // A quarter turn left: 20×40, the green corner (top right) now top left.
        const QString copy = save(path, QRectF(0, 0, 1, 1), 90, false);
        const QImage result(copy);
        QCOMPARE(result.size(), QSize(20, 40));
        QVERIFY(near(result.pixel(1, 1), Qt::green));
        QVERIFY(near(result.pixel(10, 30), Qt::red)); // the left half is at the bottom now
        // Right (270 counterclockwise): the green corner at the bottom right.
        const QImage right(save(path, QRectF(0, 0, 1, 1), 270, false));
        QVERIFY(near(right.pixel(18, 38), Qt::green));
        // Crop after turning: the crop is in the turned picture's terms (its top half).
        const QImage top(save(path, QRectF(0, 0, 1, 0.5), 90, false));
        QCOMPARE(top.size(), QSize(20, 20));
        QVERIFY(near(top.pixel(10, 10), Qt::blue));
    }

    void followsTheExifOrientation()
    {
        // Stored turned, shown upright (Exif orientation 6): edits are of what's shown.
        const QString path = write(u"turned.jpg"_s, 6);
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QCOMPARE(reader.read().size(), QSize(40, 20));
        const QString copy = save(path, QRectF(0.5, 0, 0.5, 1), 0, false); // the right half
        const QImage result(copy); // read without applying any orientation
        QCOMPARE(result.size(), QSize(20, 20));
        QVERIFY(near(result.pixel(10, 10), Qt::blue));
        QVERIFY(near(result.pixel(18, 1), Qt::green));
        QCOMPARE(exif(copy, "Exif.Image.Orientation"), u"1"_s);
    }

    void replacesThroughTheTrash()
    {
        const QString path = write(u"replace.jpg"_s);
        const QByteArray before = hash(path);
        const QString result = save(path, QRectF(0, 0, 0.5, 1), 0, true);
        QCOMPARE(result, path); // same name
        QCOMPARE(QImage(path).size(), QSize(20, 20));
        QVERIFY(hash(path) != before);
        QCOMPARE(exif(path, "Exif.Image.Make"), u"KwikLook Camera"_s);
        QVERIFY2(trashed().contains(u"replace.jpg"_s), qPrintable(trashed().join(u", "_s)));
        QVERIFY(hiddenLeftovers(m_dir).isEmpty());
    }

    void typesQtCantWrite()
    {
        QCOMPARE(ImageEditor::outputSuffix(m_dir + u"/a.gif"_s), u"png"_s);
        QCOMPARE(ImageEditor::outputSuffix(m_dir + u"/a.JPEG"_s), u"jpg"_s);
        QCOMPARE(ImageEditor::outputSuffix(m_dir + u"/a.webp"_s), u"webp"_s);
        QCOMPARE(ImageEditor::outputSuffix(m_dir + u"/a.cr2"_s), u"jpg"_s); // camera raw: a photo
        // A still GIF (Qt reads, doesn't write): the copy is a PNG; replacing gives
        // “still.png” and the GIF goes to the Trash.
        const QString magick = QStandardPaths::findExecutable(u"magick"_s);
        if (magick.isEmpty()) {
            QSKIP("ImageMagick isn't installed (makes the test GIF)");
        }
        const QString png = write(u"gifsource.png"_s);
        const QString gif = m_dir + u"/still.gif"_s;
        QFile::remove(gif);
        QCOMPARE(QProcess::execute(magick, {png, gif}), 0);
        QCOMPARE(save(gif, QRectF(0, 0, 0.5, 1), 0, false), m_dir + u"/still (edited).png"_s);
        QCOMPARE(save(gif, QRectF(0, 0, 0.5, 1), 0, true), m_dir + u"/still.png"_s);
        QVERIFY(!QFile::exists(gif));
        QVERIFY(trashed().contains(u"still.gif"_s));
        QCOMPARE(QImage(m_dir + u"/still.png"_s).size(), QSize(20, 20));
    }

    void failureLeavesNothing()
    {
        const QString path = write(u"locked.png"_s);
        const QString lockedDir = m_dir + u"/locked"_s;
        QDir().mkpath(lockedDir);
        const QString inside = lockedDir + u"/locked.png"_s;
        QFile::remove(inside);
        QVERIFY(QFile::copy(path, inside));
        QFile::setPermissions(lockedDir, QFile::ReadOwner | QFile::ExeOwner); // can't write there
        QString failure;
        QVERIFY(save(inside, QRectF(0, 0, 0.5, 1), 0, false, &failure).isEmpty());
        QVERIFY(!failure.isEmpty());
        QCOMPARE(QDir(lockedDir).entryList(QDir::Files | QDir::Hidden), QStringList{u"locked.png"_s});
        QFile::setPermissions(lockedDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
};

QTEST_MAIN(TstImageEditor)
#include "tst_imageeditor.moc"
