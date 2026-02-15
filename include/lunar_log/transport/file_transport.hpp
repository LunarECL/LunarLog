#ifndef LUNAR_LOG_FILE_TRANSPORT_HPP
#define LUNAR_LOG_FILE_TRANSPORT_HPP

#include "transport_interface.hpp"
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace minta {
    class FileTransport : public ITransport {
    public:
        explicit FileTransport(const std::string &filename, bool autoFlush = true)
            : m_autoFlush(autoFlush) {
            m_file.open(filename, std::ios::app);
            if (!m_file.is_open()) {
                throw std::runtime_error("FileTransport: failed to open file: " + filename);
            }
        }

        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_file.good()) {
                m_file.clear();
                m_file << formattedEntry << '\n';
                if (!m_file.good()) {
                    if (!m_errorReported) {
                        m_errorReported = true;
                        std::fprintf(stderr, "FileTransport: write failed, some log entries may be lost\n");
                    }
                    return;
                }
                // Recovery succeeded
                m_errorReported = false;
            } else {
                m_file << formattedEntry << '\n';
            }
            if (m_autoFlush) {
                m_file << std::flush;
            }
        }

    private:
        std::ofstream m_file;
        std::mutex m_mutex;
        bool m_autoFlush;
        bool m_errorReported = false;
    };
} // namespace minta

#endif // LUNAR_LOG_FILE_TRANSPORT_HPP
