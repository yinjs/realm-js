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

#include <functional>
#include <map>
#include <regex>

#include "common/js_plain_object.hpp"
#include "methods/accessors.hpp"
#include "methods/listeners.hpp"
#include "realm/object-store/set.hpp"

namespace realm {
namespace js {

template <typename T>
class SetAdapter {
   private:
    using ValueType = typename T::Value;
    using Context = typename T::Context;

    using GetterSetters = AccessorsConfiguration<T, AccessorsForSet<T>>;
    using Methods = ListenersMethodsForSet<T>;
    using JSSet = JSObject<T, GetterSetters, Methods>;

   public:
    ValueType wrap(Context context, object_store::Set set) {
        JSSet *js_set = new JSSet(context);
        object_store::Set *_set =
            new object_store::Set(set);

        js_set->template configure_object_destructor([=]() {
            /* GC will trigger this function, signaling that...
             * ...we can deallocate the attached C++ object.
             */
            delete js_set;
            delete _set;
        });

        js_set->set_getter_setters(_set);
        js_set->set_methods(_set);

        return js_set->get_plain_object();
    }
};

}  // namespace js
}  // namespace realm

