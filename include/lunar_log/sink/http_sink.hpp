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
#include <climits>
#include <sstream>
#include <algorithm>
#include <cctype>

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
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>

#ifdef MSG_NOSIGNAL
#define MSG_NOSIGNAL_COMPAT MSG_NOSIGNAL
#else
#define MSG_NOSIGNAL_COMPAT 0
#endif
#endif

namespace minta {

    /// Configuration options for HttpSink.
    ///
    /// @note DNS resolution (getaddrinfo) is not bounded by timeoutMs.
    ///       DNS may block for 30+ seconds on unresponsive resolvers.
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
        size_t retryDelayMs;

        explicit HttpSinkOptions(const std::string& url_)
            : url(url_)
            , contentType("application/json")
            , timeoutMs(10000)
            , verifySsl(true)
            , batchSize(50)
            , flushIntervalMs(5000)
            , maxRetries(3)
            , maxQueueSize(10000)
            , retryDelayMs(1000) {}

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
        HttpSinkOptions& setRetryDelayMs(size_t ms) {
            retryDelayMs = ms;
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
        // RFC 3986 §3.1: scheme is case-insensitive
        std::transform(result.scheme.begin(), result.scheme.end(), result.scheme.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
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

        // Note: IPv6 literal URLs (e.g. http://[::1]:8080/path) are not supported.
        // The bracket notation in the host conflicts with port-separator detection.
        if (!hostPort.empty() && hostPort[0] == '[') {
            return result; // valid=false — IPv6 not supported
        }

        // Find port
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            result.host = hostPort.substr(0, colonPos);
            std::string portStr = hostPort.substr(colonPos + 1);
            if (portStr.empty()) return result;
            char* endPtr = nullptr;
            long portLong = std::strtol(portStr.c_str(), &endPtr, 10);
            if (endPtr == portStr.c_str() || *endPtr != '\0' || portLong < 1 || portLong > 65535) {
                return result;
            }
            result.port = static_cast<int>(portLong);
        } else {
            result.host = hostPort;
            result.port = (result.scheme == "https") ? 443 : 80;
        }

        if (result.host.empty()) return result;

        // Defense-in-depth: reject control characters and spaces to prevent
        // HTTP header injection and malformed request lines (RFC 3986 §2.1).
        for (size_t i = 0; i < result.host.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(result.host[i]);
            if (c <= 0x20 || c == 0x7F) return result;
        }
        for (size_t i = 0; i < result.path.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(result.path[i]);
            if (c <= 0x20 || c == 0x7F) return result;
        }

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
        {
            // Validate URL at construction time and cache parsed result.
            // parseUrl() sets valid=false for missing host, unsupported scheme,
            // and out-of-range port, so a single check covers all cases.
            m_parsedUrl = detail::parseUrl(m_opts.url);
            if (!m_parsedUrl.valid) {
                throw std::invalid_argument("HttpSink: invalid URL: " + m_opts.url);
            }
        }

        ~HttpSink() noexcept {
            stopAndFlush();
        }

    protected:
        void writeBatch(const std::vector<const LogEntry*>& batch) override {
            std::string body = formatBatch(batch);
            if (body.empty()) return;

            std::map<std::string, std::string> allHeaders = m_opts.headers;
            allHeaders["Content-Type"] = m_opts.contentType;
            if (allHeaders.find("User-Agent") == allHeaders.end()) {
                allHeaders["User-Agent"] = "LunarLog/1.0";
            }

            bool ok = httpPost(body, allHeaders, m_opts.timeoutMs);
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
              .setMaxQueueSize(opts.maxQueueSize)
              .setRetryDelayMs(opts.retryDelayMs);
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

        bool httpPost(const std::string& body,
                      const std::map<std::string, std::string>& headers,
                      size_t timeoutMs) {
#ifdef _WIN32
            return httpPostWinHTTP(body, headers, timeoutMs);
#else
            if (m_parsedUrl.scheme == "https") {
                return httpPostCurl(m_opts.url, body, headers, timeoutMs);
            }
            return httpPostPosix(m_parsedUrl.host, m_parsedUrl.port, m_parsedUrl.path,
                                body, headers, timeoutMs);
#endif
        }

#ifndef _WIN32
        bool httpPostPosix(const std::string& host, int port,
                          const std::string& path, const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          size_t timeoutMs) {
            // Absolute deadline: caps total elapsed time across all syscalls
            // to prevent slow-drip attacks that reset per-call timeouts.
            struct timespec deadline;
            clock_gettime(CLOCK_MONOTONIC, &deadline);
            deadline.tv_sec += static_cast<time_t>(timeoutMs / 1000);
            deadline.tv_nsec += static_cast<long>((timeoutMs % 1000) * 1000000L);
            if (deadline.tv_nsec >= 1000000000L) {
                deadline.tv_sec += 1;
                deadline.tv_nsec -= 1000000000L;
            }

            auto remainingMs = [&]() -> long {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long ms = (deadline.tv_sec - now.tv_sec) * 1000L
                        + (deadline.tv_nsec - now.tv_nsec) / 1000000L;
                return ms > 0 ? ms : 0;
            };

            // Resolve host
            struct addrinfo hints, *res = nullptr;
            std::memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;  // IPv4 + IPv6
            hints.ai_socktype = SOCK_STREAM;

            std::string portStr = std::to_string(port);
            // NOTE: getaddrinfo() has no portable timeout mechanism on POSIX.
            // DNS resolution may block for 30+ seconds (system resolver timeout),
            // causing total elapsed time to exceed timeoutMs by the DNS duration.
            // The CLOCK_MONOTONIC deadline still bounds all subsequent socket operations.
            // TODO(future): consider getaddrinfo_a (glibc) or a dedicated resolver
            //               thread for portable DNS timeout support.
            int gaiResult = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
            if (gaiResult != 0 || !res) {
                return false;
            }

            struct AddrInfoGuard {
                struct addrinfo* p;
                explicit AddrInfoGuard(struct addrinfo* a) : p(a) {}
                ~AddrInfoGuard() { if (p) freeaddrinfo(p); }
                AddrInfoGuard(const AddrInfoGuard&) = delete;
                AddrInfoGuard& operator=(const AddrInfoGuard&) = delete;
            } addrGuard(res);

            int sockfd = -1;
            for (struct addrinfo* rp = res; rp; rp = rp->ai_next) {
                if (remainingMs() <= 0) return false;

                int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (fd < 0) continue;

                // Set non-blocking for connect timeout
                int flags = fcntl(fd, F_GETFL, 0);
                if (flags < 0) {
                    ::close(fd);
                    continue;
                }
                if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                    ::close(fd);
                    continue;
                }

                int connectResult = connect(fd, rp->ai_addr, rp->ai_addrlen);
                if (connectResult < 0) {
                    if (errno != EINPROGRESS) {
                        ::close(fd);
                        continue;
                    }
                    // Wait for connection with timeout
                    struct pollfd pfd;
                    pfd.fd = fd;
                    pfd.events = POLLOUT;
                    pfd.revents = 0;

                    int sel;
                    do {
                        long rm = remainingMs();
                        if (rm <= 0) { sel = 0; break; }
                        sel = poll(&pfd, 1, static_cast<int>(rm > static_cast<long>(INT_MAX) ? INT_MAX : rm));
                    } while (sel < 0 && errno == EINTR);
                    if (sel <= 0) {
                        ::close(fd);
                        continue;
                    }

                    int so_error = 0;
                    socklen_t len = sizeof(so_error);
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0 || so_error != 0) {
                        ::close(fd);
                        continue;
                    }
                }

                // Set back to blocking
                if (fcntl(fd, F_SETFL, flags) < 0) {
                    ::close(fd);
                    continue;
                }

                sockfd = fd;
                break;
            }

            if (sockfd < 0) return false;

            struct ScopedSocket {
                int fd;
                explicit ScopedSocket(int f) : fd(f) {}
                ~ScopedSocket() { if (fd >= 0) { ::close(fd); } }
                ScopedSocket(const ScopedSocket&) = delete;
                ScopedSocket& operator=(const ScopedSocket&) = delete;
                ScopedSocket(ScopedSocket&&) = delete;
                ScopedSocket& operator=(ScopedSocket&&) = delete;
            };
            ScopedSocket guard(sockfd);

#ifdef SO_NOSIGPIPE
            {
                int one = 1;
                setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
            }
#endif

            auto setSocketTimeout = [&](int socketFd, int optname) -> bool {
                long rm = remainingMs();
                if (rm <= 0) return false;
                struct timeval tv;
                tv.tv_sec = static_cast<long>(rm / 1000);
                tv.tv_usec = static_cast<long>((rm % 1000) * 1000);
                return setsockopt(socketFd, SOL_SOCKET, optname, &tv, sizeof(tv)) == 0;
            };

            // Build HTTP request
            std::string request;
            request += "POST " + path + " HTTP/1.1\r\n";
            std::string hostHeader = host;
            // httpPostPosix handles http:// only; default port is 80.
            if (port != 80) {
                hostHeader += ":" + std::to_string(port);
            }
            request += "Host: " + hostHeader + "\r\n";
            request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            request += "Connection: close\r\n";
            for (std::map<std::string, std::string>::const_iterator it = headers.begin();
                 it != headers.end(); ++it) {
                bool clean = true;
                for (size_t ci = 0; ci < it->first.size() && clean; ++ci) {
                    unsigned char ch = static_cast<unsigned char>(it->first[ci]);
                    if (ch < 0x20 || ch == 0x7F) clean = false;
                }
                for (size_t ci = 0; ci < it->second.size() && clean; ++ci) {
                    unsigned char ch = static_cast<unsigned char>(it->second[ci]);
                    if (ch < 0x20 || ch == 0x7F) clean = false;
                }
                if (!clean) continue;
                request += it->first + ": " + it->second + "\r\n";
            }
            request += "\r\n";
            request += body;

            // Send
            ssize_t totalSent = 0;
            ssize_t requestLen = static_cast<ssize_t>(request.size());
            const char* ptr = request.c_str();
            while (totalSent < requestLen) {
                if (!setSocketTimeout(sockfd, SO_SNDTIMEO)) return false;
                size_t remaining = static_cast<size_t>(requestLen - totalSent);
                ssize_t sent;
                do {
                    sent = send(sockfd, ptr + totalSent, remaining, MSG_NOSIGNAL_COMPAT);
                } while (sent < 0 && errno == EINTR);
                if (sent <= 0) {
                    return false;
                }
                totalSent += sent;
            }

            // Read HTTP status line in bulk (up to 256 B per recv) to reduce
            // syscall overhead.  Accumulate until \r\n for TCP segmentation.
            std::string responseBuf;
            responseBuf.reserve(256);
            {
                char buf[256];
                while (responseBuf.size() < 4096) {
                    if (!setSocketTimeout(sockfd, SO_RCVTIMEO)) break;
                    ssize_t n;
                    do { n = ::recv(sockfd, buf, sizeof(buf) - 1, 0); }
                    while (n < 0 && errno == EINTR);
                    if (n <= 0) break;
                    responseBuf.append(buf, static_cast<size_t>(n));
                    if (responseBuf.find("\r\n") != std::string::npos) break;
                }
            }

            if (responseBuf.empty()) return false;

            // Expected: "HTTP/1.1 200 OK\r\n"
            size_t spacePos = responseBuf.find(' ');
            if (spacePos == std::string::npos) return false;
            char* endPtr = nullptr;
            long statusCode = std::strtol(responseBuf.c_str() + spacePos + 1, &endPtr, 10);
            bool success = (endPtr != responseBuf.c_str() + spacePos + 1 && statusCode >= 200 && statusCode < 300);

            // Drain remaining response body to avoid TCP RST on socket close
            if (remainingMs() > 0) {
                char drain[1024];
                ssize_t n;
                do {
                    if (!setSocketTimeout(sockfd, SO_RCVTIMEO)) break;
                    do { n = ::recv(sockfd, drain, sizeof(drain), 0); }
                    while (n < 0 && errno == EINTR);
                } while (n > 0 && remainingMs() > 0);
            }

            return success;
        }

