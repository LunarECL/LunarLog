#ifndef LUNAR_LOG_HTTP_SINK_HPP
#define LUNAR_LOG_HTTP_SINK_HPP

#include "batched_sink.hpp"
#include "../formatter/compact_json_formatter.hpp"
#include <string>
#include <map>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

namespace minta {

    /// Configuration options for HttpSink.
    ///
    /// @code
    ///   HttpSinkOptions opts("http://localhost:8080/logs");
    ///   opts.setHeader("Authorization", "Bearer token123")
    ///       .setContentType("application/json")
    ///       .setBatchSize(100)
    ///       .setFlushIntervalMs(3000);
    /// @endcode
    struct HttpSinkOptions {
        std::string url;
        std::string contentType;
        std::map<std::string, std::string> headers;
        size_t timeoutMs;
        bool verifySsl;

        // Batch options (forwarded to BatchedSink)
        size_t batchSize;
        size_t flushIntervalMs;
        size_t maxRetries;
        size_t maxQueueSize;

        explicit HttpSinkOptions(const std::string& url_)
            : url(url_)
            , contentType("application/json")
            , timeoutMs(10000)
            , verifySsl(true)
            , batchSize(50)
            , flushIntervalMs(5000)
            , maxRetries(3)
            , maxQueueSize(10000) {}

        HttpSinkOptions& setHeader(const std::string& key, const std::string& val) {
            headers[key] = val;
            return *this;
        }
        HttpSinkOptions& setContentType(const std::string& ct) {
            contentType = ct;
            return *this;
        }
        HttpSinkOptions& setTimeoutMs(size_t ms) {
            timeoutMs = ms;
            return *this;
        }
        HttpSinkOptions& setBatchSize(size_t n) {
            batchSize = n;
            return *this;
        }
        HttpSinkOptions& setFlushIntervalMs(size_t ms) {
            flushIntervalMs = ms;
            return *this;
        }
        HttpSinkOptions& setMaxRetries(size_t n) {
            maxRetries = n;
            return *this;
        }
        HttpSinkOptions& setMaxQueueSize(size_t n) {
            maxQueueSize = n;
            return *this;
        }
        HttpSinkOptions& setVerifySsl(bool v) {
            verifySsl = v;
            return *this;
        }
    };

namespace detail {

    /// Parse a URL into scheme, host, port, and path components.
    struct ParsedUrl {
        std::string scheme;  // "http" or "https"
        std::string host;
        int port;
        std::string path;
        bool valid;

        ParsedUrl() : port(80), valid(false) {}
    };

    inline ParsedUrl parseUrl(const std::string& url) {
        ParsedUrl result;
        result.valid = false;

        // Find scheme
        size_t schemeEnd = url.find("://");
        if (schemeEnd == std::string::npos) return result;

        result.scheme = url.substr(0, schemeEnd);
        if (result.scheme != "http" && result.scheme != "https") return result;

        size_t hostStart = schemeEnd + 3;
        if (hostStart >= url.size()) return result;

        // Find path
        size_t pathStart = url.find('/', hostStart);
        std::string hostPort;
        if (pathStart == std::string::npos) {
            hostPort = url.substr(hostStart);
            result.path = "/";
        } else {
            hostPort = url.substr(hostStart, pathStart - hostStart);
            result.path = url.substr(pathStart);
        }

        // Find port
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            result.host = hostPort.substr(0, colonPos);
            std::string portStr = hostPort.substr(colonPos + 1);
            if (portStr.empty()) return result;
            result.port = std::atoi(portStr.c_str());
            if (result.port <= 0 || result.port > 65535) return result;
        } else {
            result.host = hostPort;
            result.port = (result.scheme == "https") ? 443 : 80;
        }

        if (result.host.empty()) return result;

