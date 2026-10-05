#pragma once

#include <string>
#include <vector>

#include "System/token.hpp"

namespace resiris {

class Tokenizer {
public:
    std::vector<Token> tokenize(const std::string& source) const;
};

} // namespace resiris