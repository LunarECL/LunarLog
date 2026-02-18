vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO LunarECL/LunarLog
    REF "v1.21.1"
    SHA512 eae370ab9de5e861eaf0f0a8822560277d57d4db6860b3fd97fbd0b66127b7df2c0a3ef2004b56b92fadce8242afe59b77b89559cfd21e171c55c2d436d98f3c
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
