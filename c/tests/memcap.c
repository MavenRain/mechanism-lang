/* memcap: host test support with no Bend counterpart. Usage: memcap <capMB> <seconds> <cmd> [args...]
 * Runs the command, polls its resident set size (proc_pidinfo) and its physical footprint
 * (proc_pid_rusage RUSAGE_INFO_V2) every 100 ms, sends SIGKILL when the larger of the two peaks
 * crosses the cap or the wall time crosses the time cap, and prints one summary line on stdout.
 * c-test and c-test-prelude run the prelude-scale drivers under it (the Makefile builds it as
 * _tcc/memcap on Darwin; libproc is a Darwin interface). The exit status is the status of the child. */
#include <libproc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double now_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: memcap <capMB> <seconds> <cmd> [args...]\n");
    return 2;
  }
  long cap_mb = strtol(argv[1], NULL, 10);
  long max_seconds = strtol(argv[2], NULL, 10);
  /* A caller timeout can SIGKILL the direct parent of memcap and reparent it; the child must not run
   * on uncapped. The parent pid is recorded before the fork, and a change of getppid() in the poll
   * loop sends SIGKILL to the child (killed_orphan=1). */
  pid_t parent = getppid();
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 2;
  }
  if (pid == 0) {
    execvp(argv[3], argv + 3);
    perror("execvp");
    _exit(127);
  }
  double start = now_seconds();
  long peak_mb = 0;
  long foot_mb = 0;
  int polls_ok = 0;
  int polls_failed = 0;
  int foot_ok = 0;
  int killed_mem = 0;
  int killed_time = 0;
  int killed_orphan = 0;
  int status = 0;
  for (;;) {
    pid_t done = waitpid(pid, &status, WNOHANG);
    if (done == pid) break;
    struct proc_taskinfo info;
    int got = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &info, sizeof info);
    if (got == (int)sizeof info) {
      polls_ok += 1;
      long mb = (long)(info.pti_resident_size / (1024 * 1024));
      if (mb > peak_mb) peak_mb = mb;
    } else {
      polls_failed += 1;
    }
    /* The physical footprint (RUSAGE_INFO_V2 ri_phys_footprint) counts compressed pages too; under
     * memory pressure the resident set size under-reports by gigabytes. The cap applies to the
     * larger of the two. */
    struct rusage_info_v2 ri;
    int ru = proc_pid_rusage(pid, RUSAGE_INFO_V2, (rusage_info_t *)&ri);
    if (ru == 0) {
      foot_ok += 1;
      long fmb = (long)(ri.ri_phys_footprint / (1024 * 1024));
      if (fmb > foot_mb) foot_mb = fmb;
    }
    double elapsed = now_seconds() - start;
    long gate_mb = foot_mb > peak_mb ? foot_mb : peak_mb;
    if (gate_mb > cap_mb && !killed_mem) {
      kill(pid, SIGKILL);
      killed_mem = 1;
    }
    if (elapsed > (double)max_seconds && !killed_time && !killed_mem) {
      kill(pid, SIGKILL);
      killed_time = 1;
    }
    if (getppid() != parent && !killed_orphan && !killed_mem && !killed_time) {
      kill(pid, SIGKILL);
      killed_orphan = 1;
    }
    usleep(100 * 1000);
  }
  double secs = now_seconds() - start;
  int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  int sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
  printf("memcap cmd=%s exit=%d signal=%d peakMB=%ld footMB=%ld capMB=%ld killed_mem=%d killed_time=%d killed_orphan=%d secs=%.1f polls_ok=%d polls_failed=%d foot_ok=%d\n",
         argv[3], code, sig, peak_mb, foot_mb, cap_mb, killed_mem, killed_time, killed_orphan, secs, polls_ok, polls_failed, foot_ok);
  /* A normal exit passes its code through; a signal death (including the SIGKILL at a cap) exits
   * 128 + signal, so a make recipe fails on any run that is not OK. */
  return code >= 0 ? code : 128 + sig;
}
