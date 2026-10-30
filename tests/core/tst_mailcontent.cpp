// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archivefolder.h"
#include "flowedtext.h"
#include "linkcleaner.h"
#include "mailaddresses.h"
#include "mailcontent.h"
#include "maildraft.h"

#include <QtTest>

namespace {

QStringList texts(const MailContent &content)
{
    QStringList result;
    for (const MailBlock &block : content.blocks)
        result.append(block.text());
    return result;
}

QList<int> levels(const MailContent &content)
{
    QList<int> result;
    for (const MailBlock &block : content.blocks)
        result.append(block.quoteLevel);
    return result;
}

// The spans of a block that are links, as "text -> href".
QStringList links(const MailBlock &block)
{
    QStringList result;
    for (const MailSpan &span : block.spans) {
        if (!span.href.isEmpty())
            result.append(span.text + QStringLiteral(" -> ") + span.href);
    }
    return result;
}

} // namespace

class TestMailContent : public QObject
{
    Q_OBJECT

private slots:
    void paragraphsAndWhitespace();
    void leavesOutWhatIsNotShown();
    void leavesOutHiddenElements();
    void decodesEntities();
    void keepsEmphasis();
    void keepsSafeLinks();
    void linkifiesText();
    void lineBreaks();
    void quoteLevels();
    void lists();
    void tablesBecomeBlocks();
    void images();
    void preformatted();
    void headings();
    void brokenHtml();
    void plainText();
    void toPlainText();
    void richTextUsesOnlyTheStyle();
    void richTextImages();
    void signatures();
    void quoted();
    void quoteLines();
    void splitAddresses();
    void addresses();
    void formatAddresses();
    void archiveFolder();
    void flowedShortLines();
    void flowedWraps();
    void flowedQuotes();
    void flowedSpecialLines();
    void flowedDecode();
    void draftQuoteBelow();
    void draftWrittenBelow();
    void draftWithoutQuote();
    void draftNotOurs();
    void trackingImagesLeftOut();
    void cleanLinks_data();
    void cleanLinks();
    void trackingImages();
};

void TestMailContent::paragraphsAndWhitespace()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>  Hello   \n\t world </p>\n\n<p>Second</p>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("Hello world") << QStringLiteral("Second"));
}

void TestMailContent::leavesOutWhatIsNotShown()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<!DOCTYPE html><html><head><title>Title</title><style>p { color: red }</style>"
            "<meta charset=\"utf-8\"></head><body><!-- <p>comment</p> -->"
            "<script>document.write('<p>script</p>')</script><p>Body</p></body></html>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("Body"));
}

void TestMailContent::leavesOutHiddenElements()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<div style=\"display: none; max-height: 0px\">Preheader</div>"
            "<div style=\"max-height:0;overflow:hidden\">Also preheader</div>"
            "<span style=\"font-size:0px\">tiny</span>"
            "<div style=\"mso-hide:all\">outlook</div>"
            "<div hidden>attribute</div>"
            "<p style=\"opacity:0\">invisible</p>"
            "<p style=\"opacity:0.5\">faded</p>"
            "<p>Visible&#8204;&nbsp;&#847;</p>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("faded") << QStringLiteral("Visible"));
}

void TestMailContent::decodesEntities()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>&eacute;&amp;&#39;&#x20AC; &bogus; &amp &lt;tag&gt; &hellip;</p>"));
    QCOMPARE(texts(content), QStringList() << QString::fromUtf8("é&'€ &bogus; &amp <tag> …"));
}

void TestMailContent::keepsEmphasis()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>a <b>bold</b> and <em>it</em><strong><i>both</i></strong></p>"));
    QCOMPARE(content.blocks.size(), 1);
    const QVector<MailSpan> &spans = content.blocks.first().spans;
    QCOMPARE(spans.size(), 5);
    QCOMPARE(spans.at(0).text, QStringLiteral("a "));
    QVERIFY(!spans.at(0).bold && !spans.at(0).italic);
    QCOMPARE(spans.at(1).text, QStringLiteral("bold"));
    QVERIFY(spans.at(1).bold && !spans.at(1).italic);
    QCOMPARE(spans.at(3).text, QStringLiteral("it"));
    QVERIFY(!spans.at(3).bold && spans.at(3).italic);
    QVERIFY(spans.at(4).bold && spans.at(4).italic);
}

