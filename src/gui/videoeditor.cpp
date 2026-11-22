// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "videoeditor.h"
#include "sandbox.h"

#include <KFileUtils>
#include <KIO/FileCopyJob>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <algorithm>
#include <csignal>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcVideo, "kwiklook.video")

namespace
{
struct Probe {
    QList<qreal> keyframes;
    int rotation = 0;
};

// ffprobe's JSON: video packets (time, flags; K = keyframe), the stream's display
// rotation, the file's start time (times are counted from it, as the player and
// ffmpeg's -ss do).
Probe parseProbe(const QByteArray &json)
{
    Probe probe;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const qreal startTime = root.value(u"format"_s).toObject().value(u"start_time"_s).toString().toDouble();
    const QJsonArray packets = root.value(u"packets"_s).toArray();
    for (const QJsonValue &value : packets) {
        const QJsonObject packet = value.toObject();
        if (packet.value(u"flags"_s).toString().startsWith(u'K')) {
            probe.keyframes << std::max(0.0, packet.value(u"pts_time"_s).toString().toDouble() - startTime);
        }
    }
    std::sort(probe.keyframes.begin(), probe.keyframes.end());
    const QJsonArray streams = root.value(u"streams"_s).toArray();
    if (!streams.isEmpty()) {
        const QJsonArray sideData = streams.first().toObject().value(u"side_data_list"_s).toArray();
        for (const QJsonValue &data : sideData) {
            if (data.toObject().contains(u"rotation"_s)) {
                probe.rotation = data.toObject().value(u"rotation"_s).toInt();
            }
        }
    }
    return probe;
}
} // namespace

VideoEditor::VideoEditor(QObject *parent)
    : QObject(parent)
{
}

VideoEditor::~VideoEditor()
{
    stop();
}

QUrl VideoEditor::source() const
{
    return m_source;
}

void VideoEditor::setSource(const QUrl &source)
{
    if (source == m_source) {
        return;
    }
    stop();
    m_source = source;
    m_keyframes.clear();
    m_rotation = 0;
    Q_EMIT sourceChanged();
    Q_EMIT probed();
}

bool VideoEditor::available()
{
    return !QStandardPaths::findExecutable(u"ffmpeg"_s).isEmpty() && !QStandardPaths::findExecutable(u"ffprobe"_s).isEmpty()
        && Sandbox::supported();
}

QList<qreal> VideoEditor::keyframes() const
{
    return m_keyframes;
}

int VideoEditor::rotation() const
{
    return m_rotation;
}

bool VideoEditor::probing() const
{
    return m_probe || m_parsing;
}

bool VideoEditor::saving() const
{
    return m_save;
}

QString VideoEditor::workDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/kwiklook/video/work-"_s
        + QString::number(QCoreApplication::applicationPid());
}

QString VideoEditor::copyPath(const QString &path)
{
    const QFileInfo file(path);
    const QString suffix = file.suffix().isEmpty() ? QString() : u'.' + file.suffix();
    const QString name = i18nc("file name of an edited copy, e.g. clip (edited).mp4", "%1 (edited)", file.completeBaseName()) + suffix;
    const QDir dir = file.absoluteDir();
    return dir.filePath(dir.exists(name) ? KFileUtils::suggestName(QUrl::fromLocalFile(dir.path()), name) : name);
}

QProcess *VideoEditor::run(const QStringList &args, const QString &writeDir)
{
    QStringList sandboxArgs{u"--read"_s, m_source.toLocalFile()};
    if (!writeDir.isEmpty()) {
        sandboxArgs << u"--write"_s << writeDir;
    }
    sandboxArgs << u"--"_s << args;
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->start(Sandbox::binary(), sandboxArgs);
    return process;
}

void VideoEditor::probe()
{
    if (!m_source.isLocalFile() || m_probe || !available()) {
        return;
    }
    QProcess *process = run({QStandardPaths::findExecutable(u"ffprobe"_s), u"-v"_s, u"error"_s, u"-select_streams"_s, u"v:0"_s,
                               u"-show_entries"_s, u"packet=pts_time,flags:stream_side_data=rotation:format=start_time"_s,
                               u"-of"_s, u"json"_s, m_source.toLocalFile()},
                              QString());
    m_probe = process;
    Q_EMIT busyChanged();
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (process != m_probe) {
            return; // stopped
        }
        const QByteArray json = process->readAllStandardOutput();
        if (status != QProcess::NormalExit || exitCode != 0) {
            qCWarning(lcVideo) << "ffprobe failed" << exitCode << process->readAllStandardError();
            m_probe = nullptr;
            Q_EMIT busyChanged();
            Q_EMIT failed(i18nc("@info", "Could not read the video."));
            return;
        }
        // A long video has many packets: read them off the main thread.
        m_probe = nullptr;
        m_parsing = true;
        auto *watcher = new QFutureWatcher<Probe>(this);
        const int generation = m_generation;
        connect(watcher, &QFutureWatcher<Probe>::finished, this, [this, watcher, generation] {
            watcher->deleteLater();
            if (generation != m_generation) {
                return; // stopped (another video, or the edit is over) while parsing
            }
            m_parsing = false;
            {
                const Probe probe = watcher->result();
                m_keyframes = probe.keyframes;
                m_rotation = probe.rotation;
                Q_EMIT probed();
            }
            Q_EMIT busyChanged();
        });
        watcher->setFuture(QtConcurrent::run(parseProbe, json));
    });
}

