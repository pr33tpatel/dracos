#include <cli/commands/systemCmds.h>


using namespace os;
using namespace os::common;
using namespace os::utils;
using namespace os::cli;
using namespace os::drivers;
using namespace os::hardwarecommunication;

whoami::whoami() : Command("whoami") {}
void whoami::execute(char* args) {
  printf(LIGHT_CYAN_COLOR, BLACK_COLOR, "ORIGIN\n");
}


echo::echo() : Command("echo") {}
void echo::execute(char* args) {
  printf(LIGHT_CYAN_COLOR, BLACK_COLOR, "%s\n", args);
}


clear::clear() : Command("clear") {}
void clear::execute(char* args) {
  Terminal::activeTerminal->Clear();
}


listcommands::listcommands(Shell* shell) : Command("listcommands"), shell(shell) {}
void listcommands::execute(char* args) {
  ds::LinkedList<const char*> namesList;
  ds::LinkedList<os::cli::Command*> cmdPtrsList;
  ds::LinkedList<ds::Pair<const char*, Command*>> nameCmdPtrsList;

  char* argvList[31];
  uint8_t argcVal = 0;
  argvList[0] = (char*)this->name;
  argcVal++;


  // first pass of inputs, tokenize args and append to argvList
  char* token = strtok(args, " ");
  while (token != 0 && argcVal < 31) {
    argvList[argcVal] = token;
    argcVal++;
    token = strtok(0, " ");
  }

  bool helpFlag = false;

  char* helpStr = "Usage: listcommands <flags>\n";

  FlagOption flags[] = {
      {"-h", "display help contents", &helpFlag},
  };

  int numFlags = sizeof(flags) / sizeof(flags[0]);


  // second pass
  for (int i = 1; i < argcVal; i++) {
    char* argv = argvList[i];
    if (argv[0] == '-') {  // arg is a flag if leading character is '-'
      ParseFlags(argv, flags, numFlags);
    }
  }

  if (helpFlag) {
    printf(LIGHT_RED_COLOR, BLACK_COLOR, "%s", helpStr);
    PrintFlags(flags, numFlags);
    return;
  }


  shell->GetCommandNames(namesList);

  printf(LIGHT_BLUE_COLOR, BLACK_COLOR, "Available commands:\n");
  auto* temp = namesList.head;
  if (temp == 0) {
    printf(RED_COLOR, BLACK_COLOR, "[ERROR]: CANNOT PRINT EMPTY LIST\n");
  }

  while (temp != nullptr) {
    printf(LIGHT_CYAN_COLOR, BLACK_COLOR, " - %s", temp->data);
    if (temp->next != nullptr) printf("\n");
    temp = temp->next;
  }
  printf("\n");
  return;
}
