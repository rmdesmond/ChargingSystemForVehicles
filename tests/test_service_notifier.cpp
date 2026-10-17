#include <gtest/gtest.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include "charger/ServiceNotifier.h"

using charger::ServiceNotifier;
using namespace std::chrono_literals;

namespace {

// Plays the part of systemd: a datagram socket bound to a path in a private
// temporary directory. The test reads back exactly what the notifier sent.
class NotifySocketTest : public ::testing::Test {
protected:
    NotifySocketTest() {
        char directoryTemplate[] = "/tmp/charger-notify-XXXXXX";
        directory_ = ::mkdtemp(directoryTemplate);
        path_ = directory_ + "/notify";

        fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, path_.c_str(), sizeof(address.sun_path) - 1);
        bound_ = ::bind(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;

        // Fail a receive after 2 s instead of hanging the test run.
        const timeval timeout{2, 0};
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    ~NotifySocketTest() override {
        ::close(fd_);
        ::unlink(path_.c_str());
        ::rmdir(directory_.c_str());
    }

    std::string receive() const {
        char buffer[64] = {};
        const ssize_t length = ::recv(fd_, buffer, sizeof(buffer) - 1, 0);
        return length > 0 ? std::string(buffer, static_cast<std::size_t>(length)) : "";
    }

    std::string directory_;
    std::string path_;
    int fd_{-1};
    bool bound_{false};
};

// REQ-008: readiness, liveness and shutdown are reported with the exact
// strings the sd_notify protocol defines.
TEST_F(NotifySocketTest, SendsTheProtocolStrings) {
    ASSERT_TRUE(bound_);
    ServiceNotifier notifier(path_, 10s);

    EXPECT_TRUE(notifier.ready());
    EXPECT_EQ(receive(), "READY=1");
    EXPECT_TRUE(notifier.watchdog());
    EXPECT_EQ(receive(), "WATCHDOG=1");
    EXPECT_TRUE(notifier.stopping());
    EXPECT_EQ(receive(), "STOPPING=1");
}

TEST_F(NotifySocketTest, ReadsSocketAndWatchdogFromTheEnvironment) {
    ASSERT_TRUE(bound_);
    ::setenv("NOTIFY_SOCKET", path_.c_str(), 1);
    ::setenv("WATCHDOG_USEC", "10000000", 1);

    ServiceNotifier notifier = ServiceNotifier::fromEnvironment();

    ::unsetenv("NOTIFY_SOCKET");
    ::unsetenv("WATCHDOG_USEC");
    EXPECT_TRUE(notifier.enabled());
    EXPECT_EQ(notifier.watchdogTimeout(), 10s);
    EXPECT_TRUE(notifier.ready());
    EXPECT_EQ(receive(), "READY=1");
}

// systemd may hand out a socket in Linux's abstract namespace, written with a
// leading '@'. Such a socket has no file; the kernel removes it on close.
TEST(ServiceNotifierTest, SendsToAnAbstractSocket) {
    const std::string name = "charger-notify-test-" + std::to_string(::getpid());
    const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path + 1, name.data(), name.size());  // sun_path[0] stays 0
    const auto length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + name.size());
    ASSERT_EQ(::bind(fd, reinterpret_cast<const sockaddr*>(&address), length), 0);

    ServiceNotifier notifier("@" + name, 0us);
    EXPECT_TRUE(notifier.ready());

    char buffer[16] = {};
    EXPECT_EQ(::recv(fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT), 7);
    EXPECT_STREQ(buffer, "READY=1");
    ::close(fd);
}

// Started from a shell there is no service manager: everything is a no-op.
TEST(ServiceNotifierTest, IsDisabledWithoutASocket) {
    ::unsetenv("NOTIFY_SOCKET");
    ::unsetenv("WATCHDOG_USEC");
    ServiceNotifier notifier = ServiceNotifier::fromEnvironment();

    EXPECT_FALSE(notifier.enabled());
    EXPECT_EQ(notifier.watchdogTimeout(), 0us);
    EXPECT_FALSE(notifier.ready());
    EXPECT_FALSE(notifier.watchdog());
}

// A socket path that nobody listens on must fail cleanly, not crash or block.
TEST(ServiceNotifierTest, ReportsFailureWhenNobodyListens) {
    ServiceNotifier notifier("/tmp/charger-notify-does-not-exist/notify", 0us);
    EXPECT_TRUE(notifier.enabled());
    EXPECT_FALSE(notifier.ready());
}

TEST(ServiceNotifierTest, RejectsAPathTooLongForASocketAddress) {
    ServiceNotifier notifier(std::string(200, 'x'), 0us);
    EXPECT_FALSE(notifier.ready());
}

}  // namespace