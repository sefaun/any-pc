#include "core/ScreenCapturePermission.h"

namespace anypc::ScreenCapturePermission {

bool isRestricted()
{
    return false;
}

bool isGranted()
{
    return true;
}

void request() {}

} // namespace anypc::ScreenCapturePermission
