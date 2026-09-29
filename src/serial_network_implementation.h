// Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
#ifndef SERIAL_NETWORK_IMPLEMENTATION_H_
#define SERIAL_NETWORK_IMPLEMENTATION_H_

// Included after SerialDeviceImp in serial.cc, like the other serial implementations.
// Disconnects use the existing hot-plug lifecycle to recreate and initialize the
// receiver driver, discarding all partial frames from the previous connection.
struct SerialDeviceNetwork : public SerialDeviceImp
{
    SerialDeviceNetwork(string device, int baud_rate, PARITY parity,
                        SerialCommunicationManagerImp *manager, string purpose)
        : SerialDeviceImp(manager, purpose), device_(device), baud_rate_(baud_rate), parity_(parity) {}
    ~SerialDeviceNetwork() { close(); }
    bool open(bool fail_if_not_ok);
    void close();
    bool send(vector<uchar> &data);
    int receive(vector<uchar> *data);
    string device() { return device_; }
    bool opened() { return !opening_ && SerialDeviceImp::opened(); }
    int fd() { return opening_ ? -2 : fd_; }
    bool hasBufferedData() { return buffered_; }
    bool checkIfDataIsPending()
    {
        WITH(network_mutex_, network_lock, network_pending);
        return !pending_.empty() || SerialDeviceImp::checkIfDataIsPending();
    }

private:
    using Clock = chrono::steady_clock;
    using Deadline = Clock::time_point;
    RecursiveMutex network_mutex_ = { "network_mutex" };
    string device_;
    int baud_rate_;
    PARITY parity_;
    bool rfc2217_ {};
    atomic<bool> opening_ {false};
    atomic<bool> buffered_ {false};
    Rfc2217Client protocol_;
    vector<uchar> pending_;
    bool wait(short events, Deadline deadline);
    bool writeWire(const vector<uchar> &data, Deadline deadline);
    bool readWire(vector<uchar> *data, Deadline deadline, bool block);
    bool configure(uchar command, const vector<uchar> &value, Deadline deadline);
};

bool SerialDeviceNetwork::wait(short events, Deadline deadline)
{
    while (fd_ >= 0)
    {
        auto remaining = chrono::duration_cast<chrono::milliseconds>(deadline-Clock::now()).count();
        if (remaining <= 0) return false;
        pollfd p = {fd_, events, 0};
        int rc = poll(&p, 1, int(remaining));
        if (rc < 0 && errno == EINTR) continue;
        return rc > 0 && (p.revents & (events | POLLERR | POLLHUP)) && !(p.revents & POLLNVAL);
    }
    return false;
}

bool SerialDeviceNetwork::writeWire(const vector<uchar> &data, Deadline deadline)
{
    size_t offset = 0;
    while (offset < data.size())
    {
        if (Clock::now() >= deadline || fd_ < 0) return false;
#ifdef MSG_NOSIGNAL
        int flags = MSG_NOSIGNAL;
#else
        int flags = 0; // SO_NOSIGPIPE is set on platforms without MSG_NOSIGNAL.
#endif
        ssize_t n = ::send(fd_, data.data()+offset, data.size()-offset, flags);
        if (n > 0) offset += n;
        else if (n < 0 && errno == EINTR) continue;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            if (!wait(POLLOUT, deadline)) return false;
        }
        else return false;
    }
    return true;
}

bool SerialDeviceNetwork::readWire(vector<uchar> *data, Deadline deadline, bool block)
{
    if (block && !wait(POLLIN, deadline)) return false;
    uchar buffer[4096];
    ssize_t n;
    do { n = recv(fd_, buffer, sizeof(buffer), 0); } while (n < 0 && errno == EINTR && Clock::now() < deadline);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return !block;
    if (n <= 0) return false;
    if (!rfc2217_) data->insert(data->end(), buffer, buffer+n);
    else
    {
        vector<uchar> reply;
        if (!protocol_.decode(buffer, n, data, &reply) || !writeWire(reply, deadline)) return false;
    }
    return data->size() <= 65536;
}

bool SerialDeviceNetwork::configure(uchar command, const vector<uchar> &value, Deadline deadline)
{
    if (!writeWire(protocol_.configure(command, value), deadline)) return false;
    while (!protocol_.acknowledged())
    {
        if (!readWire(&pending_, deadline, true))
        {
            verbose("(serialnet) RFC2217 command %u was not acknowledged on %s\n",
                    unsigned(command), device_.c_str());
            return false;
        }
    }
    return true;
}

