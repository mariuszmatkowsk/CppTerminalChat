#include "../Message.hpp"
#include "Connection.hpp"

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <print>
#include <variant>
#include <tuple>

using namespace std::chrono_literals;

namespace command {
    constexpr const char* Connect{"connect"};
    constexpr const char* Disconnect{"disconnect"};
    constexpr const char* PrivateMsg{"private"};
} // namespace command

struct ChatMessage {
    std::string nick;
    std::string message;
};

std::tuple<std::string, std::string> parse_command(const std::string& input) {
    std::string command;
       
    auto first_space_index = input.find_first_of(' ');
    if (first_space_index == std::string::npos) {
        return {input.substr(1, input.length() - 1), ""};
    }
    command = input.substr(1, first_space_index - 1);

    return {command, input.substr(first_space_index + 1, input.size() - first_space_index)};
}

void handle_new_input(
        Connection& connection,
        std::vector<ChatMessage>& chat_messages,
        std::vector<std::string>& chat_users,
        std::string input_text,
        bool& is_connected,
        std::string& nick) {
    if (input_text.starts_with('/')) {
        auto [command, rest] = parse_command(input_text);
        if (command == command::Connect && !is_connected) {
            nick = rest;
            is_connected = true;
            chat_users.push_back(nick);
            connection.send(ConnectMessage{.nick = std::move(rest)});
        } else if (command == command::Disconnect && is_connected) {
            connection.send(DisconnectMessage{.nick = std::move(nick)});
            chat_users.clear();
            nick.clear();;
            is_connected = false;
        } else if (command == command::PrivateMsg && is_connected) {
            auto first_space_index = rest.find_first_of(' '); 
            if (first_space_index == std::string::npos) {
                // TODO: empty message, ???
            }
            auto to = rest.substr(0, first_space_index);
            connection.send(PrivateMessage{
                .from = nick, .to = std::move(to),
                .message =
                    rest.substr(first_space_index + 1, rest.size() - first_space_index)
            });
        } else {
            // TODO: not supported command should be ignored ????
        }
    } else {
        // TODO: handle message to all clients connected to chat
        if (is_connected) {
            chat_messages.emplace_back(nick, input_text);
            connection.send(
                TextMessage{
                    .from = nick,
                    .message = std::move(input_text)
            });
        }            
    }
}

int main(int argc, char** argv) {
    asio::io_context io_context{};
    asio::ip::tcp::resolver resolver{io_context};
    asio::ip::tcp::resolver::results_type endpoints{
        resolver.resolve("127.0.0.1", "9999")};

    std::queue<Message> received_messages;
    std::vector<std::string> chat_users;

    Connection connection{io_context, std::move(endpoints), received_messages};
    bool is_connected{false};
    std::string nick{};

    asio::steady_timer pool_messages_timer(io_context);

    std::thread t{[&] { io_context.run(); }};

    // ---------------------- ftxui -------------------
    std::string input_text;
    std::vector<ChatMessage> chat_messages;

    auto input_message =
        ftxui::Input(&input_text, "Type a message...") | ftxui::CatchEvent([&](ftxui::Event event) {
            if (event == ftxui::Event::Return && !input_text.empty()) {
                handle_new_input(connection, chat_messages, chat_users, std::move(input_text), is_connected, nick);
                input_text.clear();
                return true;
            }
            return false;
        });
    
    auto chat = ftxui::Renderer([&] {
        ftxui::Elements elements; 
        for (const auto& [message_nick, message] : chat_messages) {
            if (message_nick == nick) {
                elements.push_back(
                    ftxui::text("You: " + message) | ftxui::border | ftxui::align_right | ftxui::color(ftxui::Color::Green));
            } else {
                elements.push_back(ftxui::text(std::format("{}: ", message_nick) + message) | ftxui::border);
            }
        }
        return ftxui::window(ftxui::text("Chat messages:") | ftxui::bold | ftxui::center,
            ftxui::vbox(std::move(elements)) 
        );
    });

    auto users = ftxui::Renderer([&] {
        ftxui::Elements elements;
        std::ranges::transform(chat_users, std::back_inserter(elements), [](const auto& user) {
            return ftxui::text(user) | ftxui::color(ftxui::Color::SeaGreen2); });

        return ftxui::window(ftxui::text("Chat users:") | ftxui::bold | ftxui::center,
            ftxui::vbox(std::move(elements))
        );
    });

    auto send_button = ftxui::Button("Send", [&] {
        if (!input_text.empty()) {
            handle_new_input(connection, chat_messages, chat_users, std::move(input_text), is_connected, nick);
            input_text.clear();
        }
    });

    auto input_container = ftxui::Container::Horizontal({
        input_message,
        send_button,
    });

    auto input_renderer = ftxui::Renderer(input_container, [&] {
        return ftxui::hbox({
            input_message->Render() | ftxui::flex,
            send_button->Render(),
        });
    });

    int users_panel_size{30};
    int input_panel_size{3};

    auto container = ftxui::ResizableSplitLeft(users, chat, &users_panel_size);
    container = ftxui::ResizableSplitBottom(input_renderer, container, &input_panel_size);

    auto main_container = ftxui::Container::Vertical({
        input_message,
        send_button,
        container,
    });

    auto renderer = ftxui::Renderer(main_container, [&] { return container->Render(); });

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    std::function<void(asio::error_code)> handle_pool_messages = [&] (asio::error_code ec) {
        if (!ec) {
            while (is_connected && !received_messages.empty()) {
                Message message = std::move(received_messages.front());
                received_messages.pop();

                auto visitor = [&] <typename MsgType> (const MsgType& msg) {
                    if constexpr (std::is_same_v<MsgType, ConnectMessage>) {
                        chat_users.push_back(msg.nick); 
                    } else if constexpr (std::is_same_v<MsgType, TextMessage>) {
                        chat_messages.push_back({.nick = msg.from, .message = msg.message});
                    } else if constexpr (std::is_same_v<MsgType, PrivateMessage>) {
                        chat_messages.push_back({.nick = msg.from, .message = msg.message});
                    } else if constexpr (std::is_same_v<MsgType, DisconnectMessage>) {
                        const auto it = std::ranges::find(chat_users, msg.nick);
                        if (it != std::ranges::end(chat_users)) {
                            chat_users.erase(it);
                        }
                    }  else {
                        chat_messages.push_back({.nick = "Not supported", .message = "Not supported"});
                    }
                };

                std::visit(visitor, message);
            }
            screen.PostEvent(ftxui::Event::Custom);
            pool_messages_timer.expires_after(std::chrono::milliseconds(200ms));
            pool_messages_timer.async_wait(handle_pool_messages);
        }
    };
    
    pool_messages_timer.expires_after(std::chrono::milliseconds(200ms));
    pool_messages_timer.async_wait(handle_pool_messages);

    screen.Loop(renderer);
    // ------------------------------------------------

    t.join();

    return 0;
}
