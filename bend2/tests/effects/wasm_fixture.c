#include <dirent.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
extern char** environ;
typedef struct { U32 op; char *first, *second, *result; int invalid; } WasmFixture;
static void wf_error(WasmFixture* p, const char* message) {
  size_t n = strlen(message); p->result = io_mem(malloc(n+2));
  p->result[0]='-'; memcpy(p->result+1,message,n+1);
}
static char* wf_suffix(const char* base, const char* suffix) {
  size_t a=strlen(base), b=strlen(suffix); char* out=io_mem(malloc(a+b+1));
  memcpy(out,base,a); memcpy(out+a,suffix,b+1); return out;
}
static int wf_compare(const void* a,const void* b) { return strcmp(*(const char* const*)a,*(const char* const*)b); }
static int wf_mkdir(const char* path) { return mkdir(path,0755)==0 || errno==EEXIST; }
static void wf_list(WasmFixture* p) {
  char* parent=io_mem(strdup(p->second)); char* slash=strrchr(parent,'/');
  if(slash) { if(slash==parent) slash[1]=0; else *slash=0; } else { free(parent); parent=io_mem(strdup(".")); }
  int okay=wf_mkdir(parent); free(parent);
  if(!okay || !wf_mkdir(p->second)) { wf_error(p,strerror(errno)); return; }
  char* path=wf_suffix(p->first,"/fixtures"); DIR* dir=opendir(path); free(path);
  if(!dir) { wf_error(p,strerror(errno)); return; }
  char** names=NULL; size_t count=0, length=2; struct dirent* entry;
  errno=0;
  while((entry=readdir(dir))) {
    size_t n=strlen(entry->d_name);
    if(n>=4 && strcmp(entry->d_name+n-4,".kan")==0) {
      names=io_mem(realloc(names,(count+1)*sizeof(char*)));
      names[count]=io_mem(strndup(entry->d_name,n-4)); count++; length+=n-3;
    }
  }
  int saved=errno; closedir(dir);
  if(saved) { for(size_t i=0;i<count;i++) free(names[i]); free(names); wf_error(p,strerror(saved)); return; }
  qsort(names,count,sizeof(char*),wf_compare); p->result=io_mem(malloc(length));
  char* at=p->result; *at++='+';
  for(size_t i=0;i<count;i++) { if(i) *at++='\n'; size_t n=strlen(names[i]); memcpy(at,names[i],n); at+=n; free(names[i]); }
  *at=0; free(names);
}
static void wf_validate(WasmFixture* p) {
  char* wasm=wf_suffix(p->first,".wasm"), *wat=wf_suffix(p->first,".wat"), *err=wf_suffix(p->first,".err");
  int fd=open(err,O_WRONLY|O_CREAT|O_TRUNC,0666); int code=fd<0?errno:0;
  if(!code) {
    posix_spawn_file_actions_t actions; code=posix_spawn_file_actions_init(&actions);
    if(!code) {
      code=posix_spawn_file_actions_adddup2(&actions,fd,STDERR_FILENO);
      if(!code && fd!=STDERR_FILENO) code=posix_spawn_file_actions_addclose(&actions,fd);
      char* args[]={"wasm-opt",wasm,"-S","-o",wat,"--enable-gc","--enable-reference-types","--enable-tail-call","--enable-exception-handling",NULL};
      pid_t pid; if(!code) code=posix_spawnp(&pid,"wasm-opt",&actions,NULL,args,environ);
      posix_spawn_file_actions_destroy(&actions);
      if(!code) { int status; pid_t waited; do { waited=waitpid(pid,&status,0); } while(waited<0 && errno==EINTR);
        if(waited<0) code=errno; else if(!WIFEXITED(status)||WEXITSTATUS(status)!=0) code=EINVAL;
      }
    }
    close(fd);
  }
  if(code) { FILE* file=fopen(err,"r"); char* line=NULL; size_t capacity=0;
    if(file) { if(getline(&line,&capacity,file)<0) { free(line); line=NULL; } fclose(file); }
    if(line) line[strcspn(line,"\r\n")]=0;
    wf_error(p,line && line[0]?line:strerror(code)); free(line);
  } else p->result=io_mem(strdup("+"));
  free(wasm); free(wat); free(err);
}
static void wf_call(IoWork* w) {
  WasmFixture* p=(WasmFixture*)w->data;
  if(p->invalid) wf_error(p,"fixture path contains NUL");
  else if(p->op==0) wf_list(p);
  else if(p->op==1) wf_validate(p);
  else wf_error(p,"unknown Wasm fixture operation");
}
static Term wf_pack(Env e,IoWork* w) {
  WasmFixture* p=(WasmFixture*)w->data; Term result=io_str(e,p->result,strlen(p->result));
  free(p->first); free(p->second); free(p->result); free(p); return result;
}
Term wasm_fixture_io_run(Env e,Term* f,IoWork* w) {
  WasmFixture* p=io_mem(calloc(1,sizeof(WasmFixture))); uint64_t a=0,b=0; p->op=(U32)f[0];
  p->first=io_cstr(e,f[1],&a); p->second=io_cstr(e,f[2],&b);
  p->invalid=io_nul(p->first,a)||io_nul(p->second,b); w->data=(char*)p;
  return io_work(w,wf_call,wf_pack);
}
static void __attribute__((constructor)) wf_use(void) { io_eff(CID_WASM_FIXTURE_IO,wasm_fixture_io_run,0); }
