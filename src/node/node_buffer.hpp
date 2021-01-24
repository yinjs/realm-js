////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#include <iostream>
#include "napi.h"

namespace realm {
namespace js {

using TypedArray = Napi::TypedArray;
using DataView = Napi::DataView;
using Val = const Napi::Value;

struct NodeBuffer {
    template <typename ArrayBuffer>
    static int get_size(ArrayBuffer array_buffer)
    {
        std::cout << "Something Else Length" << std::endl;
        return array_buffer.ByteLength();
    }

    template <>
    static int get_size<Napi::Buffer<char>>(Napi::Buffer<char> buffer)
    {
        std::cout << "Node Buffer" << std::endl;
        return buffer.Length();
    }

    template <typename T>
    static OwnedBinaryData from_array(Val value)
    {
        std::cout << "Data From T" << std::endl;
        auto array_buffer = value.template As<T>();
        auto buffer = array_buffer.ArrayBuffer();
        return make_binary_object(buffer, get_size(buffer));
    }

    template <class T>
    static OwnedBinaryData from_buffer(Val value)
    {
        std::cout << "Data From Napi::ArrayBuffer" << std::endl;
        auto buffer = value.template As<T>();
        return make_binary_object(buffer, get_size(buffer));
    }

private:
    template <typename T>
    static OwnedBinaryData make_binary_object(T buffer, int len)
    {
        return OwnedBinaryData(static_cast<const char*>(buffer.Data()), len);
    }
};

} // namespace js
} // namespace realm
