#pragma once

#include "Connection.hpp"

#include <algorithm>
#include <unordered_map>
// #include <unordered_set>

class ConnectionsManager {
public:
    using Connections = std::unordered_map<ConnectionPtr, std::string>;

    void start(ConnectionPtr connection) {
        connections_.insert({connection, ""});
        connection->start();
    }

    void stop(ConnectionPtr connection) {
        connection->stop();
        connections_.erase(connection);
    }

    void stop_all() {
        std::ranges::for_each(connections_, [](auto& c) { c.first->stop(); });
        connections_.clear();
    }

    void set_nick(ConnectionPtr connection, std::string nick) {
        if (auto it = connections_.find(connection); it != std::end(connections_)) {
            connections_[connection] = nick;
        }
    }

    std::optional<std::string> unset_nick(ConnectionPtr connection) {
        if (auto it = connections_.find(connection); it != std::end(connections_)) {
            auto nick = std::move(connections_[connection]);
            connections_[connection].clear();
            return nick;
        }
        return std::nullopt;
    }

    Connections& get_connections() {
        return connections_;
    }

    std::optional<const ConnectionPtr> get_connection_by_nick(const std::string& nick) {
        auto it = std::ranges::find_if(connections_, [&nick] (const auto& connection) {
                return connection.second == nick;
                });
        if (it != std::ranges::end(connections_)) {
            return it->first;
        }
        return std::nullopt;
    }

private:
    // std::unordered_set<ConnectionPtr> connections_;
    Connections connections_;
};
