#include <iostream>
#include <string>
#include <unordered_map>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::cout << "$ ";
  std::unordered_map<std::string, bool> commands;

  std::string command;
  std::getline(std::cin, command);
  if(!commands.count(command)){
    std::cout << command << ": command not found" << std::endl;;
  }
}
