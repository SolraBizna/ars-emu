#include "ars-emu.hh"
#include "ppu.hh"
#include "font.hh"
#include "utfit.hh"
#include <list>
#include <algorithm>

ARS::MessageImp ARS::ui;

namespace {
  constexpr int MESSAGES_CHARS_WIDE = 30;
  constexpr int MESSAGES_LINES_HIGH = 13;
  constexpr int MESSAGES_WIDTH = MESSAGES_CHARS_WIDE * 8;
  constexpr int MESSAGES_HEIGHT = MESSAGES_LINES_HIGH * 16;
  static_assert(MESSAGES_WIDTH < ARS::PPU::LIVE_SCREEN_WIDTH,
                "messages must not be allowed to overflow the ARS PPU's active"
                " region, as displays are not required to display anything"
                " outside this region");
  static_assert(MESSAGES_HEIGHT < ARS::PPU::TOTAL_SCREEN_HEIGHT * 9 / 10,
                "messages must not be allowed to escape the overscan region");
  // subtract 1 from the margins, to allow for the shadow
  constexpr int MESSAGES_MARGIN_X = (ARS::PPU::LIVE_SCREEN_WIDTH
                                     - MESSAGES_WIDTH)/2
    + ARS::PPU::LIVE_SCREEN_LEFT - 1;
  constexpr int MESSAGES_MARGIN_Y = (ARS::PPU::TOTAL_SCREEN_HEIGHT
                                     - MESSAGES_HEIGHT)/2 - 1;
  constexpr int MESSAGE_LIFESPAN = 300;
  struct LoggedMessage {
    std::string string;
    int lifespan;
    LoggedMessage(std::string&& string, int lifespan)
      : string(string), lifespan(lifespan) {}
  };
  std::list<LoggedMessage> logged_messages;
  bool messages_dirty = false;
  void draw_glyph(ARS::PPU::raw_screen& out,
                  int x, int y, const Font::Glyph& glyph) {
    for(int sub_y = 0; sub_y < Font::HEIGHT + 2; ++sub_y) {
      auto& out_row = out[y+sub_y];
      int w = glyph.wide ? 18 : 10;
      // uint32_t avoids negative bitshift
      uint32_t bitmap = sub_y > 0 ? glyph.tiles[sub_y-1]<<16 : 0,
        bitmap2 = sub_y < Font::HEIGHT ? glyph.tiles[sub_y]<<16 : 0,
        bitmap3 = sub_y < Font::HEIGHT-1 ? glyph.tiles[sub_y+1]<<16 : 0;
      if(glyph.wide) {
        bitmap |= sub_y > 0 ? glyph.tiles[sub_y+15]<<8 : 0;
        bitmap2 |= sub_y < Font::HEIGHT ? glyph.tiles[sub_y+16]<<8 : 0;
        bitmap3 |= sub_y < Font::HEIGHT-1 ? glyph.tiles[sub_y+17]<<8 : 0;
      }
      uint32_t shadowmap = bitmap|bitmap2|bitmap3;
      shadowmap |= shadowmap<<1; shadowmap |= shadowmap>>1;
      for(int sub_x = 0; sub_x < w; ++sub_x) {
        if(bitmap2 & (0x1000000>>sub_x)) {
          out_row[x+sub_x] = 0x47;
        }
        else if(shadowmap & (0x1000000>>sub_x))
          out_row[x+sub_x] = 0xFF;
      }
    }
  }
}

void ARS::PPU::cycleMessages() {
  while(!logged_messages.empty() && logged_messages.begin()->lifespan-- <=0){
    logged_messages.pop_front();
    messages_dirty = true;
  }
}

void ARS::PPU::renderMessages(ARS::PPU::raw_screen& out) {
  int y = MESSAGES_HEIGHT + MESSAGES_MARGIN_Y;
  auto it = logged_messages.crbegin();
  while(it != logged_messages.crend() && y > MESSAGES_MARGIN_Y) {
    y -= Font::HEIGHT;
    auto& msg = *it;
    int x = MESSAGES_MARGIN_X;
    auto cit = msg.string.cbegin();
    while(cit != msg.string.cend()) {
      uint32_t c = getNextCodePoint(cit, msg.string.cend());
      if(c == 0x20) x += 8;
      else if(c == 0x3000) x += 16;
      else {
        auto& glyph = Font::GetGlyph(c);
        if(x + (glyph.wide ? 16 : 8) > MESSAGES_WIDTH+MESSAGES_MARGIN_X) break;
        draw_glyph(out, x, y, glyph);
        x += glyph.wide ? 16 : 8;
      }
    }
    ++it;
  }
  cycleMessages();
}

void ARS::MessageImp::outputLine(std::string line, int lifespan_value) {
  std::cerr << sn.Get("UI_MESSAGE_STDERR"_Key, {line}) << std::endl;
  logged_messages.emplace_back(std::move(line), lifespan_value);
}

void ARS::MessageImp::outputBuffer() {
  std::string msg = stream.str();
  int lifespan = MESSAGE_LIFESPAN;
  for(auto& lmsg : logged_messages)
    lifespan -= lmsg.lifespan;
  auto begin = msg.begin();
  do {
    auto it = std::find(begin, msg.end(), '\n');
    outputLine(std::string(begin, it), lifespan);
    if(it != msg.end()) ++it;
    begin = it;
    lifespan = 0;
  } while(begin != msg.end());
  stream.clear();
  stream.str("");
  messages_dirty = true;
}
