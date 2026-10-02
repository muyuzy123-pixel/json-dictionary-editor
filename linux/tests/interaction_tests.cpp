#include "editor_window.hpp"
#include <QAbstractItemModelTester>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTimer>
#include <QTreeView>
#include <QAction>
using namespace jsondict_linux;
class InteractionTests:public QObject{
 Q_OBJECT
 QTemporaryDir settings_;
 void prepare(EditorWindow& w){w.errorHandler=[](const QString&){};w.unsavedDecision=[]{return UnsavedDecision::Discard;};w.draftDecision=[]{return DraftDecision::Cancel;};w.show();QCoreApplication::processEvents();}
 static jsondict::NodeId child(const EditorSession& s,std::size_t i){return s.document.root().as_object().at(i).value.id();}
 static void clickDecision(const QString& label){
  QTimer::singleShot(0,[label]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box)return;for(auto* button:box->buttons())if(button->text()==label){QTest::mouseClick(button,Qt::LeftButton);return;}});
 }
private slots:
 void initTestCase(){
  QVERIFY(settings_.isValid());QCoreApplication::setOrganizationName("JDETest");QCoreApplication::setApplicationName("Interaction");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
  qInfo()<<"Qt"<<qVersion()<<"backend"<<QGuiApplication::platformName()<<"AUTOMATION_ONLY";
 }
 void transactionalInspectorAndNavigation(){
  LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);prepare(w);QVERIFY(w.loadBytes("{\"n\":1.2300e+04,\"other\":0}"));
  const auto n=child(w.session(),0),other=child(w.session(),1);QVERIFY(w.selectNode(n));auto* key=w.findChild<QLineEdit*>("keyEdit");auto* value=w.findChild<QPlainTextEdit*>("valueEdit");
  key->setText("renamed");value->setPlainText("01");const auto before=w.session().encoded();QTest::mouseClick(w.findChild<QPushButton*>("applyDraft"),Qt::LeftButton);QCOMPARE(w.session().encoded(),before);QVERIFY(w.hasDraft());QVERIFY(!w.session().dirty);
  QVERIFY(!w.selectNode(other));QCOMPARE(w.session().selected,n);QCOMPARE(key->text(),QString("renamed"));
  value->setPlainText("-0");w.draftDecision=[]{return DraftDecision::Apply;};QVERIFY(w.selectNode(other));QVERIFY(w.session().dirty);QCOMPARE(w.session().document.find(n)->as_number().text,std::string("-0"));
  QVERIFY(w.selectNode(n));key->setText("other");value->setPlainText("2");const auto committed=w.session().encoded();QVERIFY(!w.applyDraft());QCOMPARE(w.session().encoded(),committed);
  w.draftDecision=[]{return DraftDecision::Discard;};QVERIFY(w.selectNode(other));QCOMPARE(w.session().encoded(),committed);
 }
 void actualDraftDecisionDialogs(){
  LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);prepare(w);w.draftDecision={};QVERIFY(w.loadBytes("{\"a\":\"one\",\"b\":\"two\"}"));
  const auto a=child(w.session(),0),b=child(w.session(),1);QVERIFY(w.selectNode(a));auto* value=w.findChild<QPlainTextEdit*>("valueEdit");value->setPlainText("draft");
  auto* tree=w.findChild<QTreeView*>("treeView");auto target=w.treeModel()->indexForId(b);
  clickDecision(ui("Cancel"));QTest::mouseClick(tree->viewport(),Qt::LeftButton,{},tree->visualRect(target).center());QCOMPARE(w.session().selected,a);QVERIFY(w.hasDraft());QVERIFY(!w.session().dirty);
  clickDecision(ui("Discard"));QTest::mouseClick(tree->viewport(),Qt::LeftButton,{},tree->visualRect(target).center());QCOMPARE(w.session().selected,b);QCOMPARE(w.session().document.find(a)->as_string(),std::string("one"));
  QVERIFY(w.selectNode(a));value->setPlainText("applied");clickDecision(ui("Apply"));QTest::mouseClick(tree->viewport(),Qt::LeftButton,{},tree->visualRect(target).center());QCOMPARE(w.session().selected,b);QCOMPARE(w.session().document.find(a)->as_string(),std::string("applied"));
 }
 void mainLanguageKeepsAllState(){
  LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorWindow w(lang);prepare(w);
  std::string json="{\"group\":{\"precise\":9007199254740993123456789,\"exponent\":1.2300e+04},\"text\":\"";
  for(int i=0;i<180;++i)json+="line\\n";json+="\",\"list\":[";for(int i=0;i<150;++i){if(i)json+=',';json+='0';}json+="]}";QVERIFY(w.loadBytes(json));
  auto* tree=w.findChild<QTreeView*>("treeView");tree->expand(w.treeModel()->indexForId(child(w.session(),0)));tree->expand(w.treeModel()->indexForId(child(w.session(),2)));
  const auto selected=child(w.session(),1);QVERIFY(w.selectNode(selected));
  auto* key=w.findChild<QLineEdit*>("keyEdit");auto* value=w.findChild<QPlainTextEdit*>("valueEdit");auto* search=w.findChild<QLineEdit*>("searchEdit");
  key->setText("pending_key");value->insertPlainText("pending\n");search->setText("number");QTextCursor cursor=value->textCursor();cursor.setPosition(25);cursor.setPosition(50,QTextCursor::KeepAnchor);value->setTextCursor(cursor);
  key->setFocus();QCoreApplication::processEvents();key->setSelection(2,4);tree->verticalScrollBar()->setValue(20);value->verticalScrollBar()->setValue(25);QCoreApplication::processEvents();QCOMPARE(key->selectionStart(),2);
  const QPersistentModelIndex persistent(tree->currentIndex());const auto encoded=w.session().encoded();const auto text=value->toPlainText();const int tv=tree->verticalScrollBar()->value(),vv=value->verticalScrollBar()->value();
  QSignalSpy kc(key,&QLineEdit::textChanged),vc(value,&QPlainTextEdit::textChanged),resets(w.treeModel(),&QAbstractItemModel::modelReset);
  for(const auto pref:{LanguageService::Preference::Chinese,LanguageService::Preference::English,LanguageService::Preference::System}){
   QVERIFY(lang.select(pref));QCoreApplication::processEvents();QCOMPARE(w.session().encoded(),encoded);QCOMPARE(w.session().selected,selected);QVERIFY(!w.session().dirty);QVERIFY(w.hasDraft());
   QVERIFY(persistent.isValid());QCOMPARE(tree->currentIndex(),QModelIndex(persistent));QCOMPARE(key->text(),QString("pending_key"));QCOMPARE(key->selectionStart(),2);QCOMPARE(key->selectedText(),QString("ndin"));
   QCOMPARE(value->toPlainText(),text);QCOMPARE(value->textCursor().position(),50);QCOMPARE(value->textCursor().anchor(),25);QCOMPARE(search->text(),QString("number"));QCOMPARE(tree->verticalScrollBar()->value(),tv);QCOMPARE(value->verticalScrollBar()->value(),vv);
   QVERIFY(tree->isExpanded(w.treeModel()->indexForId(child(w.session(),0))));QVERIFY(tree->isExpanded(w.treeModel()->indexForId(child(w.session(),2))));
  }
  QCOMPARE(kc.count(),0);QCOMPARE(vc.count(),0);QCOMPARE(resets.count(),0);lang.select(LanguageService::Preference::Chinese);QCOMPARE(w.treeModel()->headerData(0,Qt::Horizontal,Qt::DisplayRole).toString(),QString("键 / 索引"));
  const auto out=qEnvironmentVariable("JDE_TEST_SCREENSHOTS");if(!out.isEmpty()){QVERIFY(w.grab().save(out+"/main-zh.png"));lang.select(LanguageService::Preference::English);QVERIFY(w.grab().save(out+"/main-en.png"));}
 }
 void rawTransactionsAndLanguage(){
  LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorSession s;s.load("{\"n\":9007199254740993123456789,\"e\":1.2300e+04}");
  const auto n=child(s,0);RawDialog raw(s,n,lang);raw.show();auto* edit=raw.findChild<QPlainTextEdit*>("rawEdit");edit->setPlainText("{\"x\":1.2300e+04,\"u\":\"🙂\"}");
  QTextCursor cursor=edit->textCursor();cursor.setPosition(2);cursor.setPosition(8,QTextCursor::KeepAnchor);edit->setTextCursor(cursor);const auto before=s.encoded();const auto draft=edit->toPlainText();
  auto* language=raw.findChild<QAction*>("language1");QVERIFY(language);language->trigger();QCoreApplication::processEvents();QCOMPARE(s.encoded(),before);QVERIFY(!s.dirty);QCOMPARE(edit->toPlainText(),draft);QCOMPARE(edit->textCursor().position(),8);QCOMPARE(edit->textCursor().anchor(),2);
  raw.findChild<QAction*>("language2")->trigger();QVERIFY(raw.check());QCOMPARE(s.encoded(),before);QVERIFY(raw.format());QCOMPARE(s.encoded(),before);QVERIFY(raw.apply());QVERIFY(s.dirty);QCOMPARE(s.document.find(n)->id(),n);QCOMPARE(s.document.find(n)->as_object()[0].value.as_number().text,std::string("1.2300e+04"));
  RawDialog root(s,s.document.root_id(),lang);auto* rootEdit=root.findChild<QPlainTextEdit*>("rawEdit");rootEdit->setPlainText("[1]");const auto committed=s.encoded();QVERIFY(!root.check());QVERIFY(!root.apply());QCOMPARE(s.encoded(),committed);
  rootEdit->setPlainText("{\"a\":1,\"\\u0061\":2}");QVERIFY(!root.apply());QCOMPARE(s.encoded(),committed);rootEdit->setPlainText(QString(static_cast<int>(kMaximumRawBytes+1),QChar(' ')));QVERIFY(!root.apply());QCOMPARE(s.encoded(),committed);
 }
 void rawDecisionDialogs(){
  LanguageService lang(nullptr,false);lang.select(LanguageService::Preference::English);EditorSession s;s.load("{\"n\":0}");const auto n=child(s,0);RawDialog raw(s,n,lang);raw.show();raw.findChild<QPlainTextEdit*>("rawEdit")->setPlainText("-0");
  clickDecision(ui("Cancel"));QTest::mouseClick(raw.findChild<QPushButton*>("rawCancel"),Qt::LeftButton);QVERIFY(raw.isVisible());QVERIFY(!s.dirty);
  clickDecision(ui("Apply"));QTest::mouseClick(raw.findChild<QPushButton*>("rawCancel"),Qt::LeftButton);QVERIFY(!raw.isVisible());QCOMPARE(s.document.find(n)->as_number().text,std::string("-0"));
  RawDialog discard(s,n,lang);discard.show();discard.findChild<QPlainTextEdit*>("rawEdit")->setPlainText("2");clickDecision(ui("Discard"));QTest::mouseClick(discard.findChild<QPushButton*>("rawCancel"),Qt::LeftButton);QCOMPARE(s.document.find(n)->as_number().text,std::string("-0"));
 }
 void structuralOperationsAndModel(){
  LanguageService lang(nullptr,false);EditorWindow w(lang);prepare(w);QVERIFY(w.loadBytes("{\"z\":1.2300e+04,\"empty\":{},\"filled\":{\"x\":0},\"arr\":[],\"populated\":[0]}"));QAbstractItemModelTester tester(w.treeModel(),QAbstractItemModelTester::FailureReportingMode::QtTest);
  const auto matches=w.treeModel()->search("empty object");QCOMPARE(matches.size(),1);QCOMPARE(matches[0],child(w.session(),1));QCOMPARE(w.treeModel()->search("empty array").size(),1);QCOMPARE(w.treeModel()->search("空对象").size(),1);
  const auto z=child(w.session(),0);QVERIFY(w.selectNode(z));QVERIFY(w.perform("duplicate"));const auto duplicate=w.session().selected;QVERIFY(duplicate!=z);QCOMPARE(w.session().document.find(duplicate)->as_number().text,std::string("1.2300e+04"));
  QVERIFY(w.perform("down"));QVERIFY(w.perform("up"));QVERIFY(w.perform("delete"));QVERIFY(!w.session().document.find(duplicate));QVERIFY(w.selectNode(w.session().document.root_id()));QVERIFY(w.perform("add"));
  w.findChild<QComboBox*>("typeCombo")->setCurrentIndex(static_cast<int>(jsondict::Kind::Boolean));auto* boolean=w.findChild<QCheckBox*>("booleanEdit");QCoreApplication::processEvents();QTest::mouseClick(boolean,Qt::LeftButton,{},QPoint(8,boolean->height()/2));QVERIFY(w.hasDraft());
  QTest::mouseClick(w.findChild<QPushButton*>("applyDraft"),Qt::LeftButton);QVERIFY(w.session().document.find(w.session().selected)->as_boolean());QVERIFY(w.selectNode(w.session().document.root_id()));QVERIFY(w.perform("sort"));QCOMPARE(w.session().document.find(z)->as_number().text,std::string("1.2300e+04"));
 }
 void savingAndCloseKeepDrafts(){
  QTemporaryDir dir;LanguageService lang(nullptr,false);EditorWindow w(lang);prepare(w);QVERIFY(w.loadBytes("{\"s\":\"one\",\"n\":9007199254740993}"));QVERIFY(w.selectNode(child(w.session(),0)));
  auto* edit=w.findChild<QPlainTextEdit*>("valueEdit");edit->setPlainText("draft");const auto path=dir.filePath("doc.json");QVERIFY(!w.saveTo(path));QVERIFY(!QFile::exists(path));QVERIFY(w.hasDraft());w.draftDecision=[]{return DraftDecision::Apply;};QVERIFY(w.saveTo(path));QVERIFY(!w.session().dirty);QVERIFY(!w.hasDraft());QVERIFY(FileStore::read(path).bytes.contains("9007199254740993"));
  edit->setPlainText("cancel_close");w.draftDecision=[]{return DraftDecision::Cancel;};QVERIFY(!w.close());QVERIFY(w.isVisible());QVERIFY(w.hasDraft());w.draftDecision=[]{return DraftDecision::Apply;};QVERIFY(w.applyDraft());
  QFile external(path);QVERIFY(external.open(QIODevice::WriteOnly|QIODevice::Truncate));external.write("{\"external\":true}");external.close();QVERIFY(!w.saveTo(path));QVERIFY(w.session().dirty);QVERIFY(FileStore::read(path).bytes.contains("external"));QVERIFY(w.saveTo(dir.filePath("recovered.json")));QVERIFY(!w.session().dirty);
 }
 void postCommitFailureKeepsUnsaved(){
  QTemporaryDir dir;LanguageService lang(nullptr,false);EditorWindow w(lang);prepare(w);QVERIFY(w.loadBytes("{\"s\":\"one\"}"));const auto path=dir.filePath("doc.json");QVERIFY(w.saveTo(path));QVERIFY(w.selectNode(child(w.session(),0)));w.findChild<QPlainTextEdit*>("valueEdit")->setPlainText("two");QVERIFY(w.applyDraft());
  QString message;w.errorHandler=[&](const QString& text){message=text;};QVERIFY(!w.saveTo(path,false,{[](SaveStep step){if(step==SaveStep::AfterRename)throw std::runtime_error("test post-commit failure");}}));QVERIFY(w.session().dirty);QVERIFY(message.contains(ui("The commit may already have occurred; inspect the target before retrying.")));QVERIFY(FileStore::read(path).bytes.contains("two"));QVERIFY(!w.saveTo(path));QVERIFY(w.session().dirty);
 }
 void controlCharactersUseRawAndSample(){
  LanguageService lang(nullptr,false);EditorWindow w(lang);prepare(w);QVERIFY(w.loadBytes("{\"key\\n\":\"a\\r\\nb\\u0000\\t\",\"n\":-0}"));QVERIFY(w.selectNode(child(w.session(),0)));QVERIFY(!w.findChild<QLineEdit*>("keyEdit")->isEnabled());QVERIFY(!w.findChild<QPlainTextEdit*>("valueEdit")->isEnabled());
  const auto before=w.session().encoded();lang.select(LanguageService::Preference::Chinese);QCOMPARE(w.session().encoded(),before);QVERIFY(!w.hasDraft());w.findChild<QAction*>("sample")->trigger();QVERIFY(w.session().document.node_count()>1);
  QFile sample(":/sample/SampleDictionary.json");QVERIFY(sample.open(QIODevice::ReadOnly));EditorSession expected;const auto bytes=sample.readAll();expected.load(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())));QCOMPARE(w.session().encoded(),expected.encoded());
 }
 void languagePreferencePersistence(){
  QCOMPARE(LanguageService::resolve(LanguageService::Preference::System,"zh-TW"),QString("zh-Hans"));QCOMPARE(LanguageService::resolve(LanguageService::Preference::System,"fr-FR"),QString("en"));
  {LanguageService service(nullptr,true);QVERIFY(service.select(LanguageService::Preference::Chinese));}
  {LanguageService service(nullptr,true);QCOMPARE(service.preference(),LanguageService::Preference::Chinese);QVERIFY(service.chinese());QVERIFY(service.select(LanguageService::Preference::English));}
  {LanguageService service(nullptr,true);QCOMPARE(service.preference(),LanguageService::Preference::English);QVERIFY(!service.chinese());}
  QString invalid(1,QChar(0xd800));QVERIFY_EXCEPTION_THROWN(checkedUtf8(invalid),std::runtime_error);
 }
};
QTEST_MAIN(InteractionTests)
#include "interaction_tests.moc"
