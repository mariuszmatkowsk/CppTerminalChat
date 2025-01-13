#pragma once

#include <asio.hpp>
#include <memory>
#include <format>
#include <ostream>

class ConnectionsManager;
struct MessageHeader;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(asio::ip::tcp::socket socket, ConnectionsManager& connections_manager);
    // TODO: check if closing socket in destructor should be added

    void start();
    void stop();

private:
    struct ConnectionInfo {
        std::string address;
        asio::ip::port_type port;
    };
    friend struct std::formatter<ConnectionInfo>;

    void do_read();
    void do_read_body(const MessageHeader& header);
    void do_write();

    asio::ip::tcp::socket socket_;
    ConnectionsManager& connections_manager_;
    ConnectionInfo connection_info_;

    std::array<uint8_t, 1024> buffer_;
};

using ConnectionPtr = std::shared_ptr<Connection>;

template <>
struct std::formatter<Connection::ConnectionInfo> {
    constexpr auto parse(std::format_parse_context& ctx) {
        // Parse the format string (if needed)
        return ctx.begin();
    }

    auto format(const Connection::ConnectionInfo& ci, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}:{}", ci.address, ci.port);
    }
};
