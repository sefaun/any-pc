#pragma once

namespace anypc::ScreenCapturePermission {

/** True on systems where a missing permission can block screen capture (macOS). */
bool isRestricted();

/**
 * True when this app may capture the screen. On macOS this is the system's
 * own pre-check, which can wrongly report false (for example after an
 * update), so it must not be used to refuse a capture. Only macOS restricts this
 * (System Settings > Privacy & Security > Screen Recording); elsewhere it is
 * always true.
 */
bool isGranted();

/**
 * Asks the system for screen capture access. On macOS this shows the system
 * prompt the first time and adds AnyPC to the Screen Recording list; the
 * permission takes effect after AnyPC is restarted.
 */
void request();

} // namespace anypc::ScreenCapturePermission
