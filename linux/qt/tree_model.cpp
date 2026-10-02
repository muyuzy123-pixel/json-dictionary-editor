#include "tree_model.hpp"
#include "language.hpp"
#include "json_search_aliases.hpp"
#include <QBrush>
#include <QApplication>
#include <QPalette>
namespace jsondict_linux {
TreeModel::TreeModel(QObject* parent):QAbstractItemModel(parent){}
TreeModel::Entry* TreeModel::append(const jsondict::Node& node,Entry* parent,const std::string* key,int row) {
 auto owned=std::make_unique<Entry>(Entry{&node,parent,key,row,{}});
 auto* entry=owned.get(); entries_.push_back(std::move(owned)); byId_.insert(node.id(),entry);
 int child=0;
 if(node.kind()==jsondict::Kind::Object) for(const auto& member:node.as_object()) entry->children.push_back(append(member.value,entry,&member.key,child++));
 if(node.kind()==jsondict::Kind::Array) for(const auto& value:node.as_array()) entry->children.push_back(append(value,entry,nullptr,child++));
 return entry;
}
void TreeModel::rebuild(const jsondict::Document& doc){
 beginResetModel();byId_.clear();entries_.clear();matches_.clear();entries_.reserve(doc.node_count());append(doc.root(),nullptr,nullptr,0);endResetModel();
}
QModelIndex TreeModel::index(int row,int column,const QModelIndex& parent) const{
 if(row<0||column<0||column>=3||(parent.isValid()&&parent.column()!=0))return {};
 if(!parent.isValid())return row==0&&!entries_.empty()?createIndex(row,column,entries_[0].get()):QModelIndex{};
 const auto* entry=static_cast<Entry*>(parent.internalPointer());
 return static_cast<std::size_t>(row)<entry->children.size()?createIndex(row,column,entry->children[static_cast<std::size_t>(row)]):QModelIndex{};
}
QModelIndex TreeModel::parent(const QModelIndex& child) const{
 if(!child.isValid())return {};
 auto* entry=static_cast<Entry*>(child.internalPointer());
 return entry->parent?createIndex(entry->parent->row,0,entry->parent):QModelIndex{};
}
int TreeModel::rowCount(const QModelIndex& parent) const{
 if(!parent.isValid())return entries_.empty()?0:1;
 return parent.column()==0?static_cast<int>(static_cast<Entry*>(parent.internalPointer())->children.size()):0;
}
jsondict::NodeId TreeModel::id(const QModelIndex& index) const{return index.isValid()?static_cast<Entry*>(index.internalPointer())->node->id():0;}
QModelIndex TreeModel::indexForId(jsondict::NodeId id,int column) const{auto* entry=byId_.value(id,nullptr);return entry?createIndex(entry->row,column,entry):QModelIndex{};}
QVariant TreeModel::data(const QModelIndex& index,int role) const{
 if(!index.isValid())return {};
 const auto* entry=static_cast<Entry*>(index.internalPointer());
 if(role==Qt::UserRole)return QVariant::fromValue<qulonglong>(entry->node->id());
 if(role==Qt::BackgroundRole&&matches_.contains(entry->node->id()))return QApplication::palette().brush(QPalette::AlternateBase);
 if(role!=Qt::DisplayRole&&role!=Qt::ToolTipRole)return {};
 if(index.column()==0){
  if(!entry->parent)return ui("Root Object");
  if(!entry->key)return QStringLiteral("[%1]").arg(entry->row);
  auto text=checkedText(*entry->key);text.replace('\n',QStringLiteral("↩")).replace('\r',QStringLiteral("↩")).replace(QChar(0),QStringLiteral("\\u0000"));
  return role==Qt::ToolTipRole?text:text.left(160);
 }
 if(index.column()==1)return kindTitle(entry->node->kind());
 return summary(*entry->node);
}
QVariant TreeModel::headerData(int section,Qt::Orientation orientation,int role) const{
 if(role!=Qt::DisplayRole||orientation!=Qt::Horizontal)return {};
 const char* labels[]={"Key / Index","Type","Value"};
 return section>=0&&section<3?ui(labels[section]):QString{};
}
QList<jsondict::NodeId> TreeModel::search(const QString& query) const{
 QList<jsondict::NodeId> results;if(query.isEmpty())return results;
 for(const auto& entry:entries_){
  const auto& node=*entry->node;QString text=QString::fromStdWString(jsondict::search_aliases(node));
  if(entry->key)text+='\n'+checkedText(*entry->key);
  if(node.kind()==jsondict::Kind::String)text+='\n'+checkedText(node.as_string());
  if(node.kind()==jsondict::Kind::Number)text+='\n'+checkedText(node.as_number().text);
  if(node.kind()==jsondict::Kind::Boolean)text+=node.as_boolean()?QStringLiteral("\ntrue"):QStringLiteral("\nfalse");
  if(text.contains(query,Qt::CaseInsensitive))results.push_back(node.id());
 }
 return results;
}
QList<jsondict::NodeId> TreeModel::allIds() const{QList<jsondict::NodeId> ids;for(const auto& e:entries_)ids.push_back(e->node->id());return ids;}
void TreeModel::setMatches(const QList<jsondict::NodeId>& ids){matches_=QSet<jsondict::NodeId>(ids.begin(),ids.end());}
void TreeModel::retranslate(){emit headerDataChanged(Qt::Horizontal,0,2);}
}
