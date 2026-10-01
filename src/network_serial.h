// Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
#ifndef NETWORK_SERIAL_H_
#define NETWORK_SERIAL_H_

#include<cstdint>
#include<string>
#include<vector>
#include<array>

struct NetworkSerialEndpoint
{
    std::string host;
    std::string port;
    bool rfc2217 {};
};

bool isNetworkSerial(const std::string &device);
bool parseNetworkSerial(const std::string &device, NetworkSerialEndpoint *endpoint);

// Incremental Telnet/RFC2217 decoder. Socket I/O belongs to the transport.
struct Rfc2217Client
{
    std::vector<unsigned char> begin();
    bool decode(const unsigned char *data, size_t size,
                std::vector<unsigned char> *payload, std::vector<unsigned char> *reply);
    bool negotiated() const;
    bool suspended() const { return suspended_; }
    bool failed() const { return failed_; }
    std::vector<unsigned char> configure(unsigned char command,
                                         const std::vector<unsigned char> &value);
    bool acknowledged() const { return acknowledged_; }
    static std::vector<unsigned char> escape(const std::vector<unsigned char> &data);

private:
    enum class State { DATA, IAC, OPTION, SUB, SUB_IAC };
    enum class Option { OFF, REQUESTED, ON };
    State state_ = State::DATA;
    unsigned char verb_ {};
    std::array<Option, 256> local_ {};
    std::array<Option, 256> remote_ {};
    std::vector<unsigned char> sub_;
    unsigned char expected_command_ {};
    std::vector<unsigned char> expected_value_;
    bool acknowledged_ {};
    bool failed_ {};
    bool suspended_ {};
    void option(unsigned char value, std::vector<unsigned char> *reply);
    void subnegotiation();
};

#endif
