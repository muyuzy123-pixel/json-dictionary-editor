// Real Linux metadata races. These hooks mutate actual files, not simulated branches.
#include "file_store.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QtEndian>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace jsondict_linux;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct UnsupportedFixture:std::runtime_error{using std::runtime_error::runtime_error;};
QByteArray aclBytes(){
 QByteArray bytes;auto append16=[&](quint16 n){n=qToLittleEndian(n);bytes.append(reinterpret_cast<const char*>(&n),2);};
 auto append32=[&](quint32 n){n=qToLittleEndian(n);bytes.append(reinterpret_cast<const char*>(&n),4);};
 append32(2);const auto entry=[&](quint16 tag,quint16 permission,quint32 id){append16(tag);append16(permission);append32(id);};
 entry(1,6,0xffffffffu);entry(2,4,12345);entry(4,4,0xffffffffu);entry(16,4,0xffffffffu);entry(32,4,0xffffffffu);return bytes;
}
void putAttribute(const QString& path,const char* name,const QByteArray& value){
 if(::setxattr(QFile::encodeName(path).constData(),name,value.constData(),static_cast<size_t>(value.size()),0)<0){
  if(errno==ENOTSUP||errno==EOPNOTSUPP)throw UnsupportedFixture("fixture filesystem does not support requested xattr/ACL");
  throw std::runtime_error(std::string("setxattr fixture failed: ")+std::strerror(errno));
 }
}
QByteArray attribute(const QString& path,const char* name){
 const auto size=::getxattr(QFile::encodeName(path).constData(),name,nullptr,0);
 require(size>=0,"required xattr/ACL was not retained");QByteArray bytes(static_cast<qsizetype>(size),Qt::Uninitialized);
 require(::getxattr(QFile::encodeName(path).constData(),name,bytes.data(),static_cast<size_t>(bytes.size()))==size,"cannot read retained metadata");return bytes;
}
QString temporary(const QString& directory){
 const auto files=QDir(directory).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden);
 require(files.size()==1,"expected exactly one transaction/recovery file");return QDir(directory).filePath(files.first());
}
}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir dir;
 try{
  require(argc==2,"one metadata case name required");require(dir.isValid(),"temporary directory unavailable");
  const QString test=QString::fromLocal8Bit(argv[1]),path=dir.filePath("document.json");
  const QByteArray old="{\"precise\":1.2300e+04}\n",fresh="{\"precise\":9007199254740993123456789}\n";
  QFile file(path);require(file.open(QIODevice::WriteOnly),"fixture create failed");require(file.write(old)==old.size(),"fixture short write");file.close();
  require(::chmod(QFile::encodeName(path).constData(),0644)==0,"fixture chmod failed");
  const auto baseline=FileStore::read(path).stamp;
  const bool inherited=test=="default_acl_temporary",acl=test.contains("acl"),backup=test.startsWith("backup_");
  const bool temp=test.startsWith("temporary_")||inherited;
  const bool before=test.endsWith("before_rename")||inherited,after=test.endsWith("after_commit");
  const bool change=test.contains("change_");
  const char* name=acl?"system.posix_acl_access":"user.jde-race";
  const QByteArray value=acl?aclBytes():QByteArray("second-value");
  // Prove fixture support independently before entering the save transaction.
  const QString capability=dir.filePath("capability");QFile cap(capability);require(cap.open(QIODevice::WriteOnly),"capability fixture create failed");cap.close();
  putAttribute(capability,name,value);require(attribute(capability,name)==value,"fixture metadata round trip failed");QFile::remove(capability);
  if(inherited)putAttribute(dir.path(),"system.posix_acl_default",aclBytes());
  bool injected=false;QString injectedPath;
  const SaveStep stage=before?SaveStep::BeforeRename:after?SaveStep::AfterRename:SaveStep::AtCommit;
  SaveOptions options{[&](SaveStep step){
   if(step!=stage)return;
   injectedPath=(temp||backup)?temporary(dir.path()):path;
   if(!inherited){
    if(change)putAttribute(injectedPath,name,"first-value");
    putAttribute(injectedPath,name,value);
   }
   require(attribute(injectedPath,name)==value,"real mutation not observed");
   if(acl){struct stat st{};require(::stat(QFile::encodeName(injectedPath).constData(),&st)==0,"ACL stat failed");require((st.st_mode&0777)==0644,"ACL fixture must preserve basic mode bits");}
   injected=true;
  }};
  try{
   FileStore::save(path,fresh,baseline,options);
   std::cerr<<"COUNTEREXAMPLE: metadata mutation was silently accepted; case="<<test.toStdString()<<" injection="<<injected<<" target_new="<<(FileStore::read(path).bytes==fresh)<<"\n";return 1;
  }catch(const FileError& error){
   require(injected,"save failed without exercising the real mutation");
   require(error.type==FileFailure::UnsupportedAtomicSave||error.type==FileFailure::Conflict||error.type==FileFailure::Verification,"incorrect metadata failure classification");
   require(error.committed==!before,"wrong committed state for metadata race");
   if(before){
    require(FileStore::read(path).bytes==old,"precommit rejection changed target bytes");
    if(!temp)require(attribute(path,name)==value,"precommit rejection lost racing target metadata");
    require(QDir(dir.path()).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden).isEmpty(),"precommit temporary leaked");
   }else{
    require(!error.backup.isEmpty(),"committed metadata rejection lost recovery path");
    require(FileStore::read(error.backup).bytes==old,"actual old bytes not retained");
    require(FileStore::read(path).bytes==fresh,"committed target bytes differ");
    if(backup||(!temp&&!after))require(attribute(error.backup,name)==value,"actual replaced metadata not retained");
    else require(attribute(path,name)==value,"new target's actual metadata not retained");
   }
   std::cout<<"METADATA_RACE_OK case="<<test.toStdString()<<" committed="<<error.committed<<" actual_mutation_verified=1 recovery="<<!error.backup.isEmpty()<<" mode_preserved="<<acl<<" AUTOMATION_ONLY\n";
   return 0;
  }
 }catch(const UnsupportedFixture& e){std::cerr<<"METADATA_FIXTURE_UNSUPPORTED: "<<e.what()<<'\n';return 77;}
 catch(const std::exception& e){std::cerr<<"METADATA_RACE_FAILED: "<<e.what()<<'\n';return 1;}
}

