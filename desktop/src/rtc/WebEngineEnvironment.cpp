#include "rtc/WebEngineEnvironment.h"

#include <QCoreApplication>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

namespace anypc::WebEngineEnvironment {

void prepare()
{
    // Flags appended to whatever the user/environment already provides.
    static constexpr const char *kFlags[] = {
        "--autoplay-policy=no-user-gesture-required",
        "--disable-background-timer-throttling",
        "--disable-renderer-backgrounding",
        "--disable-backgrounding-occluded-windows",
        "--enable-usermedia-screen-capturing",
    };

    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    for (const char *flag : kFlags) {
        if (!flags.contains(flag)) {
            if (!flags.isEmpty())
                flags.append(' ');
            flags.append(flag);
        }
    }
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
}

QWebEngineProfile *profile()
{
    static QWebEngineProfile *instance = [] {
        // Parent to the application so it is destroyed after all pages.
        auto *p = new QWebEngineProfile(QCoreApplication::instance());
        p->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        p->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

        QWebEngineSettings *s = p->settings();
        s->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
        s->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
        s->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, false);
        s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
        s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
        s->setAttribute(QWebEngineSettings::PluginsEnabled, false);
        s->setAttribute(QWebEngineSettings::ShowScrollBars, false);
        return p;
    }();
    return instance;
}

} // namespace anypc::WebEngineEnvironment
