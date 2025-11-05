#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

std::string find_in_path(const std::string& cmd){
  char* path_env = std::getenv("PATH");
  if(!path_env)
    return "";

  std::string path_str(path_env);
  std::istringstream path_stream(path_str);
  std::string dir;

  while(std::getline(path_stream, dir, ':')){
    fs::path file_path = fs::path(dir) / cmd;
    if(fs::exists(file_path) && fs::is_regular_file(file_path) && (access(file_path.c_str(), X_OK) == 0)){
      return file_path.string();
    }
  }
  return "";
}


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::string command;

  while(true){
    std::cout << "$ ";
    std::getline(std::cin, command);
    if(command.empty())
      continue;

    if(command == "exit 0"){
      return 0;
    }
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    std::string rest;
    std::getline(iss, rest);
    if(!rest.empty() && rest[0] == ' ')
      rest.erase(0, 1);

    if(cmd == "echo"){ 
      std::cout << rest << std::endl;
    }
    else if(cmd == "type"){
      if(rest == "echo"){
        std::cout << "echo is a shell builtin" << std::endl; 
      }
      else if(rest == "exit"){
        std::cout << "exit is a shell builtin" << std::endl;
      }
      else if(rest == "type"){
        std::cout << "type is a shell builtin" << std::endl;
      }
      else{
        std::string pt = find_in_path(rest);
        if(pt.empty()){
          std::cout << rest << ": not found" << std::endl;
        }
        else{
          std::cout << rest << " is " << pt << std::endl;
        }
      }
    }
    else{
       std::cout << cmd << ": command not found" << std::endl;
    }
  }
}
