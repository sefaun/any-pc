#include "session/ScreenMatcher.h"

#include <QPainter>
#include <QtTest>

using namespace anypc;

namespace {
QImage solid(int w, int h, QColor color)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

QImage halves(int w, int h, QColor left, QColor right)
{
    QImage image = solid(w, h, left);
    QPainter p(&image);
    p.fillRect(w / 2, 0, w - w / 2, h, right);
    return image;
}
} // namespace

class TestScreenMatcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void singleScreenMatchesItself()
    {
        const QList<ScreenMatcher::Candidate> screens{{nullptr, QSize(1920, 1080), true}};
        QCOMPARE(ScreenMatcher::match(screens, QSize(1920, 1080), {}, {}), 0);
    }

    void aspectRatioPicksTheRightMonitor()
    {
        const QList<ScreenMatcher::Candidate> screens{{nullptr, QSize(1920, 1080), true},
                                                      {nullptr, QSize(1280, 1024), false}};
        QCOMPARE(ScreenMatcher::match(screens, QSize(1280, 1024), {}, {}), 1);
        // Downscaled 4K capture of a 16:9 monitor.
        QCOMPARE(ScreenMatcher::match(screens, QSize(2560, 1440), {}, {}), 0);
    }

    void wholeDesktopCaptureMatchesNoSingleScreen()
    {
        const QList<ScreenMatcher::Candidate> screens{{nullptr, QSize(1920, 1080), true},
                                                      {nullptr, QSize(1920, 1080), false}};
        QCOMPARE(ScreenMatcher::match(screens, QSize(3840, 1080), {}, {}), -1);
    }

    void identicalMonitorsAreDistinguishedByContent()
    {
        const QList<ScreenMatcher::Candidate> screens{{nullptr, QSize(1920, 1080), true},
                                                      {nullptr, QSize(1920, 1080), false}};
        const QImage first = halves(1920, 1080, Qt::black, Qt::white);
        const QImage second = halves(1920, 1080, Qt::white, Qt::black);
        const auto grab = [&](const ScreenMatcher::Candidate &c) { return c.primary ? first : second; };

        const QByteArray showsSecond = ScreenMatcher::thumbnailOf(second.scaled(1280, 720));
        QCOMPARE(ScreenMatcher::match(screens, QSize(1280, 720), showsSecond, grab), 1);
        const QByteArray showsFirst = ScreenMatcher::thumbnailOf(first);
        QCOMPARE(ScreenMatcher::match(screens, QSize(1920, 1080), showsFirst, grab), 0);
    }

    void identicalMonitorsWithoutThumbnailPreferPrimary()
    {
        const QList<ScreenMatcher::Candidate> screens{{nullptr, QSize(1920, 1080), false},
                                                      {nullptr, QSize(1920, 1080), true}};
        QCOMPARE(ScreenMatcher::match(screens, QSize(1920, 1080), {}, {}), 1);
    }

    void thumbnailDifference()
    {
        const QByteArray black = ScreenMatcher::thumbnailOf(solid(100, 50, Qt::black));
        const QByteArray white = ScreenMatcher::thumbnailOf(solid(100, 50, Qt::white));
        QCOMPARE(black.size(), ScreenMatcher::kThumbWidth * ScreenMatcher::kThumbHeight);
        QCOMPARE(ScreenMatcher::difference(black, black), 0.0);
        QCOMPARE(ScreenMatcher::difference(black, white), 255.0);
        QCOMPARE(ScreenMatcher::difference(black, QByteArray("x")), -1.0);
    }
};

QTEST_MAIN(TestScreenMatcher)
#include "tst_screenmatcher.moc"
