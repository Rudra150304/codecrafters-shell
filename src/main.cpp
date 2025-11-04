#include <iostream>
#include <string>
#include <sstream>


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
        std::cout << rest << ": not found" << std::endl;
      }
    }
    else{
       std::cout << cmd << ": command not found" << std::endl;
    }
  }
}