void TestMailContent::keepsSafeLinks()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p><a href=\" https://example.org/a?b=1&amp;c=2 \">here</a> "
            "<a href=\"javascript:alert(1)\">script</a> <a href=\"#top\">anchor</a> "
            "<a href=\"mailto:me@example.org\">mail</a></p>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("here script anchor mail"));
    QCOMPARE(links(content.blocks.first()), QStringList()
             << QStringLiteral("here -> https://example.org/a?b=1&c=2")
             << QStringLiteral("mail -> mailto:me@example.org"));
}

void TestMailContent::linkifiesText()
{
    const MailContent content = MailContent::fromPlainText(QStringLiteral(
            "See www.example.com. Or (https://example.org/x), "
            "https://en.wikipedia.org/wiki/Foo_(bar) and me@example.org!"));
    QCOMPARE(content.blocks.size(), 1);
    QCOMPARE(links(content.blocks.first()), QStringList()
             << QStringLiteral("www.example.com -> https://www.example.com")
             << QStringLiteral("https://example.org/x -> https://example.org/x")
             << QStringLiteral("https://en.wikipedia.org/wiki/Foo_(bar) -> https://en.wikipedia.org/wiki/Foo_(bar)")
             << QStringLiteral("me@example.org -> mailto:me@example.org"));
    QCOMPARE(content.blocks.first().text(), QStringLiteral(
            "See www.example.com. Or (https://example.org/x), "
            "https://en.wikipedia.org/wiki/Foo_(bar) and me@example.org!"));
}

void TestMailContent::lineBreaks()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<div><br>one<br>two <br> <br><br><br>three<br></br></div>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("one\ntwo\n\nthree"));
}

void TestMailContent::quoteLevels()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>reply</p><div class=\"gmail_quote\">On Monday, Jan wrote:"
            "<blockquote><p>quoted</p><blockquote>deeper</blockquote>back</blockquote></div>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("reply") << QStringLiteral("On Monday, Jan wrote:")
             << QStringLiteral("quoted") << QStringLiteral("deeper") << QStringLiteral("back"));
    QCOMPARE(levels(content), QList<int>() << 0 << 0 << 1 << 2 << 1);
}

void TestMailContent::lists()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<ul><li>a<li>b<ul><li>c</li></ul></ul><ol start=\"3\"><li>x</li><li>y</li></ol>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("a") << QStringLiteral("b")
             << QStringLiteral("c") << QStringLiteral("x") << QStringLiteral("y"));
    QStringList markers;
    QList<int> depths;
    for (const MailBlock &block : content.blocks) {
        QCOMPARE(block.kind, MailBlock::ListItem);
        markers.append(block.marker);
        depths.append(block.listDepth);
    }
    QCOMPARE(markers, QStringList() << QStringLiteral("•") << QStringLiteral("•") << QStringLiteral("•")
             << QStringLiteral("3.") << QStringLiteral("4."));
    QCOMPARE(depths, QList<int>() << 1 << 1 << 2 << 1 << 1);
}

void TestMailContent::tablesBecomeBlocks()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<table width=\"600\"><tr><td>A</td><td>B<td>C</tr><tr><td>&nbsp;</td><td></td></tr></table>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("A") << QStringLiteral("B") << QStringLiteral("C"));
}

