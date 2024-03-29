#include "fx.hh"
#include "teg.hh"

#include <iomanip>
#include <chrono>
#include <algorithm>

SN::Context sn;

namespace {
  ARS::PPU::raw_screen raw;
  uint8_t buf1[ARS::PPU::TOTAL_SCREEN_WIDTH*ARS::PPU::TOTAL_SCREEN_HEIGHT
               *4*4];
  uint8_t buf2[ARS::PPU::TOTAL_SCREEN_WIDTH*ARS::PPU::TOTAL_SCREEN_HEIGHT
               *4*4];
  struct test {
    const std::string name;
    const std::function<void()> func;
    bool enabled = true;
  };
  test tests[] = {
    {"raw_screen_to_bgra", []() {
        FX::raw_screen_to_bgra(raw, buf1);
      }},
    {"raw_screen_to_bgra_skip", []() {
        FX::raw_screen_to_bgra(raw, buf1, 0, 0, ARS::PPU::TOTAL_SCREEN_WIDTH, ARS::PPU::TOTAL_SCREEN_HEIGHT, true);
      }},
    {"raw_screen_to_bgra_overscan", []() {
        FX::raw_screen_to_bgra(raw, buf1,
                               ARS::PPU::CONVENIENT_OVERSCAN_LEFT,
                               ARS::PPU::CONVENIENT_OVERSCAN_TOP,
                               ARS::PPU::CONVENIENT_OVERSCAN_RIGHT,
                               ARS::PPU::CONVENIENT_OVERSCAN_BOTTOM);
      }},
    {"raw_screen_to_bgra_2x", []() {
        FX::raw_screen_to_bgra_2x(raw, buf1);
      }},
    {"raw_screen_to_bgra_2x_skip", []() {
        FX::raw_screen_to_bgra_2x(raw, buf1, 0, 0, ARS::PPU::TOTAL_SCREEN_WIDTH, ARS::PPU::TOTAL_SCREEN_HEIGHT, true);
      }},
    {"composite_bgra", []() {
        FX::composite_bgra(buf1, buf2,
                           ARS::PPU::CONVENIENT_OVERSCAN_WIDTH,
                           ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT,
                           false);
      }},
    {"composite_bgra_skip", []() {
        FX::composite_bgra(buf1, buf2,
                           ARS::PPU::CONVENIENT_OVERSCAN_WIDTH,
                           ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT,
                           true);
      }},
    {"svideo_bgra", []() {
        FX::svideo_bgra(buf1, buf2,
                        ARS::PPU::CONVENIENT_OVERSCAN_WIDTH,
                        ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT,
                        false);
      }},
    {"svideo_bgra_skip", []() {
        FX::svideo_bgra(buf1, buf2,
                        ARS::PPU::CONVENIENT_OVERSCAN_WIDTH,
                        ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT,
                        true);
      }},
    {"scanline_crisp_bgra", []() {
        FX::scanline_crisp_bgra(buf2,
                                ARS::PPU::CONVENIENT_OVERSCAN_WIDTH,
                                ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT);
      }},
    {"scanline_bright_bgra", []() {
        FX::scanline_bright_bgra(buf2,
                                 ARS::PPU::CONVENIENT_OVERSCAN_WIDTH,
                                 ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT);
      }},
  };
  constexpr unsigned int DEFAULT_ITERATION_COUNT = 10;
  unsigned int iteration_count = DEFAULT_ITERATION_COUNT;
  void print_usage() {
    std::cout << "Usage: fxbench [options] [testnames]\n"
      "Options:\n"
      "-L: List all known tests\n"
      "-i: Number of iterations for each test, default is "<<DEFAULT_ITERATION_COUNT<<"\n";
  }
  void print_tests() {
    for(auto& test : tests) {
      std::cout << test.name << "\n";
    }
  }
  bool parse_command_line(int argc, const char** argv) {
    int n = 1;
    bool noMoreOptions = false;
    bool valid = true;
    bool any_tests_specified = false;
    while(n < argc) {
      const char* arg = argv[n++];
      if(!strcmp(arg, "--")) noMoreOptions = true;
      else if(!noMoreOptions && arg[0] == '-') {
        ++arg;
        while(*arg) {
          switch(*arg++) {
          case '?': print_usage(); return false;
          case 'L': print_tests(); return false;
          case 'i':
            if(n >= argc) {
              sn.Out(std::cout, "MISSING_COMMAND_LINE_ARGUMENT"_Key, {"-i"});
              valid = false;
            }
            else {
              iteration_count = std::stoul(argv[n++]);
              if(iteration_count > 10000) iteration_count = 10000;
              else if(iteration_count < 1) iteration_count = 1;
            }
            break;
          default:
            sn.Out(std::cerr, "UNKNOWN_OPTION"_Key, {std::string(arg-1,1)});
            valid = false;
          }
        }
      }
      else {
        if(!any_tests_specified) {
          any_tests_specified = true;
          for(auto& test : tests) {
            test.enabled = false;
          }
        }
        bool found = false;
        for(auto& test : tests) {
          if(test.name == arg) {
            found = true;
            test.enabled = true;
            break;
          }
        }
        if(!found) {
          std::cerr << "Unknown test name: " << arg << "\n";
        }
      }
    }
    if(!valid) {
      print_usage();
      return false;
    }
    return true;
  }
}

extern "C" int teg_main(int argc, char** argv) {
  if(!parse_command_line(argc, const_cast<const char**>(argv))) return 1;
  if(SDL_Init(0)) return 1;
  atexit(SDL_Quit);
  FX::init();
  for(unsigned int y = 0; y < ARS::PPU::TOTAL_SCREEN_HEIGHT; ++y) {
    for(unsigned int x = 0; x < ARS::PPU::TOTAL_SCREEN_WIDTH; ++x) {
      raw[y][x] = rand();
    }
  }
  std::cout << "[";
  std::vector<double> execution_times(iteration_count);
  bool first_test = true;
  for(auto& test : tests) {
    // dry run to heat up instruction caches and branch predictor
    test.func();
    for(unsigned int i = 0; i < iteration_count; ++i) {
      auto begin_time = std::chrono::high_resolution_clock::now();
      test.func();
      auto end_time = std::chrono::high_resolution_clock::now();
      execution_times[i] =
        std::chrono::duration_cast<std::chrono::duration<double,
                                                         std::ratio<1,1>>>
        (end_time - begin_time).count();
    }
    std::sort(execution_times.begin(), execution_times.end());
    if(!first_test) std::cout << ",";
    else first_test = false;
    std::cout << "\n{\"name\":\"" << test.name << "\"";
    // minimum time
    std::cout << ",\"min\":" << std::fixed << std::setprecision(6)
              << execution_times[0];
    // average time
    double total = 0;
    for(auto time : execution_times) total += time;
    std::cout << ",\"mean\":" << std::fixed << std::setprecision(6)
              << (total/iteration_count);
    // median
    if(iteration_count % 2 == 0) {
      std::cout << ",\"median\":" << std::fixed << std::setprecision(6)
                << (execution_times[iteration_count/2-1]
                    +execution_times[iteration_count/2])/2;
    }
    else {
      std::cout << ",\"median\":" << std::fixed << std::setprecision(6)
                << execution_times[iteration_count/2];
    }
    // maximum time
    std::cout << ",\"max\":" << std::fixed << std::setprecision(6)
              << execution_times[iteration_count-1];
    std::cout << "}";
  }
  std::cout << "\n]\n";
  return 0;
}
