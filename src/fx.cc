#include "optimize-this-file.hh"

#include "fxinternal.hh"

#include <assert.h>

#include <cmath>

#include "fxtables.hh"

using namespace FX;

namespace {
  class Worker {
    Worker(const Worker&) = delete;
    Worker(Worker&&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker& operator=(Worker&&) = delete;
    std::function<void(unsigned int, unsigned int)> task = nullptr;
    unsigned int start, stop;
    static Worker* worker_threads;
    static unsigned int thread_count;
    static SDL_threadID main_thread;
    static SDL_cond* go_cond;
    SDL_mutex* lock;
    int body() {
      SDL_LockMutex(lock);
      while(true) {
        while(task == nullptr) {
          SDL_CondWait(go_cond, lock);
        }
        task(start, stop);
        task = nullptr;
      }
      SDL_UnlockMutex(lock);
      // NOTREACHED
      std::cerr <<"INTERNAL ERROR: FX worker thread terminated prematurely!\n";
      return 0;
    }
    static int outer_body(void*p){return reinterpret_cast<Worker*>(p)->body();}
  public:
    Worker() {
      static unsigned int num_workers_so_far = 0;
      if(go_cond == nullptr) go_cond = SDL_CreateCond();
      assert(go_cond);
      lock = SDL_CreateMutex();
      assert(lock);
      std::ostringstream str;
      str << "" << (++num_workers_so_far);
      std::string name = str.str();
      SDL_Thread* thread = SDL_CreateThread(outer_body, name.c_str(), this);
      if(thread == nullptr) {
        std::string why = sn.Get("THREAD_CREATION_ERROR"_Key,
                                 {SDL_GetError()});
        die("%s", why.c_str());
      }
      SDL_DetachThread(thread);
    }
    ~Worker() {
      std::cerr << "BUG: Worker destructor called!\n";
    }
    static void performTask(std::function<void(unsigned int,
                                               unsigned int)> task,
                            unsigned int big_start, unsigned int big_stop) {
      if(thread_count == 1 || SDL_ThreadID() != main_thread) {
        task(big_start, big_stop);
        return;
      }
      else {
        unsigned int big_size = big_stop - big_start;
        unsigned int start = 0;
        for(unsigned int i = 0; i < thread_count-1; ++i) {
          unsigned int stop = (i+1) * big_size / thread_count;
          if(stop != start) {
            // the mutex may provide a memory barrier
            SDL_LockMutex(worker_threads[i].lock);
            worker_threads[i].task = task;
            worker_threads[i].start = start;
            worker_threads[i].stop = stop;
            SDL_UnlockMutex(worker_threads[i].lock);
            start = stop;
          }
        }
        SDL_CondBroadcast(go_cond);
        if(start != big_stop) task(start, big_stop);
        for(unsigned int i = 0; i < thread_count-1; ++i) {
          SDL_LockMutex(worker_threads[i].lock);
          SDL_UnlockMutex(worker_threads[i].lock);
        }
      }
    }
    static void init(unsigned int thread_count) {
      if(Worker::thread_count != 0) {
        die("INTERNAL ERROR: init called more than once");
      }
      Worker::thread_count = thread_count;
      main_thread = SDL_ThreadID();
      worker_threads = new Worker[thread_count-1];
    }
  };
  Worker* Worker::worker_threads;
  unsigned int Worker::thread_count = 0;
  SDL_threadID Worker::main_thread;
  SDL_cond* Worker::go_cond;
}

void FX::init(unsigned int init_thread_count) {
  unsigned int thread_count;
  if(init_thread_count == 0) {
    int threads = SDL_GetCPUCount();
    if(threads < 1) thread_count = 1;
    else thread_count = threads;
  }
  else thread_count = init_thread_count;
  Worker::init(thread_count);
}

const Linearize& FX::linearizer() {
  static const Linearize linearize;
  return linearize;
}

const Delinearize& FX::delinearizer() {
  static const Delinearize delinearize;
  return delinearize;
}

void FX::raw_screen_to_bgra(const ARS::PPU::raw_screen& in,
                            void* out,
                            unsigned int left, unsigned int top,
                            unsigned int right, unsigned int bottom,
                            bool output_skips_rows) {
  static auto best_imp = FX::Imp::raw_screen_to_bgra().best([&](FX::Proto::raw_screen_to_bgra candidate) { candidate(in, out, left, top, right, bottom, output_skips_rows); });
  best_imp(in, out, left, top, right, bottom, output_skips_rows);
}

void FX::raw_screen_to_bgra_2x(const ARS::PPU::raw_screen& in,
                               void* out,
                               unsigned int left, unsigned int top,
                               unsigned int right, unsigned int bottom,
                               bool output_skips_rows) {
  static auto best_imp = FX::Imp::raw_screen_to_bgra_2x().best([&](FX::Proto::raw_screen_to_bgra_2x candidate) { candidate(in, out, left, top, right, bottom, output_skips_rows); });
  best_imp(in, out, left, top, right, bottom, output_skips_rows);
}

void FX::composite_bgra(const void* in, void* out,
                        unsigned int width, unsigned int height,
                        bool output_skips_rows) {
  static auto best_imp = FX::Imp::composite_bgra().best([&](FX::Proto::composite_bgra candidate) { candidate(in, out, width, height, output_skips_rows); });
  Worker::performTask([=](unsigned int start, unsigned int stop) {
      const void* local_in = reinterpret_cast<const uint8_t*>(in)
        +start*width*4;
      void* local_out = reinterpret_cast<uint8_t*>(out)
        +start*width*8*(output_skips_rows?2:1);
      best_imp(local_in, local_out, width, stop-start, output_skips_rows);
    }, 0, height);
}

void FX::svideo_bgra(const void* in, void* out,
                     unsigned int width, unsigned int height,
                     bool output_skips_rows) {
  static auto best_imp = FX::Imp::svideo_bgra().best([&](FX::Proto::svideo_bgra candidate) { candidate(in, out, width, height, output_skips_rows); });
  Worker::performTask([=](unsigned int start, unsigned int stop) {
      const void* local_in = reinterpret_cast<const uint8_t*>(in)
        +start*width*4;
      void* local_out = reinterpret_cast<uint8_t*>(out)
        +start*width*8*(output_skips_rows?2:1);
      best_imp(local_in, local_out, width, stop-start, output_skips_rows);
    }, 0, height);
}

void FX::scanline_crisp_bgra(void* buf,
                             unsigned int width, unsigned int height) {
  static auto best_imp = FX::Imp::scanline_crisp_bgra().best([&](FX::Proto::scanline_crisp_bgra candidate) { candidate(buf, width, height); });
  Worker::performTask([=](unsigned int start, unsigned int stop) {
      void* local_buf = reinterpret_cast<uint8_t*>(buf)+start*width*8;
      best_imp(local_buf, width, stop-start);
    }, 0, height);
}

void FX::scanline_bright_bgra(void* buf,
                              unsigned int width, unsigned int height) {
  static auto best_imp = FX::Imp::scanline_bright_bgra().best([&](FX::Proto::scanline_bright_bgra candidate) { candidate(buf, width, height); });
  Worker::performTask([=](unsigned int start, unsigned int stop) {
      void* local_buf = reinterpret_cast<uint8_t*>(buf)+start*width*8;
      best_imp(local_buf, width, stop-start);
    }, 0, height);
}

#define MAKE_IMPS(x) Imp::imps<Proto::x>& Imp::x() { static Imp::imps<Proto::x> nugget; return nugget; }
MAKE_IMPS(raw_screen_to_bgra);
MAKE_IMPS(raw_screen_to_bgra_2x);
MAKE_IMPS(composite_bgra);
MAKE_IMPS(svideo_bgra);
MAKE_IMPS(scanline_crisp_bgra);
MAKE_IMPS(scanline_bright_bgra);
