#pragma once

#include <poll.h>
#include <sys/types.h>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace charger {

// A unix stream socket that speaks lines. The server owns the listening
// socket and the connected clients but no thread: the daemon's main loop
// puts the server's descriptors into its own poll() set with addTo(), and
// after poll() returns, service() accepts new clients, reads what arrived,
// and sends the replies the callbacks produce.
//
// Two callbacks shape the conversation:
//   greet() is called once per new connection and its result sent at once;
//   onLine() is called once per complete line and its result sent back.
// With closeAfterGreeting the connection is closed after the greeting, which
// turns the server into a read-only status port: connect, read one line, done.
class LineServer {
public:
    using Greeter = std::function<std::string()>;
    using Handler = std::function<std::string(std::string_view line)>;

    // Removes a stale socket file at path, binds a new one with the given
    // permission bits, and starts listening. Check listening() afterwards.
    LineServer(std::string path, mode_t mode, bool closeAfterGreeting);
    ~LineServer();
    LineServer(const LineServer&) = delete;
    LineServer& operator=(const LineServer&) = delete;

    bool listening() const { return listenFd_ >= 0; }
    const std::string& path() const { return path_; }
    std::size_t clientCount() const { return clients_.size(); }

    // Appends the listening socket and every client to the poll set, and
    // remembers where they went so service() can find them.
    void addTo(std::vector<pollfd>& fds);

    // Handles whatever poll() reported on the descriptors added by addTo().
    void service(const std::vector<pollfd>& fds, const Greeter& greet, const Handler& onLine);

    // Longest line accepted; a client that sends more without a newline gets
    // an ERR reply and its partial input is dropped.
    static constexpr std::size_t kMaxLineLength = 256;

private:
    struct Client {
        int fd;
        std::string pending;  // bytes received that do not yet end a line
    };

    void accept(const Greeter& greet);
    static bool readFrom(Client& client, const Handler& onLine);
    static void sendLine(int fd, std::string_view text);

    std::string path_;
    bool closeAfterGreeting_;
    int listenFd_{-1};
    std::vector<Client> clients_;
    std::size_t firstIndex_{0};  // position of listenFd_ in the last poll set
};

}  // namespace charger