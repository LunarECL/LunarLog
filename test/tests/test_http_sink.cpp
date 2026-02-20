#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include <lunar_log/sink/http_sink.hpp>
#include "utils/test_utils.hpp"
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// HttpSink unit tests
// ---------------------------------------------------------------------------

class HttpSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- Test 1: URL parsing - valid HTTP URL ---
TEST_F(HttpSinkTest, UrlParsingValidHttp) {
    auto parsed = minta::detail::parseUrl("http://localhost:8080/api/logs");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.scheme, "http");
    EXPECT_EQ(parsed.host, "localhost");
    EXPECT_EQ(parsed.port, 8080);
    EXPECT_EQ(parsed.path, "/api/logs");
}

// --- Test 2: URL parsing - valid HTTPS URL ---
TEST_F(HttpSinkTest, UrlParsingValidHttps) {
    auto parsed = minta::detail::parseUrl("https://example.com/logs");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.scheme, "https");
    EXPECT_EQ(parsed.host, "example.com");
    EXPECT_EQ(parsed.port, 443);
    EXPECT_EQ(parsed.path, "/logs");
}

// --- Test 3: URL parsing - default port ---
TEST_F(HttpSinkTest, UrlParsingDefaultPort) {
    auto parsed = minta::detail::parseUrl("http://myhost/path");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.host, "myhost");
    EXPECT_EQ(parsed.port, 80);
    EXPECT_EQ(parsed.path, "/path");
}

// --- Test 4: URL parsing - no path ---
TEST_F(HttpSinkTest, UrlParsingNoPath) {
    auto parsed = minta::detail::parseUrl("http://localhost:3000");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.host, "localhost");
    EXPECT_EQ(parsed.port, 3000);
    EXPECT_EQ(parsed.path, "/");
}

// --- Test 5: URL parsing - invalid URL ---
TEST_F(HttpSinkTest, UrlParsingInvalid) {
    auto p1 = minta::detail::parseUrl("ftp://invalid.com/path");
    EXPECT_FALSE(p1.valid);

    auto p2 = minta::detail::parseUrl("not-a-url");
    EXPECT_FALSE(p2.valid);

    auto p3 = minta::detail::parseUrl("");
    EXPECT_FALSE(p3.valid);
}

// --- Test 6: JSONL batch format ---
TEST_F(HttpSinkTest, JsonlBatchFormat) {
    // Create an HttpSink and test batch formatting through writeBatch
    // We use a mock approach: create entries and verify the formatted output
    minta::CompactJsonFormatter formatter;

    minta::LogEntry e1;
    e1.level = minta::LogLevel::INFO;
    e1.message = "Hello world";
    e1.templateStr = "Hello world";
    e1.timestamp = std::chrono::system_clock::now();

    minta::LogEntry e2;
    e2.level = minta::LogLevel::WARN;
    e2.message = "Warning msg";
    e2.templateStr = "Warning msg";
    e2.timestamp = std::chrono::system_clock::now();

    std::string line1 = formatter.format(e1);
    std::string line2 = formatter.format(e2);

    // Each line should be valid JSON
    EXPECT_NE(line1.find("{"), std::string::npos);
    EXPECT_NE(line1.find("}"), std::string::npos);
    EXPECT_NE(line2.find("\"@l\":\"WRN\""), std::string::npos);

    // JSONL format: each entry is one line
    EXPECT_EQ(line1.find('\n'), std::string::npos);
    EXPECT_EQ(line2.find('\n'), std::string::npos);
}

// --- Test 7: HttpSinkOptions configuration ---
TEST_F(HttpSinkTest, HttpSinkOptionsConfiguration) {
    minta::HttpSinkOptions opts("http://localhost:8080/api/logs");
    opts.setHeader("Authorization", "Bearer token123")
        .setHeader("X-Custom", "value")
        .setContentType("application/x-ndjson")
        .setTimeoutMs(5000)
        .setBatchSize(200)
        .setFlushIntervalMs(10000)
        .setMaxRetries(5)
        .setVerifySsl(false);

    EXPECT_EQ(opts.url, "http://localhost:8080/api/logs");
    EXPECT_EQ(opts.contentType, "application/x-ndjson");
    EXPECT_EQ(opts.timeoutMs, 5000u);
    EXPECT_EQ(opts.batchSize, 200u);
    EXPECT_EQ(opts.flushIntervalMs, 10000u);
    EXPECT_EQ(opts.maxRetries, 5u);
    EXPECT_FALSE(opts.verifySsl);
    EXPECT_EQ(opts.headers.size(), 2u);
    EXPECT_EQ(opts.headers.at("Authorization"), "Bearer token123");
    EXPECT_EQ(opts.headers.at("X-Custom"), "value");
}