void TestMailContent::images()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<img src=\"https://track.example/open.gif\" width=\"1\" height=\"1\">"
            "<img src=\"https://track.example/p.gif\" style=\"width:1px;height:1px\">"
            "<p><img src=\"https://cdn.example/logo.png\" alt=\" Logo \" width=\"600\" height=\"200\"></p>"
            "<p><a href=\"https://shop.example\"><img src=\"//cdn.example/buy.png\" alt=\"Buy\"></a></p>"
            "<p><img src=\"data:image/png;base64,AAAA\" alt=\"inline\"></p>"
            "<p><img src=\"cid:123\" nemo-inline-image-loading=\"yes\"></p>"
            "<p><img src=\"file:///etc/passwd\" alt=\"local\"></p>"));
    QCOMPARE(content.images.size(), 5);
    QCOMPARE(content.remoteImageCount(), 2);

    const MailImage &logo = content.images.at(0);
    QCOMPARE(logo.source, QStringLiteral("https://cdn.example/logo.png"));
    QCOMPARE(logo.alt, QStringLiteral("Logo"));
    QCOMPARE(logo.width, 600);
    QCOMPARE(logo.height, 200);
    QVERIFY(logo.remote);

    QCOMPARE(content.images.at(1).source, QStringLiteral("https://cdn.example/buy.png"));
    QCOMPARE(content.blocks.at(1).spans.first().image, 1);
    QCOMPARE(content.blocks.at(1).spans.first().href, QStringLiteral("https://shop.example"));

    QVERIFY(!content.images.at(2).remote);
    QCOMPARE(content.images.at(2).source, QStringLiteral("data:image/png;base64,AAAA"));
    // Still downloading, and not an image Qt can load.
    QVERIFY(content.images.at(3).source.isEmpty());
    QVERIFY(content.images.at(4).source.isEmpty());
}

void TestMailContent::preformatted()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<pre>\n  code &lt;x&gt;\n    more</pre><p>after</p>"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("  code <x>\n    more") << QStringLiteral("after"));
    QCOMPARE(content.blocks.first().kind, MailBlock::Preformatted);
}

void TestMailContent::headings()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral("<h1>Title</h1>text"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("Title") << QStringLiteral("text"));
    QCOMPARE(content.blocks.at(0).kind, MailBlock::Heading);
    QCOMPARE(content.blocks.at(1).kind, MailBlock::Paragraph);
}

void TestMailContent::brokenHtml()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>one<p>two</div></b>2<p>a < b</p>three <b>unclosed <i>tags"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("one") << QStringLiteral("two2")
             << QStringLiteral("a < b") << QStringLiteral("three unclosed tags"));
    QVERIFY(content.blocks.last().spans.last().bold);
    QVERIFY(content.blocks.last().spans.last().italic);
}

void TestMailContent::plainText()
{
    const MailContent content = MailContent::fromPlainText(QStringLiteral(
            "Hi,\r\n\r\nThanks!\nSee you\n> quoted line\n> more\n>> deeper\n> > also deeper\n\n-- \nsig"));
    QCOMPARE(texts(content), QStringList() << QStringLiteral("Hi,") << QStringLiteral("Thanks!\nSee you")
             << QStringLiteral("quoted line\nmore") << QStringLiteral("deeper\nalso deeper")
             << QStringLiteral("--\nsig"));
    QCOMPARE(levels(content), QList<int>() << 0 << 0 << 1 << 2 << 0);
}

void TestMailContent::toPlainText()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>Reply</p><blockquote>line one<br>line two</blockquote><ul><li>item</li></ul>"));
    QCOMPARE(content.toPlainText(), QStringLiteral("Reply\n\n> line one\n> line two\n\n• item"));
}

void TestMailContent::richTextUsesOnlyTheStyle()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<body bgcolor=\"white\"><font color=\"red\" face=\"Comic Sans MS\" size=\"7\">"
            "<span style=\"background:yellow\">&lt;script&gt;</span></font>"
            "<blockquote>quoted</blockquote></body>"));
    MailStyle style;
    style.textColor = QStringLiteral("#eeeeee");
    style.quoteColors = QStringList() << QStringLiteral("#123456");
    const QString html = toRichText(content, style);
    QVERIFY(!html.contains(QLatin1String("red")));
    QVERIFY(!html.contains(QLatin1String("Comic")));
    QVERIFY(!html.contains(QLatin1String("yellow")));
    QVERIFY(!html.contains(QLatin1String("white")));
    QVERIFY(html.contains(QLatin1String("&lt;script&gt;")));
    QVERIFY(html.contains(QLatin1String("color:#eeeeee;\">&lt;script&gt;")));
    QVERIFY(html.contains(QLatin1String("color:#123456;\">quoted")));
}

