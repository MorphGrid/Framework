cmake_minimum_required(VERSION 3.25)
project(framework VERSION 1.0.2 LANGUAGES CXX)

option(ENABLE_TESTS "Enable testing" ON)
option(ENABLE_DEBUG "Enable debugging" ON)
option(ENABLE_STATIC_LINKING "Enable static linking feature" OFF)
option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU optimization feature" OFF)
option(ENABLE_CI "Enable CI settings" OFF)

if (ENABLE_STATIC_LINKING)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(BOOST_ALL_NO_LIB ON)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
#
    if (ENABLE_CI)
        set(OPENSSL_CRYPTO_LIBRARY "/usr/lib/libcrypto.a")
        set(OPENSSL_SSL_LIBRARY "/usr/lib/libssl.a")
    else ()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            set(ARCH_LIB_DIR "/usr/lib/aarch64-linux-gnu")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
            set(ARCH_LIB_DIR "/usr/lib/x86_64-linux-gnu")
        else()
            message(FATAL_ERROR "Arquitectura no soportada: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        set(OPENSSL_CRYPTO_LIBRARY "${ARCH_LIB_DIR}/libcrypto.a")
        set(OPENSSL_SSL_LIBRARY "${ARCH_LIB_DIR}/libssl.a")
    endif ()
endif ()


message(STATUS "Using ENABLE_STATIC_LINKING = ${ENABLE_STATIC_LINKING}")
message(STATUS "Using ENABLE_NATIVE_OPTIMIZATION = ${ENABLE_NATIVE_OPTIMIZATION}")

set(CMAKE_CXX_STANDARD 23)

if (ENABLE_DEBUG)
    add_definitions(-DDEBUG_ENABLED)
endif ()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if (ENABLE_STATIC_LINKING)
    set(OpenSSL_USE_STATIC_LIBS TRUE)
    if (ENABLE_NATIVE_OPTIMIZATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -march=native -mtune=native -g0 -static -static-libgcc -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-O3 -flto -march=native -mtune=native -g0 -static -static-libgcc -DNDEBUG")
    else ()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -g0 -static -static-libgcc -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-O3 -flto -g0 -static -static-libgcc -DNDEBUG")
    endif ()
else ()
    if (ENABLE_NATIVE_OPTIMIZATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -march=native -mtune=native -g0 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-O3 -flto -march=native -mtune=native -g0 -DNDEBUG")
    else ()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    endif ()
endif ()

find_package(Boost REQUIRED COMPONENTS program_options json charconv)

find_package(laserpants_dotenv REQUIRED)
find_package(OpenSSL REQUIRED)

list(APPEND USED_LIBRARIES ${Boost_LIBRARIES} bcrypt laserpants::dotenv)

file(GLOB_RECURSE FRAMEWORK_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/objects/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

message(STATUS "Found ${FRAMEWORK_SOURCES} as framework sources")

add_library(framework_objects OBJECT ${FRAMEWORK_SOURCES})
if (NOT ENABLE_STATIC_LINKING)
    set_target_properties(framework_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(framework_objects
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
)
target_compile_features(framework_objects PUBLIC cxx_std_23)

target_link_libraries(framework_objects
        PUBLIC
        Boost::program_options
        Boost::json
        Boost::charconv
        laserpants::dotenv
        ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
)

if (ENABLE_STATIC_LINKING)
    add_library(framework STATIC $<TARGET_OBJECTS:framework_objects>)
    set_target_properties(framework PROPERTIES OUTPUT_NAME "framework")
else()
    add_library(framework SHARED $<TARGET_OBJECTS:framework_objects>)
    set_target_properties(framework PROPERTIES OUTPUT_NAME "framework")
endif()

target_include_directories(framework PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(framework PUBLIC cxx_std_23)

if (ENABLE_STATIC_LINKING)
    target_compile_options(framework PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(framework PRIVATE -static -static-libgcc -static-libstdc++)
endif()


target_link_libraries(framework
        PUBLIC
        Boost::program_options
        Boost::json
        Boost::charconv
        laserpants::dotenv
        ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
        bcrypt
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")


install(TARGETS framework EXPORT frameworkTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT frameworkTargets
        FILE frameworkTargets.cmake
        NAMESPACE framework::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/framework
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/frameworkConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/frameworkConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/frameworkConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/framework
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/frameworkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/frameworkConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/framework
)


if (ENABLE_TESTS)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    file(GLOB_RECURSE FRAMEWORK_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc")

    add_executable(framework-tests
            deps/asio.cxx
            deps/beast.cxx
            deps/redis.cxx
            deps/mysql.cxx
            ${FRAMEWORK_TESTS})

    if (ENABLE_STATIC_LINKING)
        target_compile_options(framework-tests PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(framework-tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    target_link_libraries(framework-tests
            PRIVATE
            GTest::gtest_main
            framework
            ${OPENSSL_SSL_LIBRARY}
            ${OPENSSL_CRYPTO_LIBRARY}
    )

    include(GoogleTest)
    gtest_discover_tests(framework-tests)
endif()

add_library(framework::framework ALIAS framework)