#ifndef GAMEFOLDERHH
#define GAMEFOLDERHH

#include "ars-emu.hh"
#include "memory.hh"
#include "byuuML.hh"

namespace ARS {
  class GameFolder {
  protected:
    std::string path;
    std::unique_ptr<byuuML::document> manifest;
    GameFolder(std::string path,
               std::unique_ptr<byuuML::document> manifest)
      : path(path), manifest(std::move(manifest)) {}
  public:
    virtual ~GameFolder() {}
    /* getROMContent can't return nullptr. getRAMContent CAN, but only if the
       initialization image doesn't exist AND it's volatile RAM. */
    virtual std::unique_ptr<Memory> getROMContent(std::string name,
                                                  size_t size,
                                                  uint8_t pad = 0) = 0;
    virtual std::unique_ptr<WritableMemory>
    getRAMContent(std::string name, size_t size, bool persistent,
                  bool pad_specified = false, uint8_t pad = 0,
                  std::string id = "",
                  const uint8_t* initp = nullptr, size_t initsize = 0);
    const byuuML::document& getManifest() const { return *manifest; }
    static std::unique_ptr<GameFolder> open(std::string path);
    static std::vector<std::pair<std::string, uint8_t>> symbol_files_to_load;
    static bool load_debug_symbols;
  };
};

#endif
