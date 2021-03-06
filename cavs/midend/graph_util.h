#ifndef CAVS_MIDEND_GRAPH_UTIL_H_
#define CAVS_MIDEND_GRAPH_UTIL_H_

#include "cavs/midend/node.h"
#include "cavs/midend/scope.h"
#include "cavs/proto/op_def.pb.h"
#include "cavs/proto/func_def.pb.h"
#include "cavs/util/logging.h"

#include <unordered_map>
#include <string>

namespace midend {

class GraphUtil {
 public:
  GraphUtil(Scope* s);
  ScopedNode* AddOptimizerOp(const OpDef& op_def);
  TensorShapeDef AddFunction(const FunctionDef& func_def);

 private:
  OpDef PartialGrad(const Node* node, const std::string& edge);
  bool GenCriticalPath(std::vector<bool>* path,
      std::vector<std::unordered_map<size_t, OpDef>>* grads,
      const Edge* curr,
      const Edge* loss,
      const Scope* scope);
  //bool GenDependencyPath(std::vector<bool>* cpath,
      //const Edge* curr,
      //const Edge* loss,
      //const Scope* scope);
  void GenGradient(Scope* loss_scope,
      const std::vector<bool>& critical_path,
      const std::vector<std::unordered_map<size_t, OpDef>>& grads,
      const Edge* loss_edge);
  void ComputeGradient(Scope* loss_scope, ScopedNode* sn, 
      const std::vector<std::string>& vars,
      const Edge* loss,
      const Scope* main_scope);
  void GradientProcess(Scope* loss_scope,
      const std::vector<std::string>& vars,
      float clip);
  void ApplyGradient(Scope* loss_scope,
      const std::vector<std::string>& vars,
      const std::string& solver,
      const std::string& proj,
      float lr);
  void ComputeGradientForFunction(
      Scope* func_grad_scope,
      const Scope* func_scope);
  void GenGradientForFunction(Scope* func_grad_scope,
      const std::vector<bool>& critical_path,
      const std::vector<std::unordered_map<size_t, OpDef>>& grads,
      const Scope* func_scope);
  Scope* s_;
};

} //namespace midend 

#endif
