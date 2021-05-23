#include "gamefolderinternal.hh"

#include "teg.hh"
#include "io.hh"
#include <fstream>

namespace {
  class VolatileRAM : public ARS::WritableMemory {
  public:
    VolatileRAM(const uint8_t* init_p, size_t init_size,
                uint32_t size, bool randomize, uint8_t pad)
      : WritableMemory(new uint8_t[size], size) {
      memcpy(memory_buffer, init_p, init_size);
      if(size > init_size) {
        if(randomize)
          ARS::fillDramWithGarbage(memory_buffer + init_size,
                                   size - init_size);
        else
          memset(memory_buffer + init_size, pad, size - init_size);
      }
    }
    ~VolatileRAM() {
      delete[] memory_buffer;
    }
  };
  class PersistentRAM : public VolatileRAM {
    std::string save_path, save_name;
    bool use_config_to_save = false;
    void flush() override {
      std::unique_ptr<std::ostream> o;
      if(!use_config_to_save) {
        o = IO::OpenRawPathForWrite(save_path);
        if(!o || !*o) use_config_to_save = true;
      }
      if((!o || !*o) && use_config_to_save) {
        o = IO::OpenConfigFileForWrite(save_name);
        if(!o || !*o) {
          ARS::ui << sn.Get("SRAM_WRITE_FAILURE"_Key) << ARS::ui;
        }
      }
      if(o && *o) {
        o->exceptions(o->exceptions() | std::ios_base::badbit | std::ios_base::failbit);
        try {
          o->write(reinterpret_cast<const char*>(memory_buffer), size);
          if(use_config_to_save) {
            o.reset();
            IO::UpdateConfigFile(save_name);
          }
        }
        catch(std::system_error& e) {
          std::cerr << e.code().message() << "\n";
          ARS::ui << sn.Get("SRAM_WRITE_FAILURE"_Key) << ARS::ui;
          if(!use_config_to_save) {
            use_config_to_save = true;
            flush();
          }
        }
      }
    }
  public:
    PersistentRAM(const uint8_t* init_p, size_t init_size,
                  uint32_t size, bool randomize, uint8_t pad,
                  std::string filename)
      : VolatileRAM(init_p, init_size, size, randomize, pad),
        save_path(filename) {
      auto o = save_path.rfind(DIR_SEP);
      if(o != std::string::npos) save_name = std::string(save_path.begin()+o+1,
                                                         save_path.end());
      else save_name = save_path;
      std::unique_ptr<std::istream> i = IO::OpenConfigFileForRead(save_name);
      if(i && *i) {
        std::cerr << o << " " << save_path << " " << save_name << "\n";
        ARS::readFill(*i, "(config)" DIR_SEP + save_name,
                 memory_buffer, size);
        use_config_to_save = true;
      }
      else {
        i = IO::OpenRawPathForRead(save_path, false);
        if(i && *i) {
          ARS::readFill(*i, save_path, memory_buffer, size);
        }
      }
    }
  };
  class PersistentRAMInGameFolder : public VolatileRAM {
    std::unique_ptr<std::iostream> f;
    void flush() override {
      try {
        f->clear();
        f->seekp(0);
        f->write(reinterpret_cast<const char*>(memory_buffer), size);
      }
      catch(std::system_error& e) {
        std::cerr << e.code().message() << "\n";
        ARS::ui << sn.Get("SRAM_WRITE_FAILURE"_Key) << ARS::ui;
      }
    }
  public:
    PersistentRAMInGameFolder(const uint8_t* initp, size_t initsize,
                              uint32_t size, bool randomize, uint8_t pad,
                              std::unique_ptr<std::iostream> f,
                              const std::string& contentpath)
      : VolatileRAM(initp, initsize, size, randomize, pad),
        f(std::move(f)) {
      try {
        ARS::readFill(*this->f, contentpath, memory_buffer, size);
      }
      catch(std::system_error& e) {
        throw std::string(e.code().message());
      }
    }
  };
  class GameFolder : public ARS::GameFolder {
  public:
    GameFolder(std::string path, std::unique_ptr<byuuML::document> manifest)
      : ARS::GameFolder(path, std::move(manifest)) {}
    std::unique_ptr<ARS::Memory> getROMContent(std::string name,
                                               size_t size,
                                               uint8_t pad) override {
      std::string contentpath = path + DIR_SEP + name;
      std::unique_ptr<std::istream> f = IO::OpenRawPathForRead(contentpath);
      if(!f || !*f) throw sn.Get("GAMEFOLDER_FILE_MISSING"_Key,
                                 {contentpath});
      std::unique_ptr<uint8_t[]> buf
        = std::make_unique<uint8_t[]>(size);
      size_t red = ARS::readFill(*f, contentpath, buf.get(), size);
      if(red < size) memset(buf.get()+red, pad, size-red);
#ifndef NO_DEBUG_CORES
      if(load_debug_symbols) {
        std::string sympath = contentpath + ".sym";
        bool symbols_present = false;
        if(IO::OpenRawPathForRead(sympath, false)) symbols_present = true;
        else {
          sympath = contentpath;
          while(sympath.size() > 0 && *sympath.crbegin() != '.'
                && *sympath.crbegin() != *DIR_SEP) sympath.pop_back();
          if(sympath.size() > 0 && *sympath.crbegin() == '.') {
            sympath += "sym";
            if(IO::OpenRawPathForRead(sympath, false)) symbols_present = true;
          }
        }
        if(symbols_present) {
          symbol_files_to_load.emplace_back(sympath, 0);
        }
      }
#endif
      return std::make_unique<ARS::LoadedROMContent>(std::move(buf), size);
    }
    std::unique_ptr<ARS::WritableMemory>
    getRAMContent(std::string name, size_t size, bool persistent,
                  bool pad_specified = false, uint8_t pad = 0,
                  std::string id = "",
                  const uint8_t* initp = nullptr, size_t initsize = 0)
      override {
      assert(initsize == 0);
      std::unique_ptr<uint8_t[]> local_initp;
      std::string contentpath = path + DIR_SEP + name;
      std::unique_ptr<std::iostream> f = IO::OpenRawPathForUpdate(contentpath,
                                                                  false);
      if(!f || !*f) {
        // maybe we just failed because the file didn't exist
        std::unique_ptr<std::istream> read_check
          = IO::OpenRawPathForRead(contentpath, false);
        if(read_check && *read_check) {
          // -> file exists and is readable
          // we probably just can't overwrite it for some reason
          // read the contents and pass them along to the ARS::GameFolder
          // implementation, which will save changes outside the folder
          read_check->exceptions(read_check->exceptions()
                                 | std::ios_base::badbit
                                 | std::ios_base::failbit);
          read_check->seekg(0, std::ios_base::end);
          auto big = read_check->tellg();
          if(big >= 0) {
            read_check->seekg(0);
            initsize = std::min(size_t(big), size);
            local_initp = std::make_unique<uint8_t[]>(size);
            initp = local_initp.get();
            initsize = ARS::readFill(*read_check, contentpath,
                                     local_initp.get(), initsize);
          }
        }
        else {
          // -> file does not exist (or is not readable)
          // try opening it for write, to create it
          std::unique_ptr<std::ostream> write_check
            = IO::OpenRawPathForWrite(contentpath, true);
          if(write_check && *write_check) {
            // -> we were able to create the file
            write_check.reset();
            // opening it for updating should now work
            f = IO::OpenRawPathForUpdate(contentpath, true);
          }
        }
      }
      if(f && *f) {
        f->exceptions(f->exceptions()
                      | std::ios_base::badbit
                      | std::ios_base::failbit);
        return std::make_unique<PersistentRAMInGameFolder>(initp, initsize,
                                                           size,
                                                           !pad_specified,
                                                           pad, std::move(f),
                                                           contentpath);
      }
      else {
        return ARS::GameFolder::getRAMContent(name, size, persistent,
                                              pad_specified, pad,
                                              id, initp, initsize);
      }
    }
  };
  class ireader : public byuuML::reader {
    char buf[512];
    std::istream& f;
  public:
    ireader(std::istream& f) : f(f) {}
    void read_more(const char*& begin, const char*& end) override {
      begin = buf;
      if(!f) {
        end = begin;
        return;
      }
      f.read(buf, sizeof(buf));
      end = begin + f.gcount();
    }
  };
}

