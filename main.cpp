#include <iostream>
#include <chrono>
#include "main.hpp"

int main() {
    std::cout<<"token "<<std::getenv("TOKEN")<<std::endl;
    Bot bot(std::getenv("TOKEN"), "cacert.pem");
    bot.addHandler("/start", [&](long long chat_id) {
        bot.sendMessage(chat_id, "Hello! Welcome to the bot.");
    });
    bot.addHandler("gimme image mf", [&](long long chat_id) {
        bot.sendPhoto(chat_id, "asdasdasdasd.jfif", "jfif");
    });
    bot.addCallbackHandler("callback_1", [&](long long chat_id, long long message_id) {
        bot.sendMessage(chat_id, "You clicked Button 1!");
        bot.removeInlineKeyboard(chat_id, message_id);
    });
    bot.addCallbackHandler("callback_2", [&](long long chat_id, long long message_id) {
        bot.sendMessage(chat_id, "You clicked Button 2!");
        bot.removeInlineKeyboard(chat_id, message_id);
    });
    int num1,num2;
    bot.addCallbackHandler("callback_3", [&](long long chat_id, long long message_id) {
        Bot::Chain chain={
            [&num1, &bot, chat_id](std::string input) {
                try {
                    num1 = std::stoi(input);
                    bot.sendMessage(chat_id, "Please enter the second number:");
                    return true;
                } catch (...) {
                    return false;
                }
            },
            [&num2, &num1, &bot, chat_id](std::string input) {
                try {
                    num2 = std::stoi(input);
                    bot.sendMessage(chat_id, "The sum is: " 
                        + std::to_string(num1 + num2));
                    return true;
                } catch (...) {
                    return false;
                }
            }
        };
        bot.addChain(chat_id, chain);
        bot.sendMessage(chat_id, "Please enter the first number:");
    });
    InlineKeyboard::Button button1{"Button 1", "callback_1"};
    InlineKeyboard::Button button2{"Button 2", "callback_2"};
    InlineKeyboard::Button button3{"Button 3", "callback_3"};
    InlineKeyboard keyboard{{{{button1}, {button2}}, {{button3}}}};
    bot.addHandler("/keyboard", [&](long long chat_id) {
        bot.sendMessage(chat_id, "Here is your inline keyboard:", keyboard);
    });
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bot.Update();
    }
    return 0;
}