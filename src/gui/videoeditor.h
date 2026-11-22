// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QProcess;

/*
 * Trims and rotates a video without re-encoding it (ffmpeg copies the streams), as a
 * copy next to the original (“clip (edited).mp4”); the original is never touched.
 * ffprobe and ffmpeg run in kwiklook-sandbox: they read the video, and write only
 * into a scratch folder in the cache, from where the result is moved into place.
 *
 * A cut can only start on a keyframe: probe() lists them, and the editor snaps the
 * start there. Rotation only changes the file's display-rotation flag.
 *
 *   VideoEditor { id: editor; source: Viewer.currentUrl }
 *   editor.probe(); ... editor.save(2.0, 10.5, 90);
 */
class VideoEditor : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    // ffmpeg, ffprobe and the sandbox are there (else the editing tools are hidden).
    Q_PROPERTY(bool available READ available CONSTANT)
    // After probe(): keyframe times in seconds, and the video's own rotation
    // (degrees, counterclockwise, as ffmpeg's display matrix has it).
    Q_PROPERTY(QList<qreal> keyframes READ keyframes NOTIFY probed)
    Q_PROPERTY(int rotation READ rotation NOTIFY probed)
    Q_PROPERTY(bool probing READ probing NOTIFY busyChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY busyChanged)

public:
    explicit VideoEditor(QObject *parent = nullptr);
    ~VideoEditor() override;

    QUrl source() const;
    void setSource(const QUrl &source);
    static bool available();
    QList<qreal> keyframes() const;
    int rotation() const;
    bool probing() const;
    bool saving() const;

    // Lists the keyframes and the rotation (reads the whole file: may take a moment).
    Q_INVOKABLE void probe();
    // The last keyframe at or before `seconds` (0 before probing).
    Q_INVOKABLE qreal keyframeAtOrBefore(qreal seconds) const;
    // Writes the copy: from `start` to `end` seconds (end <= 0: to the end), turned by
    // `turn` degrees counterclockwise on top of the video's own rotation.
    Q_INVOKABLE void save(qreal start, qreal end, int turn);
    // Stops probing or saving; nothing is left behind.
    Q_INVOKABLE void cancel();

    // Where the copy of `path` goes: “name (edited).ext” next to it, or a free name.
    static QString copyPath(const QString &path);

Q_SIGNALS:
    void sourceChanged();
    void probed();
    void busyChanged();
    void saved(const QUrl &copy);
    void failed(const QString &message);

private:
    QProcess *run(const QStringList &args, const QString &writeDir);
    void stop();
    void finishSave(int exitCode);
    QString workDir() const;

    QUrl m_source;
    QList<qreal> m_keyframes;
    int m_rotation = 0;
    QPointer<QProcess> m_probe;
    bool m_parsing = false; // the probe's output is being read (off the main thread)
    int m_generation = 0; // drops a probe's result after stop()
    QPointer<QProcess> m_save;
    QString m_output; // the copy in the scratch folder while saving
};