std::unique_ptr<ARS::GameFolder> ARS::GameFolder::open(std::string path) {
  if(path.empty()) path = "." DIR_SEP;
  std::unique_ptr<GameFolder> ret;
  if(!ret) ret = openGameFolder(path);
  if(!ret && *path.rbegin() != *DIR_SEP) ret = openGameArchive(path);
  return ret;
}

std::unique_ptr<ARS::GameFolder> ARS::openGameFolder(std::string path) {
  bool definitely_intentional = false;
  while(!path.empty() && *path.rbegin() == *DIR_SEP) {
    definitely_intentional = true;
    path.resize(path.size()-1);
  }
  // unfortunately, I can think of a use case for a manifest.bml in root...
  std::string manifestpath = path + DIR_SEP "manifest.bml";
  std::unique_ptr<std::istream> file
    = IO::OpenRawPathForRead(manifestpath, definitely_intentional);
  if(!file || !*file) return nullptr;
  ireader reader(*file);
  try {
    std::unique_ptr<byuuML::document> manifest
      = std::make_unique<byuuML::document>(reader);
    return std::make_unique<::GameFolder>(path, std::move(manifest));
  }
  catch(std::string error) {
    std::string message = sn.Get("MANIFEST_PARSE_ERROR"_Key, {error});
    die("%s", message.c_str());
  }
}

