#ifndef DISALLOW_FLOPPY

#include "floppy.hh"
#include "expansions.hh"
#include "cpu.hh"
#include "apu.hh"

#include <boost/filesystem.hpp>
#include <array>

using namespace ARS;
using namespace boost::filesystem;
using boost::system::error_code;
using boost::filesystem::fstream;

bool ARS::fast_floppy_mode = false;

namespace {
  static const int SECTOR_SIZE = 256;
  static const int SECTORS_PER_TRACK = 36; // made up
  static const uint32_t READ_DELAY = 1;
  static const uint32_t WRITE_DELAY = 1;
  static const uint32_t SHORT_SEEK_DELAY = 1;
  static const uint32_t LONG_SEEK_DELAY = 23;
  static const uint32_t MOVE_DELAY = 30; // arbitrary
  static const uint32_t DELETE_DELAY = 30; // arbitrary
  static const uint32_t LIST_DELAY = 72; // arbitrary
  static const uint32_t CHECK_DELAY = 600; // arbitrary, probably too short
  static const uint32_t FORMAT_DELAY = 1200; // arbitrary, probably too short
  static const unsigned int SOUND_DELAY_PER_SHORT_SEEK = 2;
  static const unsigned int SOUND_DELAY_PER_LONG_SEEK = 24;
  static const uintmax_t MAX_FILE_SIZE = 16777216 - SECTOR_SIZE;
  static const int MAX_HANDLES = 10;
  static const uint8_t FIRST_HANDLE = '0';
  // well this function sucks. I mean, the whole file sucks, but this function
  // is the worst.
  bool unfold_filename(std::string in_filename, uint8_t dos_filename[11]) {
    auto inp = in_filename.cbegin();
    int i = 0;
    while(i < 8 && inp != in_filename.cend()) {
      if(*inp >= 'a' && *inp <= 'z') {
        dos_filename[i++] = *inp++ & ~0x20;
      }
      else if((*inp >= '0' && *inp <= '9') || *inp == '_') {
        dos_filename[i++] = *inp++;
      }
      else if(*inp == '.') {
        break;
      }
      else return false;
    }
    if(i == 0) return false; // empty filename not allowed
    if(inp != in_filename.cend() && *inp != '.') return false; // too long
    ++inp;
    while(i < 8) dos_filename[i++] = ' ';
    while(i < 11 && inp != in_filename.cend()) {
      if(*inp >= 'a' && *inp <= 'z') {
        dos_filename[i++] = *inp++ & ~0x20;
      }
      else if((*inp >= '0' && *inp <= '9') || *inp == '_') {
        dos_filename[i++] = *inp++;
      }
      else return false;
    }
    while(i < 11) dos_filename[i++] = ' ';
    return inp == in_filename.cend();
  }
  class MountedDrive;
  class Handle {
    fstream file;
    path file_path;
    std::string name;
    MountedDrive* drive;
    bool write_allowed;
  public:
    Handle(const path& in_path, std::string name, MountedDrive* drive,
           fstream::openmode mode)
      : file(in_path, mode), file_path(in_path), name(name), drive(drive),
        write_allowed((mode & fstream::out) != 0) {}
    fstream& get_file() { return file; }
    MountedDrive* get_drive() { return drive; }
    const path& get_path() { return file_path; }
    bool trunc_to(uint32_t new_size) {
      if(!write_allowed) return false;
      error_code ec;
      resize_file(file_path, new_size, ec);
      if(ec) return false;
      if(new_size == 0) {
        file.seekg(0);
      }
      else {
        uint32_t extra_bytes = new_size % SECTOR_SIZE;
        if(extra_bytes == 0) extra_bytes = SECTOR_SIZE;
        file.seekg(new_size - extra_bytes);
      }
      return file.good();
    }
    bool get_pos(uint32_t& pos) {
      auto cur_pos = file.tellg();
      if(cur_pos == fstream::pos_type(-1)) return false;
      if(cur_pos % SECTOR_SIZE != 0)
        die("Internal error: floppy \"read head\" slipped!");
      if(cur_pos >= SECTOR_SIZE * 65536) return false;
      pos = cur_pos / SECTOR_SIZE;
      return true;
    }
    bool get_len(uint32_t& len) {
      auto cur_pos = file.tellg();
      if(cur_pos == fstream::pos_type(-1)) return false;
      if(!file.seekg(0, fstream::end)) return false;
      auto end_pos = file.tellg();
      if(end_pos == fstream::pos_type(-1)) return false;
      if(!file.seekg(cur_pos)) return false;
      if(end_pos > 16777215) return false;
      len = end_pos;
      return true;
    }
    void seek_sound(uint32_t track);
    bool sector_read(uint8_t buffer[SECTOR_SIZE]) {
      uint8_t* outp = buffer;
      int rem = SECTOR_SIZE;
      seek_sound((uint32_t)(file.tellg() / (SECTOR_SIZE * SECTORS_PER_TRACK)));
      while(rem > 0) {
        file.read(reinterpret_cast<char*>(outp), rem);
        int red = file.gcount();
        if(red == rem) break;
        else if(red >= 0) {
          rem -= red;
          outp += red;
          if(file.eof()) {
            // fill the rest of the "sector" with garbage
            memset(outp, 0xA5, rem);
            // re-seat the "file pointer"
            return reseat();
          }
          else if(file.bad() || file.fail()) {
            // an error occurred
            // re-seat the "file pointer" before returning, in case it's
            // fixable, but ignore whether it succeeded in reseating
            reseat();
            return false;
          }
        }
        else {
          // go again. we assume that a system call was interrupted or
          // something, and that eventually either badbit or failbit or eofbit
          // will be set.
        }
      }
      return true;
    }
    bool sector_write(const uint8_t buffer[SECTOR_SIZE]) {
      int count;
      auto old_pos = file.tellp();
      if(old_pos == fstream::pos_type(-1)) return false;
      seek_sound((uint32_t)(old_pos / (SECTOR_SIZE*SECTORS_PER_TRACK)));
      if(!file.seekp(0, fstream::end)) {
        reseat();
        return false;
      }
      auto end_pos = file.tellp();
      if(end_pos == fstream::pos_type(-1)) return false;
      if(end_pos - old_pos >= SECTOR_SIZE) {
        count = SECTOR_SIZE;
      }
      else {
        assert(end_pos >= old_pos);
        count = end_pos - old_pos;
      }
      if(!file.seekp(old_pos)) {
        reseat();
        return false;
      }
      file.write(reinterpret_cast<const char*>(buffer), count);
      if(file) file.flush();
      if(!file) {
        file.seekp(old_pos);
        return false;
      }
      if(count < SECTOR_SIZE) return reseat();
      else return true;
    }
    bool reseat() {
      // since it's an fstream, the get and put positions are the same. we only
      // need to reseat the get position to also affect the put position. also,
      // a read operation affects the put position and a write operation
      // affects the get position. no special handling needed of the separate
      // pointers "feature".
      file.clear();
      auto cur_pos = file.tellg();
      if(cur_pos == fstream::pos_type(-1)) return false;
      auto unseated_by = cur_pos % SECTOR_SIZE;
      if(unseated_by != 0) {
        auto new_pos = cur_pos - unseated_by;
        file.seekg(new_pos);
      }
      return file.good();
    }
  };
  class MountedDrive {
    bool write_allowed, format_needed, long_seek_polarity = false;
    unsigned int index;
    path base_path;
    std::string last_sought_file;
    uint32_t last_sought_track = 0;
    uint32_t extra_seek_time = 0;
  public:
    void wipe_last_sought() {
      last_sought_file.clear();
      last_sought_track = 0;
      extra_seek_time = 0;
    }
    uint32_t consume_seek_time() {
      uint32_t ret = extra_seek_time;
      extra_seek_time = 0;
      return ret;
    }
    void seek_to(const std::string& sought_file, uint32_t sought_track) {
      if(sought_file != last_sought_file
         || sought_track + 1 < last_sought_track
         || sought_track > last_sought_track + 1) {
        long_seek_sound();
        extra_seek_time += LONG_SEEK_DELAY;
      }
      else if(sought_track != last_sought_track) {
        short_seek_sound();
        extra_seek_time += SHORT_SEEK_DELAY;
      }
      if(sought_file != last_sought_file) {
        last_sought_file = sought_file;
      }
      last_sought_track = sought_track;
    }
    void short_seek_sound(unsigned int delay = SOUND_DELAY_PER_SHORT_SEEK) {
      queue_floppy_sound(index, FloppySoundType::SHORT_SEEK, delay);
    }
    void long_seek_sound(unsigned int delay = SOUND_DELAY_PER_LONG_SEEK) {
      queue_floppy_sound(index, long_seek_polarity
                         ? FloppySoundType::LONG_SEEK_1
                         : FloppySoundType::LONG_SEEK_2, delay);
      long_seek_polarity = !long_seek_polarity;
    }
    void fake_activity(unsigned int rem) {
      wipe_last_sought();
      if(rem >= SOUND_DELAY_PER_LONG_SEEK * 3) {
        long_seek_sound();
        rem -= SOUND_DELAY_PER_LONG_SEEK;
      }
      while(rem >= SECTORS_PER_TRACK * READ_DELAY + SOUND_DELAY_PER_LONG_SEEK * 2) {
        short_seek_sound(SECTORS_PER_TRACK * READ_DELAY);
        rem -= SECTORS_PER_TRACK * READ_DELAY;
      }
      while(rem >= SOUND_DELAY_PER_LONG_SEEK) {
        long_seek_sound();
        rem -= SOUND_DELAY_PER_LONG_SEEK;
      }
    }
    bool is_write_allowed() {
      return write_allowed;
    }
    bool is_format_needed() {
      return format_needed;
    }
    bool valid_path() {
      return !base_path.empty();
    }
    bool pretend_format() {
      if(valid_path()) {
        format_needed = false;
        return true;
      }
      else return false;
    }
    void list_files(std::vector<uint8_t>& response_buf) {
      try {
        std::vector<std::array<uint8_t, 14>> result;
        for(auto&& el : directory_iterator(base_path)) {
          auto path = el.path();
          std::array<uint8_t, 14> stat_record;
          if(!unfold_filename(path.filename().string(), &stat_record[0]))
            continue;
          if(!is_regular_file(el.status())) continue;
          auto size = file_size(path);
          if(size == uintmax_t(-1) || size > MAX_FILE_SIZE) continue;
          stat_record[11] = static_cast<uint8_t>(size >> 16);
          stat_record[12] = static_cast<uint8_t>(size >> 8);
          stat_record[13] = static_cast<uint8_t>(size);
          result.push_back(stat_record);
        }
        std::sort(result.begin(), result.end());
        response_buf.reserve(3 + 14 * result.size());
        auto si = space(base_path);
        auto avail_sectors = si.available / SECTOR_SIZE;
        if(avail_sectors > 65535) avail_sectors = 65535;
        response_buf.push_back('O');
        response_buf.push_back((avail_sectors >> 8) & 255);
        response_buf.push_back(avail_sectors & 255);
        for(auto&& el : result) {
          response_buf.insert(response_buf.cend(), el.begin(), el.end());
        }
      }
      catch(const filesystem_error& e) {
        response_buf.push_back('E');
      }
    }
    void delete_file(std::vector<uint8_t>& response_buf,
                     const std::string& name) {
      error_code ec;
      auto path = base_path;
      path /= name;
      if(!is_regular_file(path)) {
        response_buf.push_back('E');
        return;
      }
      remove(path, ec);
      if(ec) {
        response_buf.push_back('E');
      }
      else {
        response_buf.push_back('O');
      }
    }
    void rename_file(std::vector<uint8_t>& response_buf,
                     const std::string& from_name,
                     const std::string& to_name) {
      error_code ec;
      auto from_path = base_path;
      from_path /= from_name;
      if(!is_regular_file(from_path)) {
        response_buf.push_back('E');
        return;
      }
      auto to_path = base_path;
      to_path /= to_name;
      if(exists(to_path) && !is_regular_file(to_path)) {
        response_buf.push_back('E');
        return;
      }
      remove(to_path, ec);
      if(ec) {
        response_buf.push_back('E');
        return;
      }
      rename(from_path, to_path, ec);
      if(ec) {
        response_buf.push_back('E');
        return;
      }
      response_buf.push_back('O');
    }
    std::unique_ptr<Handle> open_file(const std::string& name) {
      error_code ec;
      auto path = base_path;
      path /= name;
      if(exists(path) && !is_regular_file(path)) {
        return NULL;
      }
      if(write_allowed)
        return std::make_unique<Handle>(path, name, this, fstream::in
                                        | fstream::binary | fstream::out);
      else
        return std::make_unique<Handle>(path, name, this,
                                        fstream::in | fstream::binary);
    }
    std::unique_ptr<Handle> create_file(const std::string& name) {
      error_code ec;
      auto path = base_path;
      path /= name;
      if(exists(path)) {
        return NULL;
      }
      if(write_allowed)
        return std::make_unique<Handle>(path, name, this,
                                        fstream::in | fstream::binary
                                        | fstream::out | fstream::trunc);
      else
        return NULL;
    }
    MountedDrive(bool write_allowed, const std::string& base_path,
                 unsigned int index, bool format_needed = false)
      : write_allowed(write_allowed), format_needed(format_needed),
        index(index), base_path(base_path) {}
  };
  void Handle::seek_sound(uint32_t track) {
    drive->seek_to(name, track);
  }
  std::unique_ptr<MountedDrive> drive_a, drive_b;
  std::unique_ptr<Handle> handles[MAX_HANDLES];
  MountedDrive* get_mounted_drive(uint8_t letter) {
    switch(letter) {
    case 'A': return drive_a.get();
    case 'B': return drive_b.get();
    default: return NULL;
    }
  }
  class FloppyPort : public ARS::Expansion {
    uint8_t command_buf[24];
    uint16_t command_buf_pos = 0;
    uint8_t io_buf[SECTOR_SIZE];
    uint16_t io_buf_pos = 0;
    std::vector<uint8_t> response_buf;
    uint16_t response_buf_pos = 0;
    bool escaped_read = false;
    bool will_error = false;
    uint32_t delay_response = 0;
    bool writing = false;
    bool check_valid_drive(uint8_t letter) {
      auto ret = letter == 'A' || letter == 'B';
      if(!ret) error();
      return ret;
    }
    bool check_mounted_drive(MountedDrive*& drive, uint8_t letter) {
      if(!check_valid_drive(letter)) return false;
      drive = get_mounted_drive(letter);
      if(!drive || !drive->valid_path() || drive->is_format_needed()) {
        error();
        return false;
      }
      else return true;
    }
    bool check_valid_handle(std::unique_ptr<Handle>*& out, uint8_t handle) {
      if(handle >= FIRST_HANDLE && handle < (FIRST_HANDLE + MAX_HANDLES)) {
        out = &handles[handle - FIRST_HANDLE];
        return true;
      }
      else return false;
    }
    bool check_open_handle(std::unique_ptr<Handle>*& out, uint8_t handle) {
      if(!check_valid_handle(out, handle)) return false;
      if(!*out || !(*out)->get_file().is_open()) { error(); return false; }
      else return true;
    }
    bool check_valid_filename_char(uint8_t ch) {
      auto ret = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
        || ch == '_';
      if(!ret) error();
      return ret;
    }
    uint8_t fold_filename_char(uint8_t ch) {
      if(ch >= 'A' && ch <= 'Z') return ch | 0x20;
      else return ch;
    }
    bool check_filename(std::string& out, uint16_t start_pos) {
      uint16_t name_end_pos = 8;
      while(name_end_pos > 0 && command_buf[start_pos + name_end_pos - 1] == ' ')
        --name_end_pos;
      if(name_end_pos == 0) {
        error();
        return false;
      }
      out.clear();
      for(uint16_t i = 0; i < name_end_pos; ++i) {
        if(!check_valid_filename_char(command_buf[start_pos + i]))
          return false;
        out.push_back(fold_filename_char(command_buf[start_pos + i]));
      }
      name_end_pos = 11;
      while(name_end_pos > 8 && command_buf[start_pos + name_end_pos - 1] == ' ')
        --name_end_pos;
      if(name_end_pos > 8) {
        out.push_back('.');
        for(uint16_t i = 8; i < name_end_pos; ++i) {
          if(!check_valid_filename_char(command_buf[start_pos + i]))
            return false;
          out.push_back(fold_filename_char(command_buf[start_pos + i]));
        }
      }
      return true;
    }
    void error() {
      response_buf.push_back('E');
    }
    void ok() {
      response_buf.push_back('O');
    }
    bool check_buf_len(uint16_t min_len) {
      if(command_buf_pos < min_len) {
        error();
        return false;
      }
      else return true;
    }
    void fluff_response_buf() {
      int i = response_buf.size();
      while(i > 0) {
        --i;
        if(response_buf[i] == 0) {
          response_buf[i] = 1;
          response_buf.insert(response_buf.cbegin()+i, 0xFF);
        }
        else if(response_buf[i] == 0xFF) {
          response_buf.insert(response_buf.cbegin()+i, 0xFF);
        }
      }
    }
    void execute_command() {
      MountedDrive* cur_drive = NULL;
      std::unique_ptr<Handle>* cur_handle = NULL;
      uint32_t new_value;
      std::string name_a, name_b;
      std::unique_ptr<Handle> new_handle;
      int n;
      // command_buf_pos is known to be ≥1
      switch(command_buf[0]) {
      default:
        error();
        break;
      case 's': // Get drive status
        if(!check_buf_len(2)) break;
        if(!check_valid_drive(command_buf[1])) break;
        cur_drive = get_mounted_drive(command_buf[1]);
        ok();
        if(cur_drive) {
          response_buf.push_back(cur_drive->is_format_needed() ? 'X'
                                 : cur_drive->is_write_allowed() ? '1' : '2');
        }
        else {
          response_buf.push_back('0');
        }
        break;
      case 'F': // Format a drive (will not implement)
        if(!check_buf_len(2)) break;
        if(!check_valid_drive(command_buf[1])) break;
        cur_drive = get_mounted_drive(command_buf[1]);
        if(!cur_drive) {
          error();
          break;
        }
        if(!cur_drive->pretend_format()) {
          error();
          break;
        }
        delay_response = FORMAT_DELAY;
        cur_drive->fake_activity(FORMAT_DELAY);
        ok();
        break;
      case 'K': // Check disk
        if(!check_buf_len(2)) break;
        if(!check_mounted_drive(cur_drive, command_buf[1])) break;
        delay_response = CHECK_DELAY;
        cur_drive->fake_activity(CHECK_DELAY);
        ok();
        break;
        //case 'c': // Clone a disk (will not implement)
      case 'M': // Move a file
        if(!check_buf_len(24)) break;
        if(!check_mounted_drive(cur_drive, command_buf[1])) break;
        if(!check_filename(name_a, 2)) break;
        if(!check_filename(name_b, 13)) break;
        if(!cur_drive->is_write_allowed()) { error(); break; }
        cur_drive->rename_file(response_buf, name_a, name_b);
        delay_response = MOVE_DELAY;
        cur_drive->fake_activity(MOVE_DELAY);
        break;
      case 'D': // Delete a file
        if(!check_buf_len(13)) break;
        if(!check_mounted_drive(cur_drive, command_buf[1])) break;
        if(!check_filename(name_a, 2)) break;
        if(!cur_drive->is_write_allowed()) { error(); break; }
        cur_drive->delete_file(response_buf, name_a);
        delay_response = DELETE_DELAY;
        cur_drive->fake_activity(DELETE_DELAY);
        break;
      case 'l': // File listing
        if(!check_buf_len(2)) break;
        if(!check_mounted_drive(cur_drive, command_buf[1])) break;
        cur_drive->list_files(response_buf);
        delay_response = LIST_DELAY;
        cur_drive->fake_activity(LIST_DELAY);
        break;
      case '(': // Open existing file
        if(!check_buf_len(13)) break;
        if(!check_mounted_drive(cur_drive, command_buf[1])) break;
        if(!check_filename(name_a, 2)) break;
        for(n = 0; n < MAX_HANDLES; ++n) {
          if(handles[n] == NULL) break;
        }
        if(n >= MAX_HANDLES) {
          error();
          break;
        }
        handles[n] = cur_drive->open_file(name_a);
        if(handles[n] && !handles[n]->get_file().good()) {
          handles[n] = NULL;
        }
        if(handles[n]) {
          ok();
          response_buf.push_back(FIRST_HANDLE + n);
        }
        else error();
        break;
      case 'C': // Create new file
        if(!check_buf_len(13)) break;
        if(!check_mounted_drive(cur_drive, command_buf[1])) break;
        if(!check_filename(name_a, 2)) break;
        for(n = 0; n < MAX_HANDLES; ++n) {
          if(handles[n] == NULL) break;
        }
        if(n >= MAX_HANDLES) {
          error();
          break;
        }
        handles[n] = cur_drive->create_file(name_a);
        if(handles[n] && !handles[n]->get_file().good()) {
          handles[n] = NULL;
        }
        if(handles[n]) {
          ok();
          response_buf.push_back(FIRST_HANDLE + n);
        }
        else error();
        break;
      case ')': // Close file
        if(!check_buf_len(2)) break;
        if(!check_valid_handle(cur_handle, command_buf[1])) break;
        // whether there was a valid open handle in this slot before or not,
        // make it so there isn't one.
        *cur_handle = NULL;
        ok();
        break;
      case 'R': // Read next sector of file
        if(!check_buf_len(2)) break;
        if(!check_open_handle(cur_handle, command_buf[1])) break;
        if((*cur_handle)->sector_read(io_buf)) {
          response_buf.reserve(SECTOR_SIZE+1);
          ok();
          response_buf.insert(response_buf.cend(),
                              io_buf, io_buf + 256);
          delay_response = READ_DELAY;
          delay_response += (*cur_handle)->get_drive()->consume_seek_time();
        }
        else {
          error();
        }
        break;
      case 'W': // Write next sector of file
        if(!check_buf_len(2)) break;
        if(!check_open_handle(cur_handle, command_buf[1])) break;
        writing = true;
        // write_sector, below, will be called when the written data is ready
        return; // NOT break!
      case 'T': // Set file size (Truncate)
        if(!check_buf_len(5)) break;
        if(!check_open_handle(cur_handle, command_buf[1])) break;
        new_value = (static_cast<uint32_t>(command_buf[2]) << 16)
          | (static_cast<uint32_t>(command_buf[3]) << 8)
          | static_cast<uint32_t>(command_buf[4]);
        if((*cur_handle)->trunc_to(new_value))
          ok();
        else
          error();
        break;
      case 'S': // Seek in file
        if(!check_buf_len(4)) break;
        if(!check_open_handle(cur_handle, command_buf[1])) break;
        new_value = (static_cast<uint32_t>(command_buf[2]) << 8)
          | static_cast<uint32_t>(command_buf[3]);
        (*cur_handle)->get_file().seekg(new_value * SECTOR_SIZE);
        if((*cur_handle)->get_file().good()) ok();
        else error();
        break;
      case 'L': // Get size (Length) of file
        if(!check_buf_len(2)) break;
        if(!check_valid_handle(cur_handle, command_buf[1])) break;
        if((*cur_handle)->get_len(new_value)) {
          ok();
          response_buf.push_back((new_value>>16)&255);
          response_buf.push_back((new_value>>8)&255);
          response_buf.push_back(new_value&255);
        }
        else {
          error();
        }
        break;
      case 'P': // Get position in file
        if(!check_buf_len(2)) break;
        if(!check_valid_handle(cur_handle, command_buf[1])) break;
        if((*cur_handle)->get_pos(new_value)) {
          ok();
          response_buf.push_back((new_value>>8)&255);
          response_buf.push_back(new_value&255);
        }
        else {
          error();
        }
        break;
      case 'Z': // DOS sends this on shutdown
        if(!check_buf_len(1)) break;
        for(auto&& h : handles) {
          h = NULL;
        }
        ok();
        break;
      }
      fluff_response_buf();
      if(response_buf.empty()) {
        std::cerr << "Internal warning: floppy command '"<<command_buf[0]<<"' gave no response\n";
        response_buf.push_back('E');
      }
    }
    void write_sector() {
      std::unique_ptr<Handle>* cur_handle;
      /* these should not apply, but let's be safe anyway */
      if(!check_buf_len(2)) return;
      if(!check_open_handle(cur_handle, command_buf[1])) return;
      if((*cur_handle)->sector_write(io_buf)) {
        ok();
        delay_response = WRITE_DELAY;
        delay_response += (*cur_handle)->get_drive()->consume_seek_time();
      }
      else {
        error();
      }
      // there is no need to fluff the response buf because the reply will only
      // be a single O or E
    }
    void output_write(uint8_t value) {
      if(value == 0) {
        writing = false;
        escaped_read = false;
        will_error = false;
        command_buf_pos = 0;
        io_buf_pos = 0;
      }
      else if(escaped_read) {
        if(value == 0x0A) {
          io_buf[io_buf_pos++] = 0x0A;
        }
        else if(value < 0x80) {
          io_buf[io_buf_pos++] = 0x00;
        }
        else {
          io_buf[io_buf_pos++] = 0xFF;
        }
        escaped_read = false;
      }
      else {
        if(io_buf_pos == sizeof(io_buf)) {
          // NOTREACHED
          will_error = true;
        }
        else if(value == 0xFF) {
          escaped_read = true;
        }
        else {
          io_buf[io_buf_pos++] = value;
        }
      }
      if(io_buf_pos == SECTOR_SIZE) {
        if(will_error) {
          error();
        }
        else {
          write_sector();
        }
        writing = false;
        command_buf_pos = 0;
        io_buf_pos = 0;
      }
    }
    void output_cmd(uint8_t value) {
      if(response_buf.size() > 0) {
        cpu->setSO(true);
        cpu->setSO(false);
      }
      else if(value == 0) {
        // abort any in-progress command line
        command_buf_pos = 0;
        escaped_read = false;
        will_error = false;
      }
      else if(escaped_read) {
        if(command_buf_pos == sizeof(command_buf)) {
          will_error = true;
        }
        else if(value == 0x0A) {
          command_buf[command_buf_pos++] = 0x0A;
        }
        else if(value < 0x80) {
          command_buf[command_buf_pos++] = 0x00;
        }
        else {
          command_buf[command_buf_pos++] = 0xFF;
        }
        escaped_read = false;
      }
      else {
        if(value == '\n') {
          if(will_error) {
            response_buf.push_back('E');
          }
          else if(command_buf_pos != 0) {
            execute_command();
            if(command_buf_pos != 2 || command_buf[0] != 'W')
              command_buf_pos = 0;
          }
        }
        else if(command_buf_pos == sizeof(command_buf)) {
          will_error = true;
        }
        else if(value == 0xFF) {
          escaped_read = true;
        }
        else {
          command_buf[command_buf_pos++] = value;
          if(command_buf_pos == 1 && value == 'Q') {
            // hack to make DOS work
            execute_command();
            command_buf_pos = 0;
          }
        }
      }
    }
    void output(uint8_t value) override {
      if(writing) output_write(value);
      else output_cmd(value);
    }
    uint8_t input() override {
      if((!fast_floppy_mode && delay_response > 0) || command_buf_pos > 0) {
        cpu->setSO(true);
        cpu->setSO(false);
        return 0xFF;
      }
      else if(response_buf_pos < response_buf.size()) {
        auto ret = response_buf[response_buf_pos++];
        if(response_buf_pos == response_buf.size()) {
          response_buf.clear();
          response_buf_pos = 0;
        }
        return ret;
      }
      else {
        return 0x00;
      }
    }
    void on_frame() override {
      if(!fast_floppy_mode && delay_response > 0) --delay_response;
    }
  };
}

std::unique_ptr<Expansion> ARS::make_floppy_expansion() {
  setup_floppy_sounds();
  return std::make_unique<FloppyPort>();
}

bool ARS::mount_floppy(const std::string& arg) {
  {
    std::unique_ptr<MountedDrive>* which_drive;
    unsigned int which_index;
    if(arg.length() < 3) goto invalid_floppy_spec;
    switch(arg[0]) {
    case 'A': which_drive = &drive_a; which_index = 0; break;
    case 'B': which_drive = &drive_b; which_index = 1; break;
    default: goto invalid_floppy_spec;
    }
    bool write_allowed, format_needed = false;
    switch(arg[1]) {
    case '-': write_allowed = false; break;
    case '+': write_allowed = true; break;
    case 'x': write_allowed = true; format_needed = true; break;
    default: goto invalid_floppy_spec;
    }
    std::string path = arg.substr(2);
    *which_drive = std::make_unique<MountedDrive>(write_allowed, path,
                                                  which_index, format_needed);
    return true;
  }
 invalid_floppy_spec:
  sn.Out(std::cout, "INVALID_FLOPPY_SPEC_ERROR"_Key);
  return false;
}

#endif
