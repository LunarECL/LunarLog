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

        char buf[4096];
        ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            receivedBody = std::string(buf, static_cast<size_t>(n));
        }

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
}
