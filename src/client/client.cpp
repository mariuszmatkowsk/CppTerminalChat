#include "../Message.hpp"
#include "Connection.hpp"

#include <asio.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <print>

using namespace std::chrono_literals;

struct ChatMessage {
    std::string nick;
    std::string message;
};

int main(int argc, char** argv) {
    std::println("Hello from client");

    asio::io_context io_context{};
    asio::ip::tcp::resolver resolver{io_context};
    asio::ip::tcp::resolver::results_type endpoints{
        resolver.resolve("127.0.0.1", "9999")};

    std::queue<Message> received_messages;

    Connection connection{io_context, std::move(endpoints), received_messages};

    std::thread t{[&] { io_context.run(); }};


    // ---------------------- ftxui -------------------
    std::string message_to_be_send;
    std::vector<ChatMessage> chat_messages;
    std::vector<std::string> chat_users;

    auto input_message =
        ftxui::Input(&message_to_be_send, "Type a message...") | ftxui::CatchEvent([&](ftxui::Event event) {
            if (event == ftxui::Event::Return && !message_to_be_send.empty()) {
                chat_messages.emplace_back("Self", message_to_be_send);
                connection.send(TextMessage{.from = "Mariusz", .message = std::move(message_to_be_send)});
                message_to_be_send.clear();

                return true;
            }
            return false;
        });
    
    auto chat = ftxui::Renderer([&] {
        ftxui::Elements elements; 
        for (const auto& [nick, message] : chat_messages) {
            if (nick == "Self") {
                elements.push_back(
                    ftxui::text("You: " + message) | ftxui::border | ftxui::align_right | ftxui::color(ftxui::Color::Green));
            } else {
                elements.push_back(ftxui::text(message) | ftxui::border);
            }
        }
        return ftxui::vbox(std::move(elements)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::border;
    });

    auto users = ftxui::Renderer([&] {
        ftxui::Elements elements;
        for (const auto& user : chat_users) {
            elements.push_back(ftxui::text(user) | ftxui::border);
        }
        return ftxui::vbox(std::move(elements)) | ftxui::border;
    });

    auto send_button = ftxui::Button("Send", [&] {
        if (!message_to_be_send.empty()) {
            chat_messages.emplace_back("Self", message_to_be_send);

            connection.send(TextMessage{.from = "Mariusz", .message = std::move(message_to_be_send)});
            message_to_be_send.clear();
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

    screen.Loop(renderer);
    // ------------------------------------------------
    
    t.join();

    return 0;
}
