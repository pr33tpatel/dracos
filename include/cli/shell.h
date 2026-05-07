#ifndef __OS__CLI__SHELL_H
#define __OS__CLI__SHELL_H

#include <cli/command.h>
#include <common/types.h>
#include <drivers/keyboard.h>
#include <drivers/terminal.h>
#include <hardwarecommunication/pci.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <utils/ds/hashmap.h>
#include <utils/math.h>
#include <utils/print.h>
#include <utils/string.h>

#include "utils/ds/pair.h"

namespace os {
namespace cli {

#define ifCmd(req_cmd) if (strcmp(cmd, req_cmd) == 0)

struct Cmd {
  char* name;
  char** flagsList;
  common::int32_t numFlags;
};


class Shell : public os::drivers::KeyboardEventHandler {
 private:
  // CLI buffer variables
  char commandbuffer[256];  // [stores characters in command buffer]
  char commandHistory[10][256];
  common::uint16_t bufferIndex;  // [indexer for the command buffer]
  common::uint16_t cursorIndex;  // [indexer for the cursor position]

  bool InsertCharAtCursor(char c);
  bool BackspaceAtCursor();
  void ClearCommandBuffer();
  void RedrawInputLine();


 public:
  Shell();
  ~Shell();

  // Command Registry
  os::utils::ds::HashMap<const char*, Command*> commandMap;

  // event handler override
  void OnKeyDown(char c) override;
  void RegisterCommand(Command* cmd);
  void virtual ExecuteCommand();

  /**
   * @brief [fills command buffer with specifed char to specifed length ]
   * @param char [specifed char to fill with, default is ' ']
   * @param length [specified length to clear, default is entire buffer]
   */
  void FillCommandBuffer(
      char fill_char = ' ', common::uint16_t length = sizeof(commandbuffer) / sizeof(commandbuffer[0])
  );

  void PrintPrompt();
  void PrintPreviousCmd();

  void PrintCmdFlags(char* cmd, char** flagsList);

  void GetCommandNames(utils::ds::LinkedList<const char*>& dest);
  void GetCommandPointers(utils::ds::LinkedList<os::cli::Command*>& dest);
  void GetCommandNamesAndPointers(
      utils::ds::LinkedList<utils::ds::Pair<const char*, os::cli::Command*>>& dest
  );


  void ShellInit();
};
}  // namespace cli
}  // namespace os

#endif

/* TEST:
aaa
*/
