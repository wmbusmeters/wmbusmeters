/* Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later) */
#include"network_serial.h"

#include<algorithm>
#include<cctype>

using namespace std;

namespace
{
constexpr unsigned char IAC = 255, DO = 253, DONT = 254, WILL = 251, WONT = 252;
constexpr unsigned char SB = 250, SE = 240, BINARY = 0, SGA = 3, COM = 44;
bool supported(unsigned char option) { return option == BINARY || option == SGA || option == COM; }
}

bool isNetworkSerial(const string &device)
{
    return device.rfind("tcp://", 0) == 0 || device.rfind("rfc2217://", 0) == 0;
}

bool parseNetworkSerial(const string &device, NetworkSerialEndpoint *endpoint)
{
    if (!isNetworkSerial(device)) return false;
    NetworkSerialEndpoint result;
    result.rfc2217 = device.rfind("rfc2217://", 0) == 0;
    string address = device.substr(result.rfc2217 ? 10 : 6);
    size_t colon;
    if (!address.empty() && address[0] == '[')
    {
        size_t end = address.find(']');
        if (end == string::npos || end < 2 || end+1 >= address.size() || address[end+1] != ':') return false;
        result.host = address.substr(1, end-1);
        if (result.host.find(':') == string::npos) return false;
        colon = end+1;
    }
    else
    {
        colon = address.find(':');
        if (colon == string::npos) return false;
        result.host = address.substr(0, colon);
    }
    result.port = address.substr(colon+1);
    if (result.host.empty() || result.port.empty() || result.port.size() > 5) return false;
    for (unsigned char c : result.host)
    {
        if (isspace(c) || c < 33 || c > 126 || string("/@?#[]\\").find(c) != string::npos) return false;
    }
    unsigned int port = 0;
    for (unsigned char c : result.port)
    {
        if (c < '0' || c > '9') return false;
        port = port*10 + c-'0';
    }
    if (port == 0 || port > 65535) return false;
    *endpoint = result;
    return true;
}

vector<unsigned char> Rfc2217Client::begin()
{
    *this = Rfc2217Client();
    vector<unsigned char> result;
    for (unsigned char o : {BINARY, SGA, COM})
    {
        local_[o] = remote_[o] = Option::REQUESTED;
        result.insert(result.end(), {IAC, WILL, o, IAC, DO, o});
    }
    return result;
}

bool Rfc2217Client::negotiated() const
{
    // Older ser2net versions acknowledge the client's options only.
    return !failed_ && local_[BINARY] == Option::ON && local_[COM] == Option::ON;
}

void Rfc2217Client::option(unsigned char value, vector<unsigned char> *reply)
{
    bool local = verb_ == DO || verb_ == DONT;
    bool yes = verb_ == DO || verb_ == WILL;
    Option &state = local ? local_[value] : remote_[value];
    unsigned char accept = local ? WILL : DO;
    unsigned char reject = local ? WONT : DONT;
    if (yes && supported(value))
    {
        if (state == Option::OFF) reply->insert(reply->end(), {IAC, accept, value});
        state = Option::ON;
    }
    else
    {
        if (yes || state == Option::ON) reply->insert(reply->end(), {IAC, reject, value});
        state = Option::OFF;
        if (local && (value == BINARY || value == COM)) failed_ = true;
        if (!local && value == BINARY) failed_ = true;
    }
}

vector<unsigned char> Rfc2217Client::escape(const vector<unsigned char> &data)
{
    vector<unsigned char> result;
    for (auto b : data)
    {
        result.push_back(b);
        if (b == IAC) result.push_back(b);
    }
    return result;
}

vector<unsigned char> Rfc2217Client::configure(unsigned char command, const vector<unsigned char> &value)
{
    expected_command_ = command+100;
    expected_value_ = value;
    acknowledged_ = false;
    vector<unsigned char> result = {IAC, SB, COM, command};
    auto escaped = escape(value);
    result.insert(result.end(), escaped.begin(), escaped.end());
    result.insert(result.end(), {IAC, SE});
    return result;
}

void Rfc2217Client::subnegotiation()
{
    if (sub_.size() < 2 || sub_[0] != COM) return;
    if (sub_[1] == 108) suspended_ = true;
    else if (sub_[1] == 109) suspended_ = false;
    else if (expected_command_ != 0 && sub_[1] == expected_command_)
    {
        vector<unsigned char> value(sub_.begin()+2, sub_.end());
        if (value != expected_value_) failed_ = true;
        else acknowledged_ = true;
    }
}

bool Rfc2217Client::decode(const unsigned char *data, size_t size,
                           vector<unsigned char> *payload, vector<unsigned char> *reply)
{
    for (size_t i = 0; i < size && !failed_; ++i)
    {
        unsigned char b = data[i];
        switch (state_)
        {
        case State::DATA:
            if (b == IAC) state_ = State::IAC;
            else payload->push_back(b);
            break;
        case State::IAC:
            if (b == IAC) { payload->push_back(b); state_ = State::DATA; }
            else if (b == DO || b == DONT || b == WILL || b == WONT) { verb_ = b; state_ = State::OPTION; }
            else if (b == SB) { sub_.clear(); state_ = State::SUB; }
            else state_ = State::DATA; // Ignore single-byte Telnet commands.
            break;
        case State::OPTION:
            option(b, reply);
            state_ = State::DATA;
            break;
        case State::SUB:
            if (b == IAC) state_ = State::SUB_IAC;
            else sub_.push_back(b);
            break;
        case State::SUB_IAC:
            if (b == IAC) { sub_.push_back(b); state_ = State::SUB; }
            else if (b == SE) { subnegotiation(); sub_.clear(); state_ = State::DATA; }
            else failed_ = true;
            break;
        }
        // A malicious or broken peer must not grow the subnegotiation buffer indefinitely.
        if (sub_.size() > 1024) failed_ = true;
    }
    return !failed_;
}
