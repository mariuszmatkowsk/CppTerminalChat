#include "../Message.hpp"
#include "Connection.hpp"

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/executor_work_guard.hpp>
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
    constexpr const char* Help{"help"};
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
        bool& show_help) {
    if (input_text.starts_with('/')) {
        auto [command, rest] = parse_command(input_text);
        if (command == command::Connect && !connection.is_connected()) {
            const auto& nick = rest;
            chat_users.push_back(nick);
            connection.connect(nick);
        } else if (command == command::Disconnect && connection.is_connected()) {
            connection.disconnect();
            chat_users.clear();
            show_help = false;
        } else if (command == command::Help) {
            show_help = true;
        } else if (command == command::PrivateMsg && connection.is_connected()) {
            auto first_space_index = rest.find_first_of(' '); 
            if (first_space_index == std::string::npos) {
                // TODO: empty message, ???
            }
            auto to = rest.substr(0, first_space_index);
            connection.send(PrivateMessage{
                .from = connection.get_nick(), .to = std::move(to),
                .message =
                    rest.substr(first_space_index + 1, rest.size() - first_space_index)
            });
            show_help = false;
        } else {
            // TODO: not supported command should be ignored ????
        }
    } else {
        // TODO: handle message to all clients connected to chat
        if (connection.is_connected()) {
            show_help = false;
            chat_messages.emplace_back(connection.get_nick(), input_text);
            connection.send(
                TextMessage{
                    .from = connection.get_nick(),
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

    bool show_help{false};

    Connection connection{io_context, std::move(endpoints), received_messages};
    auto work_guard = asio::make_work_guard(io_context);

    asio::steady_timer pool_messages_timer(io_context);

    std::thread t{[&] { io_context.run(); }};

    // ---------------------- ftxui -------------------
    std::string input_text;
    std::vector<ChatMessage> chat_messages;

    auto input_message =
        ftxui::Input(&input_text, "Type a message...") | ftxui::CatchEvent([&](ftxui::Event event) {
            if (event == ftxui::Event::Return && !input_text.empty()) {
                handle_new_input(connection, chat_messages, chat_users, std::move(input_text), show_help);
                input_text.clear();
                return true;
            } else if (event == ftxui::Event::Escape) {
                show_help = false;
                return true;
            }
            return false;
        });
    
    auto chat = ftxui::Renderer([&] {
        if (!connection.is_connected() || show_help) {
            auto info_msg = connection.is_connected() ?
                ftxui::text("You are now connected!!!") | ftxui::color(ftxui::Color::Green) :
                ftxui::text("You are not connected to the server!!!") | ftxui::color(ftxui::Color::Red); 

            return ftxui::window(ftxui::text("Help:") | ftxui::bold | ftxui::center,
                ftxui::vbox(
                    info_msg,
                    ftxui::emptyElement(),
                    ftxui::text("Usage:") | ftxui::bold,
                    ftxui::text("       /connect <nick> - connect to the chat, set your nick"),
                    ftxui::text("       /disconnect     - leave the chat"),
                    ftxui::text("       /private <nick> - send private message to other connected user"),
                    ftxui::text("       /help           - show help")
            ));
        } else {
            ftxui::Elements elements; 
            for (const auto& [message_nick, message] : chat_messages) {
                if (message_nick == connection.get_nick()) {
                    elements.push_back(
                        ftxui::text("You: " + message) | ftxui::border | ftxui::align_right | ftxui::color(ftxui::Color::Green));
                } else {
                    elements.push_back(ftxui::text(std::format("{}: ", message_nick) + message) | ftxui::border);
                }
            }
            return ftxui::window(ftxui::text("Chat messages:") | ftxui::bold | ftxui::center,
                ftxui::vbox(std::move(elements)) 
            );
        }
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
            handle_new_input(
                connection,
                chat_messages,
                chat_users,
                std::move(input_text),
                show_help);
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
            while (connection.is_connected() && !received_messages.empty()) {
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
