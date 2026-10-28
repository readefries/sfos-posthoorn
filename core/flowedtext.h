// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef FLOWEDTEXT_H
#define FLOWEDTEXT_H

#include <QString>

// format=flowed (RFC 3676): long lines are wrapped with a trailing space at each soft break, so
// clients that understand it join them again and wrap to their own width, while others still get
// lines of a readable length.
namespace FlowedText {

// Encodes text where every line is a paragraph, and lines starting with '>' are quoted (">> " is
// level two). Lines are wrapped to width characters, quote markers included, where there is a
// space; longer words aren't broken. The signature separator "-- " is kept as it is.
QString encode(const QString &text, int width = 72);

} // namespace FlowedText

#endif // FLOWEDTEXT_H
