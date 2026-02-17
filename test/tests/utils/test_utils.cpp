#include "test_utils.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define LUNARLOG_USE_FILESYSTEM
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#ifdef _MSC_VER
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#endif

std::string TestUtils::readLogFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void TestUtils::waitForFileContent(const std::string &filename, int maxAttempts) {
    for (int i = 0; i < maxAttempts; ++i) {
        if (fileExists(filename) && getFileSize(filename) > 0) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    throw std::runtime_error("Timeout waiting for file content: " + filename);
}

void TestUtils::cleanupLogFiles() {
    std::vector<std::string> filesToRemove = {
        "test_log.txt", "level_test_log.txt", "rate_limit_test_log.txt",
        "escaped_brackets_test.txt", "test_log1.txt", "test_log2.txt",
        "validation_test_log.txt", "custom_formatter_log.txt", "json_formatter_log.txt", "xml_formatter_log.txt",
        "context_test_log.txt", "default_formatter_log.txt",
        "suffix_format_test.txt", "suffix_json_test.txt", "suffix_xml_test.txt",
        "thread_safety_test.txt", "test_log2.json",
        "custom_sink_test.txt", "source_loc_test.txt",
        "operator_test.txt", "operator_json_test.txt", "operator_xml_test.txt",
        "template_json_test.txt", "template_xml_test.txt",
        "template_validation_test.txt", "template_cache_test.txt",
        "template_concurrent_test.txt", "template_eviction_test.txt",
        "template_resize_test.txt", "template_opcache_test.txt",
        "culture_number_test.txt", "culture_number_locale_test.txt",
        "culture_datetime_test.txt", "culture_default_test.txt",
        "culture_sink0_test.txt", "culture_sink1_test.txt",
        "culture_sinkdt0_test.txt", "culture_sinkdt1_test.txt",
        "culture_json_test.txt", "culture_xml_test.txt",
        "culture_thread_test.txt", "culture_invalid_test.txt",
        "culture_same_locale_test.txt",
        "culture_conc_sink_test.txt", "culture_locale_change_test.txt",
        "culture_mixed_spec_test.txt",
        "culture_multi0_test.txt", "culture_multi1_test.txt", "culture_multi2_test.txt",
        "culture_reset_test.txt",
        // Named sinks tests
        "test_named1.txt", "test_dup1.txt", "test_dup2.txt",
        "test_errors.txt", "test_chained.txt", "test_auto0.txt",
        "test_named_json.txt", "test_all.txt", "test_err_only.txt",
        "test_idx.txt", "test_filtered.txt", "test_clr.txt",
        "test_unnamed.txt", "test_named_mix.txt", "test_unnamed2.txt",
        "test_locale.txt", "test_empty_name.txt", "test_empty_name2.txt",
        "test_pre.txt", "test_post.txt", "test_fmt.txt", "test_fmt_throw.txt",
        "test_pred_filter.txt",
        "test_collision_named.txt", "test_collision_auto.txt",
        "test_multi_proxy.txt", "test_trace.txt", "test_info_lvl.txt",
        "test_error_lvl.txt", "test_spec_json.txt",
        // Tag routing tests
        "test_metrics.txt", "test_all_tags.txt", "test_no_debug.txt",
        "test_mixed_tags.txt", "test_specific.txt", "test_general.txt",
        "test_combo.txt", "test_audit.txt", "test_human_tags.txt",
        "test_json_tags.txt", "test_xml_tags.txt", "test_clear_tags.txt",
        "test_exc.txt", "test_notag1.txt", "test_notag2.txt",
        "test_metrics_warn.txt", "test_fmtarg.txt",
        // Pipe transform tests
        "pipe_test.txt", "pipe_json.txt", "pipe_xml.txt",
        "pipe_trunc_noop.txt", "pipe_json_noxf.txt", "pipe_xml_noxf.txt"
    };

    for (const auto &filename : filesToRemove) {
        removeFile(filename);
    }
}

bool TestUtils::fileExists(const std::string &filename) {
#ifdef LUNARLOG_USE_FILESYSTEM
    return fs::exists(filename);
#else
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
#endif
}

std::uintmax_t TestUtils::getFileSize(const std::string &filename) {
#ifdef LUNARLOG_USE_FILESYSTEM
    return fs::file_size(filename);
#else
    struct stat buffer;
    if (stat(filename.c_str(), &buffer) != 0) {
        return 0;
    }
    return buffer.st_size;
#endif
}

void TestUtils::removeFile(const std::string &filename) {
#ifdef LUNARLOG_USE_FILESYSTEM
    fs::remove(filename);
#else
    std::remove(filename.c_str());
#endif
}