std::unique_ptr<ARS::WritableMemory>
ARS::GameFolder::getRAMContent(std::string, size_t size, bool persistent,
                               bool pad_specified, uint8_t pad,
                               std::string id,
                               const uint8_t* initp, size_t initsize) {
  if(persistent) {
    std::string path = this->path;
    auto f = path.find(".etars");
    if(f != std::string::npos) path.resize(f);
    f = path.find(".etarz");
    if(f != std::string::npos) path.resize(f);
    while(!path.empty() && *path.rbegin() == *DIR_SEP)
      path.resize(path.size()-1);
    if(path.empty() || *path.rbegin() != '.')
      path += '.';
    path = path + (id.empty()?"ram":id);
    return std::make_unique<PersistentRAM>(initp, initsize, size,
                                           !pad_specified, pad,
                                           path);
  }
  else
    return std::make_unique<VolatileRAM>(initp, initsize, size, !pad_specified,
                                         pad);
}

size_t ARS::readFill(std::istream& in, const std::string& path,
                     uint8_t* buffer, size_t size) {
  in.exceptions(in.exceptions() | std::ios_base::badbit | std::ios_base::failbit);
  char* p = reinterpret_cast<char*>(buffer);
  size_t rem = size;
  while(in && rem > 0) {
    try {
      try {
        in.read(p, rem);
      }
      catch(std::system_error& e) {
        // do not err on EOF
        if(!in.eof()) throw;
      }
      rem -= in.gcount();
      p += in.gcount();
    }
    catch(std::system_error& e) {
      std::string msg = sn.Get("GAME_CONTENT_READ_ERROR"_Key,
                               {path, e.code().message()});
      die("%s", msg.c_str());
    }
  }
  return size - rem;
}

std::vector<std::pair<std::string, uint8_t>>
ARS::GameFolder::symbol_files_to_load;
bool ARS::GameFolder::load_debug_symbols = false;
