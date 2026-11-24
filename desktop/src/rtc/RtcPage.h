#pragma once

#include <QStringList>
#include <QWebEnginePage>

class QWebChannel;

namespace anypc {

class RtcBridge;

/**
 * QWebEnginePage hosting the WebRTC engine (Chromium) for one session.
 *
 * - exposes the {@link RtcBridge} to JavaScript as `bridge` via QWebChannel;
 * - grants screen capture permissions for its own (qrc) origin only;
 * - forwards the JavaScript console to the Qt logging system.
 */
class RtcPage final : public QWebEnginePage
{
    Q_OBJECT

public:
    RtcPage(RtcBridge *bridge, QObject *parent = nullptr);

    void loadApp(const QUrl &url);

    /** Screen (index in Chromium's screen list) used by the next desktop capture. */
    void setScreenIndex(int index) { m_screenIndex = index; }
    int screenIndex() const { return m_screenIndex; }

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceId) override;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

Q_SIGNALS:
    /** The Chromium render process died; the page is unusable. */
    void crashed(const QString &reason);
    /** Emitted on every desktop capture with the screens Chromium can capture. */
    void screensEnumerated(const QStringList &names, int selected);

private:
    void setupPermissions();
    bool isTrustedOrigin(const QUrl &origin) const;

    QWebChannel *m_channel;
    int m_screenIndex = 0;
};

} // namespace anypc