void TestMailContent::richTextImages()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p><img src=\"https://cdn.example/a.png\" alt=\"Logo\" width=\"1200\" height=\"300\">"
            "<img src=\"https://cdn.example/b.png\"></p>"));
    MailStyle style;
    style.imagePlaceholder = QStringLiteral("[Image: %1]");
    style.imageText = QStringLiteral("no description");
    style.maxImageWidth = 400;

    const QString hidden = toRichText(content, style);
    QVERIFY(!hidden.contains(QLatin1String("<img")));
    QVERIFY(!hidden.contains(QLatin1String("cdn.example")));
    QVERIFY(hidden.contains(QLatin1String("[Image: Logo]")));
    QVERIFY(hidden.contains(QLatin1String("[Image: no description]")));

    style.showImages = true;
    const QString shown = toRichText(content, style);
    QVERIFY(shown.contains(QLatin1String("<img src=\"https://cdn.example/a.png\" width=\"400\" height=\"100\" />")));
    QVERIFY(shown.contains(QLatin1String("<img src=\"https://cdn.example/b.png\" />")));
}

void TestMailContent::signatures()
{
    const MailContent plain = MailContent::fromPlainText(QStringLiteral(
            "Thanks\n\n-- \nJan\nCompany\n\nwww.example.com\n\n> quoted"));
    QList<bool> marked;
    for (const MailBlock &block : plain.blocks)
        marked.append(block.signature);
    QCOMPARE(marked, QList<bool>() << false << true << true << false);

    const MailContent html = MailContent::fromHtml(QStringLiteral(
            "<p>Hi</p><div class=\"gmail_signature\" data-smartmail=\"gmail_signature\"><p>Jan</p></div>"
            "<div id=\"Signature\">Outlook</div><pre class=\"moz-signature\">Thunderbird</pre><p>After</p>"));
    marked.clear();
    for (const MailBlock &block : html.blocks)
        marked.append(block.signature);
    QCOMPARE(marked, QList<bool>() << false << true << true << true << false);
}

void TestMailContent::quoted()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p>Reply text</p><div class=\"gmail_signature\">Jan</div><blockquote>Earlier</blockquote>"));
    QCOMPARE(content.quoted(true), QStringLiteral("> Reply text\n>\n>> Earlier"));
    QCOMPARE(content.quoted(false), QStringLiteral("> Reply text"));
}

void TestMailContent::quoteLines()
{
    QCOMPARE(MailContent::quoteLines(QString::fromUtf8("\none\u2029two\u2028three\r\n\n> old\n\n")),
             QStringLiteral("> one\n> two\n> three\n>\n>> old"));
}

void TestMailContent::splitAddresses()
{
    QCOMPARE(MailAddresses::split(QStringLiteral(
                     "\"Doe, Jan\" <jan@example.org>, piet@example.org; <a@b.c>, ,Name <x;y@z.org>")),
             QStringList() << QStringLiteral("\"Doe, Jan\" <jan@example.org>") << QStringLiteral("piet@example.org")
             << QStringLiteral("<a@b.c>") << QStringLiteral("Name <x;y@z.org>"));
}

void TestMailContent::addresses()
{
    QCOMPARE(MailAddresses::address(QStringLiteral("\"Doe, Jan\" <Jan@Example.org>")), QStringLiteral("jan@example.org"));
    QCOMPARE(MailAddresses::address(QStringLiteral(" Piet@Example.org ")), QStringLiteral("piet@example.org"));
    QVERIFY(MailAddresses::isValid(QStringLiteral("Jan <jan@example.org>")));
    QVERIFY(MailAddresses::isValid(QStringLiteral("jan@example.org")));
    QVERIFY(!MailAddresses::isValid(QStringLiteral("jan")));
    QVERIFY(!MailAddresses::isValid(QStringLiteral("jan@localhost")));
    QVERIFY(!MailAddresses::isValid(QStringLiteral("Jan <jan@example.org")));
    QCOMPARE(MailAddresses::without(QStringList() << QStringLiteral("Me <Me@Example.org>")
                                    << QStringLiteral("b@x.org") << QStringLiteral("B <B@x.org>"),
                                    QStringList() << QStringLiteral("me@example.org")),
             QStringList() << QStringLiteral("b@x.org"));
}

