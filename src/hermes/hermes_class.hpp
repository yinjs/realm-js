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

#include "hermes_types.hpp"
#include "hermes_return_value.hpp"
#include "hermes_string.hpp"
#include "hermes_object.hpp"

#include "js_class.hpp"
#include "js_util.hpp"

#include "realm/util/demangle.hpp" // XXX only used for debugging messages

#include <ctype.h>
#include <unordered_set>
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>
#include <exception>

namespace realm::js {

// XXX These shouldn't be called in production code. OTOH, they are really nice when debugging. Maybe leave the
// functions in, but delete all calls?
inline void consoleLog(JsiEnv env, const JsiVal& toLog) {
    auto console = env->global().getPropertyAsObject(env, "console");
    console.getPropertyAsFunction(env, "log").callWithThis(env, console, toLog);
}
inline void consoleLog(JsiEnv env, std::string toLog) {
    consoleLog(env, str(env, toLog));
}

namespace Xjsi {
// Toggle this to make the JSI implementation throw the exception, rather than throwing it ourselves. This may be
// required if we go back to using ANDROID_STL=c++_static.
// XXX Remove the whole Xjsi namespace if possible and go back to throwing jsi::JSError
#if 1
using jsi::JSError;

// This is the same as throw jsi::JsiError(), but compatible with other impl
[[noreturn]] inline void throwJSError(JsiEnv env, StringData msg) {
    throw JSError(env, std::string(msg));
}
#else
struct JSError : std::exception  {
    JSError(jsi::Runtime&, std::string msg) :msg(std::move(msg)) {}
    std::string msg;
    const char * what() const noexcept override {
        return msg.c_str();
    }
};

// used in top-level catch blocks to force the JSI lib to throw its exception
[[noreturn]] inline void throwJSError(JsiEnv env, StringData msg) {
    consoleLog(env, "about to throw");
    consoleLog(env, std::string(msg));
    static auto thrower = globalType(env, "Function").call(env, "msg", R"(
            //console.log("in thrower");
            throw new Error(msg);
            )")
        .asObject(env).asFunction(env);
    thrower.call(env, str(env, msg));
    REALM_UNREACHABLE();
}
#endif
}

template<typename T>
struct RealmObjectClass;
template<typename T>
class RealmClass;

template<>
struct Arguments<hermes::Types> {
    const std::vector<JsiVal> valStorage;
    const JsiEnv ctx;
    const size_t count;
    const JsiVal* const value;
    Arguments(JsiEnv env, size_t argc, const jsi::Value* argv)
        : valStorage([&] {
            std::vector<JsiVal> out;
            out.reserve(argc);
            for (size_t i = 0; i < argc; i++) {
                out.emplace_back(env, argv[i]);
            }
            return out;
        }()),
        ctx(env),
        count(argc),
        value(valStorage.data()) {}

    // If moving or copying were allowed, we would need to update value's pointer
    Arguments(Arguments&&) = delete;

    JsiVal operator[](size_t index) const noexcept {
        if (index >= count) {
            return ctx.undefined();
        }
        return ctx(value[index]);
    }

    void validate_maximum(size_t max) const {
        if (max < count) {
            throw std::invalid_argument(util::format("Invalid arguments: at most %1 expected, but %2 supplied.", max, count));
        }
    }

    void validate_count(size_t expected) const {
        if (count != expected) {
            // XXX for some reason this is outputting just "Invalid arguments: " on android
            consoleLog(ctx, util::format("Invalid arguments: %1 expected, but %2 supplied.", expected, count));
            throw std::invalid_argument(util::format("Invalid arguments: %1 expected, but %2 supplied.", expected, count));
        }
    }

