@echo off
g++ -g main.cpp -o bot.exe -std=c++17 -O2 -DCPPHTTPLIB_OPENSSL_SUPPORT -I"C:\Program Files\OpenSSL-Win64\include" -I"." -L"C:\openssl-build" -lssl -lcrypto -lws2_32 -lcrypt32
pause