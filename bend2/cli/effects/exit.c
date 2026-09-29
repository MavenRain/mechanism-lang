Term cli_exit_run(Env e, Term* f, IoWork* w) {
  exit((int)f[0]);
  return term_pak(CID_UNIT, 0);
}

static void __attribute__((constructor)) cli_exit_use(void) {
  io_eff(CID_CLI_EXIT, cli_exit_run, 0);
}
