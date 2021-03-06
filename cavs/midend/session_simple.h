#ifndef CAVS_MIDEND_SIMPLE_SESSION_H_
#define CAVS_MIDEND_SIMPLE_SESSION_H_

#include "cavs/midend/session_base.h"
#include "cavs/midend/scope.h"
#include "cavs/midend/statement.h"

#include <set>
#include <list>

namespace midend {

class SimpleSession : public SessionBase {
 public:
  SimpleSession(int opt);
  void Run(const std::vector<std::string>& output_names, 
           std::vector<Tensor>* output_tensors,
           const std::vector<std::string>& input_names,
           const std::vector<Tensor>& input_tensors) override;
  int session_type() const override { return SIMPLE; }

 protected:
  virtual void Compile(const std::vector<std::string>& output_names);
  virtual void FeedInput(const std::vector<std::string>& input_names,
                 const std::vector<Tensor>& input_tensors);
  virtual void FetchOutput(const std::vector<std::string>& output_names,
                 std::vector<Tensor>* output_tensors);
  void DepthSearch(Node* curr,
                   std::list<Node*>* critical_path,
                   std::set<Node*>* include);
  std::string HashString(const std::vector<std::string>& input);
  std::unordered_map<std::string, std::vector<Statement*>> executors_;

 protected:
  const Scope* s_;
};

} //namespace midend

#endif
