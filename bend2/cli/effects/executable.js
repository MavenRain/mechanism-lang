function host_executable() {
  return process.argv[1] || process.execPath;
}
