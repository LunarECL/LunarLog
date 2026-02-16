#ifndef LUNAR_LOG_FORMATTER_INTERFACE_HPP
#define LUNAR_LOG_FORMATTER_INTERFACE_HPP

#include "../core/log_entry.hpp"
#include "../core/log_common.hpp"
#include <string>
#include <vector>
#include <mutex>

namespace minta {
    class IFormatter {
    public:
        virtual ~IFormatter() = default;

        virtual std::string format(const LogEntry &entry) const = 0;

        /// Set the per-sink locale for culture-specific format specifiers.
        /// When set (non-empty), the formatter re-renders the message using
        /// this locale instead of the logger-level locale stored in the entry.
        /// Thread-safe: can be called concurrently with format().
        void setLocale(const std::string& locale) {
            std::lock_guard<std::mutex> lock(m_localeMutex);
            m_locale = locale;
        }

        std::string getLocale() const {
            std::lock_guard<std::mutex> lock(m_localeMutex);
            return m_locale;
        }

    protected:
        /// Re-render the entry's message with this formatter's locale.
        /// Returns the original message if no per-sink locale is set or
        /// if the per-sink locale matches the entry's locale.
        std::string localizedMessage(const LogEntry &entry) const {
            // Copy locale under lock, then use the copy outside.
            std::string localeCopy;
            {
                std::lock_guard<std::mutex> lock(m_localeMutex);
                localeCopy = m_locale;
            }
            if (localeCopy.empty() || localeCopy == entry.locale) {
                return entry.message;
            }
            // Textually-different locale names may resolve to the same
            // locale (e.g. "en_US" vs "en_US.UTF-8") — skip re-render.
            {
                auto resolved1 = detail::tryCreateLocale(localeCopy);
                auto resolved2 = detail::tryCreateLocale(entry.locale);
                if (resolved1.name() == resolved2.name()) {
                    return entry.message;
                }
            }
            std::vector<std::string> values;
            values.reserve(entry.properties.size());
            for (const auto &prop : entry.properties) {
                values.push_back(prop.value);
            }
            return detail::reformatMessage(entry.templateStr, values, localeCopy);
        }

    private:
        mutable std::mutex m_localeMutex;
        std::string m_locale;
    };
} // namespace minta

#endif // LUNAR_LOG_FORMATTER_INTERFACE_HPP
