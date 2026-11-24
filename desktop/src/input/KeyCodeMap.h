#pragma once

#include <QString>

#include <cstdint>
#include <optional>

namespace anypc {

/**
 * Platform key identifiers for a physical key, resolved from the DOM
 * `KeyboardEvent.code` value. Using physical codes (instead of characters)
 * makes the remote keyboard layout authoritative, like a real keyboard.
 */
struct PlatformKey
{
    std::uint16_t windowsVirtualKey;  ///< VK_* code (0 = unmapped).
    bool windowsExtended;             ///< Needs KEYEVENTF_EXTENDEDKEY.
    std::uint16_t macKeyCode;         ///< kVK_* code (0xFFFF = unmapped).
    std::uint32_t x11KeySym;          ///< X11 keysym (0 = unmapped).
};

class KeyCodeMap final
{
public:
    static std::optional<PlatformKey> lookup(const QString &domCode);

    static constexpr std::uint16_t kMacUnmapped = 0xFFFF;

private:
    KeyCodeMap() = delete;
};

} // namespace anypc