    void validate_between(size_t min, size_t max) const {
        if (count < min || count > max) {
            throw std::invalid_argument(util::format("Invalid arguments: expected between %1 and %2, but %3 supplied.", min, max, count));
        }
    }
};

namespace hermes {

inline std::optional<jsi::Object> ObjectGetOwnPropertyDescriptor(JsiEnv env, const jsi::Object& target, const std::string& name) {
    auto obj = js::globalType(env, "Object");
    auto res = obj.getPropertyAsFunction(env, "getOwnPropertyDescriptor").callWithThis(env, obj, target, name);
    if (!res.isObject())
        return {};
    return std::move(res).getObject(env);
}

inline void ObjectSetPrototypeOf(JsiEnv env, const jsi::Value& target, const jsi::Value& proto) {
    auto obj = js::globalType(env, "Object");
    obj.getPropertyAsFunction(env, "setPrototypeOf").callWithThis(env, obj, target, proto);
}

inline void defineProperty(JsiEnv env, const jsi::Object& target, StringData name, const jsi::Object& descriptor) {
    auto objClass = js::globalType(env, "Object");
    objClass.getPropertyAsFunction(env, "defineProperty")
            .callWithThis(env, objClass, target, str(env, name), descriptor);
};

inline void copyProperty(JsiEnv env, const jsi::Object& from, const jsi::Object& to, const std::string& name) {
    auto prop = ObjectGetOwnPropertyDescriptor(env, from, name);
    REALM_ASSERT_RELEASE(prop);
    defineProperty(env, to, "name", *prop);
}

inline constexpr const char g_internal_field[] = "__Realm_internal";

template<typename T>
using ClassDefinition = js::ClassDefinition<js::hermes::Types, T>;

using ConstructorType = js::ConstructorType<js::hermes::Types>;
using ArgumentsMethodType = js::ArgumentsMethodType<js::hermes::Types>;
using ReturnValue = js::ReturnValue<js::hermes::Types>;
using Arguments = js::Arguments<js::hermes::Types>;
using PropertyType = js::PropertyType<js::hermes::Types>;
using IndexPropertyType = js::IndexPropertyType<js::hermes::Types>;
using StringPropertyType = js::StringPropertyType<js::hermes::Types>;

template <typename T>
class Wrapper : public jsi::HostObject {
public:
    template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
    Wrapper(Args&&... args) : obj(std::forward<Args>(args)...) {}

    T obj;
};

template <typename T>
inline T& unwrap(Wrapper<T>& wrapper) {
    return wrapper.obj;
}

template <typename T>
inline T& unwrap(const std::shared_ptr<Wrapper<T>>& wrapper) {
    return unwrap<T>(*wrapper);
}

template <typename T>
inline T& unwrap(JsiEnv env, const jsi::Object& wrapper) {
    return unwrap<T>(wrapper.getHostObject<Wrapper<T>>(env));
}

template <typename T>
inline T& unwrap(JsiEnv env, const jsi::Value& wrapper) {
    return unwrap<T>(env, wrapper.asObject(env));
}

template <typename T>
inline T& unwrap(const JsiObj& wrapper) {
    return unwrap<T>(wrapper.env(), wrapper.get());
}

template <typename T>
inline T& unwrap(const JsiVal& wrapper) {
    return unwrap<T>(wrapper.env(), wrapper.get());
}

template <typename T, typename U>
inline T* unwrapUnique(JsiEnv env, const U& arg) {
    return unwrap<std::unique_ptr<T>>(env, arg).get();
}

template <typename T>
JsiObj wrap(JsiEnv env, T arg) {
    return env(jsi::Object::createFromHostObject(env, std::make_shared<Wrapper<T>>(std::move(arg))));
}

template <typename T, typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
JsiObj wrap(JsiEnv env, Args&&... args) {
    return env(jsi::Object::createFromHostObject(env, std::make_shared<Wrapper<T>>(std::forward<Args>(args)...)));
}

template <typename T>
JsiObj wrapUnique(JsiEnv env, T* arg) {
    return wrap(env, std::unique_ptr<T>(arg));
}

template <typename T>
class ObjectWrap {
public:
    using Internal = typename T::Internal;
    using ParentClassType = typename T::Parent;

