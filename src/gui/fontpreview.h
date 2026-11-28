// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

/*
 * The font file being previewed (Viewer.fontPreview, for FontView.qml): added to
 * the application's fonts while shown (QFontDatabase, main thread) so QML can draw
 * with it, its family and style from QRawFont (a Bold file shows bold), whether it
 * is installed already, and installing it into ~/.local/share/fonts. The sample
 * text and size are the preview's own and last until it closes (reset()).
 */
class FontPreview : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Viewer.fontPreview")

    // The loaded font's family and style name (for QML's font.family and
    // font.styleName); empty when no font is loaded.
    Q_PROPERTY(QString family READ family NOTIFY changed)
    Q_PROPERTY(QString style READ style NOTIFY changed)
    // The same family and style is already among the system's or the user's fonts.
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    // The user's own sample text, and its size in pixels (− / +, Ctrl+wheel).
    Q_PROPERTY(QString sampleText MEMBER m_sampleText NOTIFY sampleTextChanged)
    Q_PROPERTY(int sampleSize MEMBER m_sampleSize NOTIFY sampleSizeChanged)

public:
    static constexpr int DefaultSampleSize = 36;

    explicit FontPreview(QObject *parent);
    ~FontPreview() override;

    // Adds the font in `data` (the file at `source`) in place of the one before;
    // false if it can't be loaded (nothing loaded then).
    bool load(const QByteArray &data, const QUrl &source);
    void unload();
    // The preview closed: no font, the sample text and size back to their defaults.
    void reset();

    QString family() const;
    QString style() const;
    bool installed() const;

    // Copies the font file into the user's fonts (~/.local/share/fonts) in the
    // background, never over a file there; installFinished() tells how it went.
    Q_INVOKABLE void install();

Q_SIGNALS:
    void changed();
    void sampleTextChanged();
    void sampleSizeChanged();
    void installFinished(bool ok);

private:
    int m_id = -1; // QFontDatabase application font
    QUrl m_source;
    QString m_family;
    QString m_style;
    bool m_installed = false;
    QString m_sampleText;
    int m_sampleSize = DefaultSampleSize;
};
