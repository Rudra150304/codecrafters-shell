#include <iostream>
#include <string>


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::string command;
  if(command == "exit 0"){
    return 0;
  }

  while(true){
    std::cout << "$ ";
    std::string command;
    std::cout << command << ": command not found" << std::endl;
  }
}
