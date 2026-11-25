#pragma once

namespace anypc::ScreenCapturePermission {

/**
 * True when this app may capture the screen. Only macOS restricts this
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
