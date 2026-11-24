// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
//
// kwiklook-cards: contacts (.vcf) and events (.ics) as cards. The only code linked
// against KContacts and KCalendarCore, loaded by KwikLook at runtime (core/cards.cpp),
// so both libraries stay optional: without them the files show as text.
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/MemoryCalendar>
#include <KContacts/Addressee>
#include <KContacts/VCardConverter>

#include <QBuffer>
#include <QImage>
#include <QRegularExpression>
#include <QTimeZone>
#include <QVariantList>

using namespace Qt::StringLiterals;

namespace
{
// A contact's photo as a data: URL (the image views take it); only embedded ones,
// never one linked from the web.
QString photoUrl(const KContacts::Picture &picture)
{
    if (!picture.isIntern() || picture.data().isNull()) {
        return QString();
    }
    const QImage image = picture.data().scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return u"data:image/png;base64,"_s + QString::fromLatin1(png.toBase64());
}

int readContacts(const QByteArray &data, int limit, QVariantList *cards)
{
    int count = 0;
    const KContacts::Addressee::List contacts = KContacts::VCardConverter().parseVCards(data);
    for (const KContacts::Addressee &contact : contacts) {
        if (count++ >= limit) {
            continue;
        }
        QVariantList phones;
        const KContacts::PhoneNumber::List numbers = contact.phoneNumbers();
        for (const KContacts::PhoneNumber &number : numbers) {
            phones << QVariantMap{{u"label"_s, number.typeLabel()}, {u"value"_s, number.number()}};
        }
        QStringList emails;
        const KContacts::Email::List mails = contact.emailList();
        for (const KContacts::Email &mail : mails) {
            emails << mail.mail();
        }
        QStringList addresses;
        const KContacts::Address::List places = contact.addresses();
        for (const KContacts::Address &place : places) {
            addresses << place.formatted(KContacts::AddressFormatStyle::MultiLineInternational).trimmed();
        }
        *cards << QVariantMap{
            {u"kind"_s, u"contact"_s},
            {u"name"_s, contact.realName().isEmpty() ? contact.assembledName() : contact.realName()},
            {u"organization"_s, QStringList{contact.title(), contact.organization()}.filter(QRegularExpression(u"\\S"_s)).join(u", "_s)},
            {u"photo"_s, photoUrl(contact.photo())},
            {u"phones"_s, phones},
            {u"emails"_s, emails},
            {u"addresses"_s, addresses},
            {u"birthday"_s, contact.birthday().date()},
            {u"url"_s, contact.url().url().toString()},
            {u"note"_s, contact.note()},
        };
    }
    return count;
}

int readEvents(const QByteArray &data, int limit, QVariantList *cards)
{
    int count = 0;
    const auto calendar = KCalendarCore::MemoryCalendar::Ptr(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    if (!KCalendarCore::ICalFormat().fromRawString(calendar, data)) {
        return 0;
    }
    const KCalendarCore::Event::List events = calendar->rawEvents(KCalendarCore::EventSortStartDate);
    for (const KCalendarCore::Event::Ptr &event : events) {
        if (count++ >= limit) {
            continue;
        }
        *cards << QVariantMap{
            {u"kind"_s, u"event"_s},
            {u"summary"_s, event->summary()},
            {u"start"_s, event->dtStart().toLocalTime()},
            {u"end"_s, event->dtEnd().toLocalTime()},
            {u"allDay"_s, event->allDay()},
            {u"location"_s, event->location()},
            {u"organizer"_s, event->organizer().fullName()},
            {u"repeats"_s, event->recurs()},
            {u"description"_s, event->description()},
        };
    }
    return count;
}
}

// The entry point (see core/cards.h); the version is in the name, so a library from
// another KwikLook version is refused instead of called wrongly.
extern "C" Q_DECL_EXPORT int kwiklook_read_cards_v1(const QByteArray *data, const QString *kind, int limit, QVariantList *cards)
{
    return *kind == u"vcard"_s ? readContacts(*data, limit, cards) : readEvents(*data, limit, cards);
}
