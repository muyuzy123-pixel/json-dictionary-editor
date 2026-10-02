#include "file_store.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace jsondict_linux;
int count = 0;
void check(bool value, const char* message) { ++count; if (!value) throw std::runtime_error(message); }
void external_write(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size())
        throw std::runtime_error("test external writer failed");
}
template<class F> FileError fails(F action) {
    try { action(); } catch (const FileError& error) { ++count; return error; }
    throw std::runtime_error("unsafe file operation unexpectedly succeeded");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    try {
        check(directory.isValid(), "temporary directory unavailable");
        const QString path = directory.filePath(QStringLiteral("Unicode 文件 🙂.json"));
        const QByteArray original = "{\"precise\":1.2300e+04}\n";
        if(argc>1&&QByteArray(argv[1])=="--fault"){
            external_write(path,original);const auto baseline=FileStore::read(path).stamp;
            qputenv("JDE_FAULT_ROOT",QFile::encodeName(directory.path()));const auto mode=qgetenv("JDE_TEST_FAULT");
            const QByteArray replacement="{\"s\":\""+QByteArray(128*1024,'x')+"\"}";
            const auto hits=reinterpret_cast<int(*)()>(dlsym(RTLD_DEFAULT,"jde_fault_hits"));
            check(hits!=nullptr,"fault interposer not loaded");
            if(mode=="write_short_eintr"){FileStore::save(path,replacement,baseline);check(FileStore::read(path).bytes==replacement,"short/EINTR writes not completed");}
            else{
                const auto error=fails([&]{FileStore::save(path,replacement,baseline);});
                const bool post=mode=="fsync_dir_commit_eio"||mode=="fsync_dir_final_eio";
                check(error.committed==post,"incorrect commit classification");check(error.system_error!=0,"system detail missing");
                check(FileStore::read(path).bytes==(post?replacement:original),"unexpected target bytes");
                if(post){check(!error.backup.isEmpty(),"late flush lost recovery path");check(FileStore::read(error.backup).bytes==original,"late flush lost old version");check(error.type==FileFailure::Durability,"incorrect flush classification");}
                if(mode.startsWith("rename_")&&mode!="rename_eio")check(error.type==FileFailure::UnsupportedAtomicSave,"unsupported rename not reported");
                std::cout<<"FAULT_DETAIL: errno="<<error.system_error<<" committed="<<error.committed<<" recovery="<<!error.backup.isEmpty()<<'\n';
            }
            check(hits()>0,"fault was not exercised");std::cout<<"LINUX_FAULT_OK: "<<mode.constData()<<"; "<<count<<" assertions; "<<hits()<<" injected syscall results\n";return 0;
        }
        auto first = FileStore::save(path, original, std::nullopt);
        check(FileStore::read(path).bytes == original, "new-file round trip failed");
        const QByteArray replacement = "{\"changed\":true}\n";
        auto second = FileStore::save(path, replacement, first);
        check(FileStore::read(path).bytes == replacement, "replacement round trip failed");
        fails([&] { FileStore::save(path, original, first); });
        check(FileStore::read(path).bytes == replacement, "stale baseline overwrote file");
        fails([&] { FileStore::save(path, original, std::nullopt); });
        check(FileStore::read(path).bytes == replacement, "absent baseline overwrote existing file");

        const QString symlink = directory.filePath("link.json");
        check(::symlink(QFile::encodeName(path).constData(), QFile::encodeName(symlink).constData()) == 0,
              "cannot create test symlink");
        check(fails([&] { FileStore::save(symlink, original, second); }).type == FileFailure::Symlink,
              "symlink save was not rejected");
        check(FileStore::read(path).bytes == replacement, "symlink rejection changed target");
        const QString hardlink = directory.filePath("hard.json");
        check(::link(QFile::encodeName(path).constData(), QFile::encodeName(hardlink).constData()) == 0,
              "cannot create test hard link");
        auto linked = FileStore::read(path).stamp;
        check(fails([&] { FileStore::save(path, original, linked); }).type == FileFailure::Hardlink,
              "hard-linked target was not rejected");
        QFile::remove(hardlink);
        second = FileStore::read(path).stamp;
        const QByteArray external = "{\"external\":true}\n";
        auto race = fails([&] { FileStore::save(path, original, second, {[&](SaveStep step) {
            if (step == SaveStep::BeforeRename) external_write(path, external);
        }}); });
        check(!race.committed && FileStore::read(path).bytes == external,
              "precommit external write was overwritten");
        second = FileStore::read(path).stamp;
        auto after = fails([&] { FileStore::save(path, original, second, {[](SaveStep step) {
            if (step == SaveStep::AfterRename) throw std::runtime_error("injected post-commit failure");
        }}); });
        check(after.committed && !after.backup.isEmpty(), "postcommit failure lost backup");
        check(FileStore::read(after.backup).bytes == external, "retained backup is not actual replaced version");
        check(FileStore::read(path).bytes == original, "postcommit failure corrupted new content");
        second = FileStore::read(path).stamp;
        auto changed_after = fails([&] { FileStore::save(path, replacement, second, {[&](SaveStep step) {
            if (step == SaveStep::BeforeBackupRemoval) external_write(path, external);
        }}); });
        check(changed_after.committed && QFileInfo::exists(changed_after.backup),
              "post-save external write discarded original backup");
        check(FileStore::read(path).bytes == external, "post-save verification overwrote external data");
        const QString readonly = directory.filePath("readonly");
        check(QDir().mkdir(readonly), "cannot create readonly fixture");
        const QString locked = readonly + "/data.json";
        external_write(locked, original);
        auto locked_stamp = FileStore::read(locked).stamp;
        ::chmod(QFile::encodeName(readonly).constData(), 0555);
        check(fails([&]{FileStore::save(locked,replacement,locked_stamp);}).system_error==EACCES,"readonly directory not rejected for root");
        check(FileStore::read(locked).bytes==original,"readonly directory caused direct overwrite");
        ::chmod(QFile::encodeName(readonly).constData(), 0755);

        const QString ancestorLink=directory.filePath("ancestor-link");
        check(::symlink(QFile::encodeName(readonly).constData(),QFile::encodeName(ancestorLink).constData())==0,"ancestor fixture failed");
        check(fails([&]{FileStore::save(ancestorLink+"/data.json",replacement,locked_stamp);}).type==FileFailure::Symlink,"symbolic ancestor accepted");
        check(fails([&]{FileStore::save(readonly,original,{});}).type==FileFailure::NotRegular,"directory target accepted");
        fails([&]{FileStore::save(path+QChar(0)+"other",original,{});});
        const auto raceBaseline=FileStore::read(path).stamp;
        const auto atCommit=fails([&]{FileStore::save(path,replacement,raceBaseline,{[&](SaveStep step){
            if(step==SaveStep::AtCommit){
                const pid_t pid=::fork();if(pid==0){external_write(path,"{\"racer\":0}");::_exit(0);}
                if(pid<0)throw std::runtime_error("fork unavailable");
                int status=0;if(::waitpid(pid,&status,0)!=pid||!WIFEXITED(status)||WEXITSTATUS(status))throw std::runtime_error("race writer failed");
            }
        }});});
        check(atCommit.committed&&atCommit.type==FileFailure::Conflict,"actual replaced version unchecked");
        check(FileStore::read(atCommit.backup).bytes=="{\"racer\":0}","actual competing version lost");
        check(FileStore::read(path).bytes==replacement,"commit gap corrupted new bytes");
        const auto sameBytes=FileStore::read(path).stamp;
        const auto identityRace=fails([&]{FileStore::save(path,original,sameBytes,{[&](SaveStep step){if(step==SaveStep::AfterRename){QFile::remove(path);external_write(path,original);}}});});
        check(identityRace.committed&&identityRace.type==FileFailure::Verification,"same-byte identity change not detected");
        check(QFileInfo::exists(identityRace.backup),"identity race lost old version");
        check(::chmod(QFile::encodeName(path).constData(),0640)==0,"permissions fixture failed");
        const auto durable=FileStore::read(path).stamp;const auto preserved=FileStore::save(path,replacement,durable);
        check(preserved.mode==0640&&preserved.group==durable.group,"mode/group lost");
        check(preserved.inode!=durable.inode&&preserved.sha256!=durable.sha256,"new identity/hash not returned");
        check(!QDir(directory.path()).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden).isEmpty(),"successful save did not retain recovery");
        const QString absentRace=directory.filePath("new-race.json");
        const auto created=fails([&]{FileStore::save(absentRace,original,{},{[&](SaveStep step){if(step==SaveStep::AtCommit)external_write(absentRace,external);}});});
        check(!created.committed&&created.type==FileFailure::Conflict,"NOREPLACE overwrote competitor");check(FileStore::read(absentRace).bytes==external,"NOREPLACE corrupted competitor");
        const QString attrs=directory.filePath("xattrs.json");external_write(attrs,original);const char attribute[]="baseline";
        check(::setxattr(QFile::encodeName(attrs).constData(),"user.jde-test",attribute,sizeof(attribute),0)==0,"xattr fixture unavailable");
        check(fails([&]{FileStore::save(attrs,replacement,FileStore::read(attrs).stamp);}).type==FileFailure::UnsupportedAtomicSave,"extended metadata silently dropped");
        check(FileStore::read(attrs).bytes==original,"xattr rejection changed content");
        const QString tampered=directory.filePath("temporary-tamper.json");external_write(tampered,original);
        const auto tempError=fails([&]{FileStore::save(tampered,replacement,FileStore::read(tampered).stamp,{[&](SaveStep step){
            if(step==SaveStep::TemporaryWritten){
                const auto list=QDir(directory.path()).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden);
                for(const auto& name:list){const auto candidate=directory.filePath(name);if(FileStore::read(candidate).bytes==replacement)external_write(candidate,"{}");}
            }
        }});});
        check(!tempError.committed&&tempError.type==FileFailure::Verification,"tampered temporary not rejected");check(FileStore::read(tampered).bytes==original,"tampered temporary reached target");
        std::cout << "LINUX_FILE_OK: " << count << " assertions; renameat2, SHA-256, conflicts, backups, links, permissions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LINUX_FILE_FAILED: " << error.what() << '\n';
        return 1;
    }
}
