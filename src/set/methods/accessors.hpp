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
#include "realm/set.hpp"

namespace realm {
namespace js {

template <typename T>
struct AccessorsForSet {
    using MixedAPI = TypeMixed<T>;

    template <typename Set>
    auto make_getter(std::string const &key_name, Set *set) {
        return [=](Napi::CallbackInfo const &info) mutable {
            auto mixed_value = set->get_any(key_name);
           return MixedAPI::get_instance().wrap(info.Env(), mixed_value);
        };
    }
    template <typename Set>
    auto make_setter(std::string const &key_name, Set *set) {
        return [=](const Napi::CallbackInfo& info) mutable {
            auto mixed = MixedAPI::get_instance().unwrap(info.Env(), info[0]);
            set->insert(key_name, mixed);
        };
    }
};
}  // namespace js
}  // namespace realm
