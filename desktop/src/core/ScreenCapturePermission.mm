#include "core/ScreenCapturePermission.h"

#include <CoreGraphics/CoreGraphics.h>

namespace anypc::ScreenCapturePermission {

bool isRestricted()
{
    return true;
}

bool isGranted()
{
    return CGPreflightScreenCaptureAccess();
}

void request()
{
    CGRequestScreenCaptureAccess();
}

} // namespace anypc::ScreenCapturePermission
