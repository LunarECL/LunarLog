vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO LunarECL/LunarLog
    REF "v1.21.0"
    SHA512 42795cfd82b66911c3a0273a812597bfa502e0115c57cc1893caa4ee4e4a79543531e7e3b2425d1f48f07a2f6f51eb9162b12390561ed62106ae3546f0e09ca7
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
