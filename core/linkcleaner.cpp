// SPDX-License-Identifier: AGPL-3.0-or-later
#include "linkcleaner.h"

#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

namespace LinkCleaner {

namespace {

bool isWeb(const QUrl &url)
{
    return url.isValid() && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"));
}

bool hostIs(const QUrl &url, const QString &domain)
{
    const QString host = url.host().toLower();
    return host == domain || host.endsWith(QLatin1Char('.') + domain);
}

QString queryValue(const QUrl &url, const QString &key)
{
    return QUrlQuery(url).queryItemValue(key, QUrl::FullyDecoded);
}

// Proofpoint URL Defense v2: the address in "u", with '-' for '%' and '_' for '/'.
QString proofpointV2(const QString &encoded)
{
    QString text = encoded;
    text.replace(QLatin1Char('_'), QLatin1Char('/'));
    text.replace(QLatin1Char('-'), QLatin1Char('%'));
    return QUrl::fromPercentEncoding(text.toUtf8());
}

// Proofpoint URL Defense v3: https://urldefense.com/v3/__<address>__;<encoded characters>!!...
// Characters it replaced with '*' come base64 encoded after the ';', which this doesn't decode: a
// link with them is left wrapped.
QString proofpointV3(const QString &url)
{
    static const QRegularExpression v3(QStringLiteral("^https?://urldefense\\.com/v3/__(.+?)__;([^!]*)!"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = v3.match(url);
    if (!match.hasMatch() || match.captured(1).contains(QLatin1Char('*')))
        return QString();
    return match.captured(1);
}

// The address a redirector wraps, or nothing when url isn't one.
QString unwrapped(const QUrl &url)
{
    if (hostIs(url, QStringLiteral("safelinks.protection.outlook.com")))
        return queryValue(url, QStringLiteral("url"));
    if ((hostIs(url, QStringLiteral("google.com")) || url.host().startsWith(QLatin1String("www.google.")))
            && url.path() == QLatin1String("/url")) {
        const QString q = queryValue(url, QStringLiteral("q"));
        return q.isEmpty() ? queryValue(url, QStringLiteral("url")) : q;
    }
    if ((url.host() == QLatin1String("l.facebook.com") || url.host() == QLatin1String("lm.facebook.com"))
            && url.path() == QLatin1String("/l.php"))
        return queryValue(url, QStringLiteral("u"));
    if (url.host() == QLatin1String("urldefense.proofpoint.com") && url.path() == QLatin1String("/v2/url"))
        return proofpointV2(queryValue(url, QStringLiteral("u")));
    if (url.host() == QLatin1String("urldefense.com"))
        return proofpointV3(url.toString());
    return QString();
}

bool isTrackingParameter(const QString &key)
{
    static const QStringList exact = {
        QStringLiteral("fbclid"), QStringLiteral("gclid"), QStringLiteral("dclid"),
        QStringLiteral("gbraid"), QStringLiteral("wbraid"), QStringLiteral("msclkid"),
        QStringLiteral("yclid"), QStringLiteral("igshid"), QStringLiteral("twclid"),
        QStringLiteral("ttclid"), QStringLiteral("mc_cid"), QStringLiteral("mc_eid"),
        QStringLiteral("mkt_tok"), QStringLiteral("_hsenc"), QStringLiteral("_hsmi"),
        QStringLiteral("__hssc"), QStringLiteral("__hstc"), QStringLiteral("__hsfp"),
        QStringLiteral("hsctatracking"), QStringLiteral("oly_anon_id"), QStringLiteral("oly_enc_id"),
        QStringLiteral("vero_id"), QStringLiteral("vero_conv"), QStringLiteral("rb_clickid"),
        QStringLiteral("_ga"), QStringLiteral("_gl"), QStringLiteral("s_cid"),
        QStringLiteral("ml_subscriber"), QStringLiteral("ml_subscriber_hash"),
        QStringLiteral("ss_source"), QStringLiteral("ss_campaign_id"), QStringLiteral("trk")
    };
    const QString lower = key.toLower();
    return exact.contains(lower) || lower.startsWith(QLatin1String("utm_"))
            || lower.startsWith(QLatin1String("pk_")) || lower.startsWith(QLatin1String("mtm_"));
}

} // namespace

Result clean(const QString &input)
{
    Result result;
    result.url = input.trimmed();

    QUrl url(result.url);
    if (!isWeb(url))
        return result;

    // Redirectors are sometimes nested (Safe Links around Proofpoint, for one).
    for (int i = 0; i < 4; ++i) {
        const QUrl inner(unwrapped(url).trimmed());
        if (!isWeb(inner))
            break;
        url = inner;
        result.unwrapped = true;
    }

    if (url.hasQuery()) {
        // On the encoded pairs, so what stays keeps its encoding exactly.
        QStringList kept;
        for (const QString &pair : url.query(QUrl::FullyEncoded).split(QLatin1Char('&'))) {
            const QString key = QUrl::fromPercentEncoding(pair.section(QLatin1Char('='), 0, 0).toUtf8());
            if (isTrackingParameter(key))
                result.stripped = true;
            else if (!pair.isEmpty())
                kept.append(pair);
        }
        if (result.stripped)
            url.setQuery(kept.isEmpty() ? QString() : kept.join(QLatin1Char('&')), QUrl::StrictMode);
    }

    if (result.changed())
        result.url = url.toString(QUrl::FullyEncoded);
    return result;
}

bool isTrackingImage(const QString &input)
{
    const QUrl url(input.trimmed());
    if (!isWeb(url))
        return false;

    static const QStringList hosts = {
        QStringLiteral("google-analytics.com"), QStringLiteral("doubleclick.net"),
        QStringLiteral("mailtrack.io"), QStringLiteral("mailtrack.com"),
        QStringLiteral("bat.bing.com"), QStringLiteral("pixel.wp.com"),
        QStringLiteral("mixpanel.com"), QStringLiteral("trk.klclick.com"),
        QStringLiteral("open.convertkit-mail.com"), QStringLiteral("t.sidekickopen.com"),
        QStringLiteral("track.hubspot.com"), QStringLiteral("pixel.mathtag.com"),
        QStringLiteral("mailfoogae.appspot.com"), QStringLiteral("app.getpipeline.com")
    };
    for (const QString &host : hosts) {
        if (hostIs(url, host))
            return true;
    }

    // Open trackers of mailing services, on their own hosts or the sender's.
    const QString path = url.path().toLower();
    static const QRegularExpression openPaths(QStringLiteral(
            "/track/open|/wf/open|/e/o/|/open\\.aspx|/trk/open|/tracking/open|/emailtracking/|/o\\.gif$"
            "|/open\\.gif$|/pixel(\\.gif|\\.png)?$|/beacon(\\.gif)?$"));
    if (openPaths.match(path).hasMatch())
        return true;
    return hostIs(url, QStringLiteral("facebook.com")) && path == QLatin1String("/tr");
}

} // namespace LinkCleaner
