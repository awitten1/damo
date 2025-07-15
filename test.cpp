

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

#define PAGE_SIZE 4096

unsigned long get_minor_page_faults() {
  std::ifstream f("/proc/self/stat");
  int idx = 0;
  for (;;) {
    ++idx;
    std::string entry;
    f >> entry;
    if (idx != 10) {
      continue;
    }
    return std::stoul(entry);
  }
}


void write_into_pages(char* buf, int num_pages, char c) {
  for (int i = 0; i < num_pages; ++i) {
    *(buf + PAGE_SIZE*i) = c;
  }
}

void check_first_char(char* buf, int num_pages, char c) {
  for (int i = 0; i < num_pages; ++i) {
    if (*(buf + PAGE_SIZE*i) != c) {
      throw std::runtime_error{"unexpected char"};
    }
  }
}

int do_stuff(int);

int main(int argc, char** argv) {
  int num_pages = std::stoi(argv[1]);
  if (argc >= 3) {
    for (;;) {
      if (int ret = do_stuff(num_pages)) {
        return ret;
      }
      sleep(1);
    }
  }
  return do_stuff(num_pages);
}

inline uint64_t __attribute__((always_inline)) rdtsc() {
	uint32_t low, high;

	asm volatile(
		"mfence \n"
		"lfence \n"
		"rdtsc \n"
		"lfence \n"
		: "=a" (low), "=d" (high)
	);

	uint64_t tsc1 = low | ((uint64_t)high << 32);
	return tsc1;
}

void log_minor_faults(char* buf, int num_pages, char c) {
  get_minor_page_faults(); rdtsc();
  unsigned long minor_faults1 = get_minor_page_faults();
  auto c1 = rdtsc();
  write_into_pages(buf, num_pages, c);
  auto c2 = rdtsc();
  unsigned long minor_faults2 = get_minor_page_faults();
  std::cout << minor_faults2 - minor_faults1 << ',' << c2 - c1 << std::endl;
  auto c3 = rdtsc();
  write_into_pages(buf, num_pages, c);
  auto c4 = rdtsc();
  unsigned long minor_faults3 = get_minor_page_faults();
  std::cout << minor_faults3 - minor_faults2 << ',' << c4 - c3 << std::endl;
}


int do_stuff(int num_pages) {
  static char* buf = (char*)mmap(NULL, PAGE_SIZE*num_pages, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 1, 0);

  if (buf == MAP_FAILED) {
    fprintf(stderr, "mmap failed");
    return EXIT_FAILURE;
  }

  log_minor_faults(buf, num_pages, 'a');

  pid_t child_pid = fork();
  if (child_pid == 0) {
    std::cout << "in child" << std::endl;
    log_minor_faults(buf, num_pages, 'b');
    exit(EXIT_SUCCESS);
  } else {
    int ret;
    waitpid(child_pid, &ret, 0);
    if (!WIFEXITED(ret)) {
      std::cerr << "something weird happened" << std::endl;
    }
  }

  check_first_char(buf, num_pages, 'b');
  return 0;

}
