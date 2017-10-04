#include "ars-emu.hh"
#include "teg.hh"
#include "cartridge.hh"
#include "mappers.hh"
#include "localquery.hh"

using namespace ARS;

namespace {
  void check_path(const std::string& path) {
    /* Paths MUST not be empty, begin with a slash, end with a slash, or
       contain a double slash */
    if(path.empty() || *path.begin() == '/' || *path.end() == '/'
       || path.find("//") != std::string::npos)
      goto invalid;
    {
      size_t cur_pos = 0;
      size_t slash_pos;
      while((slash_pos = path.find('/',cur_pos)) != std::string::npos) {
        if(path[cur_pos] == '.')
          goto invalid;
        cur_pos = slash_pos + 1;
      }
      if(path[cur_pos] == '.')
        goto invalid;
    }
    // Path is valid
    return;
  invalid:
    throw sn.Get("BOARD_MEMORY_PATH_INVALID"_Key);
  }
  class PadROM : public Memory {
  public:
    PadROM(uint32_t, uint8_t pad) : Memory(new uint8_t[1], 1) {
      memset(memory_buffer, pad, size);
    }
    ~PadROM() { delete[] memory_buffer; }
  };
  class PadRAM : public Memory {
  public:
    PadRAM(uint32_t size, uint8_t pad, bool randomize = false)
      : Memory(new uint8_t[size], size) {
      if(randomize)
        fillDramWithGarbage(memory_buffer, size);
      else
        memset(memory_buffer, pad, size);
    }
    ~PadRAM() { delete[] memory_buffer; }
  };
}

std::unique_ptr<Cartridge> ARS::cartridge;
const std::unordered_map<std::string, std::function<std::unique_ptr<Cartridge>(byuuML::cursor mapper_tag, std::unordered_map<std::string, std::unique_ptr<Memory> >)> > ARS::mapper_types {
  {"", ARS::Mappers::MakeBareChip},
  {"devcart", ARS::Mappers::MakeDevCart},
};

std::unique_ptr<Cartridge> Cartridge::load(GameFolder& gamefolder,
                                           std::string language_override) {
  std::vector<std::string> overrode { language_override };
  const std::vector<std::string>& languages
    = language_override.empty() ? GetUILanguageList() : overrode;
  auto board = LocalizedQuery(gamefolder.getManifest().query("board"),
                              languages);
  while(*board) {
    auto c = board->query("id");
    if(!c || c.data() != "ETARS") ++board;
    else break;
  }
  if(!*board) throw sn.Get("NO_ETARS_BOARD_FOUND_IN_MANIFEST"_Key);
  std::unordered_map<std::string, std::unique_ptr<Memory> > memories;
  // ROM modules
  for(auto&& module : LocalizedQuery(board->query("rom"), languages)) {
    std::string id = module.query("id").data("");
    if(memories.find(id) != memories.end())
      throw sn.Get("BOARD_CONTAINS_DUPLICATE_MEMORY"_Key);
    uint8_t pad = module.query("pad").value(0);
    auto c = module.query("size");
    if(!c)
      throw sn.Get("BOARD_MEMORY_SIZE_MISSING"_Key);
    unsigned long size = c.value<unsigned long>();
    if(size < 1 || size > Memory::MAXIMUM_POSSIBLE_MEMORY_SIZE
       || (size & (size - 1)) != 0)
      throw sn.Get("BOARD_MEMORY_SIZE_INVALID"_Key);
    c = module.query("name");
    if(c) {
      auto& path = c->data();
      check_path(path);
      memories[id] = gamefolder.getROMContent(path, size, pad);
    }
    else memories[id] = std::make_unique<PadROM>(size, pad);
    assert(memories[id]);
  }
  // RAM modules
  for(auto&& module : LocalizedQuery(board->query("ram"), languages)) {
    std::string id = module.query("id").data("");
    if(memories.find(id) != memories.end())
      throw sn.Get("BOARD_CONTAINS_DUPLICATE_MEMORY"_Key);
    bool persistent = !module.query("volatile");
    auto c = module.query("pad");
    bool randomize = !c;
    uint8_t pad = c.value(0);
    c = module.query("size");
    if(!c)
      throw sn.Get("BOARD_MEMORY_SIZE_MISSING"_Key);
    unsigned long size = c.value<unsigned long>();
    if(size < 1 || size > Memory::MAXIMUM_POSSIBLE_MEMORY_SIZE
       || (size & (size - 1)) != 0)
      throw sn.Get("BOARD_MEMORY_SIZE_INVALID"_Key);
    c = module.query("name");
    if(c) {
      auto& path = c->data();
      check_path(path);
      memories[id] = gamefolder.getRAMContent(path, size, persistent,
                                              !randomize, pad, id);
    }
    else memories[id] = std::make_unique<PadRAM>(size, pad, randomize);
    assert(memories[id]);
  }
  // Expansion hardware
  for(auto&& expansion : LocalizedQuery(board->query("expansion"), languages)){
    auto c = expansion.query("addr");
    uint16_t addr;
    if(c) {
      addr = c.value<int>();
      if(addr < 0x242 || addr > 0x247)
        throw sn.Get("BOARD_EXPANSION_ADDRESS_INVALID"_Key);
    }
    else addr = 0; // will be interpreted as "use default address"
  }
  // Mapper
  auto mapper_tag = *LocalizedQuery(board->query("mapper"));
  auto mapper_type = mapper_tag.data("");
  auto it = mapper_types.find(mapper_type);
  if(it == mapper_types.end())
    throw sn.Get("BOARD_MAPPER_TYPE_INVALID"_Key);
  return it->second(mapper_tag, std::move(memories));
}

const std::vector<std::string>& ARS::GetUILanguageList() {
  // TODO: on OSX and Windows (at least), we have a *list* of UI languages.
  static const std::vector<std::string> ret { sn.GetSystemLanguage() };
  return ret;
}
