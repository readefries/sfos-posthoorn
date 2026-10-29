// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef LINKCLEANER_H
#define LINKCLEANER_H

#include <QString>

// Links in mail often aren't where they say they go: redirectors (Outlook Safe Links, Google,
// Facebook, Proofpoint) wrap the real address, and newsletters add tracking parameters to it.
namespace LinkCleaner {

struct Result
{
    QString url;
    // A redirector's wrapping was taken off.
    bool unwrapped = false;
    // Tracking parameters were taken out.
    bool stripped = false;

    bool changed() const { return unwrapped || stripped; }
};

// The address a link really goes to, without tracking parameters. Leaves what it doesn't
// recognise alone, and anything that isn't http(s).
Result clean(const QString &url);

// Whether an image is served by a known tracking service, to never load even when images are.
bool isTrackingImage(const QString &url);

} // namespace LinkCleaner

#endif // LINKCLEANER_H
