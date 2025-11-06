#include <cstddef>
#include <cstdio>
#include <iostream>
#include <ostream>
#include <sched.h>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>

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
    std::vector<std::string> tokens;
    std::string token;
    while(iss >> token)
      tokens.push_back(token);
    
    if(tokens.empty())
      continue;

    std::string cmd = tokens[0];

    if(cmd == "echo"){ 
      for(size_t i = 1; i < tokens.size(); ++i)
        std::cout << tokens[i] << (i + 1 < tokens.size() ? " " : "\n");
      continue;
    }
    else if(cmd == "pwd"){
      std::cout << fs::current_path() << std::endl;
    }
    else if(cmd == "type"){
      if(tokens.size() < 2)
        continue;

      std::string rest = tokens[1];

      if(rest == "echo" || rest == "exit" || rest == "type" || rest == "pwd")
        std::cout << rest << " is a shell builtin" << std::endl; 
      else{
        std::string pt = find_in_path(rest);
        if(pt.empty())
          std::cout << rest << ": not found" << std::endl;
        else
          std::cout << rest << " is " << pt << std::endl;
      }
      continue;
    }

    std::string prog_path;
    if(cmd.rfind("./", 0) == 0 || cmd.rfind("/", 0) == 0)
      prog_path = cmd;
    else
     prog_path = find_in_path(cmd);
    
    if(prog_path.empty()){
      std::cout << cmd << ": command not found" << std::endl;
      continue;
    }

    pid_t pid = fork();
    if(pid == 0){
      std::vector<char*> args;
      for(auto& s : tokens)
        args.push_back(s.data());
      args.push_back(nullptr);
      execvp(prog_path.c_str(), args.data());
      perror("execvp");
      exit(1);
    }
    else if(pid > 0){
      waitpid(pid, nullptr, 0);
    }
    else {
      perror("fork");
    }
  }
 }
