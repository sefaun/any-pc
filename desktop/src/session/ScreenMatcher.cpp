#include "session/ScreenMatcher.h"

#include <QGuiApplication>
#include <QScreen>

#include <cmath>
#include <limits>

namespace anypc {

namespace {
/** Aspect ratios closer than this are considered identical. */
constexpr double kAspectTolerance = 0.03;

double aspect(const QSize &size)
{
    return size.height() > 0 ? double(size.width()) / size.height() : 0.0;
}
} // namespace

QList<ScreenMatcher::Candidate> ScreenMatcher::currentScreens()
{
    QList<Candidate> result;
    const QScreen *primary = QGuiApplication::primaryScreen();
    for (QScreen *screen : QGuiApplication::screens()) {
        const qreal dpr = screen->devicePixelRatio();
        const QSize native(qRound(screen->geometry().width() * dpr), qRound(screen->geometry().height() * dpr));
        result.append({screen, native, screen == primary});
    }
    return result;
}

QByteArray ScreenMatcher::thumbnailOf(const QImage &image)
{
    if (image.isNull())
        return {};
    const QImage gray = image.scaled(kThumbWidth, kThumbHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_Grayscale8);
    QByteArray bytes;
    bytes.reserve(kThumbWidth * kThumbHeight);
    for (int y = 0; y < kThumbHeight; ++y)
        bytes.append(reinterpret_cast<const char *>(gray.constScanLine(y)), kThumbWidth);
    return bytes;
}

double ScreenMatcher::difference(const QByteArray &a, const QByteArray &b)
{
    const int expected = kThumbWidth * kThumbHeight;
    if (a.size() != expected || b.size() != expected)
        return -1;
    qint64 sum = 0;
    for (int i = 0; i < expected; ++i)
        sum += std::abs(int(quint8(a.at(i))) - int(quint8(b.at(i))));
    return double(sum) / expected;
}

int ScreenMatcher::match(const QList<Candidate> &candidates, const QSize &captureSize, const QByteArray &thumbnail,
                         const Grabber &grab)
{
    if (candidates.isEmpty())
        return -1;
    if (!captureSize.isValid() || captureSize.isEmpty()) {
        for (int i = 0; i < candidates.size(); ++i) {
            if (candidates.at(i).primary)
                return i;
        }
        return 0;
    }

    // 1. Screens with the capture's aspect ratio (captures may be downscaled).
    const double captureAspect = aspect(captureSize);
    QList<int> shortlist;
    for (int i = 0; i < candidates.size(); ++i) {
        if (std::abs(aspect(candidates.at(i).nativeSize) - captureAspect) <= kAspectTolerance)
            shortlist.append(i);
    }
    if (shortlist.isEmpty())
        return -1; // Probably the combined virtual desktop.
    if (shortlist.size() == 1)
        return shortlist.first();

    // 2. Prefer an exact pixel size match when it is unique.
    QList<int> exact;
    for (int i : std::as_const(shortlist)) {
        if (candidates.at(i).nativeSize == captureSize)
            exact.append(i);
    }
    if (exact.size() == 1)
        return exact.first();
    const QList<int> &pool = exact.isEmpty() ? shortlist : exact;

    // 3. Compare picture content.
    if (grab && !thumbnail.isEmpty()) {
        int best = -1;
        double bestScore = std::numeric_limits<double>::max();
        for (int i : pool) {
            const double score = difference(thumbnail, thumbnailOf(grab(candidates.at(i))));
            if (score >= 0 && score < bestScore) {
                bestScore = score;
                best = i;
            }
        }
        if (best >= 0)
            return best;
    }

    // 4. Fall back to the primary screen, then enumeration order.
    for (int i : pool) {
        if (candidates.at(i).primary)
            return i;
    }
    return pool.first();
}

} // namespace anypc
