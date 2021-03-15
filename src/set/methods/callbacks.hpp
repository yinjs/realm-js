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

namespace realm {
namespace js {

// TODO:
// namespace set {
//     // TODO:   Dictionary places these in the js::realm namespace
//     //  they should probably be moved somewhere else
//     template <typename T>
//     const std::string NotificationsCallback<T>::DELETIONS = "deletions";
//     template <typename T>
//     const std::string NotificationsCallback<T>::INSERTIONS = "insertions";
//     template <typename T>
//     const std::string NotificationsCallback<T>::MODIFICATIONS = "modifications";
// }

template <typename T>
struct SetNotificationsCallback {
    using ObjectType = typename T::Object;
    using FunctionType = typename T::Function;
    using ValueType = typename T::Value;
    using ContextType = typename T::Context;
    using PFunction = Protected<FunctionType>;
    using PObject = Protected<ObjectType>;
    using PGlobalContext = Protected<typename T::GlobalContext>;
    using Object = js::Object<T>;
    using GetterSetters = AccessorsConfiguration<T, AccessorsForSet<T>>;
    using JSSetUpdate = JSObject<T, GetterSetters>;
    using Value = js::Value<T>;

    // TODO
    // constants
    // static constexpr std::string DELETIONS("deletions");
    // constexpr std::string INSERTIONS = "insertions";
    // static const std::string MODIFICATIONS = "modifications";


    PFunction fn;
    PObject plain_object;
    PGlobalContext context;

    SetNotificationsCallback(ContextType &_context, FunctionType &_fn)
        : fn(_context, _fn),
          plain_object(_context, Object::create_empty(_context)),
          context(Context<T>::get_global_context(_context)) {}

    SetNotificationsCallback(ContextType &_context, FunctionType &_fn,
                          const ObjectType &obj)
        : fn(_context, _fn),
          plain_object(_context, obj),
          context(Context<T>::get_global_context(_context)) {}

    template <typename CollectionType>
    auto build_array(CollectionType &collection) const {
        std::vector<ValueType> values;
        for (auto mixed_item : collection) {
            values.push_back(
                TypeMixed<T>::get_instance().wrap(context, mixed_item));
        }

        return Object::create_array(context, values);
    }

    bool operator==(const SetNotificationsCallback<T> &candidate) const {
        return static_cast<FunctionType>(fn) ==
               static_cast<FunctionType>(candidate.fn);
    }

    // ObjectType build_changeset_object(DictionaryChangeSet &change_set) const {
    //     ObjectType object = Object::create_empty(context);
    //     int deleted_fields_size = change_set.deletions.size();
    //     Object::set_property(context, object, DELETIONS,
    //                          Value::from_number(context, deleted_fields_size));
    //     Object::set_property(context, object, INSERTIONS,
    //                          build_array(change_set.insertions));
    //     Object::set_property(context, object, MODIFICATIONS,
    //                          build_array(change_set.modifications));

    //     return object;
    // }

    void update_object_with_new_set(object_store::Set *set,
                                           ObjectType &object) const {
        JSSetUpdate set_update(context, object);
        set_update.update_accessors(set);
        object = set_update.get_plain_object();
    }

    // void operator()(object_store::Dictionary *dict,
    //                 DictionaryChangeSet change_set, bool has_change) const {
    //     HANDLESCOPE(context)
    //     auto object = static_cast<ObjectType>(plain_object);

    //     if (has_change) {
    //         update_object_with_new_dictionary(dict, object);
    //     }

    //     ValueType arguments[]{object, build_changeset_object(change_set)};

    //     Function<T>::callback(context, fn, plain_object, 2, arguments);
    // }
};


}  // namespace js
}  // namespace realm