        result.valid = true;
        return result;
    }

} // namespace detail

    /// HTTP/HTTPS sink that sends log batches as HTTP POST requests.
    ///
    /// Extends BatchedSink to batch log entries and send them as JSONL
    /// (newline-delimited JSON) payloads. Uses CompactJsonFormatter by default.
    ///
    /// Transport strategy:
    /// - http:// on POSIX: raw TCP socket with HTTP/1.1
    /// - https:// on POSIX: curl subprocess fallback
    /// - Windows: WinHTTP API
    ///
    /// @code
    ///   // Send logs to a local endpoint
    ///   HttpSinkOptions opts("http://localhost:8080/api/logs");
    ///   opts.setHeader("Authorization", "Bearer mytoken");
    ///   logger.addSink<HttpSink>(opts);
    ///
    ///   // Datadog example
    ///   HttpSinkOptions dd("https://http-intake.logs.datadoghq.com/api/v2/logs");
    ///   dd.setHeader("DD-API-KEY", "your-api-key");
    ///   dd.setBatchSize(100).setFlushIntervalMs(5000);
    ///   logger.addSink<HttpSink>(dd);
    /// @endcode
    class HttpSink : public BatchedSink {
    public:
        explicit HttpSink(HttpSinkOptions opts)
            : BatchedSink(makeBatchOptions(opts))
            , m_opts(std::move(opts))
            , m_formatter(detail::make_unique<CompactJsonFormatter>())
        {}

        ~HttpSink() noexcept {
            stopAndFlush();
        }

    protected:
        void writeBatch(const std::vector<const LogEntry*>& batch) override {
            std::string body = formatBatch(batch);
            if (body.empty()) return;

            detail::ParsedUrl parsed = detail::parseUrl(m_opts.url);
            if (!parsed.valid) {
                throw std::runtime_error("HttpSink: invalid URL: " + m_opts.url);
            }

            std::map<std::string, std::string> allHeaders = m_opts.headers;
            allHeaders["Content-Type"] = m_opts.contentType;

            bool ok = httpPost(m_opts.url, body, allHeaders, m_opts.timeoutMs);
            if (!ok) {
                throw std::runtime_error("HttpSink: HTTP POST failed to " + m_opts.url);
            }
        }

        void onBatchError(const std::exception& e, size_t retryCount) override {
            std::fprintf(stderr, "[HttpSink] Batch error (retry %zu): %s\n",
                        retryCount, e.what());
        }

    private:
        static BatchOptions makeBatchOptions(const HttpSinkOptions& opts) {
            BatchOptions bo;
            bo.setBatchSize(opts.batchSize)
              .setFlushIntervalMs(opts.flushIntervalMs)
              .setMaxRetries(opts.maxRetries)
              .setMaxQueueSize(opts.maxQueueSize);
            return bo;
        }

        std::string formatBatch(const std::vector<const LogEntry*>& batch) {
            std::string result;
            result.reserve(batch.size() * 256);
            for (size_t i = 0; i < batch.size(); ++i) {
                if (i > 0) result += '\n';
                result += m_formatter->format(*batch[i]);
            }
            return result;
        }

        bool httpPost(const std::string& url, const std::string& body,
                      const std::map<std::string, std::string>& headers,
                      size_t timeoutMs) {
            detail::ParsedUrl parsed = detail::parseUrl(url);
            if (!parsed.valid) return false;

#ifdef _WIN32
            return httpPostWinHTTP(url, body, headers, timeoutMs);
#else
            if (parsed.scheme == "https") {
                return httpPostCurl(url, body, headers, timeoutMs);
            }
            return httpPostPosix(parsed.host, parsed.port, parsed.path,
                                body, headers, timeoutMs);
#endif
        }

#ifndef _WIN32
        bool httpPostPosix(const std::string& host, int port,
                          const std::string& path, const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          size_t timeoutMs) {
            // Resolve host
            struct addrinfo hints, *res = nullptr;
            std::memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            std::string portStr = std::to_string(port);
            int gaiResult = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
            if (gaiResult != 0 || !res) {
                if (res) freeaddrinfo(res);
                return false;
            }

            int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sockfd < 0) {
                freeaddrinfo(res);
                return false;
            }

            // Set timeout via select
            struct timeval tv;
            tv.tv_sec = static_cast<long>(timeoutMs / 1000);
            tv.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);

            // Set non-blocking for connect timeout
            int flags = fcntl(sockfd, F_GETFL, 0);
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

            int connectResult = connect(sockfd, res->ai_addr, res->ai_addrlen);
            freeaddrinfo(res);

            if (connectResult < 0) {
                if (errno != EINPROGRESS) {
                    close(sockfd);
                    return false;
                }
                // Wait for connection with timeout
                fd_set writefds;
                FD_ZERO(&writefds);
                FD_SET(sockfd, &writefds);

                int sel = select(sockfd + 1, nullptr, &writefds, nullptr, &tv);
                if (sel <= 0) {
                    close(sockfd);
                    return false;
                }

                int so_error = 0;
                socklen_t len = sizeof(so_error);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error != 0) {
                    close(sockfd);
                    return false;
                }
            }

            // Set back to blocking
            fcntl(sockfd, F_SETFL, flags);

            // Set socket timeout for send/recv
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            // Build HTTP request
            std::string request;
            request += "POST " + path + " HTTP/1.1\r\n";
            request += "Host: " + host + "\r\n";
            request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            request += "Connection: close\r\n";
            for (std::map<std::string, std::string>::const_iterator it = headers.begin();
                 it != headers.end(); ++it) {
                request += it->first + ": " + it->second + "\r\n";
            }
            request += "\r\n";
            request += body;

            // Send
            ssize_t totalSent = 0;
            ssize_t requestLen = static_cast<ssize_t>(request.size());
            while (totalSent < requestLen) {
                ssize_t sent = send(sockfd, request.c_str() + totalSent,
                                   static_cast<size_t>(requestLen - totalSent), 0);
                if (sent <= 0) {
                    close(sockfd);
                    return false;
                }
                totalSent += sent;
            }

            // Read response status line
            char buf[1024];
            ssize_t received = recv(sockfd, buf, sizeof(buf) - 1, 0);
            close(sockfd);

            if (received <= 0) return false;
            buf[received] = '\0';

            // Parse HTTP status code
            // Expected: "HTTP/1.1 200 OK\r\n..."
            const char* statusPtr = std::strstr(buf, " ");
            if (!statusPtr) return false;
            int statusCode = std::atoi(statusPtr + 1);
            return (statusCode >= 200 && statusCode < 300);
        }

        bool httpPostCurl(const std::string& url, const std::string& body,
                         const std::map<std::string, std::string>& headers,
                         size_t timeoutMs) {
            // Build curl command
            std::string cmd = "curl -s -o /dev/null -w '%{http_code}' -X POST";
            cmd += " --max-time " + std::to_string(timeoutMs / 1000);

            for (std::map<std::string, std::string>::const_iterator it = headers.begin();
                 it != headers.end(); ++it) {
                cmd += " -H '" + it->first + ": " + it->second + "'";
            }

            cmd += " --data-binary @-";
            cmd += " '" + url + "'";

            // Use popen to pipe body to curl
            FILE* pipe = popen(cmd.c_str(), "w");
            if (!pipe) return false;

            size_t written = fwrite(body.c_str(), 1, body.size(), pipe);
            int exitCode = pclose(pipe);

            if (written != body.size()) return false;

            // pclose returns the exit status of the command
            // curl returns 0 on success
            return (exitCode == 0);
        }
