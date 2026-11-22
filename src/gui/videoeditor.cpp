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
#include <cmath>
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

bool VideoEditor::exactAvailable()
{
    // Asked once: ffmpeg lists its encoders (no file is read).
    static const bool available = [] {
        const QString ffmpeg = QStandardPaths::findExecutable(u"ffmpeg"_s);
        if (ffmpeg.isEmpty()) {
            return false;
        }
        QProcess process;
        process.start(ffmpeg, {u"-hide_banner"_s, u"-encoders"_s});
        return process.waitForFinished(5000) && process.readAllStandardOutput().contains(" libx264 ");
    }();
    return available;
}

qreal VideoEditor::progress() const
{
    return m_progress;
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

QString VideoEditor::copyPath(const QString &path, const QString &newSuffix)
{
    const QFileInfo file(path);
    const QString type = newSuffix.isEmpty() ? file.suffix() : newSuffix;
    const QString suffix = type.isEmpty() ? QString() : u'.' + type;
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

qreal VideoEditor::keyframeNear(qreal seconds, qreal within) const
{
    qreal best = -1;
    for (const qreal keyframe : m_keyframes) {
        if (std::abs(keyframe - seconds) <= within && (best < 0 || std::abs(keyframe - seconds) < std::abs(best - seconds))) {
            best = keyframe;
        }
    }
    return best;
}

bool VideoEditor::isQuickCut(qreal seconds) const
{
    return seconds < 0.001 || std::any_of(m_keyframes.cbegin(), m_keyframes.cend(), [seconds](qreal keyframe) {
               return std::abs(keyframe - seconds) < 0.001;
           });
}

void VideoEditor::save(qreal start, qreal end, int turn, qreal length)
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
    const bool exact = !isQuickCut(start) && exactAvailable();
    // Re-encoding: H.264 in the file's own type where it holds it, VP9 for WebM,
    // else an MP4.
    QString suffix = QFileInfo(path).suffix().toLower();
    static const QStringList h264Types{u"mp4"_s, u"m4v"_s, u"mov"_s, u"3gp"_s, u"mkv"_s};
    const bool webm = suffix == u"webm"_s;
    if (exact && !webm && !h264Types.contains(suffix)) {
        suffix = u"mp4"_s;
    }
    m_newSuffix = suffix.compare(QFileInfo(path).suffix(), Qt::CaseInsensitive) == 0 ? QString() : suffix;
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
    args << u"-i"_s << path << u"-map"_s << u"0"_s;
    if (!exact) {
        args << u"-c"_s << u"copy"_s << u"-avoid_negative_ts"_s << u"make_zero"_s;
    } else {
        // Exact: the video is decoded from the keyframe before `start` and encoded
        // from `start` on; the turn is applied to the picture. Audio and subtitles copied,
        // or AAC where the new file type needs it.
        args << u"-c"_s << u"copy"_s;
        if (webm) {
            args << u"-c:v"_s << u"libvpx-vp9"_s << u"-crf"_s << u"32"_s << u"-b:v"_s << u"0"_s << u"-row-mt"_s << u"1"_s;
        } else {
            args << u"-c:v"_s << u"libx264"_s << u"-crf"_s << u"18"_s << u"-preset"_s << u"fast"_s;
        }
        if (!m_newSuffix.isEmpty()) {
            args << u"-c:a"_s << u"aac"_s << u"-sn"_s << u"-dn"_s;
        }
        args << u"-progress"_s << u"pipe:1"_s << u"-nostats"_s;
    }
    args << m_output;
    QProcess *process = run(args, work);
    m_save = process;
    m_progress = exact ? 0 : -1;
    Q_EMIT progressChanged();
    Q_EMIT busyChanged();
    // ffmpeg's progress lines: out_time_us=… (microseconds written so far).
    const qreal total = std::max(0.001, (end > 0 ? end : length) - start);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, total] {
        while (process->canReadLine()) {
            const QByteArray line = process->readLine().trimmed();
            if (line.startsWith("out_time_us=") && process == m_save) {
                const qreal done = line.mid(12).toDouble() / 1e6;
                m_progress = std::clamp(done / total, 0.0, 1.0);
                Q_EMIT progressChanged();
            }
        }
    });
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
    m_progress = -1;
    Q_EMIT progressChanged();
    if (exitCode != 0 || !QFileInfo::exists(output)) {
        QDir(workDir()).removeRecursively();
        Q_EMIT busyChanged();
        Q_EMIT failed(exitCode == Sandbox::ExitNoSandbox ? i18nc("@info", "Videos can't be edited safely on this system: its kernel is too old.")
                                                         : i18nc("@info", "Could not save the video."));
        return;
    }
    // Into place next to the original; never over an existing file.
    const QUrl copy = QUrl::fromLocalFile(copyPath(m_source.toLocalFile(), m_newSuffix));
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
    if (m_progress >= 0) {
        m_progress = -1;
        Q_EMIT progressChanged();
    }
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
