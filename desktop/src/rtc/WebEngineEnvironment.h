#pragma once

class QWebEngineProfile;

namespace anypc::WebEngineEnvironment {

/**
 * Must be called before the QApplication is constructed. Configures Chromium
 * flags required for unattended screen capture and background WebRTC.
 */
void prepare(bool safeMode = false);

/** Shared, off-the-record profile used by every RTC page. */
QWebEngineProfile *profile();

} // namespace anypc::WebEngineEnvironment
