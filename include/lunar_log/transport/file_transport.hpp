#ifndef LUNAR_LOG_FILE_TRANSPORT_HPP
#define LUNAR_LOG_FILE_TRANSPORT_HPP

#include "transport_interface.hpp"
#include <fstream>
#include <mutex>

namespace minta {
    class FileTransport : public ITransport {
    public:
        FileTransport(const std::string &filename) : m_filename(filename) {
            m_file.open(filename, std::ios::app);
        }

        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_file << formattedEntry << std::endl;
            m_file.flush();
        }

    private:
        std::string m_filename;
        std::ofstream m_file;
        std::mutex m_mutex;
    };
} // namespace minta

#endif // LUNAR_LOG_FILE_TRANSPORT_HPP
