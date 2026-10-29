// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MAILDRAFT_H
#define MAILDRAFT_H

#include <QString>

// The parts of a saved draft's text, as the composer put them together: the text written, the
// signature and the quote, with the quote's introduction ("On …, … wrote:") above or below.
struct MailDraft
{
    QString text;
    QString signature;
    QString attribution;
    // The '>' lines, empty when there is no quote.
    QString quote;
    bool writtenBelowQuote = false;

    // Text that doesn't look like what the composer makes is all text.
    static MailDraft split(const QString &body);
};

#endif // MAILDRAFT_H
