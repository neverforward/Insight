#pragma once

namespace insight {

// Common interface of the server / client implementation. Both are created
// and destroyed exclusively from Insight::enable() / disable().
class PlatformLogic {
public:
    virtual ~PlatformLogic() = default;

    /// Called from Insight::enable(); return false to fail enabling.
    virtual bool enable() = 0;

    /// Called from Insight::disable() / unload().
    virtual void disable() = 0;
};

} // namespace insight
