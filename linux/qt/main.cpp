#include "editor_window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>
#include <iostream>
int main(int argc,char** argv){
 QApplication app(argc,argv);QCoreApplication::setOrganizationName("JSONDictionaryEditor");QCoreApplication::setApplicationName("JSONDictionaryEditor");QCoreApplication::setApplicationVersion("1.1.1-linux-preview");
 QCommandLineParser parser;parser.setApplicationDescription("JSON Dictionary Editor — Linux Qt Widgets preview");parser.addHelpOption();parser.addVersionOption();parser.addPositionalArgument("file","UTF-8 object-root JSON file");parser.addOption({"smoke-test","Exit after the window and event loop start (automation only)."});parser.process(app);
 try{
  jsondict_linux::LanguageService languages;app.setWindowIcon(QIcon(":/icons/json-dictionary-editor.png"));jsondict_linux::EditorWindow window(languages);window.show();
  if(!parser.positionalArguments().isEmpty())window.openPath(parser.positionalArguments().first());
  if(parser.isSet("smoke-test"))QTimer::singleShot(300,&app,[&]{std::cout<<"JDE_SMOKE_OK: Qt "<<qVersion()<<"; platform="<<QGuiApplication::platformName().toStdString()<<'\n';app.quit();});
  return app.exec();
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
