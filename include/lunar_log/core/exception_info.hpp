#ifndef LUNAR_LOG_EXCEPTION_INFO_HPP
#define LUNAR_LOG_EXCEPTION_INFO_HPP

#include <string>
#include <exception>
#include <typeinfo>
#include <cstdlib>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace minta {
namespace detail {

    // abi::__cxa_demangle allocates and is not free, but this only runs
    // on exception-logging paths -- never on hot normal-logging paths.
    inline std::string demangleTypeName(const char* mangledName) {
        if (!mangledName) return "unknown";
#if defined(__GNUC__) || defined(__clang__)
        int status = 0;
        char* demangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            std::string result(demangled);
            std::free(demangled);
            return result;
        }
        std::free(demangled);
        return std::string(mangledName);
#else
        return std::string(mangledName);
#endif
    }

    inline std::string getExceptionTypeName(const std::exception& ex) {
        return demangleTypeName(typeid(ex).name());
    }

    // Cap nested exception unwinding to prevent stack overflow from
    // pathological or circular exception chains.
    static constexpr int kMaxNestedExceptionDepth = 20;

    struct ExceptionInfo {
        std::string type;
        std::string message;
        std::string chain;
    };

    inline const char* safeWhat(const std::exception& ex) {
        const char* msg = ex.what();
        return msg ? msg : "(no message)";
    }

    inline void unwindNestedExceptions(const std::exception& ex, std::string& chain, int depth) {
        if (depth >= kMaxNestedExceptionDepth) return;

        try {
            std::rethrow_if_nested(ex);
        } catch (const std::exception& nested) {
            if (!chain.empty()) chain += '\n';
            chain += demangleTypeName(typeid(nested).name());
            chain += ": ";
            chain += safeWhat(nested);
            unwindNestedExceptions(nested, chain, depth + 1);
        } catch (...) {
            if (!chain.empty()) chain += '\n';
            chain += "unknown exception";
        }
    }

    inline ExceptionInfo extractExceptionInfo(const std::exception& ex) {
        ExceptionInfo info;
        info.type = getExceptionTypeName(ex);
        info.message = safeWhat(ex);
        unwindNestedExceptions(ex, info.chain, 0);
        return info;
    }

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_EXCEPTION_INFO_HPP