void TestMailContent::formatAddresses()
{
    QCOMPARE(MailAddresses::format(QStringLiteral("Jan Doe"), QStringLiteral("jan@example.org")),
             QStringLiteral("Jan Doe <jan@example.org>"));
    QCOMPARE(MailAddresses::format(QStringLiteral("Doe, Jan"), QStringLiteral("jan@example.org")),
             QStringLiteral("\"Doe, Jan\" <jan@example.org>"));
    QCOMPARE(MailAddresses::format(QStringLiteral("J. \"Jan\" Doe"), QStringLiteral("jan@example.org")),
             QStringLiteral("\"J. \\\"Jan\\\" Doe\" <jan@example.org>"));
    QCOMPARE(MailAddresses::format(QString::fromUtf8("Jörn"), QStringLiteral("j@example.org")),
             QString::fromUtf8("Jörn <j@example.org>"));
    QCOMPARE(MailAddresses::format(QString(), QStringLiteral("jan@example.org")), QStringLiteral("jan@example.org"));
    QCOMPARE(MailAddresses::format(QStringLiteral("jan@example.org"), QStringLiteral("jan@example.org")),
             QStringLiteral("jan@example.org"));
    // What split() and address() make of it again.
    const QString formatted = MailAddresses::format(QStringLiteral("Doe, Jan"), QStringLiteral("jan@example.org"));
    QCOMPARE(MailAddresses::split(formatted + QStringLiteral(", b@example.org")).size(), 2);
    QCOMPARE(MailAddresses::address(formatted), QStringLiteral("jan@example.org"));
}

void TestMailContent::archiveFolder()
{
    QCOMPARE(ArchiveFolder::guess(QStringList()), -1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("INBOX") << QStringLiteral("Sent")), -1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("INBOX") << QStringLiteral("Archive")), 1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("INBOX") << QStringLiteral("INBOX.Archief")), 1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("Projects/Archive") << QStringLiteral("Archives")),
             1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("INBOX") << QStringLiteral("[Gmail]/All Mail")
                                  << QStringLiteral("[Gmail]/Sent Mail")), 1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("[Google Mail]/Alle berichten")), 0);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("[Gmail]/All Mail") << QStringLiteral("Archive")),
             1);
    // Only Gmail's All Mail, not a folder of one's own that happens to be called that.
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("All Mail")), -1);
    QCOMPARE(ArchiveFolder::guess(QStringList() << QStringLiteral("Archived projects")), -1);
}

void TestMailContent::flowedShortLines()
{
    QCOMPARE(FlowedText::encode(QStringLiteral("Hi Jan,\n\nThanks!   \r\nBye")),
             QStringLiteral("Hi Jan,\n\nThanks!\nBye"));
}

void TestMailContent::flowedWraps()
{
    const QString paragraph = QStringLiteral(
            "This is a long paragraph that a phone keyboard produces as one line, and that has to be "
            "wrapped for clients that don't understand format=flowed: https://example.org/a/very/long/"
            "link/that/should/never/be/broken/in/the/middle/because/it/would/stop/working ok.");
    const QString encoded = FlowedText::encode(paragraph);
    const QStringList lines = encoded.split(QLatin1Char('\n'));
    QVERIFY(lines.size() > 2);
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (!line.contains(QLatin1String("https://")))
            QVERIFY2(line.size() <= 72, qPrintable(line));
        // Every line but the last flows into the next.
        QCOMPARE(line.endsWith(QLatin1Char(' ')), i < lines.size() - 1);
    }
    QVERIFY(encoded.contains(QLatin1String("https://example.org/a/very/long/link/that/should/never/be/"
                                           "broken/in/the/middle/because/it/would/stop/working ")));
    QCOMPARE(FlowedText::decode(encoded), paragraph);
}

