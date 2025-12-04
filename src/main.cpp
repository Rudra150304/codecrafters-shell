#include <cctype>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <ostream>
#include <sched.h>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

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

    if(command == "exit"){
      return 0;
    }

    std::vector<std::string> tokens;
    std::string current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;

    for(size_t i = 0; i < command.size(); i++){
      char c = command[i];

      if(!in_single_quotes && !in_double_quotes && c =='\\'){
        if(i + 1 < command.size()){
          current.push_back(command[i + 1]);
          i++;
        }
        continue;
      }

      if(c == '\'' && !in_double_quotes){
        if(in_single_quotes){
          in_single_quotes = false;
        }
        else{
          in_single_quotes = true;
        }
        continue;
      }

      else if(c == '"' && !in_single_quotes){
        if(in_double_quotes){
          in_double_quotes = false;
        }
        else{
          in_double_quotes = true;
        }
        continue;
      }

      if(in_double_quotes && c == '\\'){
        if(i + 1 < command.size()){
          char next = command[i + 1];
          if(next =='"' || next == '\\'){
            current.push_back(next);
            ++i;
            continue;
          }
        }
        current.push_back('\\');
        continue;
      }

      if(in_single_quotes || in_double_quotes){
        current.push_back(c);
      }
      else{
        if(std::isspace(static_cast<unsigned char>(c))){
          if(!current.empty()){
            tokens.push_back(current);
            current.clear();
          }
          continue;
        }
        current.push_back(c);
      }
    }

    if(!current.empty())
      tokens.push_back(current);

    if(tokens.empty())
      continue;
    
    std::string outfile;
    std::string errfile;
    std::string appendfile;
    bool redirect = false;
    bool redirect_err = false;
    bool append_redirect = false;

    for(std::size_t i = 0; i < tokens.size(); ++i){
      if(tokens[i] == ">" || tokens[i] == "1>"){
        if(i + 1 >= tokens.size()){
          redirect = false;
          break;
        }
        outfile = tokens[i + 1];
        redirect = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }

      if(tokens[i] == "2>"){
        if(i + 1 < tokens.size()){
          errfile = tokens[i + 1];
          redirect_err = true;
          tokens.erase(tokens.begin() + i,tokens.begin() + i + 2);
        }
        break;
      }

      if(tokens[i] == ">>" || tokens[i] == "1>>"){
        if(i + 1 < tokens.size()){
          appendfile = tokens[i + 1];
          append_redirect = true;
          tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        }
        break;
      }

      if(tokens[i].rfind("1>", 0) == 0 && tokens[i].size() > 2){
        outfile = tokens[i].substr(2);
        redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }

      if(tokens[i].rfind(">", 0) == 0 && tokens[i].size() > 1){
        outfile = tokens[i].substr(1);
        redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }

      if(tokens[i].rfind("2>", 0) == 0 && tokens[i].size() > 2){
        errfile = tokens[i].substr(2);
        redirect_err = true;
        tokens.erase(tokens.begin() + i);
        break;
      }

      if(tokens[i].rfind("1>>", 0) == 0 && tokens[i].size() > 3){
        appendfile = tokens[i].substr(3);
        append_redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }

      if(tokens[i].rfind(">>", 0) == 0 && tokens[i].size() > 2){
        appendfile = tokens[i].substr(2);
        append_redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }
    }

    int saved_stdout = -1;
    int fd = -1;
    int saved_stderr = -1;
    int errfd = -1;
    int saved_stdout_append = -1;
    int fd_append = -1;
    
    if(redirect){
      fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if(fd < 0){
        perror("open");
        continue;
      }
      saved_stdout = dup(STDOUT_FILENO);
      dup2(fd, STDOUT_FILENO);
    }

    if(redirect_err){
      errfd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if(errfd < 0){
        perror("open");
        continue;
      }
      saved_stderr = dup(STDERR_FILENO);
      dup2(errfd, STDERR_FILENO);
    }

    if(append_redirect){
      fd_append = open(appendfile.c_str(), O_WRONLY | O_CREAT |  O_APPEND, 0666);
      if(fd_append < 0){
        perror("open");
        continue;
      }
      saved_stdout_append = dup(STDOUT_FILENO);
      dup2(fd_append, STDOUT_FILENO);
    }

    std::string cmd = tokens[0];

    if(cmd == "echo"){ 
      for(size_t i = 1; i < tokens.size(); ++i)
        std::cout << tokens[i] << (i + 1 < tokens.size() ? " " : "\n");

      if(redirect){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd);
      }

      if(redirect_err){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        close(errfd);
      }

      if(append_redirect){
        dup2(saved_stdout_append, STDOUT_FILENO);
        close(saved_stdout_append);
        close(fd_append);
      }

      continue;
    }

    else if(cmd == "pwd"){
      std::cout << fs::current_path().string() << std::endl;

      if(redirect){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd);
      }

      if(redirect_err){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        close(errfd);
      }

      if(append_redirect){
        dup2(saved_stdout_append, STDOUT_FILENO);
        close(saved_stdout_append);
        close(fd_append);
      }

      continue;
    }

    else if(cmd == "cd"){
      if(tokens.size() < 2){
        continue;
      }

      const std::string& path = tokens[1];

      if(path == "~"){
        const char* home = std::getenv("HOME");
        if(home){
          chdir(home);
        }
        else{
          std::cout << "cd: HOME not set" << std::endl;
        }
      }

      else if(path.size() > 0 && path[0] == '/'){
        if(chdir(path.c_str()) != 0){
          std::cout << "cd: " << path << ": No such file or directory" << std::endl;
        }
      }

      else{
        fs::path target = fs::current_path() / path;

        fs::path normalized = fs::weakly_canonical(target);

        if(chdir(normalized.c_str()) != 0){
          std::cout << "cd: " << path << ": No such file or directory" << std::endl;
        }
      }

      if(redirect){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd);
      }

      if(redirect_err){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        close(errfd);
      }

      if(append_redirect){
        dup2(saved_stdout_append, STDOUT_FILENO);
        close(saved_stdout_append);
        close(fd_append);
      }

      continue;
    }

    else if(cmd == "type"){
      if(tokens.size() < 2)
        continue;

      std::string rest = tokens[1];

      if(rest == "echo" || rest == "exit" || rest == "type" || rest == "pwd" || rest == "cd")
        std::cout << rest << " is a shell builtin" << std::endl; 
      else{
        std::string pt = find_in_path(rest);
        if(pt.empty())
          std::cout << rest << ": not found" << std::endl;
        else
          std::cout << rest << " is " << pt << std::endl;
      }

      if(redirect){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd);
      }

      if(redirect_err){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        close(errfd);
      }

      if(append_redirect){
        dup2(saved_stdout_append, STDOUT_FILENO);
        close(saved_stdout_append);
        close(fd_append);
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

      if(redirect){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd);
      }

      if(redirect_err){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        close(errfd);
      }

      if(append_redirect){
        int fd = open(appendfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd < 0){
          perror("open");
          exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }

      continue;
    }

    pid_t pid = fork();

    if(pid == 0){
      std::vector<char*> args;
      for(auto& s : tokens)
        args.push_back(const_cast<char*>(s.c_str()));
      args.push_back(nullptr);

      if(redirect){
        int fd2 = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd2 < 0){
          perror("open");
          exit(1);
        }
        dup2(fd2, STDOUT_FILENO);
        close(fd2);
      }

      if(redirect_err){
        int errfd = open(errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(errfd < 0){
          perror("open");
          exit(1);
        }
        dup2(errfd, STDERR_FILENO);
        close(errfd);
      }

      if(append_redirect){
        int fd = open(appendfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd < 0){
          perror("open");
          exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }
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

    if(redirect){
      dup2(saved_stdout, STDOUT_FILENO);
      close(saved_stdout);
      close(fd);
    }
  }
 }
