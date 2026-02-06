/*
 Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"decoding_server.h"
#include"decode.h"
#include"drivers.h"
#include"util.h"

#include<errno.h>
#include<fcntl.h>
#include<map>
#include<signal.h>
#include<string.h>
#include<string>
#include<unistd.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

using namespace std;

static volatile sig_atomic_t shutdown_requested_ = 0;

static void shutdownHandler(int signo)
{
    shutdown_requested_ = 1;
}

// Remove newlines and leading whitespace to compact pretty-printed JSON
// into a single line suitable for the JSON Lines protocol.
static string compactJson(const string &json)
{
    string out;
    out.reserve(json.size());
    bool in_string = false;
    bool escape = false;
    for (char c : json)
    {
        if (escape)
        {
            out += c;
            escape = false;
            continue;
        }
        if (c == '\\' && in_string)
        {
            out += c;
            escape = true;
            continue;
        }
        if (c == '"')
        {
            in_string = !in_string;
            out += c;
            continue;
        }
        if (!in_string)
        {
            if (c == '\n' || c == '\r') continue;
            // Collapse runs of whitespace (indentation) to nothing
            // between structural tokens.
            if (c == ' ' || c == '\t') continue;
        }
        out += c;
    }
    return out;
}

struct ClientState
{
    string line_buffer;
    string write_buffer;
    DecoderSession session;
};

static bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

int startDecodingServer(int port)
{
    loadAllBuiltinDrivers();

    // Install signal handlers for graceful shutdown
    struct sigaction sa;
    sa.sa_handler = shutdownHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Ignore SIGPIPE so writing to a disconnected client returns EPIPE
    signal(SIGPIPE, SIG_IGN);

    // Create IPv6 dual-stack socket
    int server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        error("(decodingserver) failed to create socket: %s\n", strerror(errno));
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Allow both IPv4 and IPv6 connections
    int no = 0;
    setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    if (::bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(server_fd);
        error("(decodingserver) failed to bind to port %d: %s\n", port, strerror(errno));
    }

    if (listen(server_fd, 16) < 0)
    {
        close(server_fd);
        error("(decodingserver) failed to listen: %s\n", strerror(errno));
    }

    if (!setNonBlocking(server_fd))
    {
        close(server_fd);
        error("(decodingserver) failed to set non-blocking: %s\n", strerror(errno));
    }

    notice("(decodingserver) listening on port %d\n", port);

    map<int, ClientState> clients;

    while (!shutdown_requested_)
    {
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (auto &kv : clients)
        {
            int fd = kv.first;
            FD_SET(fd, &read_fds);
            if (!kv.second.write_buffer.empty())
            {
                FD_SET(fd, &write_fds);
            }
            if (fd > max_fd) max_fd = fd;
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(max_fd + 1, &read_fds, &write_fds, NULL, &tv);
        if (ready < 0)
        {
            if (errno == EINTR) continue;
            break;
        }
        if (ready == 0) continue;

        // Accept new connections
        if (FD_ISSET(server_fd, &read_fds))
        {
            struct sockaddr_in6 client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0)
            {
                setNonBlocking(client_fd);
                clients[client_fd] = ClientState();
                debug("(decodingserver) client connected fd=%d\n", client_fd);
            }
        }

        // Process existing clients
        vector<int> to_remove;

        for (auto &kv : clients)
        {
            int fd = kv.first;
            ClientState &cs = kv.second;

            // Handle writes first
            if (FD_ISSET(fd, &write_fds) && !cs.write_buffer.empty())
            {
                ssize_t n = write(fd, cs.write_buffer.data(), cs.write_buffer.size());
                if (n > 0)
                {
                    cs.write_buffer.erase(0, n);
                }
                else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    to_remove.push_back(fd);
                    continue;
                }
            }

            // Handle reads
            if (FD_ISSET(fd, &read_fds))
            {
                char buf[4096];
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n <= 0)
                {
                    // Connection closed or error
                    to_remove.push_back(fd);
                    continue;
                }

                for (ssize_t i = 0; i < n; i++)
                {
                    char c = buf[i];
                    if (c == '\n')
                    {
                        if (!cs.line_buffer.empty())
                        {
                            string result = compactJson(decodeLine(cs.session, cs.line_buffer));
                            result += "\n";
                            cs.write_buffer += result;
                            cs.line_buffer.clear();
                        }
                    }
                    else if (c != '\r')
                    {
                        cs.line_buffer += c;
                    }
                }
            }
        }

        for (int fd : to_remove)
        {
            debug("(decodingserver) client disconnected fd=%d\n", fd);
            close(fd);
            clients.erase(fd);
        }
    }

    // Clean shutdown
    for (auto &kv : clients)
    {
        close(kv.first);
    }
    close(server_fd);

    notice("(decodingserver) stopped\n");
    return 0;
}
