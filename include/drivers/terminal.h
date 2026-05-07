#ifndef __OS__DRIVERS__TERMINAL_H
#define __OS__DRIVERS__TERMINAL_H

#include <common/types.h>
#include <hardwarecommunication/port.h>
#include <utils/print.h>

namespace os {
namespace drivers {

class Terminal {
 public:
  static const common::uint16_t VGA_WIDTH = 80;
  static const common::uint16_t VGA_HEIGHT = 25;
  static const common::uint16_t HISTORY_SIZE = 800;  // store 800 lines (32 pages) of terminal history

  static Terminal* activeTerminal;

 private:
  common::uint16_t buffer[HISTORY_SIZE][VGA_WIDTH];

  common::uint16_t cursorX;
  common::uint16_t cursorY;

  common::uint16_t viewOffset;  // offset for ring buffer, determines which line is at the top of screen

  void Render();

 public:
  Terminal();
  ~Terminal();

  void PutChar(char c, os::utils::VGAColor fg, os::utils::VGAColor bg);

  void ScrollUp();
  void ScrollDown();
  void showScrollingStatus(common::uint16_t* vgaMemory);
  void ScrollToBottom();
  void Clear();
  void Backspace();
  void ClearCurrentLine();

  void setCursorPos(common::uint8_t destX, common::uint8_t destY);
  common::uint16_t getCursorX();
  common::uint16_t getCursorY();
  void moveCursor(common::int8_t dx, common::int8_t dy);
  void setHardwareCursor(common::uint16_t x, common::uint16_t y);
};

}  // namespace drivers
}  // namespace os

#endif
