#include <cli/shell.h>


using namespace os;
using namespace os::common;
using namespace os::utils;
using namespace os::utils::ds;
using namespace os::cli;
using namespace os::hardwarecommunication;
using namespace os::drivers;
using namespace os::net;


Shell::Shell() {
  bufferIndex = 0;
  cursorIndex = 0;
  commandbuffer[0] = '\0';

  for (int i = 0; i < 10; i++) {
    commandHistory[i][0] = '\0';
  }
}


Shell::~Shell() {}


void Shell::RegisterCommand(Command* cmd) {
  Command* existingCmd = 0;
  if (commandMap.Get(cmd->name, existingCmd)) {
    printf(YELLOW_COLOR, BLACK_COLOR, "[SHELL] Warning: overwriting command \"%s\"\n", cmd->name);
  }
  commandMap.Insert(cmd->name, cmd);
}


void Shell::ExecuteCommand() {
  if (commandbuffer[0] == '\0') return;
  char* p = commandbuffer;
  while (*p != '\0' && *p != ' ') p++;
  char* args;
  if (*p == '\0') {
    args = (char*)"";  // no space found, command has no arguments
  } else {
    *p = '\0';  // null-terimate the cmdName (e.g "ping\0 ...")
    p++;
    while (*p == ' ') p++;  // skips spaces between args
    args = p;
  }
  const char* cmdName = commandbuffer;
  Command* cmdPtr = 0;
  if (commandMap.Get(cmdName, cmdPtr) && cmdPtr != 0) {
    cmdPtr->execute(args);
  } else {
    printf(YELLOW_COLOR, BLACK_COLOR, "\"%s\" not in Command Registry\n", cmdName);
  }
}


bool Shell::InsertCharAtCursor(char c) {
  if (bufferIndex >= sizeof(commandbuffer) - 1) return false;
  for (int i = bufferIndex; i > cursorIndex; i--) {
    commandbuffer[i] = commandbuffer[i - 1];
  }
  commandbuffer[cursorIndex] = c;
  bufferIndex++;
  cursorIndex++;
  commandbuffer[bufferIndex] = '\0';
  return true;
}


bool Shell::BackspaceAtCursor() {
  if (bufferIndex == 0 || cursorIndex == 0) return false;
  for (int i = cursorIndex - 1; i < bufferIndex - 1; ++i) {
    commandbuffer[i] = commandbuffer[i + 1];
  }
  bufferIndex--;
  cursorIndex--;
  commandbuffer[bufferIndex] = '\0';
  return true;
}

void Shell::ClearCommandBuffer() {
  bufferIndex = 0;
  cursorIndex = 0;
  commandbuffer[0] = '\0';
}

void Shell::RedrawInputLine() {
  if (Terminal::activeTerminal == 0) return;
  Terminal::activeTerminal->ClearCurrentLine();
  PrintPrompt();
  for (int i = 0; i < bufferIndex; i++) {
    putChar(commandbuffer[i]);
  }

  int promptLength = 2;  // "& "
  int targetX = promptLength + cursorIndex;
  int currentX = Terminal::activeTerminal->getCursorX();

  if (targetX < currentX) {
    Terminal::activeTerminal->moveCursor(targetX - currentX, 0);
  }
}


void Shell::FillCommandBuffer(char fill_char, uint16_t length) {
  for (uint16_t i = 0; i < length; i++) {
    commandbuffer[i] = fill_char;
  }
  bufferIndex = length % sizeof(commandbuffer) / sizeof(char) + 1;
}


void Shell::PrintPrompt() {
  char* prompt = "& ";
  printf(RED_COLOR, BLACK_COLOR, prompt);
}


void Shell::PrintPreviousCmd() {
  printf(RED_COLOR, BLACK_COLOR, "Command Received: ");
  printf(LIGHT_GRAY_COLOR, BLACK_COLOR, "%s\n", commandbuffer);
}

void Shell::ShellInit() {
  PrintPrompt();
}

void Shell::PrintCmdFlags(char* cmd, char** flagsList) {
  int32_t num_flags = 0;
  num_flags = sizeof(flagsList) / sizeof(flagsList[0]);  // NOLINT
  // num_flags = sizeof(*flagsList)/sizeof(**flagsList);
  printf(YELLOW_COLOR, BLACK_COLOR, "total flags for %s: %d\n", cmd, num_flags);
  printf(YELLOW_COLOR, BLACK_COLOR, "%s flags: \n", cmd);
  for (int i = 0; i < num_flags; i++) {
    printf(YELLOW_COLOR, BLACK_COLOR, "[%d] %s\n", i, flagsList[i]);
  }
}


void Shell::GetCommandNames(ds::LinkedList<const char*>& dest) {
  commandMap.GetKeys(dest);
}

void Shell::GetCommandPointers(ds::LinkedList<Command*>& dest) {
  commandMap.GetValues(dest);
}

void Shell::GetCommandNamesAndPointers(ds::LinkedList<ds::Pair<const char*, os::cli::Command*>>& dest) {
  commandMap.GetPairs(dest);
}


void Shell::OnKeyDown(char c) {
  if (Terminal::activeTerminal == 0) return;
  // cursor navigation
  if ((uint8_t)c == ARROW_UP) {
    Terminal::activeTerminal->moveCursor(0, -1);
    return;
  }
  if ((uint8_t)c == ARROW_RIGHT) {
    Terminal::activeTerminal->moveCursor(1, 0);
    cursorIndex++;
    return;
  }
  if ((uint8_t)c == ARROW_DOWN) {
    Terminal::activeTerminal->moveCursor(0, 1);
    return;
  }
  if ((uint8_t)c == ARROW_LEFT) {
    Terminal::activeTerminal->moveCursor(-1, 0);
    cursorIndex--;
    return;
  }
  if ((uint8_t)c == SHIFT_ARROW_DOWN) {
    Terminal::activeTerminal->ScrollDown();
    return;
  }
  if ((uint8_t)c == SHIFT_ARROW_UP) {
    Terminal::activeTerminal->ScrollUp();
    return;
  }


  // TODO: handle writing where data already exists by shifting existing data to the right
  /*
  if (cursorIndex < bufferIndex) {
    for (int i = bufferIndex; i > cursorIndex; i--) {
      commandbuffer[i] = commandbuffer[i-1];
    }
    commandbuffer[cursorIndex] = c;
    bufferIndex++;
    cursorIndex++;

    // TODO: printRestOfLine(commandbuffer, cursorIndex);
  }
  */


  // history
  if ((uint8_t)c == SHIFT_ARROW_LEFT) {
    printf("history");
    return;
  }

  if (c == '\n') {  // 'Enter' is pressed
    putChar('\n');  // print new line if 'Enter'

    if (bufferIndex > 0) {
      commandbuffer[bufferIndex] = '\0';  // signal end of command
      ExecuteCommand();
      bufferIndex = 0;  // reset buffer index
    }
    ClearCommandBuffer();
    PrintPrompt();
    return;
  }

  if (c == '\b') {  // 'Backspace' is pressed
    if (BackspaceAtCursor()) {
      RedrawInputLine();
    }
    return;
  }

  if (c >= ' ' && c <= '~') {
    if (InsertCharAtCursor(c)) {
      RedrawInputLine();
    }
    return;
  }

  /*
  else {
    putChar(c);                      // display character pressed
    commandbuffer[bufferIndex] = c;  // append
    bufferIndex++;
  }
  */
}
