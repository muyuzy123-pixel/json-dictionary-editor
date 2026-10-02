#pragma once
#include "tree_model.hpp"
#include "file_store.hpp"
#include "language.hpp"
#include <QMainWindow>
#include <QDialog>
#include <QMap>
#include <functional>
class QTreeView;class QLineEdit;class QPlainTextEdit;class QComboBox;class QCheckBox;class QLabel;class QPushButton;class QAction;class QMenu;class QToolBar;
namespace jsondict_linux {
enum class DraftDecision{Apply,Discard,Cancel};
enum class UnsavedDecision{Save,Discard,Cancel};
DraftDecision askDraft(QWidget*);
void addLanguageMenu(QMenu*,LanguageService&,QWidget*);
class RawDialog:public QDialog{
 Q_OBJECT
public:
 RawDialog(EditorSession&,jsondict::NodeId,LanguageService&,QWidget* parent=nullptr);
 std::function<DraftDecision()> draftDecision;
 std::function<void(const QString&)> errorHandler;
 bool apply();bool check();bool format();bool hasDraft() const;void reject() override;
private:
 void retranslate();void fail(const std::exception&);void closeEvent(QCloseEvent*) override;
 EditorSession& session_;jsondict::NodeId target_;QPlainTextEdit* edit_;QLabel* status_;QMenu* languageMenu_;
 QPushButton *check_,*format_,*apply_,*cancel_;QString original_;
};
class EditorWindow:public QMainWindow{
 Q_OBJECT
public:
 explicit EditorWindow(LanguageService&,QWidget* parent=nullptr);
 const EditorSession& session()const{return session_;}TreeModel* treeModel()const{return model_;}QString filePath()const{return path_;}
 bool hasDraft()const;bool resolveDraft();bool applyDraft();void discardDraft();bool selectNode(jsondict::NodeId);
 bool loadBytes(std::string_view);bool openPath(const QString&);
 bool saveTo(const QString&,bool replaceApproved=false,const SaveOptions& options={});
 bool newDocument();bool perform(const QString&);void showRaw(bool wholeDocument=false);
 std::function<DraftDecision()> draftDecision;
 std::function<UnsavedDecision()> unsavedDecision;
 std::function<void(const QString&)> errorHandler;
private:
 void createUi();void createActions();void retranslate();void rebuild();void loadInspector();void refreshState();void updateSearch();void nextMatch(int);void changeType(int);void fail(const std::exception&);
 bool resolveUnsaved();bool save(bool saveAs=false);void open();void closeEvent(QCloseEvent*)override;
 EditorSession session_;LanguageService& languages_;QString path_;std::optional<FileStamp> baseline_;TreeModel* model_;QTreeView* tree_;
 QLineEdit *key_,*search_;QPlainTextEdit* value_;QComboBox* type_;QCheckBox* boolean_;
 QLabel *pathLabel_,*keyLabel_,*typeLabel_,*valueLabel_,*notice_,*draftLabel_,*matchesLabel_;
 QPushButton *apply_,*discard_,*raw_;QMenu *fileMenu_,*editMenu_,*viewMenu_,*languageMenu_,*formatMenu_,*helpMenu_;QToolBar* toolbar_;
 QMap<QString,QAction*> actions_;QString baseKey_,baseValue_;bool baseBoolean_=false;bool updating_=false;bool unsafeKey_=false,unsafeValue_=false;
 QList<jsondict::NodeId> matches_;int matchPosition_=-1;
};
}
