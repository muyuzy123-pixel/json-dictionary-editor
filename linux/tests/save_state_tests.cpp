#include "editor_window.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTimer>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTreeView>
#include <dlfcn.h>
#include <sys/xattr.h>
#include <iostream>
using namespace jsondict_linux;
class SaveStateTests:public QObject{
 Q_OBJECT
 QTemporaryDir settings_;
 static jsondict::NodeId stringNode(const EditorWindow& w){return w.session().document.root().as_object().at(0).value.id();}
private slots:
 void initTestCase(){
  // Each row destroys its own top-level windows. Only QtTest owns application exit.
  QApplication::setQuitOnLastWindowClosed(false);
  QVERIFY(settings_.isValid());QCoreApplication::setOrganizationName("JDESaveState");QCoreApplication::setApplicationName("Test");
  QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
  qInfo()<<"Qt"<<qVersion()<<"backend"<<QGuiApplication::platformName()<<"AUTOMATION_ONLY";
 }
 void committedErrorKeepsLiveState_data(){
  QTest::addColumn<bool>("dirty");QTest::addColumn<int>("destination");QTest::addColumn<int>("draft");QTest::addColumn<QString>("fault");
  for(bool dirty:{false,true})for(int destination:{0,1,2})for(int draft:{0,1,2})for(const QString& fault:{QString("hook"),QString("fsync_dir_commit_eio"),QString("fsync_dir_final_eio")}){
   const auto label=QString("%1-%2-%3-%4").arg(dirty?"dirty":"clean").arg(destination==0?"current":destination==1?"saveas-new":"saveas-existing").arg(draft==0?"no-draft":draft==1?"apply-draft":"discard-draft").arg(fault);
   QTest::newRow(label.toUtf8().constData())<<dirty<<destination<<draft<<fault;
  }
 }
 void committedErrorKeepsLiveState(){
  QFETCH(bool,dirty);QFETCH(int,destination);QFETCH(int,draft);QFETCH(QString,fault);
  auto reset=reinterpret_cast<void(*)()>(dlsym(RTLD_DEFAULT,"jde_fault_reset"));
  auto hits=reinterpret_cast<int(*)()>(dlsym(RTLD_DEFAULT,"jde_fault_hits"));QVERIFY(reset&&hits);
  qunsetenv("JDE_FAULT_ROOT");qunsetenv("JDE_TEST_FAULT");reset();
  QTemporaryDir dir;QVERIFY(dir.isValid());LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);
  w.errorHandler=[](const QString&){};w.unsavedDecision=[]{return UnsavedDecision::Cancel;};w.draftDecision=[draft]{return draft==1?DraftDecision::Apply:DraftDecision::Discard;};
  w.show();QCoreApplication::processEvents();QVERIFY(w.loadBytes("{\"s\":\"one\",\"n\":1.2300e+04}"));
  const QString current=dir.filePath("current.json");QVERIFY(w.saveTo(current));QVERIFY(w.selectNode(stringNode(w)));
  auto* key=w.findChild<QLineEdit*>("keyEdit");auto* value=w.findChild<QPlainTextEdit*>("valueEdit");auto* search=w.findChild<QLineEdit*>("searchEdit");
  if(dirty){value->setPlainText("already-committed");QVERIFY(w.applyDraft());}
  QString target=destination==0?current:dir.filePath(destination==1?"new.json":"existing.json");
  const QByteArray oldBytes="{\"other\":0}\n";
  if(destination==2){QFile external(target);QVERIFY(external.open(QIODevice::WriteOnly));QCOMPARE(external.write(oldBytes),oldBytes.size());external.close();}
  const QByteArray actualOld=destination==1?QByteArray{}:FileStore::read(target).bytes;
  if(draft){key->setText("pending_key");value->setPlainText("pending value\n🙂");QTextCursor cursor=value->textCursor();cursor.setPosition(2);cursor.setPosition(7,QTextCursor::KeepAnchor);value->setTextCursor(cursor);}
  search->setText("number");const auto before=w.session().encoded();const auto selected=w.session().selected;const auto keyText=key->text(),valueText=value->toPlainText();
  const int position=value->textCursor().position(),anchor=value->textCursor().anchor();QPersistentModelIndex index(w.findChild<QTreeView*>("treeView")->currentIndex());
  QString message,titleAtError,statusAtError;bool draftAtError=false;
  w.errorHandler=[&](const QString& text){message=text;titleAtError=w.windowTitle();statusAtError=w.statusBar()->currentMessage();draftAtError=w.hasDraft();};
  SaveOptions options;options.hook=[&](SaveStep step){if(fault=="hook"&&step==SaveStep::AfterRename)throw std::runtime_error("injected exception after actual rename");};
  reset();qputenv("JDE_FAULT_ROOT",QFile::encodeName(dir.path()));if(fault!="hook")qputenv("JDE_TEST_FAULT",fault.toUtf8());
  const bool result=w.saveTo(target,destination==2,options);const int count=hits();qunsetenv("JDE_FAULT_ROOT");qunsetenv("JDE_TEST_FAULT");
  QVERIFY(!result);if(fault!="hook")QVERIFY(count>0);
  QVERIFY(w.session().dirty);QVERIFY(w.windowTitle().contains(" *"));QVERIFY(titleAtError.contains(" *"));QVERIFY(statusAtError.contains(ui("Modified")));
  QCOMPARE(w.session().encoded(),before);QCOMPARE(w.session().selected,selected);QCOMPARE(w.hasDraft(),draft!=0);QCOMPARE(draftAtError,draft!=0);
  QCOMPARE(key->text(),keyText);QCOMPARE(value->toPlainText(),valueText);QCOMPARE(value->textCursor().position(),position);QCOMPARE(value->textCursor().anchor(),anchor);
  QVERIFY(index.isValid());QCOMPARE(w.findChild<QTreeView*>("treeView")->currentIndex(),QModelIndex(index));QCOMPARE(search->text(),QString("number"));QCOMPARE(w.filePath(),current);
  QVERIFY(message.contains(ui("Document remains unsaved.")));QVERIFY(message.contains(target));
  QVERIFY(message.contains(ui("The commit may already have occurred; inspect the target before retrying.")));
  if(destination!=1){
   const auto backups=QDir(dir.path()).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden);bool exact=false;
   for(const auto& backup:backups){const auto path=dir.filePath(backup);if(FileStore::read(path).bytes==actualOld&&message.contains(path))exact=true;}
   QVERIFY(exact);
  }else QVERIFY(message.contains(ui("No previous version was available; the new target may already exist.")));
  const auto saved=FileStore::read(target).bytes;QVERIFY(saved.contains("1.2300e+04"));
  if(draft==1)QVERIFY(saved.contains("pending_key"));else QVERIFY(!saved.contains("pending_key"));
  bool ioAttempted=false;const auto encoded=w.session().encoded();
  QVERIFY(!w.saveTo(target,true,{[&](SaveStep step){if(step==SaveStep::TemporaryWritten)ioAttempted=true;}}));
  QVERIFY(!ioAttempted);QCOMPARE(FileStore::read(target).bytes,saved);QCOMPARE(w.session().encoded(),encoded);QVERIFY(w.session().dirty);QCOMPARE(w.hasDraft(),draft!=0);
  w.draftDecision=[]{return DraftDecision::Cancel;};QVERIFY(!w.newDocument());QVERIFY(!w.close());QVERIFY(w.isVisible());QCOMPARE(w.session().encoded(),encoded);QCOMPARE(w.hasDraft(),draft!=0);
  qInfo()<<"SAVE_ERROR_STATE_OK destination"<<destination<<"dirty-before"<<dirty<<"draft"<<draft<<"fault"<<fault<<"syscall-hits"<<count;
 }
 void continuationFailure_data(){
  QTest::addColumn<bool>("dirty");QTest::addColumn<QString>("action");
  for(bool dirty:{false,true})for(const QString& action:{QString("new"),QString("close"),QString("open")})QTest::newRow((QString(dirty?"dirty-":"clean-")+action).toUtf8().constData())<<dirty<<action;
 }
 void continuationFailure(){
  QFETCH(bool,dirty);QFETCH(QString,action);QTemporaryDir dir;LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);
  w.errorHandler=[](const QString&){};w.unsavedDecision=[]{return UnsavedDecision::Save;};w.draftDecision=[]{return DraftDecision::Apply;};w.show();QVERIFY(w.loadBytes("{\"s\":\"one\",\"n\":1.2300e+04}"));
  const auto path=dir.filePath("current.json");QVERIFY(w.saveTo(path));QVERIFY(w.selectNode(stringNode(w)));auto* value=w.findChild<QPlainTextEdit*>("valueEdit");
  if(dirty){value->setPlainText("already-applied");QVERIFY(w.applyDraft());}value->setPlainText("keep pending");
  const auto before=w.session().encoded();const auto selected=w.session().selected;bool atErrorDraft=false;QString atErrorDocument;
  w.errorHandler=[&](const QString&){atErrorDraft=w.hasDraft();atErrorDocument=QString::fromStdString(w.session().encoded());};
  w.saveOptionsForTarget=[](const QString&){return SaveOptions{[](SaveStep step){if(step==SaveStep::AfterRename)throw std::runtime_error("actual committed failure during continuation");}};};
  const auto other=dir.filePath("other.json");QFile file(other);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{\"different\":0}");file.close();
  const bool result=action=="new"?w.newDocument():action=="open"?w.openPath(other):w.close();
  QVERIFY(!result);QVERIFY(w.isVisible());QVERIFY(w.session().dirty);QCOMPARE(w.session().encoded(),before);QCOMPARE(w.session().selected,selected);QVERIFY(w.hasDraft());
  QCOMPARE(value->toPlainText(),QString("keep pending"));QVERIFY(atErrorDraft);QCOMPARE(atErrorDocument,QString::fromStdString(before));QCOMPARE(w.filePath(),path);
  QVERIFY(FileStore::read(path).bytes.contains("keep pending"));qInfo()<<"CONTINUATION_ERROR_OK"<<action<<"dirty-before"<<dirty;
 }
 void precommitFailureAndCancelledContinuationKeepDraft(){
  QTemporaryDir dir;LanguageService lang(nullptr,false);EditorWindow w(lang);w.errorHandler=[](const QString&){};w.draftDecision=[]{return DraftDecision::Apply;};w.unsavedDecision=[]{return UnsavedDecision::Cancel;};w.show();
  QVERIFY(w.loadBytes("{\"s\":\"one\"}"));const auto path=dir.filePath("doc.json");QVERIFY(w.saveTo(path));QVERIFY(w.selectNode(stringNode(w)));
  auto* value=w.findChild<QPlainTextEdit*>("valueEdit");value->setPlainText("pending");const auto before=w.session().encoded(),disk=FileStore::read(path).bytes.toStdString();
  QVERIFY(!w.saveTo(path,false,{[](SaveStep step){if(step==SaveStep::BeforeRename)throw std::runtime_error("injected precommit failure");}}));
  QCOMPARE(w.session().encoded(),before);QCOMPARE(FileStore::read(path).bytes.toStdString(),disk);QVERIFY(w.hasDraft());QVERIFY(!w.session().dirty);QCOMPARE(value->toPlainText(),QString("pending"));
  QVERIFY(!w.newDocument());QVERIFY(!w.close());QVERIFY(w.hasDraft());QCOMPARE(w.session().encoded(),before);
  w.draftDecision=[]{return DraftDecision::Cancel;};QVERIFY(!w.saveTo(dir.filePath("cancel.json")));QVERIFY(!QFile::exists(dir.filePath("cancel.json")));QVERIFY(w.hasDraft());
 }
 void actualCommittedErrorDialogShowsUnsavedBeforeDismissal(){
  QTemporaryDir dir;LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);w.show();QVERIFY(w.loadBytes("{\"s\":\"one\"}"));
  const auto path=dir.filePath("dialog.json");QVERIFY(w.saveTo(path));QVERIFY(!w.session().dirty);bool seen=false,marked=false,target=false,backup=false;
  QTimer timer;timer.setInterval(10);
  connect(&timer,&QTimer::timeout,&w,[&]{
   auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box)return;seen=true;marked=w.session().dirty&&w.windowTitle().contains(" *")&&w.statusBar()->currentMessage().contains(ui("Modified"));target=box->text().contains(path);backup=box->text().contains(".jsondict-backup-");
   const auto output=qEnvironmentVariable("JDE_TEST_SCREENSHOTS");if(!output.isEmpty()){w.grab().save(output+"/committed-main.png");box->grab().save(output+"/committed-error.png");}
   timer.stop();QTest::mouseClick(box->button(QMessageBox::Ok),Qt::LeftButton);
  });timer.start();
  QTimer::singleShot(3000,&w,[&]{if(!seen){if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->reject();}});
  QVERIFY(!w.saveTo(path,false,{[](SaveStep step){if(step==SaveStep::AfterRename)throw std::runtime_error("committed dialog failure");}}));
  QVERIFY(seen);QVERIFY(marked);QVERIFY(target);QVERIFY(backup);QVERIFY(w.session().dirty);qInfo()<<"ACTUAL_QMESSAGEBOX_AUTOMATION_ONLY";
 }
 void metadataCommittedError_data(){QTest::addColumn<bool>("newTarget");QTest::newRow("existing-target-racing-xattr")<<false;QTest::newRow("new-target-racing-temporary-xattr")<<true;}
 void metadataCommittedError(){
  QFETCH(bool,newTarget);QTemporaryDir dir;LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);w.errorHandler=[](const QString&){};w.draftDecision=[]{return DraftDecision::Apply;};w.show();
  QVERIFY(w.loadBytes("{\"s\":\"one\"}"));const auto current=dir.filePath("current.json");QVERIFY(w.saveTo(current));QVERIFY(w.selectNode(stringNode(w)));auto* edit=w.findChild<QPlainTextEdit*>("valueEdit");edit->setPlainText("pending");
  const auto before=w.session().encoded();const auto target=newTarget?dir.filePath("new.json"):current;QString message;w.errorHandler=[&](const QString& text){message=text;};
  bool mutation=false;
  QVERIFY(!w.saveTo(target,false,{[&](SaveStep step){if(step!=SaveStep::AtCommit)return;QString path=target;if(newTarget){const auto files=QDir(dir.path()).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden);if(files.size()!=1)throw std::runtime_error("temporary fixture missing");path=dir.filePath(files.first());}
   if(::setxattr(QFile::encodeName(path).constData(),"user.jde-ui-race","metadata",8,0)<0)throw std::runtime_error("actual UI metadata mutation failed");mutation=true;}}));
  QVERIFY(mutation);QVERIFY(w.session().dirty);QVERIFY(w.hasDraft());QCOMPARE(w.session().encoded(),before);QCOMPARE(edit->toPlainText(),QString("pending"));QVERIFY(message.contains(target));QVERIFY(message.contains(ui("Document remains unsaved.")));
  const auto retained=newTarget?target:dir.filePath(QDir(dir.path()).entryList({".jsondict-backup-*"},QDir::Files|QDir::Hidden).first());char bytes[8];
  QCOMPARE(::getxattr(QFile::encodeName(retained).constData(),"user.jde-ui-race",bytes,8),ssize_t(8));QCOMPARE(QByteArray(bytes,8),QByteArray("metadata"));
  bool attempted=false;QVERIFY(!w.saveTo(target,true,{[&](SaveStep){attempted=true;}}));QVERIFY(!attempted);QVERIFY(w.hasDraft());QVERIFY(w.session().dirty);
 }
};
QTEST_MAIN(SaveStateTests)
#include "save_state_tests.moc"
