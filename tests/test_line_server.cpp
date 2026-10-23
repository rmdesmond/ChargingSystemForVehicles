#include <gtest/gtest.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "charger/LineServer.h"

using charger::LineServer;

namespace {

// A blocking client, as nc would be. Each helper fails loudly on timeout.
class Client {
public:
    explicit Client(const std::string& path)
        : fd_(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) {
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
        connected_ =
            ::connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
        const timeval timeout{2, 0};
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }
    ~Client() { ::close(fd_); }
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connected() const { return connected_; }
    void send(const std::string& text) const {
        ::send(fd_, text.data(), text.size(), MSG_NOSIGNAL);
    }
    // One line, without its newline; "" on timeout or when the peer closed.
    std::string readLine() {
        while (pending_.find('\n') == std::string::npos) {
            char buffer[64];
            const ssize_t got = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (got <= 0) {
                return "";
            }
            pending_.append(buffer, static_cast<std::size_t>(got));
        }
        const auto newline = pending_.find('\n');
        const std::string line = pending_.substr(0, newline);
        pending_.erase(0, newline + 1);
        return line;
    }

private:
    int fd_;
    bool connected_{false};
    std::string pending_;
};

class LineServerTest : public ::testing::Test {
protected:
    LineServerTest() {
        char directoryTemplate[] = "/tmp/charger-lines-XXXXXX";
        directory_ = ::mkdtemp(directoryTemplate);
        path_ = directory_ + "/sock";
    }
    ~LineServerTest() override {
        ::unlink(path_.c_str());
        ::rmdir(directory_.c_str());
    }

    // One turn of the daemon's loop: poll with a short timeout, then service.
    void turn(LineServer& server) {
        std::vector<pollfd> fds;
        server.addTo(fds);
        ::poll(fds.data(), fds.size(), 200);
        server.service(
            fds, [this] { return greeting_; },
            [this](std::string_view line) {
                lines_.emplace_back(line);
                return "ECHO " + std::string(line);
            });
    }

    std::string directory_;
    std::string path_;
    std::string greeting_;
    std::vector<std::string> lines_;
};

// REQ-012: a line in, a reply out, in order, for as long as the client stays.
TEST_F(LineServerTest, AnswersEveryLineOnACommandSocket) {
    LineServer server(path_, 0600, false);
    ASSERT_TRUE(server.listening());

    Client client(path_);
    ASSERT_TRUE(client.connected());
    turn(server);  // accept
    EXPECT_EQ(server.clientCount(), 1U);

    client.send("PLUG\nAUTH\n");
    turn(server);
    EXPECT_EQ(client.readLine(), "ECHO PLUG");
    EXPECT_EQ(client.readLine(), "ECHO AUTH");
    ASSERT_EQ(lines_.size(), 2U);
}

TEST_F(LineServerTest, ReassemblesALineThatArrivesInPieces) {
    LineServer server(path_, 0600, false);
    Client client(path_);
    turn(server);

    client.send("CURR");
    turn(server);
    EXPECT_TRUE(lines_.empty());
    client.send("ENT 16\nSTA");
    turn(server);
    ASSERT_EQ(lines_.size(), 1U);
    EXPECT_EQ(lines_[0], "CURRENT 16");
    EXPECT_EQ(client.readLine(), "ECHO CURRENT 16");
}

TEST_F(LineServerTest, RefusesALineLongerThanTheLimit) {
    LineServer server(path_, 0600, false);
    Client client(path_);
    turn(server);

    client.send(std::string(LineServer::kMaxLineLength + 1, 'x'));
    turn(server);
    EXPECT_EQ(client.readLine(), "ERR line too long");
    EXPECT_TRUE(lines_.empty());
}

// REQ-013: the status socket greets and hangs up.
TEST_F(LineServerTest, GreetsAndClosesOnAStatusSocket) {
    LineServer server(path_, 0666, true);
    greeting_ = "STATE Available";

    Client client(path_);
    turn(server);
    EXPECT_EQ(client.readLine(), "STATE Available");
    EXPECT_EQ(client.readLine(), "");  // closed by the server
    EXPECT_EQ(server.clientCount(), 0U);
}

TEST_F(LineServerTest, ServesSeveralClientsAtOnceAndForgetsTheOnesThatLeave) {
    LineServer server(path_, 0600, false);
    Client first(path_);
    Client second(path_);
    turn(server);
    EXPECT_EQ(server.clientCount(), 2U);

    second.send("B\n");
    first.send("A\n");
    turn(server);
    EXPECT_EQ(first.readLine(), "ECHO A");
    EXPECT_EQ(second.readLine(), "ECHO B");

    {
        Client third(path_);
        turn(server);
        EXPECT_EQ(server.clientCount(), 3U);
    }  // third disconnects
    turn(server);
    EXPECT_EQ(server.clientCount(), 2U);
}

TEST_F(LineServerTest, SetsThePermissionBitsAndReplacesAStaleSocketFile) {
    {
        const LineServer stale(path_, 0600, false);
        ASSERT_TRUE(stale.listening());
    }  // destructor removes the file...
    struct stat info {};
    EXPECT_NE(::stat(path_.c_str(), &info), 0);

    // ...but a crash would not, so a fresh server must cope with a leftover.
    ::close(::open(path_.c_str(), O_CREAT | O_WRONLY, 0600));
    const LineServer server(path_, 0640, false);
    ASSERT_TRUE(server.listening());
    ASSERT_EQ(::stat(path_.c_str(), &info), 0);
    EXPECT_TRUE(S_ISSOCK(info.st_mode));
    EXPECT_EQ(info.st_mode & 0777, 0640U);
}

TEST(LineServerStandaloneTest, DoesNotListenOnAnUnusablePath) {
    const LineServer empty("", 0600, false);
    EXPECT_FALSE(empty.listening());
    const LineServer tooLong(std::string(200, 'x'), 0600, false);
    EXPECT_FALSE(tooLong.listening());
    const LineServer noDirectory("/tmp/charger-no-such-directory/sock", 0600, false);
    EXPECT_FALSE(noDirectory.listening());

    // Nothing to poll on: addTo adds nothing and service does nothing.
    std::vector<pollfd> fds;
    LineServer none("", 0600, false);
    none.addTo(fds);
    EXPECT_TRUE(fds.empty());
    none.service(fds, [] { return std::string{}; }, [](std::string_view) { return std::string{}; });
}

}  // namespace
