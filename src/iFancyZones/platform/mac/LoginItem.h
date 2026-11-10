#pragma once

namespace ifz {

// Registers the app bundle itself as a login item through SMAppService
// (macOS 13+). The user can also toggle it in System Settings > General >
// Login Items, so the system state is the source of truth.
class LoginItem {
public:
    // True when the app is registered, including "registered but waiting for
    // the user's approval in System Settings".
    static bool isEnabled();

    // Register or unregister. Returns false and logs a warning on failure.
    static bool setEnabled(bool enabled);
};

} // namespace ifz
