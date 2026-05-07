#ifndef __OS__CLI__SYSTEMCMDS_H
#define __OS__CLI__SYSTEMCMDS_H

#include <cli/command.h>
#include <cli/shell.h>
#include <common/types.h>
#include <drivers/terminal.h>
#include <drivers/timer.h>
#include <hardwarecommunication/pci.h>
#include <memorymanagement.h>


namespace os {
namespace cli {

class whoami : public Command {
 private:
 public:
  whoami();
  void execute(char* args) override;
};


class echo : public Command {
 private:
 public:
  echo();
  void execute(char* args) override;
};


class clear : public Command {
 private:
 public:
  clear();
  void execute(char* args) override;
};


class strToInt : public Command {
 private:
 public:
  strToInt();
  void execute(char* args) override;
};


class convert : public Command {
 private:
 public:
  convert();
  void execute(char* args) override;
};


class intToStr : public Command {
 private:
 public:
  intToStr();
  void execute(char* args) override;
};


class lspci : public Command {
 private:
  os::hardwarecommunication::PeripheralComponentInterconnectController* pci;

 public:
  lspci(os::hardwarecommunication::PeripheralComponentInterconnectController* pci);
  void execute(char* args) override;
};


class listcommands : public Command {
 private:
  Shell* shell;

 public:
  listcommands(Shell* shell);
  void execute(char* args) override;
};


}  // namespace cli
}  // namespace os

#endif
