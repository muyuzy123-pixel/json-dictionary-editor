#pragma once
#include "editor_session.hpp"
#include <QAbstractItemModel>
#include <QHash>
#include <QSet>
#include <memory>
#include <vector>
namespace jsondict_linux {
class TreeModel : public QAbstractItemModel {
 Q_OBJECT
public:
 explicit TreeModel(QObject* parent = nullptr);
 void rebuild(const jsondict::Document&);
 QModelIndex index(int row,int column,const QModelIndex& parent={}) const override;
 QModelIndex parent(const QModelIndex&) const override;
 int rowCount(const QModelIndex& parent={}) const override;
 int columnCount(const QModelIndex& = {}) const override { return 3; }
 QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
 QVariant headerData(int,Qt::Orientation,int) const override;
 QModelIndex indexForId(jsondict::NodeId,int column=0) const;
 jsondict::NodeId id(const QModelIndex&) const;
 QList<jsondict::NodeId> search(const QString&) const;
 QList<jsondict::NodeId> allIds() const;
 void setMatches(const QList<jsondict::NodeId>&);
 void retranslate();
private:
 struct Entry { const jsondict::Node* node; Entry* parent; const std::string* key; int row; std::vector<Entry*> children; };
 Entry* append(const jsondict::Node&,Entry*,const std::string*,int);
 std::vector<std::unique_ptr<Entry>> entries_;
 QHash<jsondict::NodeId,Entry*> byId_;
 QSet<jsondict::NodeId> matches_;
};
}