        // CWE-78 fix: uses fork/execvp instead of popen to avoid shell injection.
        // argv elements go directly to curl with no shell interpretation.
        // Requires C++11 or later (SSO guaranteed, no COW string implementations).
        // argv strings are valid in child process after fork() due to value semantics.
        // NOTE: execvp searches PATH for 'curl'. In environments where an
        // attacker controls PATH entries, this could resolve to a malicious
        // binary (CWE-426). Acceptable for a logging transport; harden by
        // ensuring PATH is trusted in production deployments.
        bool httpPostCurl(const std::string& url, const std::string& body,
                         const std::map<std::string, std::string>& headers,
                         size_t timeoutMs) {
            std::vector<std::string> args;
            args.push_back("curl");
            args.push_back("--silent");
            args.push_back("--fail");
            args.push_back("-o");
            args.push_back("/dev/null");
            args.push_back("-X");
            args.push_back("POST");
            if (!m_opts.verifySsl) {
                args.push_back("--insecure");
            }
            args.push_back("--max-time");
            {
                size_t maxTimeSec = (timeoutMs + 999) / 1000;
                if (maxTimeSec == 0) maxTimeSec = 1;
                args.push_back(std::to_string(maxTimeSec));
            }

            for (std::map<std::string, std::string>::const_iterator it = headers.begin();
                 it != headers.end(); ++it) {
                bool clean = true;
                for (size_t ci = 0; ci < it->first.size() && clean; ++ci) {
                    unsigned char ch = static_cast<unsigned char>(it->first[ci]);
                    if (ch < 0x20 || ch == 0x7F) clean = false;
                }
                for (size_t ci = 0; ci < it->second.size() && clean; ++ci) {
                    unsigned char ch = static_cast<unsigned char>(it->second[ci]);
                    if (ch < 0x20 || ch == 0x7F) clean = false;
                }
                if (!clean) continue;
                args.push_back("-H");
                args.push_back(it->first + ": " + it->second);
            }

            args.push_back("--data-binary");
            args.push_back("@-");
            args.push_back(url);

            std::vector<char*> argv;
            argv.reserve(args.size() + 1);
            for (size_t i = 0; i < args.size(); ++i) {
                argv.push_back(const_cast<char*>(args[i].c_str()));
            }
            argv.push_back(nullptr);

            // Absolute deadline: caps total elapsed time (pipe write + child wait)
            // to prevent 2*timeoutMs blocking.
            struct timespec deadline;
            clock_gettime(CLOCK_MONOTONIC, &deadline);
            deadline.tv_sec += static_cast<time_t>(timeoutMs / 1000);
            deadline.tv_nsec += static_cast<long>((timeoutMs % 1000) * 1000000L);
            if (deadline.tv_nsec >= 1000000000L) {
                deadline.tv_sec += 1;
                deadline.tv_nsec -= 1000000000L;
            }
            auto remainingMs = [&]() -> long {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long ms = (deadline.tv_sec - now.tv_sec) * 1000L
                        + (deadline.tv_nsec - now.tv_nsec) / 1000000L;
                return ms > 0 ? ms : 0;
            };

            int pipefd[2];
#if defined(__linux__) || defined(__FreeBSD__)
            if (pipe2(pipefd, O_CLOEXEC) != 0) return false;
#else
            if (pipe(pipefd) != 0) return false;
            fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
            fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
#endif

            pid_t pid = fork();
            if (pid < 0) {
                close(pipefd[0]);
                close(pipefd[1]);
                return false;
            }

            if (pid == 0) {
                // Child: wire pipe read-end to stdin, discard stdout/stderr
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);

                int devnull = open("/dev/null", O_WRONLY);
                if (devnull >= 0) {
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                    close(devnull);
                }

                execvp("curl", argv.data());
                _exit(127);
            }

