#include <iostream>
#include <string>


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::string command;

  while(true){
    std::cout << "$ ";
    std::getline(std::cin, command);
    if(command == "exit 0"){
      return 0;
    }
    std::cout << command << ": command not found" << std::endl;
  }
}