void TestMailContent::flowedQuotes()
{
    const QString text = QStringLiteral(
            "My answer\n\nOn Monday, Jan wrote:\n> A quoted paragraph that is long enough to need "
            "wrapping at least once, maybe twice, to fit.\n>\n>> Older, quoted twice, and also long "
            "enough that it has to be wrapped.");
    const QString encoded = FlowedText::encode(text);
    for (const QString &line : encoded.split(QLatin1Char('\n')))
        QVERIFY2(line.size() <= 72, qPrintable(line));
    QVERIFY(encoded.contains(QLatin1String("\n>\n>> Older")));
    for (const QString &line : encoded.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String(">>")))
            QVERIFY(line.startsWith(QLatin1String(">> ")));
    }
    QCOMPARE(FlowedText::decode(encoded), text);
}

void TestMailContent::flowedSpecialLines()
{
    QCOMPARE(FlowedText::encode(QStringLiteral(" indented\nFrom here on\nFromage\n-- \nJan")),
             QStringLiteral("  indented\n From here on\nFromage\n-- \nJan"));
}

void TestMailContent::flowedDecode()
{
    // Soft breaks are joined, hard ones and quote depth changes end a paragraph, stuffing and
    // the signature separator survive.
    QCOMPARE(FlowedText::decode(QStringLiteral(
                     "One long \r\nparagraph.\n  indented\n> quoted \n> on\n>> deeper\n>\n-- \nJan")),
             QStringLiteral("One long paragraph.\n indented\n> quoted on\n>> deeper\n>\n-- \nJan"));
    const QString text = QStringLiteral(" stuffed\nFrom here\n-- \nJan");
    QCOMPARE(FlowedText::decode(FlowedText::encode(text)), text);
}

void TestMailContent::draftQuoteBelow()
{
    const MailDraft draft = MailDraft::split(QStringLiteral(
            "Thanks!\n\nSee you\n\n-- \nJan\n\nOn 3 Oct, Piet wrote:\n> hello\n>\n>> older\n"));
    QCOMPARE(draft.text, QStringLiteral("Thanks!\n\nSee you"));
    QCOMPARE(draft.signature, QStringLiteral("Jan"));
    QCOMPARE(draft.attribution, QStringLiteral("On 3 Oct, Piet wrote:"));
    QCOMPARE(draft.quote, QStringLiteral("> hello\n>\n>> older"));
    QVERIFY(!draft.writtenBelowQuote);
}

void TestMailContent::draftWrittenBelow()
{
    const MailDraft draft = MailDraft::split(QStringLiteral(
            "On 3 Oct, Piet wrote:\n> hello\n\nMy answer\n\n-- \nJan"));
    QCOMPARE(draft.attribution, QStringLiteral("On 3 Oct, Piet wrote:"));
    QCOMPARE(draft.quote, QStringLiteral("> hello"));
    QCOMPARE(draft.text, QStringLiteral("My answer"));
    QCOMPARE(draft.signature, QStringLiteral("Jan"));
    QVERIFY(draft.writtenBelowQuote);
}

void TestMailContent::draftWithoutQuote()
{
    const MailDraft draft = MailDraft::split(QStringLiteral("Just text\n\n-- \nJan\nCompany"));
    QCOMPARE(draft.text, QStringLiteral("Just text"));
    QCOMPARE(draft.signature, QStringLiteral("Jan\nCompany"));
    QVERIFY(draft.quote.isEmpty());
}

void TestMailContent::draftNotOurs()
{
    // A quote without an introduction, as other clients may leave it: all text.
    const MailDraft draft = MailDraft::split(QStringLiteral("> quoted\n> more\nmy reply in between\n> more quote"));
    QCOMPARE(draft.text, QStringLiteral("> quoted\n> more\nmy reply in between\n> more quote"));
    QVERIFY(draft.quote.isEmpty());
    QVERIFY(draft.attribution.isEmpty());
}

void TestMailContent::trackingImagesLeftOut()
{
    const MailContent content = MailContent::fromHtml(QStringLiteral(
            "<p><img src=\"https://u1.ct.sendgrid.net/wf/open?upn=x\" width=\"10\" height=\"10\">"
            "<img src=\"https://cdn.example.org/a.png?utm_source=news&amp;w=600\" alt=\"A\"></p>"));
    QCOMPARE(content.images.size(), 1);
    QCOMPARE(content.images.first().source, QStringLiteral("https://cdn.example.org/a.png?w=600"));
}

