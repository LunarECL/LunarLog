vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO LunarECL/LunarLog
    REF "v1.24.0"
    SHA512 29590514c004687c7ac0d2cc9648c6e701abbf94d1bf5c60742338a49eac63ffcc4c22819b5f4e178b42a44d1e42e362310112983ff9b02f14eda39edc76bf6e
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DLUNARLOG_BUILD_TESTS=OFF
        -DLUNARLOG_BUILD_EXAMPLES=OFF
        -DLUNARLOG_BUILD_BENCHMARKS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/LunarLog)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
