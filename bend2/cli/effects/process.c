#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;

typedef struct {
  char* fields[6];
  int invalid;
  U32 result;
} HostProcess;

static void host_process_call(IoWork* w) {
  HostProcess* p = (HostProcess*)w->data;
  p->result = 127;
  if (p->invalid) return;
  int out = open(p->fields[4], O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (out < 0) return;
  int err = open(p->fields[5], O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (err < 0) { close(out); return; }
  if (p->invalid) {
    dprintf(err, "invalid: process argument contains a NUL byte\n");
  } else {
    posix_spawn_file_actions_t actions;
    int code = posix_spawn_file_actions_init(&actions);
    if (code == 0) {
      code = posix_spawn_file_actions_adddup2(&actions, out, STDOUT_FILENO);
      if (code == 0) code = posix_spawn_file_actions_adddup2(&actions, err, STDERR_FILENO);
      if (code == 0 && out != STDOUT_FILENO && out != STDERR_FILENO)
        code = posix_spawn_file_actions_addclose(&actions, out);
      if (code == 0 && err != STDOUT_FILENO && err != STDERR_FILENO)
        code = posix_spawn_file_actions_addclose(&actions, err);
      pid_t pid = 0;
      char* argv[] = {p->fields[0], p->fields[1], p->fields[2], p->fields[3], NULL};
      if (code == 0) code = posix_spawnp(&pid, p->fields[0], &actions, NULL, argv, environ);
      posix_spawn_file_actions_destroy(&actions);
      if (code == 0) {
        int status = 0;
        pid_t waited;
        do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
        if (waited < 0) code = errno;
        else if (WIFEXITED(status)) p->result = (U32)WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) p->result = (U32)(128 + WTERMSIG(status));
      }
    }
    if (code != 0) dprintf(err, "invalid: %s: %s\n", p->fields[0], strerror(code));
  }
  close(out);
  close(err);
}

static Term host_process_pack(Env e, IoWork* w) {
  HostProcess* p = (HostProcess*)w->data;
  U32 code = p->result;
  for (int i = 0; i < 6; ++i) free(p->fields[i]);
  free(p);
  return code;
}

Term host_process_run(Env e, Term* f, IoWork* w) {
  HostProcess* p = io_mem(calloc(1, sizeof(HostProcess)));
  for (int i = 0; i < 6; ++i) {
    uint64_t len = 0;
    p->fields[i] = io_cstr(e, f[i], &len);
    if (io_nul(p->fields[i], len)) p->invalid = 1;
  }
  w->data = (char*)p;
  return io_work(w, host_process_call, host_process_pack);
}

static void __attribute__((constructor)) host_process_use(void) {
  io_eff(CID_HOST_PROCESS, host_process_run, 0);
}
