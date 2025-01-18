#include "ChatServer.hpp"

#include <asio.hpp>
#include <print>

ChatServer::ChatServer(const std::string& address, const std::string& port)
    : io_context_(), acceptor_(io_context_), connections_manager_() {

    asio::ip::tcp::resolver resolver{io_context_};
    asio::ip::tcp::endpoint endpoint{*resolver.resolve(address, port).begin()};

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    do_accept();
}

void ChatServer::start() {
    io_context_.run();
}

void ChatServer::do_accept() {
    auto handle_accept = [this](asio::error_code ec,
                                asio::ip::tcp::socket socket) {
        if (!ec) {
            connections_manager_.start(std::make_shared<Connection>(
                io_context_, std::move(socket), connections_manager_));
        } else {
            std::println("New connection was not accepted");
        }
        do_accept();
    };

    acceptor_.async_accept(handle_accept);
}
