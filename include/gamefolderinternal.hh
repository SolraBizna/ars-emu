#ifndef GAMEFOLDERINTERNALHH
#define GAMEFOLDERINTERNALHH

#include "gamefolder.hh"

namespace ARS {
  class LoadedROMContent : public Memory {
  public:
    LoadedROMContent(uint8_t* data, size_t size)
      : Memory(data, size) {}
    LoadedROMContent(std::unique_ptr<uint8_t[]> data, size_t size)
      : Memory(data.release(), size) {}
    ~LoadedROMContent() { delete[] memory_buffer; }
  };
  std::unique_ptr<GameFolder> openGameFolder(std::string path);
  std::unique_ptr<GameFolder> openGameArchive(std::string path);
  size_t readFill(std::istream& in, const std::string& path,
                  uint8_t* buffer, size_t size);
}

#endif