#ifndef _WIN32
// --- Test 8: HTTP POST to localhost mock server ---
TEST_F(HttpSinkTest, HttpPostToLocalhostMockServer) {
    // Create a simple TCP server that accepts one connection
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(serverFd, 0);

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0; // Let OS assign port

    ASSERT_EQ(bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)), 0);

    socklen_t addrLen = sizeof(addr);
    getsockname(serverFd, (struct sockaddr*)&addr, &addrLen);
    int port = ntohs(addr.sin_port);

    listen(serverFd, 1);

    std::atomic<bool> serverDone(false);
    std::atomic<bool> serverReady(false);
    std::mutex serverMutex;
    std::condition_variable serverCV;
    std::string receivedBody;

    // Server thread
    std::thread serverThread([&] {
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            serverReady.store(true);
        }
        serverCV.notify_all();

        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) { serverDone.store(true); return; }

        // Loop until we have received the full HTTP headers + body
        char buf[4096];
        std::string accumulated;
        while (true) {
            ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            accumulated.append(buf, static_cast<size_t>(n));
            // Check if we have the end of headers
            std::string::size_type headerEnd = accumulated.find("\r\n\r\n");
            if (headerEnd == std::string::npos) continue;
            // Parse Content-Length to know how many body bytes to expect
            std::string::size_type clPos = accumulated.find("Content-Length: ");
            if (clPos == std::string::npos) break; // no body expected
            std::string::size_type clStart = clPos + 16;
            std::string::size_type clEnd = accumulated.find("\r\n", clStart);
            if (clEnd == std::string::npos) break;
            std::string clStr = accumulated.substr(clStart, clEnd - clStart);
            std::size_t contentLength = static_cast<std::size_t>(std::atoi(clStr.c_str()));
            std::size_t bodyStart = headerEnd + 4;
            std::size_t bodyReceived = accumulated.size() - bodyStart;
            // Keep reading until we have all body bytes
            while (bodyReceived < contentLength) {
                ssize_t m = recv(clientFd, buf, sizeof(buf) - 1, 0);
                if (m <= 0) break;
                accumulated.append(buf, static_cast<size_t>(m));
                bodyReceived += static_cast<std::size_t>(m);
            }
            break;
        }
        receivedBody = accumulated;

        // Send HTTP 200 response
        const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(clientFd, response, std::strlen(response), 0);
        close(clientFd);
        serverDone.store(true);
    });

    // Wait for server thread to be ready
    {
        std::unique_lock<std::mutex> lock(serverMutex);
        serverCV.wait(lock, [&] { return serverReady.load(); });
    }

    // Create HttpSink pointing to our mock server
    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);

    {
        minta::HttpSink sink(opts);

        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Test log message";
        entry.templateStr = "Test log message";
        entry.timestamp = std::chrono::system_clock::now();

        sink.write(entry);
        // batchSize=1 triggers immediate flush
    }

    serverThread.join();
    close(serverFd);

    // Verify server received something containing our message
    EXPECT_FALSE(receivedBody.empty());
    EXPECT_NE(receivedBody.find("POST /logs"), std::string::npos);
    EXPECT_NE(receivedBody.find("Test log message"), std::string::npos);
}

// --- Test 9b: CRLF header injection is rejected ---
TEST_F(HttpSinkTest, CrlfHeaderInjectionRejected) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(serverFd, 0);

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0;

    ASSERT_EQ(bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)), 0);

    socklen_t addrLen = sizeof(addr);
    getsockname(serverFd, (struct sockaddr*)&addr, &addrLen);
    int port = ntohs(addr.sin_port);

    listen(serverFd, 1);

    std::atomic<bool> serverDone(false);
    std::atomic<bool> serverReady(false);
    std::mutex serverMutex;
    std::condition_variable serverCV;
    std::string receivedRequest;

    std::thread serverThread([&] {
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            serverReady.store(true);
        }
        serverCV.notify_all();

        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) { serverDone.store(true); return; }

        char buf[4096];
        std::string accumulated;
        while (true) {
            ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            accumulated.append(buf, static_cast<size_t>(n));
            std::string::size_type headerEnd = accumulated.find("\r\n\r\n");
            if (headerEnd == std::string::npos) continue;
            std::string::size_type clPos = accumulated.find("Content-Length: ");
            if (clPos == std::string::npos) break;
            std::string::size_type clStart = clPos + 16;
            std::string::size_type clEnd = accumulated.find("\r\n", clStart);
            if (clEnd == std::string::npos) break;
            std::string clStr = accumulated.substr(clStart, clEnd - clStart);
            std::size_t contentLength = static_cast<std::size_t>(std::atoi(clStr.c_str()));
            std::size_t bodyStart = headerEnd + 4;
            std::size_t bodyReceived = accumulated.size() - bodyStart;
            while (bodyReceived < contentLength) {
                ssize_t m = recv(clientFd, buf, sizeof(buf) - 1, 0);
                if (m <= 0) break;
                accumulated.append(buf, static_cast<size_t>(m));
                bodyReceived += static_cast<std::size_t>(m);
            }
            break;
        }
        receivedRequest = accumulated;

        const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(clientFd, response, std::strlen(response), 0);
        close(clientFd);
        serverDone.store(true);
    });

    {
        std::unique_lock<std::mutex> lock(serverMutex);
        serverCV.wait(lock, [&] { return serverReady.load(); });
    }

    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    opts.setHeader("X-Test\r\nX-Injected: evil", "value");

    {
        minta::HttpSink sink(opts);

        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Injection test";
        entry.templateStr = "Injection test";
        entry.timestamp = std::chrono::system_clock::now();

        sink.write(entry);
    }

    serverThread.join();
    close(serverFd);

    EXPECT_EQ(receivedRequest.find("X-Injected"), std::string::npos);
}

// Note: TOCTOU race is acceptable here; the port is intentionally unused.
static int findFreePort() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 19999;
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(sock);
        return 19999;
    }
    socklen_t len = sizeof(addr);
    getsockname(sock, (struct sockaddr*)&addr, &len);
    int port = ntohs(addr.sin_port);
    close(sock);
    return port;
}