bool SerialDeviceNetwork::open(bool fail_if_not_ok)
{
    WITH(network_mutex_, network_lock, network_open);
    if (fd_ >= 0) return true;
    // Failed resolution must be a closed device, not an unopened device that
    // lookup() will keep treating as configured forever.
    fd_ = -1;
    opening_ = true;
    struct OpeningGuard
    {
        atomic<bool> &opening;
        ~OpeningGuard() { opening = false; }
    } opening_guard {opening_};
    NetworkSerialEndpoint endpoint;
    if (!parseNetworkSerial(device_, &endpoint)) return false;
    rfc2217_ = endpoint.rfc2217;
    pending_.clear();
    buffered_ = false;
    addrinfo hints {}, *addresses = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    int rc = getaddrinfo(endpoint.host.c_str(), endpoint.port.c_str(), &hints, &addresses);
    if (rc != 0)
    {
        verbose("(serialnet) cannot resolve %s: %s\n", device_.c_str(), gai_strerror(rc));
        return false;
    }
    fd_ = -1;
    Deadline deadline = Clock::now()+chrono::seconds(5);
    for (auto a = addresses; a && Clock::now() < deadline; a = a->ai_next)
    {
        int socket_fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (socket_fd < 0) continue;
        // select() must never receive an out-of-range descriptor.
        if (socket_fd >= FD_SETSIZE ||
            fcntl(socket_fd, F_SETFL, O_NONBLOCK) < 0 ||
            fcntl(socket_fd, F_SETFD, FD_CLOEXEC) < 0)
        {
            ::close(socket_fd);
            continue;
        }
        int yes = 1;
        setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
        int idle = 30, interval = 10, count = 3;
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#elif defined(TCP_KEEPALIVE)
        int idle = 30;
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
#endif
#ifdef SO_NOSIGPIPE
        setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
        fd_ = socket_fd;
        rc = connect(fd_, a->ai_addr, a->ai_addrlen);
        if (rc == 0) break;
        if (errno == EINPROGRESS && wait(POLLOUT, deadline))
        {
            int err = 0;
            socklen_t length = sizeof(err);
            if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &length) == 0 && err == 0) break;
        }
        ::close(fd_);
        fd_ = -1;
    }
    freeaddrinfo(addresses);
    bool ok = fd_ >= 0;
    if (ok && rfc2217_)
    {
        deadline = Clock::now()+chrono::seconds(5);
        ok = writeWire(protocol_.begin(), deadline);
        while (ok && !protocol_.negotiated()) ok = readWire(&pending_, deadline, true);
        uint32_t baud = baud_rate_;
        uchar parity = parity_ == PARITY::EVEN ? 3 : parity_ == PARITY::ODD ? 2 : 1;
        // RFC2217 SET-CONTROL 1 applies to both directions. Servers such as
        // pySerial legitimately ignore separate inbound flow-control commands.
        ok = ok && baud > 0 &&
            configure(1, {uchar(baud >> 24), uchar(baud >> 16), uchar(baud >> 8), uchar(baud)}, deadline) &&
            configure(2, {8}, deadline) && configure(3, {parity}, deadline) &&
            configure(4, {1}, deadline) && configure(5, {1}, deadline);
    }
    if (!ok)
    {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
        pending_.clear();
        if (fail_if_not_ok) error(EXIT_SERIAL_ERROR, "Could not open network serial device %s\n", device_.c_str());
        verbose("(serialnet) could not connect/configure %s\n", device_.c_str());
        return false;
    }
    // Discard pre-initialization payload (including optional server banners).
    pending_.clear();
    buffered_ = false;
    manager_->tickleEventLoop();
    verbose("(serialnet) opened %s fd %d (%s)\n", device_.c_str(), fd_, purpose_.c_str());
    return true;
}

void SerialDeviceNetwork::close()
{
    WITH(network_mutex_, network_lock, network_close);
    if (fd_ < 0) return;
    ::close(fd_);
    fd_ = -1;
    pending_.clear();
    buffered_ = false;
    if (on_disappear_ && !resetting_)
    {
        auto callback = on_disappear_;
        on_disappear_ = NULL;
        callback();
    }
    manager_->tickleEventLoop();
    verbose("(serialnet) closed %s (%s)\n", device_.c_str(), purpose_.c_str());
}

bool SerialDeviceNetwork::send(vector<uchar> &data)
{
    WITH(network_mutex_, network_lock, network_send);
    Deadline deadline = Clock::now()+chrono::seconds(5);
    if (fd_ < 0) return false;
    while (rfc2217_ && protocol_.suspended())
    {
        if (!readWire(&pending_, deadline, true)) { close(); return false; }
    }
    auto wire = rfc2217_ ? Rfc2217Client::escape(data) : data;
    if (!writeWire(wire, deadline)) { close(); return false; }
    buffered_ = !pending_.empty();
    manager_->tickleEventLoop();
    return true;
}

int SerialDeviceNetwork::receive(vector<uchar> *data)
{
    WITH(network_mutex_, network_lock, network_receive);
    data->clear();
    data->swap(pending_);
    buffered_ = false;
    if (fd_ < 0) return data->size();
    Deadline deadline = Clock::now()+chrono::seconds(5);
    // Bound callback work even when a peer continuously streams data.
    for (int i = 0; i < 16; ++i)
    {
        pollfd p = {fd_, POLLIN, 0};
        if (poll(&p, 1, 0) <= 0) break;
        if (!readWire(data, deadline, false)) { close(); break; }
    }
    return data->size();
}

#endif
