#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

struct JSONValue
{
    struct Null {};
    struct Bool { bool value; };
    struct Number { double value; };
    struct String { std::string value; };
    struct Array { std::vector<JSONValue> values; };

    struct Object 
    {
        // C++23 standard doesn't gurantee that map works with an incomplete 
        // type, so workaround by using a pointer
        using ObjectMap = std::map<std::string, JSONValue>;
        std::unique_ptr<ObjectMap> members;
    };

    using JSONValueType = std::variant<
        Null, Bool, Number, String, Array, Object>;

    JSONValueType value;
};
