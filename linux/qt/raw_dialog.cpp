#include "editor_window.hpp"
#include <QActionGroup>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
namespace jsondict_linux {
DraftDecision askDraft(QWidget* parent){
 QMessageBox box(QMessageBox::Question,ui("Unapplied draft"),ui("Apply this draft, discard it, or cancel the action?"),QMessageBox::NoButton,parent);
 auto* apply=box.addButton(ui("Apply"),QMessageBox::AcceptRole);auto* discard=box.addButton(ui("Discard"),QMessageBox::DestructiveRole);auto* cancel=box.addButton(ui("Cancel"),QMessageBox::RejectRole);
 box.setDefaultButton(cancel);box.setEscapeButton(cancel);box.exec();
 return box.clickedButton()==apply?DraftDecision::Apply:box.clickedButton()==discard?DraftDecision::Discard:DraftDecision::Cancel;
}
void addLanguageMenu(QMenu* menu,LanguageService& languages,QWidget* owner){
 auto* group=new QActionGroup(menu);
 const QStringList labels={QStringLiteral("System / 跟随系统"),QStringLiteral("简体中文"),QStringLiteral("English")};
 for(int i=0;i<3;++i){
  auto* action=menu->addAction(labels[i]);action->setObjectName(QStringLiteral("language%1").arg(i));action->setCheckable(true);action->setData(i);group->addAction(action);action->setChecked(static_cast<int>(languages.preference())==i);
  QObject::connect(action,&QAction::triggered,owner,[&languages,owner,i]{
   try{if(!languages.select(static_cast<LanguageService::Preference>(i)))QMessageBox::warning(owner,ui("Preferences"),ui("Language changed, but the preference could not be saved."));}
   catch(const std::exception& error){QMessageBox::critical(owner,ui("Error"),errorText(error));}
  });
 }
 QObject::connect(&languages,&LanguageService::changed,menu,[group,&languages]{for(auto* action:group->actions())action->setChecked(action->data().toInt()==static_cast<int>(languages.preference()));});
}
RawDialog::RawDialog(EditorSession& session,jsondict::NodeId target,LanguageService& languages,QWidget* parent):QDialog(parent),session_(session),target_(target){
 const auto bytes=session.document.encoded_text(target);if(bytes.size()>kMaximumRawBytes)throw std::runtime_error("The raw JSON draft exceeds 4 MiB.");
 original_=checkedText(bytes);setObjectName("rawDialog");resize(760,560);auto* layout=new QVBoxLayout(this);auto* bar=new QMenuBar(this);languageMenu_=bar->addMenu(QString{});addLanguageMenu(languageMenu_,languages,this);layout->setMenuBar(bar);
 edit_=new QPlainTextEdit(this);edit_->setObjectName("rawEdit");edit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));edit_->setLineWrapMode(QPlainTextEdit::NoWrap);edit_->setPlainText(original_);layout->addWidget(edit_,1);
 status_=new QLabel(this);status_->setWordWrap(true);layout->addWidget(status_);auto* buttons=new QHBoxLayout;
 check_=new QPushButton(this);check_->setObjectName("rawCheck");buttons->addWidget(check_);format_=new QPushButton(this);format_->setObjectName("rawFormat");buttons->addWidget(format_);buttons->addStretch();
 apply_=new QPushButton(this);apply_->setObjectName("rawApply");buttons->addWidget(apply_);cancel_=new QPushButton(this);cancel_->setObjectName("rawCancel");buttons->addWidget(cancel_);layout->addLayout(buttons);
 connect(check_,&QPushButton::clicked,this,[this]{check();});connect(format_,&QPushButton::clicked,this,[this]{format();});connect(apply_,&QPushButton::clicked,this,[this]{apply();});connect(cancel_,&QPushButton::clicked,this,&RawDialog::reject);connect(&languages,&LanguageService::changed,this,&RawDialog::retranslate);retranslate();
}
void RawDialog::retranslate(){
 setWindowTitle(ui("Raw JSON"));languageMenu_->setTitle(ui("Language / 语言"));check_->setText(ui("Check"));format_->setText(ui("Format"));apply_->setText(ui("Apply"));cancel_->setText(ui("Cancel"));status_->setText(ui("Raw edits are transactional. Maximum UTF-8 size: 4 MiB."));
}
bool RawDialog::hasDraft()const{return edit_->toPlainText()!=original_;}
void RawDialog::fail(const std::exception& error){status_->setText(errorText(error));if(errorHandler)errorHandler(errorText(error));}
bool RawDialog::check(){
 try{auto candidate=session_;candidate.apply_raw(target_,checkedUtf8(edit_->toPlainText()));status_->setText(ui("Valid JSON; document and capacity checks passed."));return true;}
 catch(const std::exception& error){fail(error);return false;}
}
bool RawDialog::format(){
 try{
  auto candidate=session_;const auto text=checkedUtf8(edit_->toPlainText());candidate.apply_raw(target_,text);
  const auto formatted=jsondict::write(jsondict::parse(text),{session_.document.formatting(),false,target_==session_.document.root_id()});
  if(formatted.size()>kMaximumRawBytes)throw std::runtime_error("The raw JSON draft exceeds 4 MiB.");
  edit_->setPlainText(checkedText(formatted));status_->setText(ui("Formatted draft; Apply to update the document."));return true;
 }catch(const std::exception& error){fail(error);return false;}
}
bool RawDialog::apply(){try{session_.apply_raw(target_,checkedUtf8(edit_->toPlainText()));original_=edit_->toPlainText();accept();return true;}catch(const std::exception& error){fail(error);return false;}}
void RawDialog::reject(){
 if(hasDraft()){const auto decision=draftDecision?draftDecision():askDraft(this);if(decision==DraftDecision::Cancel)return;if(decision==DraftDecision::Apply){apply();return;}}
 QDialog::reject();
}
void RawDialog::closeEvent(QCloseEvent* event){reject();if(isVisible())event->ignore();else event->accept();}
}