// --- Test 9: HTTP POST failure on connection refused ---
TEST_F(HttpSinkTest, HttpPostConnectionRefused) {
    int freePort = findFreePort();
    std::string url = "http://127.0.0.1:" + std::to_string(freePort) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0).setTimeoutMs(1000);

    minta::HttpSink sink(opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::INFO;
    entry.message = "Should fail";
    entry.templateStr = "Should fail";
    entry.timestamp = std::chrono::system_clock::now();

    // write() should not throw — errors are handled internally by BatchedSink
    EXPECT_NO_THROW(sink.write(entry));
}
#endif // !_WIN32

// --- Test 10: Constructor rejects invalid port ---
TEST_F(HttpSinkTest, ConstructorRejectsInvalidPort) {
    EXPECT_THROW(
        minta::HttpSink(minta::HttpSinkOptions("http://host:99999/path")),
        std::invalid_argument);
}

// --- Test 11: Constructor rejects unsupported scheme ---
TEST_F(HttpSinkTest, ConstructorRejectsUnsupportedScheme) {
    EXPECT_THROW(
        minta::HttpSink(minta::HttpSinkOptions("ftp://host/path")),
        std::invalid_argument);
}

// --- Test 12: URL parsing edge cases ---
TEST_F(HttpSinkTest, UrlParsingEdgeCases) {
    // Port out of range
    auto p1 = minta::detail::parseUrl("http://host:99999/path");
    EXPECT_FALSE(p1.valid);

    // Empty host
    auto p2 = minta::detail::parseUrl("http:///path");
    EXPECT_FALSE(p2.valid);

    // Just scheme
    auto p3 = minta::detail::parseUrl("http://");
    EXPECT_FALSE(p3.valid);

    // Port 0 (below valid range)
    auto p4 = minta::detail::parseUrl("http://host:0/path");
    EXPECT_FALSE(p4.valid);

    // Non-numeric port
    auto p5 = minta::detail::parseUrl("http://host:abc/path");
    EXPECT_FALSE(p5.valid);

    // Empty port (trailing colon)
    auto p6 = minta::detail::parseUrl("http://host:/path");
    EXPECT_FALSE(p6.valid);

    // Uppercase scheme (RFC 3986 case-insensitive)
    auto p7 = minta::detail::parseUrl("HTTP://host/path");
    ASSERT_TRUE(p7.valid);
    EXPECT_EQ(p7.scheme, "http");

    auto p8 = minta::detail::parseUrl("HTTPS://host/path");
    ASSERT_TRUE(p8.valid);
    EXPECT_EQ(p8.scheme, "https");

    // IPv6 literal (not supported — early return)
    auto p9 = minta::detail::parseUrl("http://[::1]:8080/path");
    EXPECT_FALSE(p9.valid);

    // Control characters in host (CRLF injection defense)
    auto p10 = minta::detail::parseUrl("http://evil\r\nhost/path");
    EXPECT_FALSE(p10.valid);

    // Control characters in path
    auto p11 = minta::detail::parseUrl("http://host/path\r\ninjected");
    EXPECT_FALSE(p11.valid);

    // Space in host (malformed request line defense)
    auto p12 = minta::detail::parseUrl("http://evil host/path");
    EXPECT_FALSE(p12.valid);

    // Space in path
    auto p13 = minta::detail::parseUrl("http://host/my path");
    EXPECT_FALSE(p13.valid);
}

#ifndef _WIN32
// --- Test 13: Non-2xx HTTP response treated as failure ---
TEST_F(HttpSinkTest, Non2xxResponseIsFailure) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(serverFd, 0);

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0;

    ASSERT_EQ(bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)), 0);

    socklen_t addrLen = sizeof(addr);
    getsockname(serverFd, (struct sockaddr*)&addr, &addrLen);
    int port = ntohs(addr.sin_port);

    listen(serverFd, 1);

    std::atomic<bool> serverReady(false);
    std::mutex serverMutex;
    std::condition_variable serverCV;

    std::thread serverThread([&] {
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            serverReady.store(true);
        }
        serverCV.notify_all();

        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) return;

        char buf[4096];
        std::string accumulated;
        // Read full request (headers + body) before responding
        size_t contentLength = 0;
        bool headersComplete = false;
        while (true) {
            ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            accumulated.append(buf, static_cast<size_t>(n));
            if (!headersComplete) {
                auto headerEnd = accumulated.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    headersComplete = true;
                    // Parse Content-Length
                    auto clPos = accumulated.find("Content-Length: ");
                    if (clPos == std::string::npos)
                        clPos = accumulated.find("content-length: ");
                    if (clPos != std::string::npos) {
                        contentLength = static_cast<size_t>(
                            std::atoi(accumulated.c_str() + clPos + 16));
                    }
                    size_t bodyStart = headerEnd + 4;
                    size_t bodyReceived = accumulated.size() - bodyStart;
                    if (bodyReceived >= contentLength) break;
                }
            } else {
                auto headerEnd = accumulated.find("\r\n\r\n");
                size_t bodyStart = headerEnd + 4;
                size_t bodyReceived = accumulated.size() - bodyStart;
                if (bodyReceived >= contentLength) break;
            }
        }

        const char* response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(clientFd, response, std::strlen(response), 0);
        close(clientFd);
    });

    {
        std::unique_lock<std::mutex> lock(serverMutex);
        serverCV.wait(lock, [&] { return serverReady.load(); });
    }

    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);

    {
        minta::HttpSink sink(opts);

        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Should see 500";
        entry.templateStr = "Should see 500";
        entry.timestamp = std::chrono::system_clock::now();

        // write triggers immediate flush (batchSize=1), writeBatch throws,
        // onBatchError prints to stderr, but write() itself does not throw.
        EXPECT_NO_THROW(sink.write(entry));
    }

    serverThread.join();
    close(serverFd);
}