            // Parent: write body then wait
            close(pipefd[0]);

            // Block SIGPIPE so broken-pipe returns EPIPE instead of killing the process.
            // If the child exits early (exec failure, connection refused), the pipe read-end
            // closes and write() would otherwise raise SIGPIPE with the default handler.
            struct ScopedSigpipeBlock {
                sigset_t m_old;
                ScopedSigpipeBlock() {
                    sigset_t block;
                    sigemptyset(&block);
                    sigaddset(&block, SIGPIPE);
                    pthread_sigmask(SIG_BLOCK, &block, &m_old);
                }
                ~ScopedSigpipeBlock() {
                    pthread_sigmask(SIG_SETMASK, &m_old, nullptr);
                }
                ScopedSigpipeBlock(const ScopedSigpipeBlock&) = delete;
                ScopedSigpipeBlock& operator=(const ScopedSigpipeBlock&) = delete;
            };

            bool writeOk = true;
            {
                ScopedSigpipeBlock sigGuard;
                const char* data = body.c_str();
                size_t remaining = body.size();

                struct pollfd wfd;
                wfd.fd = pipefd[1];
                wfd.events = POLLOUT;
                while (remaining > 0) {
                    long rem = remainingMs();
                    if (rem <= 0) { writeOk = false; break; }
                    int pr;
                    do { pr = ::poll(&wfd, 1, static_cast<int>(rem > static_cast<long>(INT_MAX) ? INT_MAX : rem)); }
                    while (pr < 0 && errno == EINTR);
                    if (pr <= 0) { writeOk = false; break; }
                    ssize_t w;
                    do { w = ::write(pipefd[1], data, remaining); }
                    while (w < 0 && errno == EINTR);
                    if (w <= 0) { writeOk = false; break; }
                    data += w;
                    remaining -= static_cast<size_t>(w);
                }
                close(pipefd[1]);
            }

