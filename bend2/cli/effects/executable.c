#include <limits.h>

Term host_executable_run(Env e, Term* f, IoWork* w) {
  size_t cap = 1024;
  char* path = NULL;
#ifdef __APPLE__
  uint32_t len = (uint32_t)cap;
  path = io_mem(malloc(cap));
  if (_NSGetExecutablePath(path, &len) != 0) {
    free(path);
    path = io_mem(malloc(len));
    if (_NSGetExecutablePath(path, &len) != 0) path[0] = 0;
  }
#else
  for (;;) {
    path = io_mem(malloc(cap));
    ssize_t len = readlink("/proc/self/exe", path, cap - 1);
    if (len < 0) { path[0] = 0; break; }
    if ((size_t)len < cap - 1) { path[len] = 0; break; }
    free(path);
    cap *= 2;
  }
#endif
  Term value = io_str(e, path, strlen(path));
  free(path);
  return value;
}

static void __attribute__((constructor)) host_executable_use(void) {
  io_eff(CID_HOST_EXECUTABLE, host_executable_run, 0);
}
