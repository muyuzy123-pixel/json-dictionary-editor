#include "editor_window.hpp"
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>
namespace jsondict_linux {
namespace {
bool unsafeControl(const QString& text,bool newline){return std::any_of(text.begin(),text.end(),[newline](QChar c){return c.unicode()<32&&!(newline&&c=='\n');});}
}
EditorWindow::EditorWindow(LanguageService& languages,QWidget* parent):QMainWindow(parent),languages_(languages){
 setObjectName("editorWindow");resize(1080,700);model_=new TreeModel(this);createUi();createActions();rebuild();
 connect(&languages_,&LanguageService::changed,this,&EditorWindow::retranslate);retranslate();
 QSettings settings;restoreGeometry(settings.value("windowGeometry").toByteArray());
}
void EditorWindow::createUi(){
 auto* central=new QWidget(this);auto* layout=new QVBoxLayout(central);auto* searchRow=new QHBoxLayout;
 search_=new QLineEdit(central);search_->setObjectName("searchEdit");search_->setClearButtonEnabled(true);searchRow->addWidget(search_,1);
 matchesLabel_=new QLabel(central);searchRow->addWidget(matchesLabel_);layout->addLayout(searchRow);
 auto* splitter=new QSplitter(central);splitter->setObjectName("editorSplitter");layout->addWidget(splitter,1);
 tree_=new QTreeView(splitter);tree_->setObjectName("treeView");tree_->setModel(model_);tree_->setUniformRowHeights(true);tree_->setAlternatingRowColors(true);tree_->setSelectionBehavior(QAbstractItemView::SelectRows);tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);tree_->setColumnWidth(0,230);tree_->setColumnWidth(1,100);
 auto* inspector=new QWidget(splitter);auto* form=new QVBoxLayout(inspector);
 pathLabel_=new QLabel(inspector);pathLabel_->setObjectName("nodePath");pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);pathLabel_->setWordWrap(true);form->addWidget(pathLabel_);
 keyLabel_=new QLabel(inspector);form->addWidget(keyLabel_);key_=new QLineEdit(inspector);key_->setObjectName("keyEdit");key_->setMaxLength(static_cast<int>(kMaximumKeyCharacters));form->addWidget(key_);
 typeLabel_=new QLabel(inspector);form->addWidget(typeLabel_);type_=new QComboBox(inspector);type_->setObjectName("typeCombo");for(int i=0;i<6;++i)type_->addItem(QString{},i);form->addWidget(type_);
 valueLabel_=new QLabel(inspector);form->addWidget(valueLabel_);value_=new QPlainTextEdit(inspector);value_->setObjectName("valueEdit");form->addWidget(value_,1);
 boolean_=new QCheckBox(inspector);boolean_->setObjectName("booleanEdit");form->addWidget(boolean_);
 notice_=new QLabel(inspector);notice_->setWordWrap(true);form->addWidget(notice_);draftLabel_=new QLabel(inspector);draftLabel_->setObjectName("draftLabel");form->addWidget(draftLabel_);
 auto* buttons=new QHBoxLayout;apply_=new QPushButton(inspector);apply_->setObjectName("applyDraft");buttons->addWidget(apply_);discard_=new QPushButton(inspector);discard_->setObjectName("discardDraft");buttons->addWidget(discard_);raw_=new QPushButton(inspector);raw_->setObjectName("openRaw");buttons->addWidget(raw_);form->addLayout(buttons);
 splitter->setStretchFactor(0,3);splitter->setStretchFactor(1,2);setCentralWidget(central);
 connect(tree_->selectionModel(),&QItemSelectionModel::currentChanged,this,[this](const QModelIndex& current){if(updating_)return;const auto next=model_->id(current);if(next&&next!=session_.selected)selectNode(next);});
 connect(key_,&QLineEdit::textChanged,this,[this]{if(!updating_)refreshState();});connect(value_,&QPlainTextEdit::textChanged,this,[this]{if(!updating_)refreshState();});connect(boolean_,&QCheckBox::toggled,this,[this]{if(!updating_)refreshState();});
 connect(type_,&QComboBox::currentIndexChanged,this,&EditorWindow::changeType);connect(apply_,&QPushButton::clicked,this,[this]{applyDraft();});connect(discard_,&QPushButton::clicked,this,&EditorWindow::discardDraft);connect(raw_,&QPushButton::clicked,this,[this]{showRaw();});
 connect(search_,&QLineEdit::textChanged,this,[this]{updateSearch();});connect(search_,&QLineEdit::returnPressed,this,[this]{nextMatch(1);});
}
void EditorWindow::createActions(){
 fileMenu_=menuBar()->addMenu(QString{});editMenu_=menuBar()->addMenu(QString{});viewMenu_=menuBar()->addMenu(QString{});formatMenu_=viewMenu_->addMenu(QString{});languageMenu_=menuBar()->addMenu(QString{});helpMenu_=menuBar()->addMenu(QString{});
 addLanguageMenu(languageMenu_,languages_,this);toolbar_=addToolBar(QString{});toolbar_->setObjectName("mainToolbar");
 auto add=[this](const QString& name,QMenu* menu,const QKeySequence& shortcut,const std::function<void()>& callback,bool tool=false){
  auto* action=new QAction(this);action->setObjectName(name);action->setShortcut(shortcut);actions_.insert(name,action);menu->addAction(action);if(tool)toolbar_->addAction(action);connect(action,&QAction::triggered,this,callback);
 };
 add("new",fileMenu_,QKeySequence::New,[this]{newDocument();},true);add("open",fileMenu_,QKeySequence::Open,[this]{open();},true);
 add("sample",fileMenu_,{},[this]{QFile sample(":/sample/SampleDictionary.json");if(sample.open(QIODevice::ReadOnly)){const auto bytes=sample.readAll();loadBytes(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())));}});
 add("save",fileMenu_,QKeySequence::Save,[this]{save();},true);add("saveAs",fileMenu_,QKeySequence::SaveAs,[this]{save(true);});fileMenu_->addSeparator();add("close",fileMenu_,QKeySequence::Close,[this]{close();});
 const QList<QString> operations={"add","duplicate","delete","up","down","sort"};
 for(const auto& operation:operations)add(operation,editMenu_,operation=="delete"?QKeySequence(Qt::CTRL|Qt::Key_Delete):QKeySequence{},[this,operation]{perform(operation);},true);
 add("raw",editMenu_,QKeySequence(Qt::CTRL|Qt::Key_R),[this]{showRaw();},true);add("rawDocument",editMenu_,{},[this]{showRaw(true);});editMenu_->addSeparator();add("apply",editMenu_,QKeySequence(Qt::CTRL|Qt::Key_Return),[this]{applyDraft();});add("discard",editMenu_,{},[this]{discardDraft();});
 add("find",viewMenu_,QKeySequence::Find,[this]{search_->setFocus();search_->selectAll();});add("next",viewMenu_,QKeySequence::FindNext,[this]{nextMatch(1);});add("previous",viewMenu_,QKeySequence::FindPrevious,[this]{nextMatch(-1);});add("expand",viewMenu_,{},[this]{tree_->expandAll();});add("collapse",viewMenu_,{},[this]{tree_->collapseAll();});
 for(int i=0;i<4;++i)add(QStringLiteral("format%1").arg(i),formatMenu_,{},[this,i]{if(!resolveDraft())return;session_.mutate([i](jsondict::Document& doc){doc.set_formatting(static_cast<jsondict::Formatting>(i));return true;});refreshState();});
 add("about",helpMenu_,{},[this]{QMessageBox::about(this,ui("About"),ui("JSON Dictionary Editor 1.1.1 — Linux preview\nOrdered JSON, original number text, transactional edits.\nMIT License. Qt is dynamically linked."));});
}
void EditorWindow::retranslate(){
 fileMenu_->setTitle(ui("File"));editMenu_->setTitle(ui("Edit"));viewMenu_->setTitle(ui("View"));formatMenu_->setTitle(ui("Output formatting"));languageMenu_->setTitle(ui("Language / 语言"));helpMenu_->setTitle(ui("Help"));toolbar_->setWindowTitle(ui("Tools"));
 const QMap<QString,const char*> labels={{"new","New"},{"open","Open…"},{"sample","Open English Sample"},{"save","Save"},{"saveAs","Save As…"},{"close","Close"},{"add","Add"},{"duplicate","Duplicate"},{"delete","Delete"},{"up","Move Up"},{"down","Move Down"},{"sort","Sort Object"},{"raw","Raw JSON"},{"rawDocument","Raw JSON — Document"},{"apply","Apply"},{"discard","Discard"},{"find","Find"},{"next","Next Match"},{"previous","Previous Match"},{"expand","Expand All"},{"collapse","Collapse All"},{"format0","Two Spaces"},{"format1","Four Spaces"},{"format2","Tabs"},{"format3","Compact"},{"about","About"}};
 for(auto it=labels.cbegin();it!=labels.cend();++it)actions_[it.key()]->setText(ui(it.value()));
 keyLabel_->setText(ui("Key"));typeLabel_->setText(ui("Type"));valueLabel_->setText(ui("Value"));boolean_->setText(ui("True"));apply_->setText(ui("Apply"));discard_->setText(ui("Discard"));raw_->setText(ui("Raw JSON"));search_->setPlaceholderText(ui("Search keys, values, or types; Enter / F3 for next match"));
 const QSignalBlocker blocker(type_);for(int i=0;i<6;++i)type_->setItemText(i,kindTitle(static_cast<jsondict::Kind>(i)));
 const auto* node=session_.document.find(session_.selected);
 notice_->setText(unsafeKey_||unsafeValue_?ui("Control characters or long text require Raw JSON. The original value is preserved."):node&&node->is_container()?summary(*node):node&&node->kind()==jsondict::Kind::Null?QStringLiteral("null"):QString{});
 model_->retranslate();tree_->viewport()->update();refreshState();
}
bool EditorWindow::hasDraft()const{return(key_->isEnabled()&&key_->text()!=baseKey_)||(value_->isEnabled()&&value_->toPlainText()!=baseValue_)||(boolean_->isEnabled()&&boolean_->isChecked()!=baseBoolean_);}
void EditorWindow::refreshState(){
 if(!actions_.contains("save"))return;
 const auto* node=session_.document.find(session_.selected);const bool root=session_.selected==session_.document.root_id(),draft=hasDraft();
 setWindowTitle((path_.isEmpty()?ui("Untitled"):QFileInfo(path_).fileName())+(session_.dirty?QStringLiteral(" *"):QString{})+QStringLiteral(" — ")+ui("JSON Dictionary Editor"));
 draftLabel_->setText(draft?ui("Unapplied draft"):ui("No pending draft"));apply_->setEnabled(draft);discard_->setEnabled(draft);actions_["apply"]->setEnabled(draft);actions_["discard"]->setEnabled(draft);
 actions_["delete"]->setEnabled(!root);actions_["duplicate"]->setEnabled(!root);actions_["up"]->setEnabled(session_.document.can_move(session_.selected,-1));actions_["down"]->setEnabled(session_.document.can_move(session_.selected,1));actions_["sort"]->setEnabled(node&&node->kind()==jsondict::Kind::Object);actions_["next"]->setEnabled(!matches_.isEmpty());actions_["previous"]->setEnabled(!matches_.isEmpty());
 for(int i=0;i<4;++i){auto* action=actions_[QStringLiteral("format%1").arg(i)];action->setCheckable(true);action->setChecked(i==static_cast<int>(session_.document.formatting()));}
 matchesLabel_->setText(ui("%1 matches").arg(matches_.size()));statusBar()->showMessage(ui("%1 nodes · %2").arg(session_.document.node_count()).arg(session_.dirty?ui("Modified"):ui("No unsaved changes")));
}
void EditorWindow::loadInspector(){
 const bool previous=updating_;updating_=true;const auto* node=session_.document.find(session_.selected);if(!node){updating_=previous;return;}
 const auto location=session_.document.location(session_.selected);pathLabel_->setText(location?checkedText(location->path):QStringLiteral("$"));const bool hasKey=location&&location->key;
 baseKey_=hasKey?checkedText(*location->key):QString{};unsafeKey_=hasKey&&(baseKey_.size()>static_cast<qsizetype>(kMaximumKeyCharacters)||unsafeControl(baseKey_,false));
 key_->setEnabled(hasKey&&!unsafeKey_);key_->setText(unsafeKey_?QString{}:baseKey_);key_->setPlaceholderText(unsafeKey_?ui("Use Raw JSON"):QString{});
 type_->setCurrentIndex(static_cast<int>(node->kind()));type_->setEnabled(session_.selected!=session_.document.root_id());
 const bool scalarText=node->kind()==jsondict::Kind::String||node->kind()==jsondict::Kind::Number;
 baseValue_=node->kind()==jsondict::Kind::String?checkedText(node->as_string()):node->kind()==jsondict::Kind::Number?checkedText(node->as_number().text):QString{};
 unsafeValue_=scalarText&&(baseValue_.size()>static_cast<qsizetype>(kMaximumInspectorCharacters)||unsafeControl(baseValue_,true));value_->setEnabled(scalarText&&!unsafeValue_);value_->setVisible(scalarText);value_->setPlainText(unsafeValue_?QString{}:baseValue_);value_->setPlaceholderText(unsafeValue_?ui("Use Raw JSON"):QString{});
 baseBoolean_=node->kind()==jsondict::Kind::Boolean&&node->as_boolean();boolean_->setEnabled(node->kind()==jsondict::Kind::Boolean);boolean_->setVisible(node->kind()==jsondict::Kind::Boolean);boolean_->setChecked(baseBoolean_);
 notice_->setText(unsafeKey_||unsafeValue_?ui("Control characters or long text require Raw JSON. The original value is preserved."):node->is_container()?summary(*node):node->kind()==jsondict::Kind::Null?QStringLiteral("null"):QString{});
 updating_=previous;refreshState();
}
void EditorWindow::rebuild(){
 QList<jsondict::NodeId> expanded;for(const auto id:model_->allIds())if(tree_->isExpanded(model_->indexForId(id)))expanded.push_back(id);
 const int vertical=tree_->verticalScrollBar()->value(),horizontal=tree_->horizontalScrollBar()->value();updating_=true;model_->rebuild(session_.document);
 if(!session_.document.find(session_.selected))session_.selected=session_.document.root_id();
 for(const auto id:expanded)tree_->setExpanded(model_->indexForId(id),true);
 tree_->setExpanded(model_->indexForId(session_.document.root_id()),true);tree_->setCurrentIndex(model_->indexForId(session_.selected));tree_->verticalScrollBar()->setValue(vertical);tree_->horizontalScrollBar()->setValue(horizontal);loadInspector();updating_=false;updateSearch();refreshState();
}
void EditorWindow::fail(const std::exception& error){refreshState();if(errorHandler)errorHandler(errorText(error));else QMessageBox::critical(this,ui("Error"),errorText(error));}
InspectorDraft EditorWindow::inspectorDraft()const{
 InspectorDraft draft;const auto* node=session_.document.find(session_.selected);if(!node)throw std::runtime_error("The selected node no longer exists.");
 draft.kind=node->kind();if(key_->isEnabled()&&key_->text()!=baseKey_)draft.key=checkedUtf8(key_->text());if(value_->isEnabled()&&value_->toPlainText()!=baseValue_)draft.value=checkedUtf8(value_->toPlainText());if(boolean_->isEnabled()&&boolean_->isChecked()!=baseBoolean_)draft.boolean=boolean_->isChecked();return draft;
}
bool EditorWindow::applyDraft(){
 if(!hasDraft())return true;
 try{
  session_.apply_draft(inspectorDraft());rebuild();return true;
 }catch(const std::exception& error){fail(error);return false;}
}
void EditorWindow::discardDraft(){loadInspector();}
bool EditorWindow::resolveDraft(){if(!hasDraft())return true;const auto decision=draftDecision?draftDecision():askDraft(this);if(decision==DraftDecision::Cancel)return false;if(decision==DraftDecision::Apply)return applyDraft();discardDraft();return true;}
bool EditorWindow::selectNode(jsondict::NodeId id){
 if(!session_.document.find(id))return false;
 if(id!=session_.selected&&!resolveDraft()){const QSignalBlocker blocker(tree_->selectionModel());tree_->setCurrentIndex(model_->indexForId(session_.selected));return false;}
 session_.selected=id;updating_=true;tree_->setCurrentIndex(model_->indexForId(id));loadInspector();updating_=false;return true;
}
void EditorWindow::updateSearch(){matches_=model_->search(search_->text());model_->setMatches(matches_);matchPosition_=-1;tree_->viewport()->update();refreshState();}
void EditorWindow::nextMatch(int direction){
 if(matches_.isEmpty())return;
 const int count=static_cast<int>(matches_.size());const int next=matchPosition_<0?(direction>0?0:count-1):(matchPosition_+direction+count)%count;const auto target=matches_[next];
 if(!selectNode(target))return;
 matchPosition_=matches_.indexOf(target);auto index=model_->indexForId(target);for(auto parent=index.parent();parent.isValid();parent=parent.parent())tree_->expand(parent);tree_->scrollTo(index);
}
void EditorWindow::changeType(int index){
 if(updating_||index<0)return;
 const auto* node=session_.document.find(session_.selected);if(!node)return;const auto original=node->kind();if(index==static_cast<int>(original))return;
 if(!resolveDraft()){const QSignalBlocker blocker(type_);type_->setCurrentIndex(static_cast<int>(original));return;}
 try{session_.mutate([&](jsondict::Document& doc){return doc.change_kind(session_.selected,static_cast<jsondict::Kind>(index));});rebuild();}catch(const std::exception& error){fail(error);loadInspector();}
}
bool EditorWindow::perform(const QString& operation){
 if(!resolveDraft())return false;
 try{
  auto destination=session_.selected;const auto location=session_.document.location(session_.selected);
  const bool changed=session_.mutate([&](jsondict::Document& doc){
   if(operation=="add"){const auto id=doc.add_child(session_.selected);if(id)destination=*id;return id.has_value();}
   if(operation=="duplicate"){const auto id=doc.duplicate_node(session_.selected);if(id)destination=*id;return id.has_value();}
   if(operation=="delete"){if(location&&location->parent_id)destination=*location->parent_id;return doc.delete_node(session_.selected);}
   if(operation=="up")return doc.move_node(session_.selected,-1);
   if(operation=="down")return doc.move_node(session_.selected,1);
   if(operation=="sort")return doc.sort_object(session_.selected);
   throw std::runtime_error("Unknown editor operation.");
  });
  if(changed){session_.selected=destination;rebuild();}return changed;
 }catch(const std::exception& error){fail(error);return false;}
}
bool EditorWindow::resolveUnsaved(){
 // Resolve against a candidate. Save/Cancel cannot consume the live draft.
 std::optional<DraftDecision> draft;
 try{
  auto candidate=session_;
  if(hasDraft()){
   draft=draftDecision?draftDecision():askDraft(this);
   if(*draft==DraftDecision::Cancel)return false;
   if(*draft==DraftDecision::Apply)candidate.apply_draft(inspectorDraft());
  }
  if(!candidate.dirty)return true;
 }catch(const std::exception& error){fail(error);return false;}
 UnsavedDecision decision;if(unsavedDecision)decision=unsavedDecision();else{
  QMessageBox box(QMessageBox::Question,ui("Unsaved document"),ui("Save changes before continuing?"),QMessageBox::NoButton,this);
  auto* saveButton=box.addButton(ui("Save"),QMessageBox::AcceptRole);auto* discardButton=box.addButton(ui("Discard"),QMessageBox::DestructiveRole);auto* cancelButton=box.addButton(ui("Cancel"),QMessageBox::RejectRole);box.setDefaultButton(cancelButton);box.setEscapeButton(cancelButton);box.exec();
  decision=box.clickedButton()==saveButton?UnsavedDecision::Save:box.clickedButton()==discardButton?UnsavedDecision::Discard:UnsavedDecision::Cancel;
 }
 if(decision!=UnsavedDecision::Save)return decision==UnsavedDecision::Discard;
 savingDraftDecision_=draft;const bool saved=save();savingDraftDecision_.reset();return saved;
}
bool EditorWindow::loadBytes(std::string_view bytes){
 if(!resolveUnsaved())return false;
 try{session_.load(bytes);path_.clear();baseline_.reset();uncertainTargets_.clear();rebuild();return true;}catch(const std::exception& error){fail(error);return false;}
}
bool EditorWindow::openPath(const QString& path){
 if(!resolveUnsaved())return false;
 try{const auto snapshot=FileStore::read(path);session_.load(std::string_view(snapshot.bytes.constData(),static_cast<std::size_t>(snapshot.bytes.size())));path_=QFileInfo(path).absoluteFilePath();baseline_=snapshot.stamp;uncertainTargets_.clear();rebuild();return true;}catch(const std::exception& error){fail(error);return false;}
}
void EditorWindow::open(){const auto path=QFileDialog::getOpenFileName(this,ui("Open JSON"),path_,ui("JSON files (*.json);;All files (*)"));if(!path.isEmpty())openPath(path);}
bool EditorWindow::newDocument(){return loadBytes("{}\n");}
bool EditorWindow::saveTo(const QString& path,bool replaceApproved,const SaveOptions& options){
 const auto absolute=QFileInfo(path).absoluteFilePath();
 try{
  if(uncertainTargets_.contains(absolute))throw FileError(FileFailure::Verification,QStringLiteral("Uncertain previous save"),0,false,uncertainTargets_.value(absolute),absolute);
  const bool pending=hasDraft();DraftDecision decision=DraftDecision::Discard;auto candidate=session_;
  if(pending){decision=savingDraftDecision_?*savingDraftDecision_:draftDecision?draftDecision():askDraft(this);if(decision==DraftDecision::Cancel)return false;if(decision==DraftDecision::Apply)candidate.apply_draft(inspectorDraft());}
  const auto expected=absolute==path_?baseline_:replaceApproved?FileStore::probe(absolute):std::nullopt;const auto bytes=candidate.encoded();
  const auto effective=options.hook?options:saveOptionsForTarget?saveOptionsForTarget(absolute):options;
  const auto stamp=FileStore::save(absolute,QByteArray(bytes.data(),static_cast<qsizetype>(bytes.size())),expected,effective);
  baseline_=stamp;path_=absolute;
  if(pending&&decision==DraftDecision::Apply){session_=std::move(candidate);session_.dirty=false;rebuild();}
  else{session_.dirty=false;if(pending)discardDraft();refreshState();}
  statusBar()->showMessage(ui("Saved. Replaced versions remain as .jsondict-backup-* recovery files."),10000);return true;
 }catch(const FileError& error){
  if(error.committed){session_.dirty=true;uncertainTargets_.insert(absolute,error.backup);if(absolute==path_)baseline_.reset();}
  const FileError annotated(error.type,error.operation,error.system_error,error.committed,error.backup,absolute);fail(annotated);return false;
 }catch(const std::exception& error){fail(error);return false;}
}
bool EditorWindow::save(bool saveAs){
 if(!saveAs&&!path_.isEmpty())return saveTo(path_);
 QFileDialog dialog(this,ui("Save JSON"),path_,ui("JSON files (*.json);;All files (*)"));dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setDefaultSuffix("json");dialog.setOption(QFileDialog::DontConfirmOverwrite,true);
 if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().isEmpty())return false;
 const auto target=dialog.selectedFiles().first();bool approved=false;
 if(QFileInfo::exists(target)){approved=QMessageBox::question(this,ui("Replace file?"),ui("Replace the selected existing file?"),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes;if(!approved)return false;}
 return saveTo(target,approved);
}
void EditorWindow::showRaw(bool wholeDocument){
 if(!resolveDraft())return;
 try{RawDialog raw(session_,wholeDocument?session_.document.root_id():session_.selected,languages_,this);raw.exec();rebuild();}catch(const std::exception& error){fail(error);}
}
void EditorWindow::closeEvent(QCloseEvent* event){
 if(!resolveUnsaved()){event->ignore();return;}
 QSettings settings;settings.setValue("windowGeometry",saveGeometry());settings.sync();event->accept();
}
}