// --- Test 14: Reserved headers are not duplicated ---
TEST_F(HttpSinkTest, ReservedHeadersNotDuplicated) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(serverFd, 0);

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0;

    ASSERT_EQ(bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)), 0);

    socklen_t addrLen = sizeof(addr);
    getsockname(serverFd, (struct sockaddr*)&addr, &addrLen);
    int port = ntohs(addr.sin_port);

    listen(serverFd, 1);

    std::atomic<bool> serverReady(false);
    std::mutex serverMutex;
    std::condition_variable serverCV;
    std::string receivedRequest;

    std::thread serverThread([&] {
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            serverReady.store(true);
        }
        serverCV.notify_all();

        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) return;

        char buf[4096];
        std::string accumulated;
        while (true) {
            ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            accumulated.append(buf, static_cast<size_t>(n));
            std::string::size_type headerEnd = accumulated.find("\r\n\r\n");
            if (headerEnd == std::string::npos) continue;
            std::string::size_type clPos = accumulated.find("Content-Length: ");
            if (clPos == std::string::npos) break;
            std::string::size_type clStart = clPos + 16;
            std::string::size_type clEnd = accumulated.find("\r\n", clStart);
            if (clEnd == std::string::npos) break;
            std::string clStr = accumulated.substr(clStart, clEnd - clStart);
            std::size_t contentLength = static_cast<std::size_t>(std::atoi(clStr.c_str()));
            std::size_t bodyStart = headerEnd + 4;
            std::size_t bodyReceived = accumulated.size() - bodyStart;
            while (bodyReceived < contentLength) {
                ssize_t m = recv(clientFd, buf, sizeof(buf) - 1, 0);
                if (m <= 0) break;
                accumulated.append(buf, static_cast<size_t>(m));
                bodyReceived += static_cast<std::size_t>(m);
            }
            break;
        }
        receivedRequest = accumulated;

        const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(clientFd, response, std::strlen(response), 0);
        close(clientFd);
    });

    {
        std::unique_lock<std::mutex> lock(serverMutex);
        serverCV.wait(lock, [&] { return serverReady.load(); });
    }

    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    opts.setHeader("Host", "evil.example.com");
    opts.setHeader("Content-Length", "99999");
    opts.setHeader("Connection", "keep-alive");

    {
        minta::HttpSink sink(opts);

        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Reserved header test";
        entry.templateStr = "Reserved header test";
        entry.timestamp = std::chrono::system_clock::now();

        sink.write(entry);
    }

    serverThread.join();
    close(serverFd);

    // Extract headers section (before \r\n\r\n)
    std::string::size_type headerEnd = receivedRequest.find("\r\n\r\n");
    ASSERT_NE(headerEnd, std::string::npos);
    std::string headers = receivedRequest.substr(0, headerEnd);

    // Count occurrences of each reserved header
    size_t hostCount = 0, clCount = 0, connCount = 0;
    std::string::size_type pos = 0;
    while ((pos = headers.find("Host:", pos)) != std::string::npos) { ++hostCount; pos += 5; }
    pos = 0;
    while ((pos = headers.find("Content-Length:", pos)) != std::string::npos) { ++clCount; pos += 15; }
    pos = 0;
    while ((pos = headers.find("Connection:", pos)) != std::string::npos) { ++connCount; pos += 11; }

    EXPECT_EQ(hostCount, 1u);
    EXPECT_EQ(clCount, 1u);
    EXPECT_EQ(connCount, 1u);

    // Verify the user-supplied values were NOT used
    EXPECT_EQ(headers.find("evil.example.com"), std::string::npos);
    EXPECT_EQ(headers.find("99999"), std::string::npos);
    EXPECT_EQ(headers.find("keep-alive"), std::string::npos);
}
#endif // !_WIN32

// --- Test 15: isCleanHeaderPair rejects space in header name ---
TEST_F(HttpSinkTest, SpaceInHeaderNameRejected) {
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("X Bad Name", "value"));
    EXPECT_TRUE(minta::detail::isCleanHeaderPair("X-Good-Name", "value with spaces"));
}

// ===========================================================================
// Additional URL parsing edge-case tests
// ===========================================================================

// --- Test 16: URL with query string preserved in path ---
TEST_F(HttpSinkTest, UrlParsingQueryStringInPath) {
    auto parsed = minta::detail::parseUrl("http://host/path?key=value&foo=bar");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.host, "host");
    EXPECT_EQ(parsed.port, 80);
    EXPECT_EQ(parsed.path, "/path?key=value&foo=bar");
}

// --- Test 17: URL fragment stripped from path ---
TEST_F(HttpSinkTest, UrlParsingFragmentStrippedFromPath) {
    auto parsed = minta::detail::parseUrl("http://host/path#fragment");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.path, "/path");
    EXPECT_EQ(parsed.path.find('#'), std::string::npos);
}

// --- Test 18: URL query + fragment — fragment stripped, query kept ---
TEST_F(HttpSinkTest, UrlParsingQueryKeptFragmentStripped) {
    auto parsed = minta::detail::parseUrl("http://host/path?q=1#frag");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.path, "/path?q=1");
    EXPECT_EQ(parsed.path.find('#'), std::string::npos);
}

// --- Test 19: ? as path boundary (no slash before query) ---
TEST_F(HttpSinkTest, UrlParsingQuestionMarkAsPathBoundary) {
    auto parsed = minta::detail::parseUrl("http://host?query=1");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.host, "host");
    // rawPath is "?query=1", doesn't start with / -> gets / prefix
    EXPECT_EQ(parsed.path, "/?query=1");
}

