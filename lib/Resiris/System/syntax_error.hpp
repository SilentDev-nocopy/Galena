#pragma once

#include "System/errors.hpp"

namespace resiris {

class ResirisSyntaxError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

} // namespace resiris