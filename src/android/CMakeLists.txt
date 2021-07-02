# // XXX this is passed in some times (eg from build-android.js) . Would need to fixs all users before renaming to remove JSC.
# JSC_ROOT_DIR allows sharing the same directory for all architectures and skip re-downloading
if(NOT JSC_ROOT_DIR)
    # set JSC_ROOT_DIR to current dir
    set(JSC_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(NOT RN_ROOT_DIR)
    set(RN_ROOT_DIR "${JSC_ROOT_DIR}")
endif()

set(RN_VERSION 0.64.2)

set(RN_LIB_DIR "${RN_ROOT_DIR}/package/android/com/facebook/react/react-native/${RN_VERSION}/jni/${ANDROID_ABI}")
set (RN_LIB_FILE "${RN_LIB_DIR}/libreactnativejni.so")

if(NOT EXISTS ${RN_LIB_FILE})
    set(RN_LIB_URL "https://registry.npmjs.org/react-native/-/react-native-${RN_VERSION}.tgz")

    message(STATUS "Getting ${RN_LIB_URL}...")
    file(DOWNLOAD "${RN_LIB_URL}" "${RN_ROOT_DIR}/react-native-${RN_VERSION}.tgz" STATUS download_status)

    list(GET download_status 0 status_code)
    if (NOT "${status_code}" STREQUAL "0")
        message(FATAL_ERROR "Downloading ${url}... Failed. Status: ${download_status}")
    endif()
    
    message(STATUS "Uncompressing ${RN_ROOT_DIR}/react-native-${RN_VERSION}.tgz")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xfz "react-native-${RN_VERSION}.tgz"
        WORKING_DIRECTORY "${RN_ROOT_DIR}"
    )

message(STATUS "Uncompressing ${RN_ROOT_DIR}/package/android/com/facebook/react/react-native/${RN_VERSION}/react-native-${RN_VERSION}.aar")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xfz "react-native-${RN_VERSION}.aar"
        WORKING_DIRECTORY "${RN_ROOT_DIR}/package/android/com/facebook/react/react-native/${RN_VERSION}/"
    )
endif()

add_library(realm-js-android SHARED
    # $<TARGET_OBJECTS:realm-js-shared>
    hack.cpp
    platform.cpp
    jni_utils.cpp
    io_realm_react_RealmReactModule.cpp
)

set_target_properties(realm-js-android PROPERTIES 
    OUTPUT_NAME "realm"
    PREFIX "lib" 
    SUFFIX ".so"
)

set_target_properties(realm-js-android
                      PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY "libs/${ANDROID_ABI}")

target_compile_definitions(realm-js-android PRIVATE
    REALM_HAVE_UV=0
    REALM_ANDROID=1
    REALM_HAVE_CONFIG=1
)

if (ANDROID_ABI MATCHES "^armeabi")
    target_compile_definitions(realm-js-android PUBLIC REALM_WRAP_MEMMOVE=1)
    target_link_options(realm-js-android PUBLIC -Wl,--wrap=memmove -Wl,--wrap=memcpy)
else()
    target_compile_definitions(realm-js-android PUBLIC REALM_WRAP_MEMMOVE=0)
endif()

#target_link_options(realm-js-android PUBLIC  -fvisibility=hidden)

if(REALM_JS_BUILD_CORE_FROM_SOURCE AND TARGET ObjectStore)
    target_compile_definitions(ObjectStore PUBLIC
        REALM_PLATFORM=Android
        REALM_ANDROID=1
    )
endif()

set(JSI_INCLUDES_DIR "${RN_ROOT_DIR}/package/ReactCommon/jsi")

# XXX decide if we want to use these headers.
# If so, inject them on iOS as well, and delete src/hermes/jsi/.
# If not go back to injecting that into the include path.
target_include_directories(realm-js-shared PRIVATE ${JSI_INCLUDES_DIR})
target_include_directories(realm-js-hermes PRIVATE ${JSI_INCLUDES_DIR})
target_include_directories(realm-js-android PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${JSI_INCLUDES_DIR})

# XXX toggle between building jsi.cpp in our lib vs linking against libreactnativejni which has the symbols we need.
# Things break slightly differently with each option.
if(0)
    target_sources(realm-js-hermes PRIVATE ${JSI_INCLUDES_DIR}/jsi/jsi.cpp)
else()
    find_library(
            REACT_NATIVE_JNI_LIB
            reactnativejni
            PATHS ${RN_LIB_DIR}
            NO_CMAKE_FIND_ROOT_PATH
    )
    find_library(
            RN_CPP_SHARED
            c++_shared
            PATHS ${RN_LIB_DIR}
            NO_CMAKE_FIND_ROOT_PATH
    )

    target_link_libraries(realm-js-android PRIVATE ${REACT_NATIVE_JNI_LIB})
    target_link_libraries(realm-js-hermes PUBLIC ${RN_CPP_SHARED})
endif()

#target_link_libraries(realm-js-android PRIVATE -l:libgcc.a)
#target_link_libraries(realm-js-android PRIVATE -l:libc++abi.a)
target_link_libraries(realm-js-android PRIVATE realm-js-hermes realm-js-shared -v )


if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_custom_command(TARGET realm-js-android
        POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:realm-js-android>)
endif()
