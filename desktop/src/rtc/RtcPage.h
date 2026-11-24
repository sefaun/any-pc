#pragma once

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

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceId) override;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

private:
    void setupPermissions();
    bool isTrustedOrigin(const QUrl &origin) const;

    QWebChannel *m_channel;
};

} // namespace anypc
