#include <algorithm>
#include <cctype>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <ios>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <strings.h>
#include <sys/types.h>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

namespace fs = std::filesystem;

//Find executable in PATH
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

//Tokenizer supporting:
// - backslash escaping outside quotes
// - single quotes: literal
// - double quotes: literal for now but suppoert \" and \\
// - concatenation of adjacent quotes pieces
// - preserves whitespace inside quotes, collapses outside
std::vector<std::string> tokenize(const std::string& command){
  std::vector<std::string> tokens;
  std::string current;
  bool in_single = false;
  bool in_double = false;

  for(size_t i = 0; i < command.size(); ++i){
    char c = command[i];

    //Outside quotes: backslash escapes next character
    if(!in_single && !in_double && c == '\\'){
      if(i + 1 < command.size()){
        current.push_back(command[i + 1]);
        ++i;
      }
      continue;
    }

    //Toggle single quotes (ignored inside double quotes)
    if(c == '\'' && !in_double){
      in_single = !in_single;
      continue;
    }

    //Toggle double quotes (ignored inside single quotes)
    if(c == '"' && !in_single){
      in_double = !in_double;
      continue;
    }

    //Inside double quotes: special handling for \" and \\.        
    if(in_double && c == '\\'){
      if(i + 1 < command.size()){
        char next = command[i + 1];
        if(next == '"' || next == '\\'){
          current.push_back(next);
          ++i;
          continue;
        }
      }
      //if not \" or \\, keep the backslash as literal
      current.push_back('\\');
      continue;
    }

    //if inside any quotes, take characters literally
    if(in_single || in_double){
      current.push_back(c);
      continue;
    }

    //Outside quotes: whitespace is a delimeter (collapsed)
    if(std::isspace(static_cast<unsigned char>(c))){
      if(!current.empty()){
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    //Normal character outside quotes
    current.push_back(c);
  }
  if(!current.empty())
    tokens.push_back(current);
  return tokens;
}

char* builtin_generator(const char* text, int state){
  static int idx;
  static const char* builtins[] = {"echo", "exit", nullptr};

  if(state == 0)
    idx = 0;

  while(builtins[idx]){
    const char* cmd = builtins[idx];
    idx++;
    if(strncmp(cmd, text, strlen(text)) == 0)
      return strdup(cmd);
  }
  return nullptr;
}

char* executable_generator(const char* text, int state){
  static std::vector<std::string> executables;
  static size_t index;

  if(state == 0){
    executables.clear();
    index = 0;

    const char* path_env = std::getenv("PATH");
    if(!path_env)
      return nullptr;

    std::istringstream ss(path_env);
    std::string dir;

    while(std::getline(ss, dir, ':')){
      fs::path p(dir);
      if(!fs::exists(p) || !fs::is_directory(p))
        continue;

      for(const auto& entry : fs::directory_iterator(p)){
        if(!entry.is_regular_file())
          continue;

        //must be executable
        if(access(entry.path().c_str(), X_OK) != 0)
          continue;

        std::string name = entry.path().filename().string();

        if(name.rfind(text, 0) == 0) //Starts with "text"
          executables.push_back(name);
      }
    }
  }

  if(index < executables.size())
    return strdup(executables[index++].c_str());

  return nullptr;
}

char** completion_callback(const char* text, int start, int end){
  (void)end; //Silence unused variable warning
  
  //We only autocomplete the first token (command name)
  if(start != 0)
    return nullptr;
  
  //Combine builtin and executable genrators
  //rl_completion_matches handles merging automatically
  return rl_completion_matches(text, [](const char* t, int state) -> char* {
    static int phase = 0;

    //Phase 0 -> builtins
    if(phase == 0){
      char* res = builtin_generator(t, state);
      if(res)
        return res;
      phase = 1;
      state = 0; // rest for executables
    }
    //Phase 1 -> executables
    return executable_generator(t, state);
  });
}

void run_builtin(const std::vector<std::string>& tokens){
  const std::string& cmd = tokens[0];

  if(cmd == "echo"){
    for(size_t i = 1; i < tokens.size(); ++i){
      std::cout << tokens[i];
      if(i + 1 < tokens.size())
        std::cout << " ";
    }
    std::cout << "\n";
  }

  else if(cmd == "pwd")
    std::cout << fs::current_path().string() << "\n";

  else if(cmd == "history"){
    //history -r <file>
    if(tokens.size() == 3 && tokens[1] == "-r"){
      if(read_history(tokens[2].c_str()) != 0)
        perror("history");
      return;
    }

    //Normal history
    HIST_ENTRY **list = history_list();
    if(!list)
      return;

    int total = history_length;

    int n = total;
    if(tokens.size() == 2)
      n = std::stoi(tokens[1]);

    int start = std::max(0, total - n);

    for(int i = start; i < total; i++){
      std::cout << " " << i + 1 << " " << list[i] -> line << "\n";
    }
  }

  else if(cmd == "type"){
    if(tokens.size() < 2)
      return;
    const std::string t = tokens[1];
    if(t == "echo" || t == "exit" || t == "pwd" || t == "cd" || t == "type" || t == "history")
      std::cout << t << " is a shell builtin\n";
    else{
      std::string p = find_in_path(t);
      if(p.empty())
        std::cout << t << ": not found\n";
      else
        std::cout << t << " is " << p << "\n";
    }
  }
}


int main(){
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  rl_attempted_completion_function = completion_callback;
  rl_bind_key('\t', rl_complete);

  std::string command;
  while (true){
    char* input = readline("$ ");

    if(!input) //Ctrl+D or EOF
      break;
    
    if(strlen(input) == 0){
      free(input);
      continue;
    }

    add_history(input);

    std::string command(input);
    free(input);

    //exit builtin
    if(command == "exit" || command == "exit 0"){
      return 0;
    }

    //Tokenize input line
    std::vector<std::string> tokens = tokenize(command);
    if(tokens.empty())
      continue;

    //Parse redirections: prefer longer operators (>>/1>> then >/1> then 2>)
    //We'll support exact operator tokens and attached forms like ">>file" or "1>file"
    std::string outfile, appendfile, errfile, appenderrfile;
    bool redirect = false;
    bool append_redirect = false;
    bool redirect_err = false;
    bool append_redirect_err = false;

    for(size_t i = 0; i < tokens.size(); ++i){
      const std::string &t = tokens[i];

      //exact 2>>
      if(t == "2>>"){
        if(i + 1 >= tokens.size()){
          break;
        }
        appenderrfile = tokens[i + 1];
        append_redirect_err = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }

      //exact >> or 1>>
      if(t == ">>" || t == "1>>"){
        if(i + 1 >= tokens.size()){
          break;
        }
        appendfile = tokens[i + 1];
        append_redirect = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }

      //exact 2>
      if(t == "2>"){
        if(i + 1 >= tokens.size()){
          break;
        }
        errfile = tokens[i + 1];
        redirect_err = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        break;
      }

      //exact > or 1>
      if(t == ">" || t == "1>"){
        if(i + 1 >= tokens.size()){
          break;
        }
        outfile = tokens[i + 1];
        redirect = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i +2);
        break;
      }

      //Attached forms: check 2>> then 1>> then >> then 2> then 1> then >
      if(t.rfind("2>>", 0) == 0 && t.size() > 3){
        appenderrfile = t.substr(3);
        append_redirect_err = true;
        tokens.erase(tokens.begin() + i);
        break;
      } 
      if(t.rfind("1>>", 0) == 0 && t.size() > 3){
        appendfile = t.substr(3);
        append_redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }
      if(t.rfind(">>", 0) == 0 && t.size() > 2){
        appendfile = t.substr(2);
        append_redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }
      if(t.rfind("2>", 0) == 0 && t.size() > 2){
        errfile = t.substr(2);
        redirect_err = true;
        tokens.erase(tokens.begin() + i);
        break;
      }
      if(t.rfind("1>", 0) == 0 && t.size() > 2){
        outfile = t.substr(1);
        redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }
      if(t.rfind(">", 0) == 0 && t.size() > 2){
        outfile = t.substr(1);
        redirect = true;
        tokens.erase(tokens.begin() + i);
        break;
      }
    } //End parse
    
    //If tokens became empty after removing redirection tokens, skip
    if(tokens.empty())
      continue;
   
    auto pipe_it = std::find(tokens.begin(), tokens.end(), "|");
    bool has_pipe = (pipe_it != tokens.end());

    std::string cmd = tokens[0];

    //Builtins set
    auto is_builtin = [&](const std::string &c){
      return c == "echo" || c == "pwd" || c == "cd" || c == "type" || c == "exit" || c == "history";
    };

    bool builtin = is_builtin(cmd);

    //For builtins we perform redirection in the parent: open files and dup2
    int saved_stdout = -1, fd_out = -1;
    int saved_stderr = -1, fd_err = -1;
    int saved_stdout_append = -1, fd_append = -1;
    int saved_stderr_append = -1, fd_err_append = -1; 

    if(builtin){
      //stdout overwrite
      if(redirect){
        fd_out = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd_out < 0){
          perror("open");
          goto builtin_cleanup;
        }
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd_out, STDOUT_FILENO);
      }

      //stdout append
      if(append_redirect){
        fd_append = open(appendfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd_append < 0){
          perror("open");
          goto builtin_cleanup;
        }
        saved_stdout_append = dup(STDOUT_FILENO);
        dup2(fd_append, STDOUT_FILENO);
      }

      //stdout redirect 
      if(redirect_err){
        fd_err = open(errfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd_err < 0){
          perror("open");
          goto builtin_cleanup;
        }
        saved_stderr = dup(STDERR_FILENO);
        dup2(fd_err, STDERR_FILENO);
      }

      //stderr overwrite
      if(append_redirect_err){
        fd_err_append = open(appenderrfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd_err_append < 0){
          perror("open");
          goto builtin_cleanup;
        }
        saved_stderr_append = dup(STDERR_FILENO);
        dup2(fd_err_append, STDERR_FILENO);
      }
    }

    if(has_pipe){
      //Split tokens into commands
      std::vector<std::vector<std::string>> commands;
      std::vector<std::string> current;

      for(auto &t : tokens){
        if(t == "|"){
          commands.push_back(current);
          current.clear();
        }
        else
          current.push_back(t);
      }
      if(!current.empty())
        commands.push_back(current);

      int n = commands.size();
      if(n < 2)
        continue;

      //Create Pipes
      std::vector<int> pipes(2 * (n - 1));
      for(int i = 0; i < n - 1; i++){
        if(pipe(&pipes[2*i]) < 0){
          perror("pipe");
          goto pipeline_cleanup;
        }
      }

      //Fork for each command
      for(int i = 0; i < n; i++){
        pid_t pid = fork();
        if(pid == 0){
          //stdin 
          if(i > 0)
            dup2(pipes[2*(i-1)], STDIN_FILENO);

          //stdout
          if(i < n - 1)
            dup2(pipes[2*i + 1], STDOUT_FILENO);

          //close all pipes
          for(int fd : pipes)
            close(fd);

          //run command
          if(is_builtin(commands[i][0])){
            run_builtin(commands[i]);
            exit(0);
          }

          std::string path = find_in_path(commands[i][0]);
          std::vector<char*> args;
          for(auto &s : commands[i])
            args.push_back(const_cast<char*>(s.c_str()));
          args.push_back(nullptr);

          execvp(path.c_str(), args.data());
          perror("execvp");
          exit(1);
        }
      }

    pipeline_cleanup:
      //Parent close pipes and waits
      for(int fd : pipes)
        close(fd);

      for(int i = 0; i < n; i++)
        wait(nullptr);

      continue;
    }


    //Execute builtins
    if(cmd == "echo"){
      for(size_t i = 1; i < tokens.size(); ++i){
        std::cout << tokens[i] << (i + 1 < tokens.size() ? " " : "\n");
      }

    //restore fds for builtins
    builtin_cleanup:
      if(redirect && saved_stdout != -1){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(fd_out);
        saved_stdout = -1;
      }
      if(append_redirect && saved_stdout_append != -1){
        dup2(saved_stdout_append, STDOUT_FILENO);
        close(saved_stdout_append);
        close(fd_append);
        saved_stdout_append = -1;
      }
      if(redirect_err && saved_stderr != -1){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        close(fd_err);
        saved_stderr = -1;
      }
      if(append_redirect_err && saved_stderr_append != -1){
        dup2(saved_stderr_append, STDERR_FILENO);
        close(saved_stderr_append);
        close(fd_err_append);
        saved_stderr_append = -1;
      }
      continue;
    }

    if(cmd == "pwd"){
      std::cout << fs::current_path().string() << std::endl;
      goto builtin_cleanup;
    }

    if(cmd == "cd"){
      if(tokens.size() < 2){
        //No arguments. Do nothing
      }
      else{
        const std::string &path = tokens[1];
        if(path == "~"){
          //Home directory
          const char* home = std::getenv("HOME");
          if(home)
            chdir(home);
          else
           std::cout << "cd: HOME not set" << std::endl;
        }
        else if(!path.empty() && path[0] == '/'){
          //absolute path
          if(chdir(path.c_str()) != 0)
            std::cout << "cd: " << path << ": No such file or directory" << std::endl;
        }
        else{
          //relative path
          fs::path target = fs::current_path() / path;
          if(chdir(target.c_str()) != 0)
            std::cout << "cd: " << path << ": No such file or directory" << std::endl;
        }
      }
      goto builtin_cleanup;
    }

    if(cmd == "history"){
      //history -r <file>
      if(tokens.size() == 3 && tokens[1] == "-r"){
        if(read_history(tokens[2].c_str()) != 0)
          perror("history");
        continue;
      }

      //normal history
      HIST_ENTRY **list = history_list();
      if(!list)
        continue;

      int total = history_length;

      int n = total;
      if(tokens.size() == 2)
        n = std::stoi(tokens[1]);

      int start = std::max(0, total - n);

      for(int i = start; i < total; i++){
        std::cout << " " << i + 1 << " " << list[i] -> line << "\n";
      }
      goto builtin_cleanup;
    }

    if(cmd == "type"){
      if(tokens.size() < 2){
        //nothing
      }
      else{
        std::string rest = tokens[1];
        if(rest == "echo" || rest == "exit" || rest == "type" || rest == "pwd" || rest == "cd" || rest == "history"){
          std::cout << rest << " is a shell builtin" << std::endl;
        }
        else{
          std::string pt = find_in_path(rest);
          if(pt.empty())
            std::cout << rest << ": not found" << std::endl;
          else
           std::cout << rest << " is " << pt <<std::endl;
        }
      }
      goto builtin_cleanup;
    }

   //Not a builtin -> external command
    std::string prog_path;
    if(cmd.rfind("./", 0) == 0 || cmd.rfind("/", 0) == 0)
      prog_path = cmd;
    else
     prog_path = find_in_path(cmd);

    if(prog_path.empty()){
      std::cout << cmd << ": command not found" << std::endl;
      //no parent fds changed for external commands, so nothing to restore;
      continue;
    }

    pid_t pid = fork();
    if(pid == 0){
      //child: set redirections here (Do not affect parent)
      if(redirect){
        int fd2 = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fd2 < 0){
          perror("open");
          exit(1);
        }
        dup2(fd2, STDOUT_FILENO);
        close(fd2);
      }
      if(append_redirect){
        int fd2 = open(appendfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd2 < 0){
          perror("open");
          exit(1);
        }
        dup2(fd2, STDOUT_FILENO);
        close(fd2);
      }
      if(redirect_err){
        int fd2 = open(errfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd2 < 0){
          perror("open");
          exit(1);
        }
        dup2(fd2, STDERR_FILENO);
        close(fd2);
      }
      if(append_redirect_err){
        int fd2 = open(appenderrfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(fd2 < 0){
          perror("open");
          exit(1);
        }
        dup2(fd2, STDERR_FILENO);
        close(fd2);
      }

      //build argv
      std::vector<char*> args;
      for(auto &s : tokens)
        args.push_back(const_cast<char*>(s.c_str()));
      args.push_back(nullptr);
      execvp(prog_path.c_str(), args.data());
      perror("execvp");
      exit(1);
    }
    else if (pid > 0) {
      waitpid(pid, nullptr, 0);
    }
    else{
      perror("fork");
    }

    //Parent: nothing to restore for external commands
  } //while  
  return 0 ;
}
