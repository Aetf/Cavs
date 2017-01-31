#ifndef CAVS_MIDEND_EDGE_H_
#define CAVS_MIDEND_EDGE_H_

#include "cavs/midend/scope.h"
#include "cavs/midend/node.h"
#include "cavs/proto/op_def.pb.h"
#include "cavs/util/logging.h"

#include <string>
#include <vector>
#include <algorithm>

namespace midend {

class Scope;
class Node;
extern const Scope* GetGlobalScope();

class Edge {
 public:
  explicit Edge(const std::string& name, bool stateful, 
      const Scope* s = GetGlobalScope())
    : tensor_name_(name), stateful_(stateful),
      s_(s), sink_(NULL) {} 
  bool isStateful() const { return stateful_; }
  inline const std::string& name() const {
    return tensor_name_;
  }
  inline void SetShape(const TensorShapeDef& def) {
    tensor_shape_ = def;  
  }
  inline const TensorShapeDef& shape() const {
    return tensor_shape_; 
  }
  inline void AddSource(Node* node) {
    CHECK(!stateful_ || src_.empty());
    src_.push_back(node); 
  }
  void AddDst(Node* node);
  inline void RemoveDst(Node* node) {
    std::remove(dst_.begin(), dst_.end(), node); 
  }
  inline const Node* src(size_t i) {
    CHECK(i < src_.size());
    return src_[i];
  }
  inline int src_size() const {
    return src_.size();
  }
  inline const Node* dst(size_t i) {
    CHECK(i < dst_.size());
    return dst_[i];
  }
  inline int dst_size() const {
    return dst_.size();
  }

 private:
  Node* sink_;
  std::string tensor_name_;
  TensorShapeDef tensor_shape_;
  std::vector<Node*> src_;
  std::vector<Node*> dst_;
  bool stateful_;
  const Scope* s_;
};

} //namespace midend

#endif
