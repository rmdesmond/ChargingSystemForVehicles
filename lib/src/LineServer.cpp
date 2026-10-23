#include "charger/LineServer.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

namespace charger {

LineServer::LineServer(std::string path, mode_t mode, bool closeAfterGreeting)
    : path_(std::move(path)), closeAfterGreeting_(closeAfterGreeting) {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (path_.empty() || path_.size() >= sizeof(address.sun_path)) {
        return;
    }
    std::memcpy(address.sun_path, path_.data(), path_.size());

    // Non-blocking: poll() says when there is work, and nothing below may stall
    // the main loop, which also feeds the service manager's watchdog.
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return;
    }
    // A socket file left behind by an earlier run would make bind() fail.
    ::unlink(path_.c_str());
    // bind() takes the generic sockaddr type; this cast is how the API is used.
    const auto* genericAddress = reinterpret_cast<const sockaddr*>(&address);
    if (::bind(fd, genericAddress, sizeof(address)) != 0 || ::chmod(path_.c_str(), mode) != 0 ||
        ::listen(fd, 4) != 0) {
        ::close(fd);
        ::unlink(path_.c_str());
        return;
    }
    listenFd_ = fd;
}

LineServer::~LineServer() {
    for (const Client& client : clients_) {
        ::close(client.fd);
    }
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        ::unlink(path_.c_str());
    }
}

void LineServer::addTo(std::vector<pollfd>& fds) {
    firstIndex_ = fds.size();
    if (!listening()) {
        return;
    }
    fds.push_back(pollfd{listenFd_, POLLIN, 0});
    for (const Client& client : clients_) {
        fds.push_back(pollfd{client.fd, POLLIN, 0});
    }
}

void LineServer::service(const std::vector<pollfd>& fds, const Greeter& greet,
                         const Handler& onLine) {
    if (!listening() || fds.size() <= firstIndex_) {
        return;
    }
    // Clients first, from a copy of the flags: accept() below may grow the
    // client list, and the poll set only describes the clients it was built from.
    const std::size_t clientCount = std::min(clients_.size(), fds.size() - firstIndex_ - 1);
    std::vector<Client*> readable;
    for (std::size_t i = 0; i < clientCount; ++i) {
        if (fds[firstIndex_ + 1 + i].revents != 0) {
            readable.push_back(&clients_[i]);
        }
    }
    std::vector<int> closed;
    for (Client* client : readable) {
        if (!readFrom(*client, onLine)) {
            closed.push_back(client->fd);
        }
    }
    for (const int fd : closed) {
        ::close(fd);
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                      [fd](const Client& c) { return c.fd == fd; }),
                       clients_.end());
    }
    if ((fds[firstIndex_].revents & POLLIN) != 0) {
        accept(greet);
    }
}

void LineServer::accept(const Greeter& greet) {
    // Drain every pending connection; poll() reported the listener once.
    for (;;) {
        const int fd = ::accept4(listenFd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            return;
        }
        const std::string greeting = greet();
        if (!greeting.empty()) {
            sendLine(fd, greeting);
        }
        if (closeAfterGreeting_) {
            ::close(fd);
        } else {
            clients_.push_back(Client{fd, {}});
        }
    }
}

// Reads everything that arrived, answers every complete line. Returns false
// when the client has gone away.
bool LineServer::readFrom(Client& client, const Handler& onLine) {
    for (;;) {
        char buffer[kMaxLineLength];
        const ssize_t received = ::recv(client.fd, buffer, sizeof(buffer), 0);
        if (received == 0) {
            return false;  // closed by the peer
        }
        if (received < 0) {
            // The socket is non-blocking: "nothing more" is normal; anything
            // else means the client is no longer usable.
            return errno == EAGAIN || errno == EWOULDBLOCK;
        }
        client.pending.append(buffer, static_cast<std::size_t>(received));

        std::size_t newline = 0;
        while ((newline = client.pending.find('\n')) != std::string::npos) {
            const std::string line = client.pending.substr(0, newline);
            client.pending.erase(0, newline + 1);
            sendLine(client.fd, onLine(line));
        }
        if (client.pending.size() > kMaxLineLength) {
            client.pending.clear();
            sendLine(client.fd, "ERR line too long");
        }
    }
}

void LineServer::sendLine(int fd, std::string_view text) {
    const std::string line = std::string(text) + '\n';
    // MSG_NOSIGNAL: a peer that vanished must not kill the daemon with SIGPIPE.
    // A short or failed send is the peer's problem; the next read notices.
    ::send(fd, line.data(), line.size(), MSG_NOSIGNAL);
}

}  // namespace charger