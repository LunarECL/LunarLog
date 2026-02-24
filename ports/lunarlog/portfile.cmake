vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO LunarECL/LunarLog
    REF "v1.29.0"
    SHA512 0  # SHA512 will be updated after release tag is created
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