            int status = 0;
            // WNOHANG poll loop bounded by deadline
            while (true) {
                pid_t w;
                do { w = waitpid(pid, &status, WNOHANG); }
                while (w < 0 && errno == EINTR);
                if (w > 0) break;           // child exited
                if (w < 0) return false;    // error
                // w == 0: still running
                long rem = remainingMs();
                if (rem <= 0) {
                    kill(pid, SIGTERM);
                    // Wait up to 200ms for graceful exit before SIGKILL
                    for (int gi = 0; gi < 20; ++gi) {
                        pid_t gw;
                        do { gw = waitpid(pid, &status, WNOHANG); }
                        while (gw < 0 && errno == EINTR);
                        if (gw > 0) return false;
                        struct timespec gts = {0, 10000000L}; // 10ms
                        nanosleep(&gts, nullptr);
                    }
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);  // reap — SIGKILL is unconditional
                    return false;
                }
                long sleepMs = std::min(rem, 50L);
                struct timespec ts = { sleepMs / 1000, (sleepMs % 1000) * 1000000L };
                nanosleep(&ts, nullptr);
            }

            if (!writeOk) return false;
            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                return false; // curl not found in PATH
            }
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
#endif // !_WIN32

#ifdef _WIN32
        struct WinHttpHandleGuard {
            HINTERNET h;
            explicit WinHttpHandleGuard(HINTERNET handle) : h(handle) {}
            ~WinHttpHandleGuard() { if (h) WinHttpCloseHandle(h); }
            operator HINTERNET() const { return h; }
            WinHttpHandleGuard(const WinHttpHandleGuard&) = delete;
            WinHttpHandleGuard& operator=(const WinHttpHandleGuard&) = delete;
        };

        static std::wstring utf8ToWide(const std::string& str) {
            if (str.empty()) return std::wstring();
            int needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(),
                             static_cast<int>(str.size()), nullptr, 0);
            if (needed <= 0) return std::wstring(str.begin(), str.end());
            std::wstring result(static_cast<size_t>(needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(),
                static_cast<int>(str.size()), &result[0], needed);
            return result;
        }

        bool httpPostWinHTTP(const std::string& body,
                            const std::map<std::string, std::string>& headers,
                            size_t timeoutMs) {
            std::wstring wHost = utf8ToWide(m_parsedUrl.host);
            std::wstring wPath = utf8ToWide(m_parsedUrl.path);

            WinHttpHandleGuard hSession(WinHttpOpen(L"LunarLog/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0));
            if (!hSession) return false;

            DWORD timeout = static_cast<DWORD>(timeoutMs);
            WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

            WinHttpHandleGuard hConnect(WinHttpConnect(hSession,
                wHost.c_str(),
                static_cast<INTERNET_PORT>(m_parsedUrl.port), 0));
            if (!hConnect) return false;

            DWORD flags = (m_parsedUrl.scheme == "https") ? WINHTTP_FLAG_SECURE : 0;
            WinHttpHandleGuard hRequest(WinHttpOpenRequest(hConnect,
                L"POST", wPath.c_str(),
                NULL, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
            if (!hRequest) return false;

            if (!m_opts.verifySsl) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                               | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE
                               | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                                 &secFlags, sizeof(secFlags));
            }

            for (std::map<std::string, std::string>::const_iterator it = headers.begin();
                 it != headers.end(); ++it) {
                std::string headerLine = it->first + ": " + it->second;
                std::wstring wHeader = utf8ToWide(headerLine);
                WinHttpAddRequestHeaders(hRequest, wHeader.c_str(),
                    static_cast<DWORD>(wHeader.size()),
                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            }

            BOOL sendOk = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                static_cast<LPVOID>(const_cast<char*>(body.c_str())),
                static_cast<DWORD>(body.size()),
                static_cast<DWORD>(body.size()), 0);
            if (!sendOk) return false;

            if (!WinHttpReceiveResponse(hRequest, NULL)) return false;

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

            return (statusCode >= 200 && statusCode < 300);
        }
#endif // _WIN32

        HttpSinkOptions m_opts;
        detail::ParsedUrl m_parsedUrl;
        std::unique_ptr<CompactJsonFormatter> m_formatter;
    };

} // namespace minta

#endif // LUNAR_LOG_HTTP_SINK_HPP
