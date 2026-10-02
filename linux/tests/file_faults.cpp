// Test-only interposer; never shipped with or loaded by the editor.
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <fcntl.h>
#include <cstdarg>
#include <sys/stat.h>
#include <unistd.h>
namespace {
std::atomic<int> hits{0},dirFlushes{0},writes{0};
const char* fault(){const auto* value=std::getenv("JDE_TEST_FAULT");return value?value:"";}
bool mode(const char* name){return std::strcmp(fault(),name)==0;}
bool scoped(int fd){
 const auto* root=std::getenv("JDE_FAULT_ROOT");if(!root||!*root)return false;
 char link[64],path[4096];std::snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
 const auto n=::readlink(link,path,sizeof(path)-1);if(n<=0)return false;path[n]=0;
 return std::strncmp(path,root,std::strlen(root))==0;
}
bool regular(int fd){struct stat st{};return ::fstat(fd,&st)==0&&S_ISREG(st.st_mode);}
}
extern "C" int jde_fault_hits(){return hits.load();}
extern "C" void jde_fault_reset(){hits=0;dirFlushes=0;writes=0;}
extern "C" ssize_t write(int fd,const void* buffer,size_t size){
 static auto real=reinterpret_cast<ssize_t(*)(int,const void*,size_t)>(dlsym(RTLD_NEXT,"write"));
 if(scoped(fd)&&regular(fd)){
  if(mode("write_enospc")){++hits;errno=ENOSPC;return -1;}
  if(mode("write_short_eintr")){++hits;if(writes++==0){errno=EINTR;return -1;}if(size>1024)size=1024;}
 }
 return real(fd,buffer,size);
}
extern "C" int fsync(int fd){
 static auto real=reinterpret_cast<int(*)(int)>(dlsym(RTLD_NEXT,"fsync"));
 if(scoped(fd)){
  if(regular(fd)&&mode("fsync_file_eio")){++hits;errno=EIO;return -1;}
  if(!regular(fd)){const int call=++dirFlushes;if((mode("fsync_dir_pre_eio")&&call==1)||(mode("fsync_dir_commit_eio")&&call==2)||(mode("fsync_dir_final_eio")&&call==3)){++hits;errno=EIO;return -1;}}
 }
 return real(fd);
}
extern "C" int renameat2(int from,const char* oldname,int to,const char* newname,unsigned flags){
 static auto real=reinterpret_cast<int(*)(int,const char*,int,const char*,unsigned)>(dlsym(RTLD_NEXT,"renameat2"));
 if(scoped(to)){
  const int failure=mode("rename_enosys")?ENOSYS:mode("rename_eopnotsupp")?EOPNOTSUPP:mode("rename_einval")?EINVAL:mode("rename_eio")?EIO:0;
  if(failure){++hits;errno=failure;return -1;}
 }
 return real(from,oldname,to,newname,flags);
}
extern "C" int openat(int dir,const char* name,int flags,...){
 static auto real=reinterpret_cast<int(*)(int,const char*,int,...)>(dlsym(RTLD_NEXT,"openat"));
 mode_t permissions=0;if(flags&O_CREAT){va_list args;va_start(args,flags);permissions=static_cast<mode_t>(va_arg(args,int));va_end(args);}
 if(scoped(dir)&&mode("create_eacces")&&(flags&O_CREAT)){++hits;errno=EACCES;return -1;}
 return real(dir,name,flags,permissions);
}
