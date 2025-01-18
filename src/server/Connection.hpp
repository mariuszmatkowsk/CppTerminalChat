#pragma once

#include "../Message.hpp"

#include <asio.hpp>
#include <memory>
#include <format>

class ConnectionsManager;
struct MessageHeader;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(asio::io_context& io_context, asio::ip::tcp::socket socket, ConnectionsManager& connections_manager);
    // TODO: close socket in destructor ???

    void start();
    void stop();

    inline asio::ip::tcp::socket& get_socket() {
        return socket_;
    }

private:
    struct ConnectionInfo {
        std::string address;
        asio::ip::port_type port;
    };
    friend struct std::formatter<ConnectionInfo>;

    void do_read_header();
    void do_read_body(MessageHeader header);

    void broadcast_message(Message msg);

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    ConnectionsManager& connections_manager_;
    asio::steady_timer timer_;
    ConnectionInfo connection_info_;

    std::array<uint8_t, 1024> buffer_;
};

using ConnectionPtr = std::shared_ptr<Connection>;

template <>
struct std::formatter<Connection::ConnectionInfo> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Connection::ConnectionInfo& ci, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}:{}", ci.address, ci.port);
    }
};
