#include <string>
#include <variant>
#include <map>
#include <any>
#include <utility>
#include <initializer_list>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <exception>
#include <fstream>
#include <sstream>
#define raise throw

class JSONerror:public std::runtime_error{
  public:
  explicit JSONerror(const std::string& mes) : std::runtime_error(mes){}
};

std::string ParseQuotes(std::string text){
  std::string res;
  bool open=false;
  for (int i=0;i<text.size();i++){
    if (text[i]=='"'){
      open=!open;
      continue;
    }
    if (open)
        res+=text[i];
  }
  if (open) raise JSONerror("Quote unclosed");
  return res;
}

enum class Type{
  Int,Float,Bool,String,Obj,Null,Array
};

struct Val{
  Type type;
  std::any data;
  Val()=default;
  template<typename T>
  T get() const{
    return std::any_cast<T>(data);
  }
  template<typename T>
  T get(){
    return std::any_cast<T>(data);
  }
  template <typename T>
  Val(Type t,T d) : type(t){data=d;}
};

typedef std::map<std::string,Val> Object;
typedef std::vector<Val> Array;

Val execline(std::string);

Object ParseMini(std::string text){
  std::string temp;
  Object res;
  std::vector<std::string> tokens;
  int depth=0;
  bool q_open=false;
  for (auto& i:text){
    if (i=='"') q_open=!q_open;
    if (( i=='{' || i=='[') && !q_open)
      depth++;
    else if ((i=='}' || i==']') && !q_open)
      depth--;
    else if (i==',' && depth==0 && !q_open){
      tokens.push_back(temp);
      temp="";
      continue;
    }
    temp+=i;
  }
  if (depth!=0) raise JSONerror("Brace unclosed");

  if (!temp.empty())
      tokens.push_back(temp);
  bool quote_open=false;
  for (auto& i:tokens){
    for (int j=0;j<i.size();j++){
      if (i[j]=='"')
        quote_open=!quote_open;
      if ((i[j]==' ' || i[j]=='\n') && !quote_open){
        i.erase(i.begin()+j);
        j--;
      }
    }
    std::string key,value;
    std::string t;
    int a=-1;
    for (int j=0;j<i.size();j++){
      if (i[j]==':'){
          a=j+1;
          break;
      }
      t+=i[j];
    }
    if (a==-1) raise JSONerror("Invalid syntax (no key)");
    key=ParseQuotes(t);
    for (int j=a;j<i.size();j++){
      value+=i[j];
      }
    Val val=execline(value);
    res.insert({key,val});
  }
  return res;
}

Val execline(std::string code){
  if (code.empty())
   raise JSONerror("Empty value");
  //std::cout<<"EXECING "<<code<<std::endl;
  Val res;
  if (code[0]=='['){
    Array vals;
    code.erase(code.begin());
    code.erase(code.end()-1);
    std::string temp;
    int depth=0;
    bool q_open=false;
    for (auto& i:code){
      temp+=i;
      if (i=='[' || i=='{')
        depth++;
      if (i==']' || i=='}')
        depth--;
      if (i=='"') q_open=!q_open;
      if (i==',' && depth==0 && !q_open){
        temp.pop_back();
        vals.push_back(execline(temp));
        temp="";
      }
    }
    if (!temp.empty())
      vals.push_back(execline(temp));
    res.type=Type::Array;
    res.data=vals;
    return res;
  }
  if (code[0]=='{'){
    std::string correct;
    for (int i=1;i<code.size()-1;i++)
        correct+=code[i];
    res.data=ParseMini(correct);
    res.type=Type::Obj;
    return res;
  }
  if (code[0]=='"'){
    res.data=ParseQuotes(code);
    res.type=Type::String;
    return res;
  }
  if (code=="true" || code=="false"){
    res.data=code=="true" ? true : false;
    res.type=Type::Bool;
    return res;
  }
  if (code=="null"){
    res.data=0;
    res.type=Type::Null;
    return res;
  }
  bool isFloat=false;
  for (auto i:code)
    if (i=='.')
        isFloat=true;
  if (isFloat){
    res.data=std::stof(code);
    res.type=Type::Float;
    return res;
  }
  else{
    res.data=std::stoi(code);
    res.type=Type::Int;
    return res;
  }
}

Object OpenJson(std::string name){
  std::ofstream test(name,std::ios::app);
  test.close();
  std::ifstream file(name);
  if (!file.is_open()) raise JSONerror("Failed at opening .json file");
  size_t size;
  file.seekg(0,std::ios::end);
  size=file.tellg();
  file.seekg(0,std::ios::beg);
  std::string correct=std::string(
    std::istreambuf_iterator<char>(file),
    std::istreambuf_iterator<char>()
  );
  auto pos = correct.find_last_of('}');
  if (pos != std::string::npos)
    correct.erase(pos, 1);
  auto pos2 = correct.find_first_of('{');
  if (pos2 != std::string::npos)
    correct.erase(pos2, 1);
  file.close();
  return ParseMini(correct);
}

std::string JsonToString(Object table);

std::string Unexecline(Val var){
  if (var.type==Type::Null)
      return "null";
  if (var.type==Type::Bool){
    bool flag=std::any_cast<bool>(var.data);
    return flag ? "true":"false";
  }
  if (var.type==Type::String)
      return ('"'+std::any_cast<std::string>(var.data)+'"');
  if (var.type==Type::Float){
    float num=std::any_cast<float>(var.data);
    return std::to_string(num);
  }
  if (var.type==Type::Int){
    int num=std::any_cast<int>(var.data);
    return std::to_string(num);
  }
  if (var.type==Type::Obj){
    return JsonToString(std::any_cast<Object>(var.data));
  }
  if (var.type==Type::Array){
    std::string res;
    res+="[\n";
    Array o=var.get<Array>();
    for (auto i:o){
      res+=Unexecline(i);
      res+=",\n";
    }
    res+="]";
    return res;
  }
  throw JSONerror("Unknown type");
}

std::string ClearBraces(std::string income){
  std::string res=income;
  for (int i=0;i<res.size();i++){
    if (res[i]=='{'){
      res.erase(res.begin()+i);
      break;
    }
    if (res[i]!=' ' && res[i]!='\n')
        break;
  }
  for (int i=res.size()-1;i>0;i--){
    if (res[i]=='}'){
      res.erase(res.begin()+i);
      break;
    }
    if (res[i]!=' ' && res[i]!='\n')
      break;
  }
  return res;
}

std::string JsonToString(Object table){
  std::string res;
  res+="{\n";

  for (const auto& [key,val] : table){
    res+='"';
    res+=key;
    res+='"';
    res+=':';
    res+=Unexecline(val);
    res+=',';
    res+='\n';
  }
  res.erase(res.end()-2);
  res+="}\n";
  return res;
}

void SaveJson(Object table,std::string name){
  std::ofstream file(name);
  file<<JsonToString(table);
  file.close();
}

Object ParseJson(std::string str){
  std::string p=ClearBraces(str);
  return ParseMini(p);
}

Object (*Parse)(std::string)=ParseMini;
