#include "gamefolderinternal.hh"

#include "io.hh"
#include <zlib.h>

namespace {
  constexpr unsigned int END_OF_CENTRAL_DIRECTORY_LENGTH = 22;
  constexpr unsigned int CENTRAL_FILE_HEADER_LENGTH = 46;
  constexpr unsigned int LOCAL_FILE_HEADER_LENGTH = 30;
  constexpr unsigned int BUFFER_SIZE = 1024;
  constexpr uint16_t get16(uint8_t* p) {
    return p[0] | (p[1] << 8);
  }
  constexpr uint32_t get32(uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
  }
  class CRC32 {
    uint32_t crc = crc32(0, nullptr, 0);
  public:
    void update(const uint8_t* buf, size_t len) {
      crc = crc32(crc, buf, len);
    }
    bool check(uint32_t crc) {
      return crc == this->crc;
    }
  };
  class ReadyToDecodeFile {
    std::shared_ptr<std::istream> in;
    uint32_t offset;
    std::string name;
  public:
    ReadyToDecodeFile() : offset(0xFFFFFFFF) {}
    ReadyToDecodeFile(std::shared_ptr<std::istream> in,
                      uint32_t offset, std::string name)
      : in(std::move(in)), offset(offset), name(name) {}
    std::unique_ptr<uint8_t[]> Load(size_t& out_size,
                                    uint32_t pad_to_size = ~uint32_t(0),
                                    uint8_t pad = 0) {
      try {
        in->seekg(offset);
        uint8_t buf[LOCAL_FILE_HEADER_LENGTH];
        in->read(reinterpret_cast<char*>(buf), sizeof(buf));
        if(buf[0] != 0x50 || buf[1] != 0x4B || buf[2] != 0x03 || buf[3] !=0x04)
          throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
        if(get16(buf + 4) > 20)
          throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
        uint16_t general_purpose_bit_flag = get16(buf + 6);
        if(general_purpose_bit_flag & 0x4069) {
          // these bits enote features we don't support
          // (they're normally supposed to be accompanied by a "version needed
          // to extract" value > 20, but...)
          // exception: bit 3 (0x0008) denotes a zipfile that contains Data
          // Descriptors. We don't support them because of the relative
          // complexity of doing it safely.
          throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
        }
        uint16_t method = get16(buf + 8);
        // ignore modification time and date
        uint32_t crc32 = get32(buf + 14);
        uint32_t compressed_size = get32(buf + 18);
        uint32_t uncompressed_size = get32(buf + 22);
        // skip filename and extra field
        uint32_t toskip = get16(buf + 26);
        toskip += get16(buf + 28);
        in->seekg(toskip, std::ios_base::cur);
        // and now, the fun part!
        std::unique_ptr<uint8_t[]> ret;
        if(pad_to_size == ~uint32_t(0)) pad_to_size = uncompressed_size;
        ret = std::make_unique<uint8_t[]>(pad_to_size);
        if(pad_to_size > uncompressed_size) {
          memset(ret.get() + uncompressed_size, pad,
                 pad_to_size - uncompressed_size);
        }
        CRC32 calculated_crc32;
        switch(method) {
        default:
          throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
        case 0: {
          if(compressed_size != uncompressed_size)
            throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
          in->read(reinterpret_cast<char*>(ret.get()),
                   std::min(uncompressed_size, uint32_t(pad_to_size)));
          calculated_crc32.update(ret.get(), std::min(uncompressed_size,
                                                    uint32_t(pad_to_size)));
          if(uncompressed_size > pad_to_size) {
            uint8_t buf[BUFFER_SIZE];
            uint32_t rem = uncompressed_size - pad_to_size;
            while(rem > 0) {
              uint32_t amount = std::min(rem, uint32_t(sizeof(buf)));
              in->read(reinterpret_cast<char*>(buf), amount);
              rem -= in->gcount();
              calculated_crc32.update(buf, in->gcount());
            }
          }
        }
        case 8: {
          z_stream z;
          z.next_in = nullptr;
          z.avail_in = 0;
          z.next_out = ret.get();
          z.avail_out = std::min(uncompressed_size, pad_to_size);
          z.zalloc = nullptr;
          z.zfree = nullptr;
          z.opaque = nullptr;
          // negative window size -> raw deflate
          auto result = inflateInit2(&z, -MAX_WBITS);
          assert(result == Z_OK);
          // automatically call inflateEnd no matter what
          class autoEnd {
            z_streamp zp;
          public:
            autoEnd(z_streamp zp) : zp(zp) {}
            ~autoEnd() { inflateEnd(zp); }
          } autoEndInstance(&z);
          uint32_t remaining_compressed_data = compressed_size;
          uint32_t remaining_uncompressed_data = uncompressed_size;
          uint8_t buf[BUFFER_SIZE];
          std::unique_ptr<uint8_t[]> trailerbuf;
          while(remaining_compressed_data > 0) {
            if(z.avail_in == 0) {
              in->read(reinterpret_cast<char*>(buf),
                       std::min(uint32_t(sizeof(buf)),
                                remaining_compressed_data));
              z.next_in = buf;
              z.avail_in = in->gcount();
            }
            if(z.avail_out == 0) {
              if(!trailerbuf)
                trailerbuf = std::make_unique<uint8_t[]>(BUFFER_SIZE);
              z.next_out = trailerbuf.get();
              z.avail_out = BUFFER_SIZE;
            }
            auto before_next_in = z.next_in;
            auto before_next_out = z.next_out;
            result = inflate(&z, remaining_compressed_data == z.avail_in
                             ? Z_FINISH : Z_NO_FLUSH);
            auto after_next_in = z.next_in;
            auto after_next_out = z.next_out;
            uint32_t amount_read = after_next_in - before_next_in;
            uint32_t amount_written = after_next_out - before_next_out;
            assert(amount_read <= remaining_compressed_data);
            remaining_compressed_data -= amount_read;
            calculated_crc32.update(before_next_out, amount_written);
            assert(amount_written <= remaining_uncompressed_data);
            remaining_uncompressed_data -= amount_written;
            if(result == Z_OK) {
              // OK!
              if(remaining_uncompressed_data == 0
                 && remaining_compressed_data == 0) {
                // done!
                break;
              }
            }
            else if(result == Z_STREAM_END) {
              // er... done?
              break;
            }
            else
              throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
          }
          if(remaining_uncompressed_data != 0
             || remaining_compressed_data != 0)
            throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
          // done!
        }
        }
        if(!calculated_crc32.check(crc32))
          throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
        assert(ret);
        out_size = uncompressed_size;
        return ret;
      }
      catch(std::system_error& e) {
        std::string msg = sn.Get("GAME_CONTENT_READ_ERROR"_Key,
                                 {name, e.code().message()});
        die("%s", msg.c_str());
      }
    }
  };
  class GameArchive : public ARS::GameFolder {
    std::unordered_map<std::string, ReadyToDecodeFile> files;
    std::string prefix;
  public:
    GameArchive(std::unordered_map<std::string, ReadyToDecodeFile> files,
                std::string path, std::string prefix,
                std::unique_ptr<byuuML::document> manifest)
      : GameFolder(path, std::move(manifest)),
        files(std::move(files)), prefix(std::move(prefix)) {}
    std::unique_ptr<ARS::Memory> getROMContent(std::string name,
                                               size_t size,
                                               uint8_t pad = 0) override {
      std::string path = prefix + name;
      auto it = files.find(path);
      if(it == files.end())
        throw sn.Get("GAMEFOLDER_FILE_MISSING"_Key, {name});
      size_t loaded_size;
      auto ptr = it->second.Load(loaded_size, size, pad);
      return std::make_unique<ARS::LoadedROMContent>(std::move(ptr), size);
    }
    std::unique_ptr<ARS::WritableMemory>
    getRAMContent(std::string name, size_t size, bool persistent,
                  bool pad_specified = false, uint8_t pad = 0,
                  std::string id = "",
                  const uint8_t* initp = nullptr, size_t initsize = 0)
      override {
      assert(initsize == 0);
      std::string path = prefix + name;
      auto it = files.find(name);
      std::unique_ptr<uint8_t[]> local_initp;
      if(it != files.end()) {
        local_initp = it->second.Load(initsize, size, 0);
        initp = local_initp.get();
      }
      return ARS::GameFolder::getRAMContent(name, size, persistent,
                                            pad_specified, pad,
                                            id, initp, initsize);
    }
  };
}