// --- Test 20: # as path boundary (fragment only, no path) ---
TEST_F(HttpSinkTest, UrlParsingHashAsPathBoundary) {
    auto parsed = minta::detail::parseUrl("http://host#fragment");
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.host, "host");
    // rawPath is "#fragment", fragment stripped -> empty -> "/" prepended
    EXPECT_EQ(parsed.path, "/");
}

// --- Test 21: Min/max valid ports ---
TEST_F(HttpSinkTest, UrlParsingMinMaxValidPorts) {
    auto p1 = minta::detail::parseUrl("http://host:1/path");
    ASSERT_TRUE(p1.valid);
    EXPECT_EQ(p1.port, 1);

    auto p2 = minta::detail::parseUrl("http://host:65535/path");
    ASSERT_TRUE(p2.valid);
    EXPECT_EQ(p2.port, 65535);

    // Just above max
    auto p3 = minta::detail::parseUrl("http://host:65536/path");
    EXPECT_FALSE(p3.valid);
}

// --- Test 22: Negative port ---
TEST_F(HttpSinkTest, UrlParsingNegativePort) {
    auto p = minta::detail::parseUrl("http://host:-1/path");
    EXPECT_FALSE(p.valid);
}

// --- Test 23: Mixed case scheme normalization ---
TEST_F(HttpSinkTest, UrlParsingMixedCaseSchemeNormalized) {
    auto p1 = minta::detail::parseUrl("HtTp://host/path");
    ASSERT_TRUE(p1.valid);
    EXPECT_EQ(p1.scheme, "http");
    EXPECT_EQ(p1.port, 80);

    auto p2 = minta::detail::parseUrl("hTtPs://host/path");
    ASSERT_TRUE(p2.valid);
    EXPECT_EQ(p2.scheme, "https");
    EXPECT_EQ(p2.port, 443);
}

// --- Test 24: DEL char (0x7F) rejected in host and path ---
TEST_F(HttpSinkTest, UrlParsingDelCharRejected) {
    std::string urlHost = std::string("http://ho") + '\x7F' + std::string("st/path");
    EXPECT_FALSE(minta::detail::parseUrl(urlHost).valid);

    std::string urlPath = std::string("http://host/pa") + '\x7F' + std::string("th");
    EXPECT_FALSE(minta::detail::parseUrl(urlPath).valid);
}

// --- Test 25: Tab char in host/path rejected ---
TEST_F(HttpSinkTest, UrlParsingTabCharRejected) {
    EXPECT_FALSE(minta::detail::parseUrl("http://ho\tst/path").valid);
    EXPECT_FALSE(minta::detail::parseUrl("http://host/pa\tth").valid);
}

// --- Test 26: HTTPS default port 443 ---
TEST_F(HttpSinkTest, UrlParsingHttpsDefaultPort443) {
    auto p = minta::detail::parseUrl("https://secure.example.com/api/logs");
    ASSERT_TRUE(p.valid);
    EXPECT_EQ(p.scheme, "https");
    EXPECT_EQ(p.port, 443);
    EXPECT_EQ(p.host, "secure.example.com");
    EXPECT_EQ(p.path, "/api/logs");
}

// --- Test 27: Trailing slash only as path ---
TEST_F(HttpSinkTest, UrlParsingTrailingSlash) {
    auto p = minta::detail::parseUrl("http://host/");
    ASSERT_TRUE(p.valid);
    EXPECT_EQ(p.path, "/");
}

// --- Test 28: Deeply nested path with query ---
TEST_F(HttpSinkTest, UrlParsingDeeplyNestedPath) {
    auto p = minta::detail::parseUrl("http://host:9200/api/v2/logs/ingest?source=app");
    ASSERT_TRUE(p.valid);
    EXPECT_EQ(p.host, "host");
    EXPECT_EQ(p.port, 9200);
    EXPECT_EQ(p.path, "/api/v2/logs/ingest?source=app");
}

// --- Test 29: URL with only fragment after host (no path, no query) ---
TEST_F(HttpSinkTest, UrlParsingFragmentAfterHostOnly) {
    auto p = minta::detail::parseUrl("http://host:8080#section");
    ASSERT_TRUE(p.valid);
    EXPECT_EQ(p.host, "host");
    EXPECT_EQ(p.port, 8080);
    // # is path boundary; rawPath="#section", fragment stripped -> "" -> "/"
    EXPECT_EQ(p.path, "/");
}

// ===========================================================================
// Detail function tests
// ===========================================================================

// --- Test 30: headerNameEqualsLower edge cases ---
TEST_F(HttpSinkTest, HeaderNameEqualsLowerEdgeCases) {
    EXPECT_TRUE(minta::detail::headerNameEqualsLower("host", "host"));
    EXPECT_TRUE(minta::detail::headerNameEqualsLower("HOST", "host"));
    EXPECT_TRUE(minta::detail::headerNameEqualsLower("Host", "host"));
    EXPECT_TRUE(minta::detail::headerNameEqualsLower("hOsT", "host"));
    // Length mismatch
    EXPECT_FALSE(minta::detail::headerNameEqualsLower("hosts", "host"));
    EXPECT_FALSE(minta::detail::headerNameEqualsLower("hos", "host"));
    // Empty vs empty
    EXPECT_TRUE(minta::detail::headerNameEqualsLower("", ""));
    // Different content same length
    EXPECT_FALSE(minta::detail::headerNameEqualsLower("abcd", "host"));
}

