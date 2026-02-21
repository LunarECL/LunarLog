#ifndef LUNAR_LOG_CALLBACK_SINK_HPP
#define LUNAR_LOG_CALLBACK_SINK_HPP

#include "sink_interface.hpp"
#include "../formatter/compact_json_formatter.hpp"
#include <functional>
#include <string>
#include <memory>

namespace minta {

    /// Sink that invokes a user-provided callback for each log entry.
    ///
    /// Two variants:
    ///   1. EntryCallback — receives the raw LogEntry for custom processing.
    ///   2. StringCallback — receives the formatted string (CompactJsonFormatter
    ///      by default, or a user-supplied formatter).
    ///
    /// @note The callback is called **without** a lock. If the callback accesses
    ///       shared state, the user is responsible for thread safety inside
    ///       the callback.
    /// @note If the callback throws an exception, the exception is silently
    ///       caught by the logger's internal processing thread.  The log
    ///       entry that triggered the throw is lost, but subsequent entries
    ///       are unaffected.  Callbacks should be noexcept or handle their
    ///       own errors internally.
    class CallbackSink : public ISink {
    public:
        using EntryCallback  = std::function<void(const LogEntry&)>;
        using StringCallback = std::function<void(const std::string&)>;

        /// Variant 1: raw LogEntry callback.
        /// The entry is passed directly; no formatting is performed.
        ///
        /// In C++11, wrap lambdas in the typedef to avoid overload ambiguity:
        /// @code
        ///   CallbackSink(EntryCallback([](const LogEntry& e) { ... }))
        /// @endcode
        explicit CallbackSink(EntryCallback cb)
            : m_entryCallback(std::move(cb))
            , m_mode(Mode::Entry) {}

        /// Variant 2: formatted string callback with optional formatter.
        /// If formatter is nullptr, CompactJsonFormatter is used as default.
        ///
        /// In C++11, wrap lambdas in the typedef to avoid overload ambiguity:
        /// @code
        ///   CallbackSink(StringCallback([](const std::string& s) { ... }))
        /// @endcode
        explicit CallbackSink(StringCallback cb, std::unique_ptr<IFormatter> fmt = nullptr)
            : m_stringCallback(std::move(cb))
            , m_mode(Mode::String) {
            if (fmt) {
                setFormatter(std::move(fmt));
            } else {
                setFormatter(detail::make_unique<CompactJsonFormatter>());
            }
        }

        void write(const LogEntry& entry) override {
            if (m_mode == Mode::Entry) {
                if (m_entryCallback) {
                    m_entryCallback(entry);
                }
            } else {
                if (m_stringCallback) {
                    IFormatter* fmt = formatter();
                    if (fmt) {
                        m_stringCallback(fmt->format(entry));
                    }
                }
            }
        }

        void flush() override {} // no-op, callbacks are synchronous

    private:
        enum class Mode { Entry, String };

        EntryCallback  m_entryCallback;
        StringCallback m_stringCallback;
        Mode           m_mode;
    };

} // namespace minta

#endif // LUNAR_LOG_CALLBACK_SINK_HPP
