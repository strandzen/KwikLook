// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "fontpreview.h"

#include <KFileUtils>
#include <KIO/FileCopyJob>

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QProcess>
#include <QRawFont>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcFont, "kwiklook.font")

FontPreview::FontPreview(QObject *parent)
    : QObject(parent)
{
}

FontPreview::~FontPreview()
{
    unload();
}

bool FontPreview::load(const QByteArray &data, const QUrl &source)
{
    unload();
    // FreeType parses the font here, on the main thread: a few milliseconds.
    const QRawFont raw(data, 12);
    // Installed already? Asked before the preview adds its own copy.
    const bool installed = raw.isValid() && QFontDatabase::families().contains(raw.familyName())
        && QFontDatabase::styles(raw.familyName()).contains(raw.styleName(), Qt::CaseInsensitive);
    const int id = QFontDatabase::addApplicationFontFromData(data);
    if (id < 0 || !raw.isValid()) {
        qCWarning(lcFont) << "could not load the font" << source;
        if (id >= 0) {
            QFontDatabase::removeApplicationFont(id);
        }
        return false;
    }
    m_id = id;
    m_source = source;
    m_family = raw.familyName();
    m_style = raw.styleName();
    m_installed = installed;
    Q_EMIT changed();
    return true;
}

void FontPreview::unload()
{
    if (m_id < 0) {
        return;
    }
    QFontDatabase::removeApplicationFont(m_id);
    m_id = -1;
    m_source.clear();
    m_family.clear();
    m_style.clear();
    m_installed = false;
    Q_EMIT changed();
}

void FontPreview::reset()
{
    unload();
    if (!m_sampleText.isEmpty()) {
        m_sampleText.clear();
        Q_EMIT sampleTextChanged();
    }
    if (m_sampleSize != DefaultSampleSize) {
        m_sampleSize = DefaultSampleSize;
        Q_EMIT sampleSizeChanged();
    }
}

QString FontPreview::family() const
{
    return m_family;
}

QString FontPreview::style() const
{
    return m_style;
}

bool FontPreview::installed() const
{
    return m_installed;
}

void FontPreview::install()
{
    const QUrl source = m_source;
    if (m_family.isEmpty() || m_installed || !source.isLocalFile()) {
        return;
    }
    // Where KDE's font installer puts fonts for one user; fontconfig reads it.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/fonts"_s;
    if (!QDir().mkpath(dir)) {
        Q_EMIT installFinished(false);
        return;
    }
    QString name = source.fileName();
    if (QFileInfo::exists(dir + u'/' + name)) {
        name = KFileUtils::suggestName(QUrl::fromLocalFile(dir), name); // never overwrite
    }
    auto *job = KIO::file_copy(source, QUrl::fromLocalFile(dir + u'/' + name), -1, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job, source, dir] {
        const bool ok = !job->error();
        if (!ok) {
            qCWarning(lcFont) << "could not install the font:" << job->errorString();
        } else {
            // Other applications see it once fontconfig has read the folder again.
            if (!QStandardPaths::isTestModeEnabled()) { // tests: not the user's font cache
                QProcess::startDetached(u"fc-cache"_s, {dir});
            }
            if (m_source == source) { // still the font shown
                m_installed = true;
                Q_EMIT changed();
            }
        }
        Q_EMIT installFinished(ok);
    });
}
