// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>

#include <functional>
#include <QQmlEngine>
#include <QRectF>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

/*
 * Crops and turns a picture, keeping its metadata (exiv2), as a copy next to it
 * (“photo (edited).jpg”) or in its place, the original going to the Trash. The
 * picture is decoded as shown (EXIF orientation applied), turned, cropped and
 * written in its own format (JPEG, WebP, AVIF, … at quality 95) where Qt can write
 * it, else as PNG (JPEG for camera raw files). Decoding and writing run off the main
 * thread; the result is written beside the original under a hidden name and only
 * then given its name, so a failure leaves nothing behind.
 */
class ImageEditor : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit ImageEditor(QObject *parent = nullptr);

    bool busy() const;

    // `crop`: the part to keep, as fractions (0-1) of the turned picture; `turn`:
    // degrees counterclockwise (0, 90, 180, 270). `replace`: in place of the original.
    Q_INVOKABLE void save(const QUrl &source, const QRectF &crop, int turn, bool replace);

    // The file type the edited picture is written in (suffix, lower case).
    static QString outputSuffix(const QString &path);

    // How Replace moves the original away: KIO's Trash; tests put a stand-in here,
    // so no test ever touches a real Trash. `done(error)`: empty on success.
    using TrashFunction = std::function<void(const QString &path, QObject *context, std::function<void(const QString &error)> done)>;
    void setTrashFunction(TrashFunction trash);

Q_SIGNALS:
    void busyChanged();
    // `file`: the copy, or the replaced picture (its name may differ when its type
    // had to change, e.g. a GIF written as PNG).
    void saved(const QUrl &file, bool replaced);
    void failed(const QString &message);

private:
    void finish(const QString &source, const QString &written, bool replace);
    void fail(const QString &written, const QString &message);

    bool m_busy = false;
    TrashFunction m_trash;
};
