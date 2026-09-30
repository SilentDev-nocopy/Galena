#include "resiris/value.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace resiris {

namespace {

// Renders digits * 10^exponent in scientific notation, the way printf and
// std::to_chars write the exponent (a sign plus at least two digits).
std::string render_scientific(const std::string& digits, int exponent) {
    std::string out;
    out.reserve(digits.size() + 8);
    out.push_back(digits[0]);
    if (digits.size() > 1) {
        out.push_back('.');
        out.append(digits, 1, std::string::npos);
    }
    const int magnitude = exponent < 0 ? -exponent : exponent;
    out.push_back('e');
    out.push_back(exponent < 0 ? '-' : '+');
    if (magnitude < 10) {
        out.push_back('0');
    }
    out += std::to_string(magnitude);
    return out;
}

// Renders digits * 10^exponent in fixed notation.
std::string render_fixed(const std::string& digits, int exponent) {
    const int point = exponent + 1;  // digit count in front of the radix point
    if (point <= 0) {
        std::string out = "0.";
        out.append(static_cast<std::size_t>(-point), '0');
        out += digits;
        return out;
    }
    if (static_cast<std::size_t>(point) >= digits.size()) {
        std::string out = digits;
        out.append(static_cast<std::size_t>(point) - digits.size(), '0');
        return out;
    }
    std::string out = digits.substr(0, static_cast<std::size_t>(point));
    out.push_back('.');
    out += digits.substr(static_cast<std::size_t>(point));
    return out;
}

// Splits a printf "%e" rendering into its sign, significant digits and
// exponent; the value is `digits * 10^(exponent - (digits.size() - 1))`.
struct ParsedScientific {
    bool negative = false;
    std::string digits;
    int exponent = 0;
};

ParsedScientific parse_scientific(const char* text) {
    ParsedScientific parsed;
    std::size_t index = 0;
    if (text[index] == '-') {
        parsed.negative = true;
        ++index;
    }
    const std::size_t exponent_position = std::string(text).find('e', index);
    parsed.exponent = std::atoi(text + exponent_position + 1);
    for (; index < exponent_position; ++index) {
        if (text[index] != '.') {
            parsed.digits.push_back(text[index]);
        }
    }
    return parsed;
}

// The last digit steps up, carrying over into a new leading digit.
std::string incremented(const std::string& digits) {
    std::string result = digits;
    for (std::size_t index = result.size(); index-- > 0;) {
        if (result[index] != '9') {
            result[index] = static_cast<char>(result[index] + 1);
            return result;
        }
        result[index] = '0';
    }
    return "1" + result;
}

// The last digit steps down. A borrow borrows across the zeros without changing
// the digit count, though it can leave a leading zero - "0999" at exponent e is
// the same number as "999" at exponent e - 1, which `normalize` sorts out.
std::string decremented(const std::string& digits) {
    std::string result = digits;
    for (std::size_t index = result.size(); index-- > 0;) {
        if (result[index] != '0') {
            result[index] = static_cast<char>(result[index] - 1);
            return result;
        }
        result[index] = '9';
    }
    return result;
}

// Trailing zeros are never part of the shortest form. Removing them keeps the
// value untouched and the length comparison below honest.
std::string without_trailing_zeros(const std::string& digits) {
    std::string trimmed = digits;
    while (trimmed.size() > 1 && trimmed.back() == '0') {
        trimmed.pop_back();
    }
    return trimmed;
}

// The distances compared below reach a few dozen decimal digits, which is past
// what a 32 bit integer holds and past what the xtensa GCC 8.4 toolchain offers
// as __int128, so the arithmetic is done on plain digit strings.
using BigDigits = std::string;

// Reads a string of decimal digits, dropping the leading zeros.
BigDigits to_digit_string(const std::string& digits) {
    std::size_t first = digits.find_first_not_of('0');
    return first == std::string::npos ? BigDigits("0") : digits.substr(first);
}

BigDigits times_power_of_ten(const BigDigits& digits, int power) {
    BigDigits result = digits;
    result.append(static_cast<std::size_t>(power), '0');
    return result;
}

// Negative when `left` is the smaller of the two.
int compare(const BigDigits& left, const BigDigits& right) {
    if (left.size() != right.size()) {
        return left.size() < right.size() ? -1 : 1;
    }
    if (left == right) {
        return 0;
    }
    return left < right ? -1 : 1;
}

// |left - right|, which needs the larger of the two as the minuend.
BigDigits absolute_difference(const BigDigits& left, const BigDigits& right) {
    const bool left_is_smaller = compare(left, right) < 0;
    const BigDigits& larger = left_is_smaller ? right : left;
    const BigDigits& smaller = left_is_smaller ? left : right;

    BigDigits result = larger;
    int borrow = 0;
    int index = static_cast<int>(result.size()) - 1;
    int smaller_index = static_cast<int>(smaller.size()) - 1;
    for (; index >= 0; --index) {
        int digit = (result[static_cast<std::size_t>(index)] - '0') - borrow;
        if (smaller_index >= 0) {
            digit -= smaller[static_cast<std::size_t>(smaller_index)] - '0';
            --smaller_index;
        }
        if (digit < 0) {
            digit += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[static_cast<std::size_t>(index)] = static_cast<char>('0' + digit);
    }
    return to_digit_string(result);
}

// std::to_chars for floating point values is missing from the GCC 8.4
// libstdc++ shipped with the ESP32 toolchain, so the shortest round-trip
// representation is produced here instead.
//
// The result is the shape std::to_chars produces: the fewest characters that
// read back as the very same double, written in fixed or scientific notation -
// whichever is shorter, with an equal length tie going to the rendering closest
// to the value. The special values render as "inf"/"-inf"/"nan".
std::string shortest_double_repr(double value) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return value < 0.0 ? "-inf" : "inf";
    }