std::unique_ptr<ARS::GameFolder> ARS::openGameArchive(std::string path) {
  try {
    std::unique_ptr<std::istream> inu = IO::OpenRawPathForRead(path, true);
    if(!inu || !*inu) return nullptr;
    std::shared_ptr<std::istream> in(inu.release());
    in->exceptions(in->exceptions() | std::ios_base::badbit
                  | std::ios_base::failbit);
    in->seekg(0, std::ios_base::end);
    size_t told = in->tellg();
    if(told < END_OF_CENTRAL_DIRECTORY_LENGTH) return nullptr; // too small
    in->seekg(told - END_OF_CENTRAL_DIRECTORY_LENGTH);
    uint16_t num_entries;
    uint32_t cd_start_offset;
    {
      uint8_t buf[END_OF_CENTRAL_DIRECTORY_LENGTH];
      in->read(reinterpret_cast<char*>(buf), sizeof(buf));
      unsigned int comment_length = 0;
      while(true) {
        if(buf[0] == 0x50 && buf[1] == 0x4b
           && buf[2] == 0x05 && buf[3] == 0x06) break; // found!
        ++comment_length;
        if(comment_length > 65536
           || comment_length + END_OF_CENTRAL_DIRECTORY_LENGTH > told)
          return nullptr; // not a zipfile
        else {
          in->seekg(told - END_OF_CENTRAL_DIRECTORY_LENGTH - comment_length);
          memmove(buf+1, buf, END_OF_CENTRAL_DIRECTORY_LENGTH-1);
        }
      }
      if(comment_length == 65536) return nullptr; // not a zipfile
      if(buf[4] || buf[5] || buf[6] || buf[7]) {
        // this is a multidisk zipfile
        throw sn.Get("GAME_ARCHIVE_ZIP_MULTIDISK_NOT_SUPPORTED"_Key);
      }
      if(buf[8] != buf[10] || buf[9] != buf[11]) {
        // "total number of entries in the central directory on this disk"
        // and "total number of entries in the central directory" did not match
        throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
      }
      num_entries = get16(buf+8);
      // ignore "size of the central directory"
      cd_start_offset = get32(buf+16);
      if(cd_start_offset == 0xFFFFFFFF) {
        // Zip64 archive
        throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
      }
      // ignore the .ZIP file comment
    }
    in->seekg(cd_start_offset);
    std::unordered_map<std::string, ReadyToDecodeFile> files;
    std::vector<char> filename_buf;
    bool found_manifest_prefix = false;
    std::string manifest_prefix;
    for(unsigned int n = 0; n < num_entries; ++n) {
      uint8_t buf[CENTRAL_FILE_HEADER_LENGTH];
      in->read(reinterpret_cast<char*>(buf), sizeof(buf));
      if(buf[0] != 0x50 || buf[1] != 0x4B || buf[2] != 0x01 || buf[3] != 0x02)
        throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
      // ignore "version made by"
      uint16_t needed_to_extract = get16(buf + 6);
      if(needed_to_extract > 20)
        throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
      // other than checking that, we ignore much of the information in the
      // central directory
      uint16_t method = get16(buf + 10);
      if(method != 0 && method != 8)
        throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
      uint16_t filename_length = get16(buf + 28);
      if(filename_length == 0)
        throw sn.Get("GAME_ARCHIVE_ZIP_CORRUPTED"_Key);
      // "extra field length" + "comment length"
      uint32_t to_skip = get16(buf + 30);
      to_skip += get16(buf + 32);
      uint32_t file_offset = get32(buf + 42);
      if(file_offset == 0xFFFFFFFF) {
        // another sign of a Zip64 archive... technically
        throw sn.Get("GAME_ARCHIVE_ZIP_TOO_ADVANCED"_Key);
      }
      filename_buf.resize(filename_length);
      in->read(filename_buf.data(), filename_length);
      in->seekg(to_skip, std::ios_base::cur);
      if(!filename_buf.empty() && *filename_buf.rbegin() == '/') {
        // It's a directory, skip it
      }
      else {
        std::string filename(filename_buf.begin(), filename_buf.end());
        if(filename == "manifest.bml") {
          if(found_manifest_prefix)
            throw sn.Get("GAME_ARCHIVE_MULTIPLE_MANIFESTS"_Key);
          found_manifest_prefix = true;
          assert(manifest_prefix.empty());
        }
        else if(filename.length() > 12 // strlen("manifest.bml")
                && std::equal(filename.begin() + filename.length() - 13,
                              filename.end(),
                              "/manifest.bml")) {
          if(found_manifest_prefix)
            throw sn.Get("GAME_ARCHIVE_MULTIPLE_MANIFESTS"_Key);
          found_manifest_prefix = true;
          // manifest_prefix := the path up to and including the final /
          manifest_prefix.assign(filename.begin(),
                                 filename.begin() + filename.length() - 12);
        }
        // if we've already found the manifest prefix, don't bother adding any
        // files not inside that directory to our list
        if(!found_manifest_prefix
           || filename.length() > manifest_prefix.length()
           || std::equal(manifest_prefix.begin(), manifest_prefix.end(),
                         filename.begin()))
          files[filename] = ReadyToDecodeFile(in, file_offset,
                                              path+':'+filename);
      }
    }
    if(!found_manifest_prefix)
      throw sn.Get("GAME_ARCHIVE_NO_MANIFEST"_Key);
    // remove any non-manifest-prefix files that may have slipped in
    auto it = files.begin();
    while(it != files.end()) {
      auto dis = it++;
      if(dis->first.length() <= manifest_prefix.length()
         || !std::equal(manifest_prefix.begin(), manifest_prefix.end(),
                        dis->first.begin()))
         files.erase(dis);
    }
    // time to load the manifest!
    size_t manifest_size;
    std::unique_ptr<uint8_t[]> manifest_data
      = files[manifest_prefix+"manifest.bml"].Load(manifest_size);
    class bufreader : public byuuML::reader {
      const char* data;
      size_t size;
    public:
      bufreader(const char* data, size_t size)
        : data(data), size(size) {}
      void read_more(const char*& begin, const char*& end) override {
        begin = data;
        end = data + size;
        data = nullptr;
        size = 0;
      }
    } reader(reinterpret_cast<const char*>(manifest_data.get()),manifest_size);
    try {
      std::unique_ptr<byuuML::document> manifest
        = std::make_unique<byuuML::document>(reader);
      return std::make_unique<GameArchive>(files, path, manifest_prefix,
                                           std::move(manifest));
    }
    catch(std::string error) {
      std::string message = sn.Get("MANIFEST_PARSE_ERROR"_Key, {error});
      die("%s", message.c_str());
    }
  }
  catch(std::system_error& e) {
    std::string msg = sn.Get("GAME_CONTENT_READ_ERROR"_Key,
                             {path, e.code().message()});
    die("%s", msg.c_str());
  }
  return nullptr;
}