    // XXX if this is static, it won't support multiple runtimes.
    // Also, may need to suppress destruction.
    inline static std::optional<JsiFunc> s_ctor;

    static JsiFunc create_constructor(JsiEnv env) {
        auto& s_type = get_class();

        auto nativeFunc = 
            !bool(s_type.constructor)
            ? jsi::Value()
            : jsi::Function::createFromHostFunction(
                env, propName(env, s_type.name), /* XXX paramCount */0,
                [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) -> jsi::Value {
#if 1
                        REALM_ASSERT_RELEASE(count >= 1);
                        auto env = JsiEnv(rt);
                        auto& s_type = get_class();
                        auto arguments = Arguments{env, count - 1, args + 1};
                        s_type.constructor(env, env(args[0]).asObject(), arguments);
                        return jsi::Value();
#else 
                    try  {
                        REALM_ASSERT_RELEASE(count >= 1);
                        auto env = JsiEnv(rt);
                        auto& s_type = get_class();
                        auto arguments = Arguments{env, count - 1, args + 1};
                        s_type.constructor(env, env(args[0]).asObject(), arguments);
                        return jsi::Value();
                    } catch (const jsi::JSIException& ex) {
                        throw; // Propagate these as-is
                    } catch (const std::exception& ex) {
                        consoleLog(rt, ex.what());
                        Xjsi::throwJSError(rt, ex.what());
#if 0 // XXX For some reason, we aren't catching all exceptions derived from std::exception. If you flip this, we crash.
                    } catch (...) {
                        std::terminate();
#endif
                    }
#endif
                });

        s_ctor = env(globalType(env, "Function")
            .call(env,
                  "nativeFunc",
                  util::format(R"(
                      return function %1(...args) {
                          if (!nativeFunc && false) // XXX only disable for Realm.Object
                              throw TypeError("%1() cannot be constructed directly from javascript");
                          if (!new.target && false) { // XXX find another way to detect this correctly
                              throw TypeError("%1() must be called as a constructor");
                          }
                          if (nativeFunc)
                              nativeFunc(this, ...args); 

                          if ('_proxyWrapper' in %1)
                              return %1._proxyWrapper(this);
                      })", s_type.name))
            .asObject(env).asFunction(env)
            .call(env, std::move(nativeFunc))
            .asObject(env).asFunction(env)
        );

        for (auto&& [name, prop] : s_type.static_properties) {
            auto desc = jsi::Object(env);
            if (prop.getter) {
                desc.setProperty(env, "get", funcVal(env, "get_" + name, 0, prop.getter));
            }
            if (prop.setter) {
                desc.setProperty(env, "set", funcVal(env, "set_" + name, 1, prop.setter));
            }
            defineProperty(env, *s_ctor, name, desc);
        }

        for (auto&& [name, method] : s_type.static_methods) {
            auto desc = jsi::Object(env);
            desc.setProperty(env, "value", funcVal(env, name, /* XXX paramCount */ 0, method));
            defineProperty(env, *s_ctor, name, desc);
        }

        auto proto = (*s_ctor)->getPropertyAsObject(env, "prototype");

        for (auto&& [name, prop] : s_type.properties) {
            auto desc = jsi::Object(env);
            if (prop.getter) {
                desc.setProperty(env, "get", funcVal(env, "get_" + name, 0, prop.getter));
            }
            if (prop.setter) {
                desc.setProperty(env, "set", funcVal(env, "set_" + name, 1, prop.setter));
            }
            defineProperty(env, proto, name, desc);
        }

        for (auto&& [name, method] : s_type.methods) {
            auto desc = jsi::Object(env);
            desc.setProperty(env, "value", funcVal(env, name, /* XXX paramCount */ 0, method));
            defineProperty(env, proto, name, desc);
        }

        if constexpr (!std::is_void_v<ParentClassType>) {
            REALM_ASSERT_RELEASE(ObjectWrap<ParentClassType>::s_ctor);
            JsiFunc parentCtor = *ObjectWrap<ParentClassType>::s_ctor;

            auto parentProto = parentCtor->getProperty(env, "prototype");
            if (parentProto.isUndefined()) {
                throw std::runtime_error("undefined 'prototype' on parent constructor");
            }

            ObjectSetPrototypeOf(env, jsi::Value(env, proto), jsi::Value(std::move(parentProto)));
            ObjectSetPrototypeOf(env, jsi::Value(env, s_ctor->get()), jsi::Value(std::move(parentCtor.get())));
        }
        
        if (s_type.index_accessor) {
            // Code below assumes getter is present, and it doesn't make sense to have setter without one.
            REALM_ASSERT_RELEASE(s_type.index_accessor.getter);

            // XXX Do we want to trap things like ownKeys() and getOwnPropertyDescriptors() to support for...in?
            auto [getter, setter] = s_type.index_accessor;
            auto desc = jsi::Object(env);
            desc.setProperty(
                env,
                "value",
                globalType(env, "Function").call(env, "getter", "setter", R"(
                        const isNumber = /^[-+]?\d+$/;
                        const handler = {
                            ownKeys(target) {
                                const out = Reflect.ownKeys(target)
                                const end = target.length
                                for (let i = 0; i < end; i++) {
                                    out.push(String(i));
                                }
                                return out;
                            },
                            getOwnPropertyDescriptor(target, prop) {
                                if (typeof(prop) != 'string' || !isNumber.test(prop))
                                    return Reflect.getOwnPropertyDescriptor(target, prop)
                                const index =  Number(prop);
                                if (index >= 0 && index < target.length)
                                    return {
                                        configurable: true,
                                        enumerable: true,
                                    };
                            },
                            get(target, prop, receiver) {
                                if (typeof(prop) != 'string' || !isNumber.test(prop))
                                    return Reflect.get(target, prop, receiver);
                                return getter(target, Number(prop))
                            },
                            set(target, prop, receiver, val) {
                                if (typeof(prop) != 'string' || !isNumber.test(prop))
                                    return Reflect.set(target, prop, receiver, val);
                                if (!setter)
                                    return false;
                                return setter(target, Number(prop), val)
                            }
                        }
                        return (obj) => new Proxy(obj, handler);
                    )")
                    .asObject(env).asFunction(env)
                    .call(env, funcVal(env, "getter", 0, getter), funcVal(env, "setter", 1, setter))
                    .asObject(env).asFunction(env)
            );
            defineProperty(env, *s_ctor, "_proxyWrapper", desc);
        }
        
        return env((*s_ctor)->getFunction(env));
    }

    static JsiObj create_instance(JsiEnv env, Internal* ptr = nullptr) {
        auto proto = (*s_ctor)->getPropertyAsObject(env, "prototype");
        auto objClass = js::globalType(env, "Object");
        auto obj = env(objClass.getPropertyAsFunction(env, "create").callWithThis(env, objClass, proto)).asObject();
        set_internal(env, obj, ptr);

        auto wrapper = (*s_ctor)->getProperty(env, "_proxyWrapper");
        if (!wrapper.isUndefined()) {
            obj = env(wrapper.asObject(env).asFunction(env).call(env, std::move(obj.get()))).asObject();
        }

        return obj;
    }

    static JsiObj create_instance_by_schema(JsiEnv env, JsiFunc& constructor, const realm::ObjectSchema& schema, Internal* internal = nullptr) {
        return create_instance_by_schema(env, &constructor, schema, internal);
    }
    static JsiObj create_instance_by_schema(JsiEnv env, const realm::ObjectSchema& schema, Internal* internal = nullptr) {
        return create_instance_by_schema(env, nullptr, schema, internal);
    }

    static void on_context_destroy(JsiEnv, std::string realmPath) {
        get_schemaObjectTypes().erase(realmPath);
    }

    static bool is_instance(JsiEnv env, JsiObj object) {
        return object->instanceOf(env, *s_ctor);
    }

    static Internal* get_internal(JsiEnv env, const JsiObj& object) {
        if (!JsiObj(object)->instanceOf(env, *s_ctor)) {
            throw Xjsi::JSError(env, "calling method on wrong type of object");
        }
        // XXX we should be using a Symbol here, but https://github.com/facebook/hermes/issues/521 means that doesn't
        // work. If we need to, we could work around it by pre-cooking a JS Function that will get/set the Symbol field.
        auto internal = object->getProperty(env, g_internal_field);
        if (internal.isUndefined()) {
            if constexpr (std::is_same_v<T, RealmObjectClass<hermes::Types>>) // XXX comment why
                return nullptr;
            throw Xjsi::JSError(env, "no internal field");
        }
        return unwrapUnique<Internal>(env, std::move(internal));
    }
    static void set_internal(JsiEnv env, const JsiObj& object, Internal* data) {
        auto desc = jsi::Object(env);
        desc.setProperty(env, "value", wrapUnique(env, data));
        desc.setProperty(env, "configurable", true);
        // XXX See above comment about using Symbol here.
        defineProperty(env, object, g_internal_field, desc);
    }

private:
    template <typename Func>
    static jsi::Value funcVal(JsiEnv env, const std::string& name, size_t args, Func&& func) {
        if constexpr (std::is_pointer_v<std::decay_t<Func>>) {
            if (!func)
                return jsi::Value();
        }
        return jsi::Value(jsi::Function::createFromHostFunction(env, propName(env, name), uint32_t(args), 
#if 1
                                                                           func
#else // XXX wrap func in something that catches exceptions. May not be needed, but leaving for now in case it is.
            [func = std::forward<Func>(func)] (jsi::Runtime& rt, const jsi::Value& this_val,
                                               const jsi::Value* args, size_t count) -> jsi::Value {
                try {
                    //consoleLog(rt, std::to_string(__LINE__));
                    auto val = func(rt, this_val, args, count);
                    //consoleLog(rt, std::to_string(__LINE__));
                    return val;
                } catch (const jsi::JSIException& ex) {
                    consoleLog(rt, std::to_string(__LINE__));
                    throw;
                } catch (const std::exception& ex) {
                    consoleLog(rt, std::to_string(__LINE__));
                    consoleLog(rt, util::get_type_name(ex));
                    consoleLog(rt, ex.what());
                    Xjsi::throwJSError(rt, ex.what());
#if 0 // XXX For some reason, we aren't catching all exceptions derived from std::exception. If you flip this, we crash.
                } catch (...) {
                    consoleLog(rt, std::to_string(__LINE__));
                    std::terminate();
#endif
                }
            }
#endif
        ));
    }

    static void defineSchemaProperties(JsiEnv env, const jsi::Object& constructorPrototype, const realm::ObjectSchema& schema, bool redefine) {
        // Do the same thing for all computed and persisted properties
        auto loopBody = [&] (const Property& property) {
            const auto& name = property.public_name.empty() ? property.name : property.public_name;
            // TODO should this use hasOwnProperty?
            if (!redefine && constructorPrototype.hasProperty(env, str(env, name))) {
                return;
            }

            auto desc = jsi::Object(env);
            desc.setProperty(env, "enumerable", true);

            desc.setProperty(env, "get", funcVal(env, "get_" + name, 0, [name = String(name)] (jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) {
                if (count != 0)
                    throw Xjsi::JSError(rt, "getters take no arguments");
		return get_class().string_accessor.getter(rt, thisVal, name);
            }));
            desc.setProperty(env, "set", funcVal(env, "set_" + name, 1, [name = String(name)] (jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) {
                if (count != 1)
                    throw Xjsi::JSError(rt, "setters take exactly 1 argument");
		return get_class().string_accessor.setter(rt, thisVal, name, args[0]);
            }));

            defineProperty(env, constructorPrototype, name, desc);
        };

        for (auto&& property : schema.persisted_properties) { loopBody(property); }
        for (auto&& property : schema.computed_properties) { loopBody(property); }
    }

    static JsiObj create_instance_by_schema(JsiEnv env, JsiFunc* maybeConstructor, const realm::ObjectSchema& schema, Internal* internal = nullptr) {
        auto& s_schemaObjectTypes = get_schemaObjectTypes();
        auto& s_class = get_class();

        bool isRealmObjectClass = std::is_same_v<T, RealmObjectClass<hermes::Types>>;
        if (!isRealmObjectClass) {
            throw Xjsi::JSError(env, "Creating instances by schema is supported for RealmObjectClass only");
        }

        if (!internal) {
            throw Xjsi::JSError(env, "RealmObjectClass requires an internal realm object when creating instances by schema");
        }

        REALM_ASSERT_RELEASE(!s_class.index_accessor); // assume we don't need a ProxyWrapper

        auto config = internal->realm()->config();
        std::string path = config.path;
        auto version = internal->realm()->schema_version();
        std::string schemaName = schema.name + ":" + std::to_string(version); 

        const JsiFunc& realmObjectClassConstructor = *ObjectWrap<T>::s_ctor;

        auto& schemaObjects = s_schemaObjectTypes[path];

        //jsi::Symbol externalSymbol = ExternalSymbol;

        //if we are creating a RealmObject from schema with no user defined constructor
        if (!maybeConstructor) {
            //1.Check by name if the constructor is already created for this RealmObject 
            if (!schemaObjects.count(schemaName)) {

                //2.Create the constructor

                //create the RealmObject function by name
                // XXX May need to escape/sanitize schema.name to avoid code injection
                // This is gross, but might work if you pass "name" as an argument:
                //   return {[name]: function() {}}[name]
                auto schemaObjectConstructor =
                    globalType(env, "Function")
                            .callAsConstructor(env, "return function " + schema.name + "() {}")
                            .asObject(env).asFunction(env).call(env)
                            .asObject(env).asFunction(env);


                auto schemaProto = schemaObjectConstructor.getProperty(env, "prototype");
                ObjectSetPrototypeOf(env,
                                     schemaProto,
                                     realmObjectClassConstructor->getProperty(env, "prototype"));
                ObjectSetPrototypeOf(env,
                                     JsiVal(env(schemaObjectConstructor)),
                                     JsiVal(realmObjectClassConstructor));

                defineSchemaProperties(env, std::move(schemaProto).asObject(env), schema, true);

                schemaObjects.emplace(schemaName, std::move(schemaObjectConstructor));
            }
        } else {
            //creating a RealmObject with user defined constructor
            auto& constructor = *maybeConstructor;

            bool schemaExists = schemaObjects.count(schemaName);
            if (schemaExists) {
                //check if constructors have changed for the same schema object and name
                if (!jsi::Function::strictEquals(env, schemaObjects.at(schemaName), constructor)) {
                    schemaExists = false;
                    schemaObjects.erase(schemaName);
                }
            }

            if (!schemaExists) {
                schemaObjects.emplace(schemaName, JsiFunc(constructor).get());
                auto constructorPrototype = constructor->getPropertyAsObject(env, "prototype");

                //get all properties from the schema 
                defineSchemaProperties(env, env(constructorPrototype), schema, false);

                //Skip if the user defined constructor inherited the RealmObjectClass. All RealmObjectClass members are available already.
                if (!constructorPrototype.instanceOf(env, realmObjectClassConstructor)) {
                    //setup all RealmObjectClass<T> methods to the prototype of the object
                    auto realmObjectClassProto = realmObjectClassConstructor->getPropertyAsObject(env, "prototype");
                    for (auto& [name, method] : s_class.methods) {
                        //don't redefine if exists
                        // TODO should this use hasOwnProperty?
                        if (!constructorPrototype.hasProperty(env, propName(env, name))) {
                            copyProperty(env, realmObjectClassProto, constructorPrototype, name);
                        }
                    }

                    for (auto& [name, property] : s_class.properties) {
                        // TODO should this use hasOwnProperty?
                        if (!constructorPrototype.hasProperty(env, propName(env, name))) {
                            copyProperty(env, realmObjectClassProto, constructorPrototype, name);
                        }
                    }
                }
            }
        }

        const auto& schemaObjectCtor = schemaObjects.at(schemaName);
        auto instanceVal = schemaObjectCtor.callAsConstructor(env);
        if (!instanceVal.isObject()) {
            throw Xjsi::JSError(env, "Realm object constructor must not return another value");
        }
        auto instance = env(std::move(instanceVal).getObject(env));
        if (!instance->instanceOf(env, schemaObjectCtor)) {
            throw Xjsi::JSError(env, "Realm object constructor must not return another value");
        }

        set_internal(env, instance, internal);
        return instance;
    }

    static auto& get_class() {
        // TODO this is silly. These should be static properties.
        static T s_class;
        return s_class;
    }

    inline static auto& get_schemaObjectTypes() {
        // XXX this being static prevents using multiple runtimes.
        static std::unordered_map<std::string, std::unordered_map<std::string, jsi::Function>> s_schemaObjectTypes;
        return s_schemaObjectTypes;
    }
};

} // hermes

template<typename ClassType>
    class ObjectWrap<hermes::Types, ClassType> : public hermes::ObjectWrap<ClassType> {};

template<hermes::ArgumentsMethodType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) {
    auto env = JsiEnv(rt);
    auto result = hermes::ReturnValue(env);
    auto arguments = hermes::Arguments{env, count, args};

    F(env, env(thisVal).asObject(), arguments, result);
    return std::move(result).ToValue();
}

template<hermes::PropertyType::GetterType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) {
    auto env = JsiEnv(rt);
    auto result = hermes::ReturnValue(env);
    auto arguments = hermes::Arguments{env, count, args};
    arguments.validate_count(0);

    F(env, env(thisVal).asObject(), result);
    return std::move(result).ToValue();
}