void TestMailContent::cleanLinks_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("unwrapped");
    QTest::addColumn<bool>("stripped");

    QTest::newRow("untouched") << "https://example.org/a?id=3#top" << "https://example.org/a?id=3#top" << false << false;
    QTest::newRow("not web") << "mailto:jan@example.org?subject=utm_source" << "mailto:jan@example.org?subject=utm_source"
                             << false << false;
    QTest::newRow("utm") << "https://example.org/a?utm_source=news&id=3&utm_medium=mail#top"
                         << "https://example.org/a?id=3#top" << false << true;
    QTest::newRow("only tracking") << "https://example.org/?fbclid=abc&mc_eid=1&_hsenc=x"
                                   << "https://example.org/" << false << true;
    QTest::newRow("keeps encoding") << "https://example.org/s?q=a%26b%3Dc&gclid=1&next=%2Fhome"
                                    << "https://example.org/s?q=a%26b%3Dc&next=%2Fhome" << false << true;
    QTest::newRow("safe links")
            << "https://eur01.safelinks.protection.outlook.com/?url=https%3A%2F%2Fexample.org%2Fpage%3Fx%3D1%26utm_campaign%3Dy&data=05%7C01&reserved=0"
            << "https://example.org/page?x=1" << true << true;
    QTest::newRow("google") << "https://www.google.com/url?q=https://example.org/&sa=D&ust=1"
                            << "https://example.org/" << true << false;
    QTest::newRow("facebook") << "https://l.facebook.com/l.php?u=https%3A%2F%2Fexample.org%2F%3Fa%3D1&h=AT0"
                              << "https://example.org/?a=1" << true << false;
    QTest::newRow("proofpoint v2")
            << "https://urldefense.proofpoint.com/v2/url?u=https-3A__example.org_path_page.html&d=DwMF&c=abc"
            << "https://example.org/path/page.html" << true << false;
    QTest::newRow("proofpoint v3")
            << "https://urldefense.com/v3/__https://example.org/path?a=1__;!!AbCdE!xyz$"
            << "https://example.org/path?a=1" << true << false;
    QTest::newRow("nested")
            << "https://nam02.safelinks.protection.outlook.com/?url=https%3A%2F%2Fwww.google.com%2Furl%3Fq%3Dhttps%3A%2F%2Fexample.org%2F"
            << "https://example.org/" << true << false;
    QTest::newRow("not a redirect") << "https://www.google.com/search?q=https://example.org"
                                    << "https://www.google.com/search?q=https://example.org" << false << false;
}

void TestMailContent::cleanLinks()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QFETCH(bool, unwrapped);
    QFETCH(bool, stripped);
    const LinkCleaner::Result result = LinkCleaner::clean(input);
    QCOMPARE(result.url, expected);
    QCOMPARE(result.unwrapped, unwrapped);
    QCOMPARE(result.stripped, stripped);
}

void TestMailContent::trackingImages()
{
    QVERIFY(LinkCleaner::isTrackingImage(QStringLiteral("https://www.google-analytics.com/collect?v=1")));
    QVERIFY(LinkCleaner::isTrackingImage(QStringLiteral("https://u123.ct.sendgrid.net/wf/open?upn=abc")));
    QVERIFY(LinkCleaner::isTrackingImage(QStringLiteral("https://mandrillapp.com/track/open.php?u=1")));
    QVERIFY(LinkCleaner::isTrackingImage(QStringLiteral("https://news.example.org/e/o/eyJ")));
    QVERIFY(LinkCleaner::isTrackingImage(QStringLiteral("https://www.facebook.com/tr?id=1&ev=PageView")));
    QVERIFY(!LinkCleaner::isTrackingImage(QStringLiteral("https://cdn.example.org/images/logo.png")));
    QVERIFY(!LinkCleaner::isTrackingImage(QStringLiteral("https://www.facebook.com/images/fb_icon.png")));
    QVERIFY(!LinkCleaner::isTrackingImage(QStringLiteral("data:image/png;base64,AAAA")));
}

QTEST_APPLESS_MAIN(TestMailContent)
#include "tst_mailcontent.moc"
