#include "Connection.hpp"
#include "ConnectionsManager.hpp"

#include <asio.hpp>
#include <print>

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionsManager& connections_manager)
    : socket_(std::move(socket)), connections_manager_(connections_manager),
      connection_info_(ConnectionInfo{
          .address = socket_.remote_endpoint().address().to_string(),
          .port = socket_.remote_endpoint().port()}) {
    std::println("New connection: {}", connection_info_);
}

void Connection::start() {
    do_read();
}

void Connection::stop() {
    socket_.close();
}

void Connection::do_read() {
    auto self{shared_from_this()};
    socket_.async_read_some(asio::buffer(buffer_), [this, self](
                                                       asio::error_code ec,
                                                       std::size_t bytes_read) {
        if (!ec) {
                std::println("Received {} bytes from: {}", bytes_read,
                             connection_info_);
                do_read();
        } else if (ec == asio::error::eof) {
            std::println("Connection closed by client: {}", connection_info_);

        } else if (ec != asio::error::operation_aborted) {
            connections_manager_.stop(shared_from_this());
        }
    });
}

void Connection::do_write() {
}
