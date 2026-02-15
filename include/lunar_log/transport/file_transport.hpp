#ifndef LUNAR_LOG_FILE_TRANSPORT_HPP
#define LUNAR_LOG_FILE_TRANSPORT_HPP

#include "transport_interface.hpp"
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace minta {
    class FileTransport : public ITransport {
    public:
        explicit FileTransport(const std::string &filename) : m_filename(filename) {
            m_file.open(filename, std::ios::app);
            if (!m_file.is_open()) {
                throw std::runtime_error("FileTransport: failed to open file: " + filename);
            }
        }

        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_file.good()) {
                return;
            }
            m_file << formattedEntry << '\n' << std::flush;
        }

    private:
        std::string m_filename;
        std::ofstream m_file;
        std::mutex m_mutex;
    };
} // namespace minta

#endif // LUNAR_LOG_FILE_TRANSPORT_HPP
