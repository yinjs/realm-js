////////////////////////////////////////////////////////////////////////////
//
// Copyright 2021 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <regex>

#include "common/type_deduction.hpp"

namespace realm {
namespace js {
class SetSchema {
   private:
    const std::string SET_SCHEMA = R"((\w+)?(\<\>))";

    std::string type;
    std::smatch matches;
    bool valid_schema;

   public:
    SetSchema(std::string const &_schema) {
        std::regex const set_schema_regex(SET_SCHEMA,
                                     std::regex_constants::ECMAScript);
        valid_schema = std::regex_search(_schema, matches, set_schema_regex);
        if (valid_schema) {
            type = matches[1];
        }
    }

    realm::PropertyType make_generic() {
        return realm::PropertyType::Set | realm::PropertyType::Mixed;
    }

    realm::PropertyType schema() {
        if (type.empty()) {
            return make_generic();
        }

        if (TypeDeduction::realm_type_exist(type)) {
            throw std::runtime_error("Schema type: " + type +
                                     " not supported for Set.");
        }

        auto set_type_value = TypeDeduction::realm_type(type);
        return (realm::PropertyType::Set |
                static_cast<realm::PropertyType>(set_type_value));
    }

    bool is_set() { return valid_schema; }
};
}  // namespace js
}  // namespace realm