qreal VideoEditor::keyframeAtOrBefore(qreal seconds) const
{
    const auto after = std::upper_bound(m_keyframes.cbegin(), m_keyframes.cend(), seconds + 0.0005);
    return after == m_keyframes.cbegin() ? 0 : *std::prev(after);
}

void VideoEditor::save(qreal start, qreal end, int turn)
{
    if (!m_source.isLocalFile() || m_save || !available()) {
        return;
    }
    const QString work = workDir();
    QDir(work).removeRecursively();
    if (!QDir().mkpath(work)) {
        Q_EMIT failed(i18nc("@info", "Could not save the video."));
        return;
    }
    const QString path = m_source.toLocalFile();
    const QString suffix = QFileInfo(path).suffix();
    m_output = work + u"/out"_s + (suffix.isEmpty() ? QString() : u'.' + suffix);
    QStringList args{QStandardPaths::findExecutable(u"ffmpeg"_s), u"-nostdin"_s, u"-loglevel"_s, u"error"_s, u"-y"_s};
    if (start > 0) {
        args << u"-ss"_s << QString::number(start, 'f', 3);
    }
    if (end > 0) {
        args << u"-to"_s << QString::number(end, 'f', 3);
    }
    if (turn % 360 != 0) {
        // Replaces the video's own rotation: the sum, as ffmpeg counts (counterclockwise).
        args << u"-display_rotation:v:0"_s << QString::number(((m_rotation + turn) % 360 + 360) % 360);
    }
    args << u"-i"_s << path << u"-c"_s << u"copy"_s << u"-map"_s << u"0"_s << u"-avoid_negative_ts"_s << u"make_zero"_s << m_output;
    QProcess *process = run(args, work);
    m_save = process;
    Q_EMIT busyChanged();
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (process != m_save) {
            return; // cancelled
        }
        if (status != QProcess::NormalExit || exitCode != 0) {
            qCWarning(lcVideo) << "ffmpeg failed" << exitCode << process->readAllStandardError();
        }
        finishSave(status == QProcess::NormalExit ? exitCode : -1);
    });
}

void VideoEditor::finishSave(int exitCode)
{
    const QString output = m_output;
    m_save = nullptr;
    if (exitCode != 0 || !QFileInfo::exists(output)) {
        QDir(workDir()).removeRecursively();
        Q_EMIT busyChanged();
        Q_EMIT failed(exitCode == Sandbox::ExitNoSandbox ? i18nc("@info", "Videos can't be edited safely on this system: its kernel is too old.")
                                                         : i18nc("@info", "Could not save the video."));
        return;
    }
    // Into place next to the original; never over an existing file.
    const QUrl copy = QUrl::fromLocalFile(copyPath(m_source.toLocalFile()));
    auto *job = KIO::file_move(QUrl::fromLocalFile(output), copy, -1, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job, copy] {
        QDir(workDir()).removeRecursively();
        Q_EMIT busyChanged();
        if (job->error()) {
            qCWarning(lcVideo) << "could not move the edited video:" << job->errorString();
            Q_EMIT failed(i18nc("@info", "Could not save the video next to the original."));
        } else {
            Q_EMIT saved(copy);
        }
    });
}

void VideoEditor::stop()
{
    // Cleared first: the processes' finished() handlers then know they were stopped.
    const QList<QPointer<QProcess>> processes{m_probe, m_save};
    const bool wasBusy = m_probe || m_save || m_parsing;
    m_probe = nullptr;
    m_parsing = false;
    ++m_generation; // a probe still being parsed is dropped
    m_save = nullptr;
    for (const QPointer<QProcess> &process : processes) {
        if (process && process->state() != QProcess::NotRunning) {
            // The sandbox starts its own process group.
            if (process->processId() > 0) {
                ::kill(-pid_t(process->processId()), SIGKILL);
            }
            process->waitForFinished(1000);
        }
    }
    QDir(workDir()).removeRecursively();
    if (wasBusy) {
        Q_EMIT busyChanged();
    }
}

void VideoEditor::cancel()
{
    stop();
}
