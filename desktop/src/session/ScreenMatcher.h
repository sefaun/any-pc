#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QSize>

#include <functional>

class QScreen;

namespace anypc {

/**
 * Identifies which local monitor a captured video stream shows.
 *
 * Chromium enumerates screens independently of Qt, so the captured stream is
 * matched back to a QScreen: first by aspect ratio, then (when several
 * monitors share it) by comparing a small grayscale thumbnail of the video
 * with a grab of each candidate screen.
 */
class ScreenMatcher final
{
public:
    /** Thumbnail geometry shared with host.js (grayscale, row-major, 1 byte/pixel). */
    static constexpr int kThumbWidth = 64;
    static constexpr int kThumbHeight = 36;

    struct Candidate
    {
        QScreen *screen = nullptr;  ///< May be null in tests.
        QSize nativeSize;           ///< Size in device pixels.
        bool primary = false;
    };

    using Grabber = std::function<QImage(const Candidate &)>;

    /**
     * Returns the index of the best candidate, or -1 when the capture does not
     * correspond to a single screen (e.g. the whole virtual desktop).
     */
    static int match(const QList<Candidate> &candidates, const QSize &captureSize, const QByteArray &thumbnail,
                     const Grabber &grab);

    /** Converts an image into the grayscale thumbnail format. */
    static QByteArray thumbnailOf(const QImage &image);

    /** Mean absolute difference between two thumbnails (0..255), or -1 if incomparable. */
    static double difference(const QByteArray &a, const QByteArray &b);

    /** Candidates for every connected screen. */
    static QList<Candidate> currentScreens();

private:
    ScreenMatcher() = delete;
};

} // namespace anypc
