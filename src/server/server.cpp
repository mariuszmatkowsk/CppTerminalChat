#include <print>

#include "ChatServer.hpp"

int main(int argc, char** argv) {
    ChatServer server{"127.0.0.1", "9999"};
    server.start();
    return 0;
}
