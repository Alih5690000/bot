
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "json.hpp"
static std::string urlEncode(const std::string& str)
{
    std::ostringstream encoded;

    for (unsigned char c : str)
    {
        if (
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '_' ||
            c == '.' ||
            c == '~'
        )
        {
            encoded << c;
        }
        else
        {
            encoded << '%'
                    << std::uppercase
                    << std::hex
                    << std::setw(2)
                    << std::setfill('0')
                    << static_cast<int>(c)
                    << std::nouppercase
                    << std::dec;
        }
    }

    return encoded.str();
}

struct InlineKeyboard{
  struct Button{
    std::string text;
    std::string callback_data;
  };
  std::vector<std::vector<Button>> buttons;
  InlineKeyboard(std::vector<std::vector<Button>> Buttons):buttons(Buttons){}
};

class Bot{
  public:
  using Chain=std::vector<std::function<bool(std::string)>>;
  public:
  std::queue<std::function<void()>> tasks;
  std::condition_variable tasks_cv;
  std::mutex tasks_mtx;
  std::vector<std::thread> threads;
  httplib::SSLClient cli{"api.telegram.org"};
  std::string token;
  long long offset=0;
  std::unordered_map<std::string,std::function<void(long long chat_id)>> handlers;
  std::unordered_map<std::string,std::function<void(long long chat_id, long long message_id)>> CallbackHandlers;
  std::unordered_map<long long,Chain> chains;
  Object getUpdates(long long offset){
    auto res=cli.Get("/bot"+token+"/getUpdates?offset="+std::to_string(offset));
    if (!res){
      std::cout << "Error: "
        << httplib::to_string(res.error())
        << std::endl;
    }
    Object obj=ParseJson(res->body);
    return obj;
  }
  Object getUpdates(){
    auto res=cli.Get("/bot"+token+"/getUpdates");
    if (!res){
      std::cout << "Error: "
        << httplib::to_string(res.error())
        << std::endl;
    }
    std::cout<<res->body<<std::endl;
    Object obj=ParseJson(res->body);
    return obj;
  }
  void addTask(std::function<void()> func){
    std::unique_lock<std::mutex> lok(tasks_mtx);
    tasks.push(func);
    tasks_cv.notify_one();
  }
  void Update(){
    static bool wasUnupdated=false;
    Object obj=getUpdates(offset);
    Array arr=obj["result"].get<Array>();
    if (arr.size()<1){ 
      if (!wasUnupdated)
        std::cout<<"No updates "<<"offset "<<offset<<std::endl;
      wasUnupdated=true;
      return;
    }
    if (!offset){
      offset=arr[arr.size()-1].get<Object>()["update_id"].get<long long>()+1;
    }
    offset=arr[arr.size()-1].get<Object>()["update_id"].get<long long>()+1;
    wasUnupdated=false;
    std::cout<<"Updates "<<arr.size()<<" offset "<<offset<<std::endl;

    for (auto& i:arr){
      Object o=i.get<Object>();
      for (auto& [key,val]:o){
        std::cout<<key<<" "<<Unexecline(val)<<std::endl;
      }
      if (o.find("message")!=o.end() && chains.find(o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>())!=chains.end()){
        std::cout<<"Found chain"<<std::endl;
        bool res=chains[o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>()][0](o["message"].get<Object>()["text"].get<std::string>());
        if (res){
          if (!chains[o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>()].empty())
          chains[o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>()].erase(chains[o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>()].begin());
          if (chains[o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>()].empty()){
            std::cout<<"Chain finished"<<std::endl;
            chains.erase(o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>());
          }
        }
        continue;
      }
      if (o.find("message")!=o.end()){
        std::cout<<"Message "<<o["message"].get<Object>()["text"].get<std::string>()<<std::endl;
        long long chat_id=o["message"].get<Object>()["chat"].get<Object>()["id"].get<long long>();
        if (handlers.find(o["message"].get<Object>()["text"].get<std::string>())!=handlers.end()){
          std::cout<<"Found handler"<<std::endl;
          handlers[o["message"].get<Object>()["text"].get<std::string>()](chat_id);
        }
      }
      else if (o.find("callback_query")!=o.end()){
        long long chat_id=o["callback_query"].get<Object>()["message"].get<Object>()["chat"].get<Object>()                       ["id"].get<long long>();
        long long message_id=o["callback_query"].get<Object>()["message"].get<Object>()["message_id"].get<long long>();
        if (CallbackHandlers.find(o["callback_query"].get<Object>()["data"].get<std::string>                                     ())!=CallbackHandlers.end())
          CallbackHandlers[o["callback_query"].get<Object>()["data"].get<std::string>()](chat_id, message_id);
      }
    }
  }
  public:
  Bot(const std::string& token, std::string CertPath){
    this->token=token;
    cli.set_ca_cert_path(CertPath.c_str());
    for (int i=0;i<4;i++){
      threads.push_back(std::thread([&](){
        while(true){
          std::function<void()> f;
          std::unique_lock<std::mutex> lok(tasks_mtx);
          if (tasks.empty()){
            tasks_cv.wait(lok, [&] { return !tasks.empty(); });
            continue;
          }
          f=tasks.front();
          tasks.pop();
          f();
        }
      }));
    }
  }
  void sendMessage(long long chat_id,const std::string& text){
    std::cout<<"Sending message to "<<chat_id<<std::endl;
    auto res=cli.Get("/bot"+token+"/sendMessage?chat_id="+std::to_string(chat_id)+"&text="+urlEncode(text));
    if (!res) throw std::runtime_error("Failed at sending message");
    std::cout<<res->body<<std::endl;
  }
  void addChain(long long chat_id,Chain chain){
    chains.insert({chat_id,chain});
  }
  void removeInlineKeyboard(long long chat_id, long long message_id) {
      std::string markup = "{\"inline_keyboard\":[]}";

      auto res = cli.Get(
          "/bot" + token +
          "/editMessageReplyMarkup"
          "?chat_id=" + std::to_string(chat_id) +
          "&message_id=" + std::to_string(message_id) +
          "&reply_markup=" + urlEncode(markup)
      );

      if (!res)
          throw std::runtime_error(
              "Failed at removing inline keyboard: " +
              httplib::to_string(res.error())
          );

      if (res->status != 200)
          throw std::runtime_error(
              "Telegram error: " + res->body
          );
  }
  void sendMessage(long long chat_id, const std::string& text, const InlineKeyboard& keyboard){
    Object kb;
    kb["inline_keyboard"]=Val(Type::Array,Array());
    Array arr=kb["inline_keyboard"].get<Array>();
    for (auto& i:keyboard.buttons){
      Array Arr;
      for (auto& j:i){
        Object obj;
        obj["text"]=Val(Type::String,j.text);
        obj["callback_data"]=Val(Type::String,j.callback_data);
        Arr.push_back(Val(Type::Obj,obj));
      }
      arr.push_back(Val(Type::Array,Arr));
    }
    kb["inline_keyboard"]=Val(Type::Array,arr);
    auto res=cli.Get("/bot"+token+"/sendMessage?chat_id="+std::to_string(chat_id)+"&text="+urlEncode(text)+"&reply_markup="+urlEncode(JsonToString(kb)));
    if (!res) throw std::runtime_error("Failed at sending message");
    std::cout<<"JS\n"<<JsonToString(kb)<<std::endl;
    std::cout<<"Inline kb\n"<<res->body<<std::endl;
  }
  void _sendVideo(long long chat_id,const std::string& videoPath, const std::string& format){
    std::ifstream file(videoPath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed at opening file");
    std::string content=std::string(std::istreambuf_iterator<char>(file),
       std::istreambuf_iterator<char>());
    httplib::UploadFormDataItems items = {
        httplib::UploadFormData{"chat_id", std::to_string(chat_id), "", ""},
        httplib::UploadFormData{"video", content, videoPath, "video/"+format}
    };
    auto res=cli.Post("/bot"+token+"/sendVideo", items);
    if (!res) {
      throw std::runtime_error(
          "Failed at sending video: " +
          httplib::to_string(res.error())
      );
    }

    if (res->status != 200) {
      throw std::runtime_error(
          "Telegram error: " + res->body
      );
    }
  }
  void _sendPhoto(long long chat_id,const std::string& photoPath, const std::string& format){
    std::ifstream file(photoPath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed at opening file");
    std::string content=std::string(std::istreambuf_iterator<char>(file),
       std::istreambuf_iterator<char>());
    httplib::UploadFormDataItems items = {
        httplib::UploadFormData{"chat_id", std::to_string(chat_id), "", ""},
        httplib::UploadFormData{"photo", content, photoPath, "image/"+format}
    };
    auto res=cli.Post("/bot"+token+"/sendPhoto",items);
    if (!res) {
      throw std::runtime_error(
          "Failed at sending photo: " +
          httplib::to_string(res.error())
      );
    }

    if (res->status != 200) {
      throw std::runtime_error(
          "Telegram error: " + res->body
      );
    }
  }
  void _sendAudio(long long chat_id,const std::string& audioPath, const std::string& format){
    std::ifstream file(audioPath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed at opening file");
    std::string content=std::string(std::istreambuf_iterator<char>(file),
       std::istreambuf_iterator<char>()
    );
    httplib::UploadFormDataItems items = {
        httplib::UploadFormData{"chat_id", std::to_string(chat_id), "", ""},
        httplib::UploadFormData{"audio", content, audioPath, "audio/"+format}
    };
    auto res=cli.Post("/bot"+token+"/sendAudio",items);
    if (!res) {
      throw std::runtime_error(
          "Failed at sending photo: " +
          httplib::to_string(res.error())
      );
    }

    if (res->status != 200) {
      throw std::runtime_error(
          "Telegram error: " + res->body
      );
    }
  }
  void sendPhoto(long long chat_id,const std::string& photoPath, const std::string& format){
    addTask([this,chat_id,photoPath,format](){
      _sendPhoto(chat_id,photoPath,format);
    });
  }
  void sendVideo(long long chat_id,const std::string& videoPath, const std::string& format){
    addTask([this,chat_id,videoPath,format](){
      _sendVideo(chat_id,videoPath,format);
    });
  }
  void sendAudio(long long chat_id,const std::string& audioPath, const std::string& format){
    addTask([this,chat_id,audioPath,format](){
      _sendAudio(chat_id,audioPath,format);
    });
  }
  void addHandler(const std::string& text,std::function<void(long long)> func){
    handlers.insert({text,func});
  }
  void addCallbackHandler(const std::string& data,std::function<void(long long, long long)> func){
    CallbackHandlers.insert({data,func});
  }
  void infinite_poll(){
    while (true){
      Update();
    }
  }
};
