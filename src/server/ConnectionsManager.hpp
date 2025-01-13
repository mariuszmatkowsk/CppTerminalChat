#pragma once

#include "Connection.hpp"

#include <algorithm>
#include <unordered_set>

class ConnectionsManager {
public:
    void start(ConnectionPtr connection) {
        connections_.insert(connection);
        connection->start();
    }

    void stop(ConnectionPtr connection) {
        connection->stop();
        connections_.erase(connection);
    }

    void stop_all() {
        std::ranges::for_each(connections_, [](auto& c) { c->stop(); });
        connections_.clear();
    }

private:
    std::unordered_set<ConnectionPtr> connections_;
};