template<hermes::PropertyType::SetterType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) {
    auto env = JsiEnv(rt);
    auto arguments = hermes::Arguments{env, count, args};
    arguments.validate_count(1);

    F(env, env(thisVal).asObject(), JsiVal(env, args[0]));

    return jsi::Value();
}

template<hermes::IndexPropertyType::GetterType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
    REALM_ASSERT_RELEASE(count == 2);
    auto env = JsiEnv(rt);
    auto out = hermes::ReturnValue(env);
    F(env, env(args[0]).asObject(), uint32_t(args[1].asNumber()), out);
    return std::move(out).ToValue();
}

template<hermes::IndexPropertyType::SetterType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
    REALM_ASSERT_RELEASE(count == 3);
    auto env = JsiEnv(rt);
    return jsi::Value(F(env, env(args[0]).asObject(), uint32_t(args[1].asNumber()), env(args[2])));
}

template<hermes::StringPropertyType::GetterType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value& thisVal, const hermes::String& str) {
    auto env = JsiEnv(rt);
    auto result = hermes::ReturnValue(env);
    F(env, env(thisVal).asObject(), str, result);
    return std::move(result).ToValue();
}

template<hermes::StringPropertyType::SetterType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value& thisVal, const hermes::String& str, const jsi::Value& value) {
    auto env = JsiEnv(rt);
    F(env, env(thisVal).asObject(), str, env(value));
    return jsi::Value();
}

template<hermes::StringPropertyType::EnumeratorType F>
jsi::Value wrap(jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) {
    // This is only used in the JSC impl.
    REALM_UNREACHABLE();
}

} // realm::js