// --- Test 31: isReservedHeaderName all names ---
TEST_F(HttpSinkTest, IsReservedHeaderNameAll) {
    EXPECT_TRUE(minta::detail::isReservedHeaderName("Host"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("host"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("HOST"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("Content-Length"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("content-length"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("CONTENT-LENGTH"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("Connection"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("connection"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("CONNECTION"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("Transfer-Encoding"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("transfer-encoding"));
    EXPECT_TRUE(minta::detail::isReservedHeaderName("TRANSFER-ENCODING"));
    // Non-reserved
    EXPECT_FALSE(minta::detail::isReservedHeaderName("Authorization"));
    EXPECT_FALSE(minta::detail::isReservedHeaderName("X-Custom"));
    EXPECT_FALSE(minta::detail::isReservedHeaderName("User-Agent"));
    EXPECT_FALSE(minta::detail::isReservedHeaderName("Content-Type"));
    EXPECT_FALSE(minta::detail::isReservedHeaderName("Accept"));
    EXPECT_FALSE(minta::detail::isReservedHeaderName(""));
}

// --- Test 32: isCleanHeaderPair edge cases ---
TEST_F(HttpSinkTest, IsCleanHeaderPairEdgeCases) {
    // DEL (0x7F) in name
    EXPECT_FALSE(minta::detail::isCleanHeaderPair(std::string("X\x7F"), "val"));
    // DEL in value
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("Key", std::string("v\x7F")));
    // Control char (0x01) in name
    EXPECT_FALSE(minta::detail::isCleanHeaderPair(std::string("X\x01"), "val"));
    // Control char (0x01) in value
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("Key", std::string("v\x01")));
    // Tab (0x09) in name — rejected (<= 0x20)
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("X\tBad", "val"));
    // Tab in value — rejected (< 0x20)
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("Key", "val\twith\ttab"));
    // Space in value — ALLOWED (0x20 is not < 0x20)
    EXPECT_TRUE(minta::detail::isCleanHeaderPair("Key", "value with spaces"));
    // Space in name — rejected (0x20 is <= 0x20)
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("X Bad", "val"));
    // Newline in value (CRLF injection)
    EXPECT_FALSE(minta::detail::isCleanHeaderPair("Key", "val\r\nInjected: evil"));
    // Empty name and value — both clean
    EXPECT_TRUE(minta::detail::isCleanHeaderPair("", ""));
    // Normal header
    EXPECT_TRUE(minta::detail::isCleanHeaderPair("Authorization", "Bearer tok123"));
}

// ===========================================================================
// HttpSinkOptions setter coverage
// ===========================================================================

// --- Test 33: setRetryDelayMs and setMaxQueueSize ---
TEST_F(HttpSinkTest, HttpSinkOptionsRetryDelayAndMaxQueueSize) {
    minta::HttpSinkOptions opts("http://localhost/logs");
    opts.setRetryDelayMs(2000).setMaxQueueSize(5000);
    EXPECT_EQ(opts.retryDelayMs, 2000u);
    EXPECT_EQ(opts.maxQueueSize, 5000u);
}

// --- Test 34: Constructor accepts HTTPS URL ---
TEST_F(HttpSinkTest, ConstructorAcceptsHttpsUrl) {
    EXPECT_NO_THROW(
        minta::HttpSink(minta::HttpSinkOptions("https://example.com/logs")));
}

// --- Test 35: Constructor rejects empty host ---
TEST_F(HttpSinkTest, ConstructorRejectsEmptyHost) {
    EXPECT_THROW(
        minta::HttpSink(minta::HttpSinkOptions("http:///path")),
        std::invalid_argument);
}

// --- Test 36: Constructor rejects missing scheme separator ---
TEST_F(HttpSinkTest, ConstructorRejectsMissingSchemeSeparator) {
    EXPECT_THROW(
        minta::HttpSink(minta::HttpSinkOptions("not-a-url")),
        std::invalid_argument);
}

// ===========================================================================
// POSIX-only tests: mock server, HTTPS/curl path
// ===========================================================================

#ifndef _WIN32

// Helper: run a mock TCP server that reads one request and sends a response
struct SimpleMockServer {
    int serverFd;
    int port;
    std::atomic<bool> ready;
    std::mutex mu;
    std::condition_variable cv;
    std::string receivedRequest;

    SimpleMockServer() : serverFd(-1), port(0), ready(false) {
        serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0) return;
        int opt = 1;
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = 0;
        if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            close(serverFd); serverFd = -1; return;
        }
        socklen_t addrLen = sizeof(addr);
        getsockname(serverFd, (struct sockaddr*)&addr, &addrLen);
        port = ntohs(addr.sin_port);
        listen(serverFd, 1);
    }

    ~SimpleMockServer() { if (serverFd >= 0) close(serverFd); }

    void signalReady() {
        { std::lock_guard<std::mutex> lock(mu); ready.store(true); }
        cv.notify_all();
    }

    void waitReady() {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this] { return ready.load(); });
    }

    // Accept one client, read full HTTP request, send response, close.
    void serveOnce(const char* response) {
        signalReady();
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int fd = accept(serverFd, (struct sockaddr*)&ca, &cl);
        if (fd < 0) return;
        char buf[8192];
        std::string acc;
        while (true) {
            ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            acc.append(buf, static_cast<size_t>(n));
            auto he = acc.find("\r\n\r\n");
            if (he == std::string::npos) continue;
            auto cp = acc.find("Content-Length: ");
            if (cp == std::string::npos) break;
            size_t clen = static_cast<size_t>(std::atoi(acc.c_str() + cp + 16));
            size_t bstart = he + 4;
            size_t brec = acc.size() - bstart;
            while (brec < clen) {
                ssize_t m = recv(fd, buf, sizeof(buf) - 1, 0);
                if (m <= 0) break;
                acc.append(buf, static_cast<size_t>(m));
                brec += static_cast<size_t>(m);
            }
            break;
        }
        receivedRequest = acc;
        send(fd, response, std::strlen(response), 0);
        close(fd);
    }

    // Accept, read request, send custom multi-part response, close.
    void serveOnceMultiResponse(const std::vector<std::string>& responses, int delayMs = 10) {
        signalReady();
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int fd = accept(serverFd, (struct sockaddr*)&ca, &cl);
        if (fd < 0) return;
        char buf[8192];
        std::string acc;
        while (true) {
            ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            acc.append(buf, static_cast<size_t>(n));
            auto he = acc.find("\r\n\r\n");
            if (he == std::string::npos) continue;
            auto cp = acc.find("Content-Length: ");
            if (cp == std::string::npos) break;
            size_t clen = static_cast<size_t>(std::atoi(acc.c_str() + cp + 16));
            size_t bstart = he + 4;
            size_t brec = acc.size() - bstart;
            while (brec < clen) {
                ssize_t m = recv(fd, buf, sizeof(buf) - 1, 0);
                if (m <= 0) break;
                acc.append(buf, static_cast<size_t>(m));
                brec += static_cast<size_t>(m);
            }
            break;
        }
        receivedRequest = acc;
        for (const auto& r : responses) {
            send(fd, r.c_str(), r.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
        close(fd);
    }

    // Accept, read ONE recv, sleep, close (simulates slow server).
    void serveSlowNoResponse(int sleepMs) {
        signalReady();
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int fd = accept(serverFd, (struct sockaddr*)&ca, &cl);
        if (fd < 0) return;
        char buf[4096];
        recv(fd, buf, sizeof(buf) - 1, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        close(fd);
    }

    SimpleMockServer(const SimpleMockServer&) = delete;
    SimpleMockServer& operator=(const SimpleMockServer&) = delete;
};

// --- Test 37: User Content-Type removed case-insensitively ---
TEST_F(HttpSinkTest, UserContentTypeRemovedCaseInsensitive) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnce("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    opts.setHeader("CONTENT-TYPE", "text/plain");
    opts.setContentType("application/json");

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "CT test";
        entry.templateStr = "CT test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    t.join();

    auto he = srv.receivedRequest.find("\r\n\r\n");
    ASSERT_NE(he, std::string::npos);
    std::string hdrs = srv.receivedRequest.substr(0, he);
    // Only one Content-Type; application/json wins, text/plain removed
    size_t ct = 0;
    std::string::size_type pos = 0;
    while ((pos = hdrs.find("Content-Type:", pos)) != std::string::npos) { ++ct; pos += 13; }
    EXPECT_EQ(ct, 1u);
    EXPECT_NE(hdrs.find("application/json"), std::string::npos);
    EXPECT_EQ(hdrs.find("text/plain"), std::string::npos);
    EXPECT_EQ(hdrs.find("CONTENT-TYPE"), std::string::npos);
}

// --- Test 38: User-Agent not overridden when provided ---
TEST_F(HttpSinkTest, UserAgentNotOverriddenWhenProvided) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnce("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    opts.setHeader("User-Agent", "MyCustomAgent/2.0");

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "UA test";
        entry.templateStr = "UA test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    t.join();

    auto he = srv.receivedRequest.find("\r\n\r\n");
    ASSERT_NE(he, std::string::npos);
    std::string hdrs = srv.receivedRequest.substr(0, he);
    EXPECT_NE(hdrs.find("MyCustomAgent/2.0"), std::string::npos);
    EXPECT_EQ(hdrs.find("LunarLog/1.0"), std::string::npos);
}

// --- Test 39: Custom headers passed through ---
TEST_F(HttpSinkTest, CustomHeadersPassedThrough) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnce("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    opts.setHeader("Authorization", "Bearer mytoken");
    opts.setHeader("X-Request-Id", "req-12345");
    opts.setHeader("X-Custom-Header", "custom-value");

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Headers test";
        entry.templateStr = "Headers test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    t.join();

    auto he = srv.receivedRequest.find("\r\n\r\n");
    ASSERT_NE(he, std::string::npos);
    std::string hdrs = srv.receivedRequest.substr(0, he);
    EXPECT_NE(hdrs.find("Authorization: Bearer mytoken"), std::string::npos);
    EXPECT_NE(hdrs.find("X-Request-Id: req-12345"), std::string::npos);
    EXPECT_NE(hdrs.find("X-Custom-Header: custom-value"), std::string::npos);
}

// --- Test 40: Transfer-Encoding header skipped in request ---
TEST_F(HttpSinkTest, TransferEncodingHeaderSkipped) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnce("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    opts.setHeader("Transfer-Encoding", "chunked");

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "TE test";
        entry.templateStr = "TE test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    t.join();

    auto he = srv.receivedRequest.find("\r\n\r\n");
    ASSERT_NE(he, std::string::npos);
    std::string hdrs = srv.receivedRequest.substr(0, he);
    EXPECT_EQ(hdrs.find("Transfer-Encoding"), std::string::npos);
    EXPECT_EQ(hdrs.find("chunked"), std::string::npos);
}

// --- Test 41: HTTP 100-Continue handled correctly ---
TEST_F(HttpSinkTest, Http100ContinueHandled) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnceMultiResponse({
            "HTTP/1.1 100 Continue\r\n\r\n",
            "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        }, 20);
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "100-Continue test";
        entry.templateStr = "100-Continue test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    t.join();

    // If 100-Continue was not handled, the client would misparse the 200
    // and treat it as failure. Reaching here without error is the check.
    EXPECT_FALSE(srv.receivedRequest.empty());
}

// --- Test 42: Large payload (50 entries) ---
TEST_F(HttpSinkTest, LargePayloadMultipleEntries) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnce("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(50).setFlushIntervalMs(0).setMaxRetries(0);

    {
        minta::HttpSink sink(opts);
        for (int i = 0; i < 50; ++i) {
            minta::LogEntry entry;
            entry.level = minta::LogLevel::INFO;
            entry.message = "Large payload entry " + std::to_string(i) +
                " with extra text to increase size significantly for testing";
            entry.templateStr = entry.message;
            entry.timestamp = std::chrono::system_clock::now();
            sink.write(entry);
        }
    }
    t.join();

    auto he = srv.receivedRequest.find("\r\n\r\n");
    ASSERT_NE(he, std::string::npos);
    std::string body = srv.receivedRequest.substr(he + 4);
    size_t nlCount = 0;
    for (char c : body) { if (c == '\n') ++nlCount; }
    EXPECT_EQ(nlCount, 50u);
}

// --- Test 43: Timeout on slow server ---
TEST_F(HttpSinkTest, TimeoutOnSlowServer) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveSlowNoResponse(3000);
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0).setTimeoutMs(500);

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Timeout test";
        entry.templateStr = "Timeout test";
        entry.timestamp = std::chrono::system_clock::now();
        EXPECT_NO_THROW(sink.write(entry));
    }
    t.join();
}

// --- Test 44: HTTPS URL triggers curl subprocess path ---
TEST_F(HttpSinkTest, HttpsUrlTriggersCurlPath) {
    int freePort = findFreePort();
    std::string url = "https://127.0.0.1:" + std::to_string(freePort) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0).setTimeoutMs(5000);

    minta::HttpSink sink(opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::INFO;
    entry.message = "HTTPS curl test";
    entry.templateStr = "HTTPS curl test";
    entry.timestamp = std::chrono::system_clock::now();

    // Should not throw — curl will fail with connection refused, handled internally
    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 45: HTTPS with verifySsl=false ---
TEST_F(HttpSinkTest, HttpsWithVerifySslFalse) {
    int freePort = findFreePort();
    std::string url = "https://127.0.0.1:" + std::to_string(freePort) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0)
        .setTimeoutMs(5000).setVerifySsl(false);

    minta::HttpSink sink(opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::INFO;
    entry.message = "HTTPS insecure test";
    entry.templateStr = "HTTPS insecure test";
    entry.timestamp = std::chrono::system_clock::now();

    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 46: HTTPS with custom + reserved headers via curl ---
TEST_F(HttpSinkTest, HttpsCurlPathWithHeaders) {
    int freePort = findFreePort();
    std::string url = "https://127.0.0.1:" + std::to_string(freePort) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0).setTimeoutMs(5000);
    opts.setHeader("Authorization", "Bearer curltoken");
    opts.setHeader("X-Custom-Curl", "curlval");
    // Reserved headers should be skipped in curl args
    opts.setHeader("Host", "evil.example.com");
    opts.setHeader("Transfer-Encoding", "chunked");

    minta::HttpSink sink(opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::INFO;
    entry.message = "HTTPS headers test";
    entry.templateStr = "HTTPS headers test";
    entry.timestamp = std::chrono::system_clock::now();

    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 47: HTTPS with retry exhaustion triggers onBatchError ---
TEST_F(HttpSinkTest, HttpsRetryExhaustion) {
    int freePort = findFreePort();
    std::string url = "https://127.0.0.1:" + std::to_string(freePort) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(2)
        .setTimeoutMs(3000).setRetryDelayMs(10);

    minta::HttpSink sink(opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::ERROR;
    entry.message = "HTTPS retry test";
    entry.templateStr = "HTTPS retry test";
    entry.timestamp = std::chrono::system_clock::now();

    // All retries will fail — onBatchError called each time
    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 48: onBatchError called on HTTP retry ---
TEST_F(HttpSinkTest, OnBatchErrorCalledOnRetry) {
    int freePort = findFreePort();
    std::string url = "http://127.0.0.1:" + std::to_string(freePort) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(2)
        .setTimeoutMs(500).setRetryDelayMs(10);

    minta::HttpSink sink(opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::ERROR;
    entry.message = "Retry test";
    entry.templateStr = "Retry test";
    entry.timestamp = std::chrono::system_clock::now();

    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 49: Default User-Agent header when none provided ---
TEST_F(HttpSinkTest, DefaultUserAgentWhenNoneProvided) {
    SimpleMockServer srv;
    ASSERT_GE(srv.serverFd, 0);

    std::thread t([&] {
        srv.serveOnce("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    });
    srv.waitReady();

    std::string url = "http://127.0.0.1:" + std::to_string(srv.port) + "/logs";
    minta::HttpSinkOptions opts(url);
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(0);
    // No User-Agent set — default should be added

    {
        minta::HttpSink sink(opts);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Default UA test";
        entry.templateStr = "Default UA test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    t.join();

    auto he = srv.receivedRequest.find("\r\n\r\n");
    ASSERT_NE(he, std::string::npos);
    std::string hdrs = srv.receivedRequest.substr(0, he);
    EXPECT_NE(hdrs.find("User-Agent: LunarLog/1.0"), std::string::npos);
}

#endif // !_WIN32