    // printf rounds correctly, so for a given precision it emits the rendering
    // nearest to `value` - no other candidate on that grid can be closer, which
    // leaves one candidate per precision. A rendering is faithful when reading
    // it back yields `value` again.
    //
    // The search runs past the 17 digits a double needs to round trip, because a
    // longer yet equally long rendering still wins the length tie:
    // 5.8187031860904178483e+20 is the exact 581870318609041784832 rather than
    // the 21 character 581870318609041785000.
    //
    // Ties are settled against a 25 digit rendering, which is finer than any
    // candidate the search produces, so comparing on its grid is enough.
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.24e", value);
    const ParsedScientific reference = parse_scientific(buffer);
    const BigDigits reference_digits = to_digit_string(reference.digits);
    const int reference_scale =
        reference.exponent - (static_cast<int>(reference.digits.size()) - 1);

    std::string best;
    bool have_best = false;
    BigDigits best_distance;
    for (int precision = 0; precision <= 24; ++precision) {
        // A candidate with `precision + 1` digits can never be shorter than
        // that, so nothing left to check once the best is this short already.
        if (have_best && static_cast<std::size_t>(precision) >= best.size()) {
            break;
        }

        std::snprintf(buffer, sizeof(buffer), "%.*e", precision, value);
        if (std::strtod(buffer, nullptr) != value) {
            continue;
        }

        const ParsedScientific parsed = parse_scientific(buffer);
        const std::string digits = without_trailing_zeros(parsed.digits);

        const std::string scientific = render_scientific(digits, parsed.exponent);
        const std::string fixed = render_fixed(digits, parsed.exponent);
        std::string candidate =
            fixed.size() <= scientific.size() ? fixed : scientific;
        if (parsed.negative) {
            candidate.insert(candidate.begin(), '-');
        }

        const int scale = parsed.exponent - (static_cast<int>(digits.size()) - 1);
        const int common = scale < reference_scale ? scale : reference_scale;
        const BigDigits distance = absolute_difference(
            times_power_of_ten(to_digit_string(digits), scale - common),
            times_power_of_ten(reference_digits, reference_scale - common));

        if (!have_best || candidate.size() < best.size() ||
            (candidate.size() == best.size() && compare(distance, best_distance) < 0)) {
            best = std::move(candidate);
            best_distance = std::move(distance);
            have_best = true;
        }
    }

    return best;
}

} // namespace

std::string float_to_string(double value) {
    std::string out = shortest_double_repr(value);

    // Python's repr appends ".0" to integral floats that printed in decimal
    // form, e.g. repr(2.0) == "2.0". Values printed in scientific notation
    // keep their exponent.
    if (out.find('.') == std::string::npos &&
        out.find('e') == std::string::npos &&
        out.find('E') == std::string::npos &&
        out != "inf" && out != "-inf" && out != "nan") {
        out += ".0";
    }
    return out;
}

std::string value_to_string(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return value.bool_value ? "True" : "False";
        case Value::Type::Int: return std::to_string(value.int_value);
        case Value::Type::Float: return float_to_string(value.float_value);
        case Value::Type::String: return value.string_value;
        case Value::Type::ModuleObject:
            return value.module_value ? value.module_value->to_string() : "ModuleObject";
        case Value::Type::FunctionalObject: return "FunctionalObject";
        default: return "None";
    }
}

std::string value_repr(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return value.bool_value ? "True" : "False";
        case Value::Type::Int: return std::to_string(value.int_value);
        case Value::Type::Float: return float_to_string(value.float_value);
        case Value::Type::String: {
            std::string out = "'";
            for (char ch : value.string_value) {
                switch (ch) {
                    case '\\': out += "\\\\"; break;
                    case '\'': out += "\\'"; break;
                    case '\n': out += "\\n"; break;
                    case '\t': out += "\\t"; break;
                    default: out.push_back(ch); break;
                }
            }
            out += "'";
            return out;
        }
        case Value::Type::ModuleObject:
            return value.module_value ? value.module_value->to_string() : "ModuleObject";
        case Value::Type::FunctionalObject: return "FunctionalObject";
        default: return "None";
    }
}

std::string ModuleObject::to_string() const {
    return "ModuleObject(" + module_name + ")";
}

bool operator==(const Value& a, const Value& b) {
    if (a.type == Value::Type::Bool && b.type == Value::Type::Bool) {
        return a.bool_value == b.bool_value;
    }
    if (a.type == Value::Type::Bool || b.type == Value::Type::Bool) {
        return false;
    }
    if (a.type == Value::Type::Int && b.type == Value::Type::Int) {
        return a.int_value == b.int_value;
    }
    if (a.type == Value::Type::Int && b.type == Value::Type::Float) {
        return static_cast<double>(a.int_value) == b.float_value;
    }
    if (a.type == Value::Type::Float && b.type == Value::Type::Int) {
        return a.float_value == static_cast<double>(b.int_value);
    }
    if (a.type == Value::Type::Float && b.type == Value::Type::Float) {
        return a.float_value == b.float_value;
    }
    if (a.type == Value::Type::String && b.type == Value::Type::String) {
        return a.string_value == b.string_value;
    }
    if (a.type == Value::Type::ModuleObject && b.type == Value::Type::ModuleObject) {
        return a.module_value == b.module_value;
    }
    if (a.type == Value::Type::FunctionalObject && b.type == Value::Type::FunctionalObject) {
        return a.function_value == b.function_value;
    }
    return false;
}

} // namespace resiris