#endif // !_WIN32

#ifdef _WIN32
        bool httpPostWinHTTP(const std::string& url, const std::string& body,
                            const std::map<std::string, std::string>& headers,
                            size_t timeoutMs) {
            detail::ParsedUrl parsed = detail::parseUrl(url);
            if (!parsed.valid) return false;

            // Convert strings to wide chars for WinHTTP
            std::wstring wHost(parsed.host.begin(), parsed.host.end());
            std::wstring wPath(parsed.path.begin(), parsed.path.end());

            HINTERNET hSession = WinHttpOpen(L"LunarLog/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) return false;

            // Set timeouts
            DWORD timeout = static_cast<DWORD>(timeoutMs);
            WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

            HINTERNET hConnect = WinHttpConnect(hSession,
                wHost.c_str(),
                static_cast<INTERNET_PORT>(parsed.port), 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return false;
            }

            DWORD flags = (parsed.scheme == "https") ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                L"POST", wPath.c_str(),
                NULL, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            // Add headers
            for (std::map<std::string, std::string>::const_iterator it = headers.begin();
                 it != headers.end(); ++it) {
                std::string headerLine = it->first + ": " + it->second;
                std::wstring wHeader(headerLine.begin(), headerLine.end());
                WinHttpAddRequestHeaders(hRequest, wHeader.c_str(),
                    static_cast<DWORD>(wHeader.size()),
                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            }

            // Send request
            BOOL result = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                const_cast<char*>(body.c_str()),
                static_cast<DWORD>(body.size()),
                static_cast<DWORD>(body.size()), 0);

            if (!result) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            result = WinHttpReceiveResponse(hRequest, NULL);
            if (!result) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            // Get status code
            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            return (statusCode >= 200 && statusCode < 300);
        }
#endif // _WIN32

        HttpSinkOptions m_opts;
        std::unique_ptr<CompactJsonFormatter> m_formatter;
    };

} // namespace minta

#endif // LUNAR_LOG_HTTP_SINK_HPP
