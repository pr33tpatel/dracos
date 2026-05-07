#include <drivers/terminal.h>
#include <utils/print.h>

#include "utils/string.h"

using namespace os;
using namespace os::common;
using namespace os::utils;
using namespace os::drivers;
using namespace os::hardwarecommunication;

Terminal* Terminal::activeTerminal = 0;
static Port8Bit vgaIndexPort(0x3D4);
static Port8Bit vgaDataPort(0x3D5);

Terminal::Terminal() {
  activeTerminal = this;
  cursorX = 0;
  cursorY = 0;
  viewOffset = 0;
  uint8_t cursorTopPixelLine = 1;
  uint8_t cursorBottomPixelLine = 15;

  vgaIndexPort.Write(0x0A);
  uint8_t start_val = vgaDataPort.Read();
  vgaIndexPort.Write(0x0A);
  vgaDataPort.Write((start_val & 0xC0) | cursorTopPixelLine);

  vgaIndexPort.Write(0x0B);
  uint8_t end_val = vgaDataPort.Read();
  vgaIndexPort.Write(0x0B);
  vgaDataPort.Write((end_val & 0xE0) | cursorBottomPixelLine);

  Clear();
}


void Terminal::setHardwareCursor(uint16_t x, uint16_t y) {
  uint16_t position = y * VGA_WIDTH + x;

  // set high byte for cursor position
  vgaIndexPort.Write(0x0E);
  vgaDataPort.Write((uint8_t)(position >> 8) & 0xFF);

  // set low byte
  vgaIndexPort.Write(0x0F);
  vgaDataPort.Write((uint8_t)(position & 0xFF));
}


void Terminal::Render() {
  uint16_t* vgaMemory = (uint16_t*)0xB8000;
  int stopPoint = viewOffset + VGA_HEIGHT;
  if (stopPoint > HISTORY_SIZE) stopPoint = HISTORY_SIZE;

  int vgaY = 0;
  for (int y = viewOffset; y < stopPoint; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      vgaMemory[vgaY * VGA_WIDTH + x] = buffer[y][x];
    }
    vgaY++;
  }

  if (cursorY >= viewOffset && cursorY < viewOffset + VGA_HEIGHT) {
    setHardwareCursor(cursorX, cursorY - viewOffset);
  } else {
    setHardwareCursor(0, VGA_HEIGHT + 1);
  }

  showScrollingStatus(vgaMemory);
}


void Terminal::PutChar(char c, VGAColor fg, VGAColor bg) {
  uint8_t color = (uint8_t)fg | ((uint8_t)bg << 4);

  if (c == '\n') {  // new line
    cursorY++;
    cursorX = 0;
  } else if (c == '\b') {  // backspace
    Backspace();
    return;
  } else if (c == '\t') {  // tabs are 4 space
    cursorX += 4;
  } else {
    buffer[cursorY][cursorX] = os::utils::vga_entry(c, color);  // write to buffer
    cursorX++;
  }

  // line wrap
  if (cursorX >= VGA_WIDTH) {
    cursorY++;
    cursorX = 0;
  }

  // scroll buffer
  if (cursorY >= HISTORY_SIZE) {
    // shift view up by one line
    for (int y = 0; y < HISTORY_SIZE - 1; y++) {
      for (int x = 0; x < VGA_WIDTH; x++) {
        buffer[y][x] = buffer[y + 1][x];
      }
    }

    uint16_t blank = 0x0700 | ' ';
    for (int x = 0; x < VGA_WIDTH; x++) {
      buffer[HISTORY_SIZE - 1][x] = blank;
    }
    cursorY = HISTORY_SIZE - 1;
  }
  ScrollToBottom();  // snap to bottom so you are always typing on the bottom line
  Render();
}


void Terminal::Backspace() {
  if (cursorX > 0) {
    cursorX--;
  } else if (cursorX <= 0 && cursorY > 0) {
    cursorX = VGA_WIDTH - 1;
    cursorY--;
  }
  buffer[cursorY][cursorX] = 0x0700 | ' ';  // overwrite with blank space
  ScrollToBottom();
}


void Terminal::ClearCurrentLine() {
  uint16_t blank = 0x0700 | ' ';
  for (int x = 0; x < VGA_WIDTH; x++) {
    buffer[cursorY][x] = blank;
  }
  cursorX = 0;
  Render();
}


void Terminal::ScrollUp() {
  if (viewOffset > 0) {
    viewOffset--;
    Render();
  }
}


void Terminal::ScrollDown() {
  // check to make sure you don't scroll past bottom of buffer contents
  if (viewOffset + VGA_HEIGHT < cursorY + 1) {
    // allow scrolling up to last line of buffer contents
    viewOffset++;
    Render();
  }
}


void Terminal::ScrollToBottom() {
  if (cursorY < VGA_HEIGHT) {
    viewOffset = 0;
  } else {
    viewOffset = cursorY - VGA_HEIGHT + 1;
  }
  Render();
}


void Terminal::showScrollingStatus(uint16_t* vgaMemory) {
  if (cursorY >= viewOffset + VGA_HEIGHT) {
    char* msg = "[ SCROLLING HISTORY ]";
    int msgLen = strlen(msg);

    uint8_t textcolor = vga_color_entry(utils::BLACK_COLOR, utils::LIGHT_MAGENTA_COLOR);
    // bottom left aligned
    int startX = 0;
    int startY = VGA_HEIGHT - 1;

    int currentX = startY * VGA_WIDTH;  // used in loop
    for (int i = 0; i < VGA_WIDTH; i++) {
      char c;
      if (i < msgLen)  // print the message contents
        c = msg[i];
      else  // print spaces to fill the status line
        c = ' ';

      vgaMemory[currentX + i] = utils::vga_entry(c, textcolor);
    }
  }
}


uint16_t Terminal::getCursorX() {
  return cursorX;
}


uint16_t Terminal::getCursorY() {
  return cursorY;
}


void Terminal::moveCursor(int8_t dx, int8_t dy) {
  int newX = cursorX + dx;
  int newY = cursorY + dy;
  if (newX < 0) newX = 0;
  if (newX >= VGA_WIDTH) newX = VGA_WIDTH - 1;
  if (newY < 0) newY = 0;
  if (newY >= HISTORY_SIZE) newY = HISTORY_SIZE - 1;

  cursorX = newX;
  cursorY = newY;

  Render();
}


void Terminal::Clear() {
  uint16_t blank = 0x0700 | ' ';
  for (int y = 0; y < HISTORY_SIZE; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      buffer[y][x] = blank;
    }
  }
  cursorX = 0;
  cursorY = 0;
  viewOffset = 0;
  Render();
}
