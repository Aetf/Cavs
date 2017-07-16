#include "cavs/midend/op_context.h"

#include <string>

using std::string;
using std::unordered_map;

namespace midend {

unordered_map<string, void*> OpContext::repo_;
int OpContext::dyn_dim_ = -1;

void OpContext::SetTensorOffset() {
  int job_id = 0;
  if (gs_ && (job_id = gs_->GetJobId()) >= 0) {
    //input'id should be set, think about the graphoutput_grad case
    //we dont't change the value or the size of input tensor buffer,
    //just choose the right offset of the input tensor buffer
    for (auto* t : inputs_) {
      if (!const_cast<Tensor*>(t)->SetOffsetWithId(job_id))
        VLOG(V_DEBUG) << t->name() << " must be a global tensor, "
                      << "and referenced as an input in a function";
    }
    for (auto* t : outputs_) {
      if (!t->SetOffsetWithId(job_id)) 
        VLOG(V_DEBUG) << t->name() << " must be a global tensor, "
                      << "and referenced as an output in a function";
    }
    
  }
}

void OpContext::ScaleTensor() {
  //Input tensor buffer size should never be modified in the operator
  //for (auto& t : inputs_) {
    //if (t.IsDynamicSize() && t.dims(0) != dyn_dim()) {
      //VLOG(V_DEBUG) << t.name() << " [INPUT] first dimension change from "
                    //<< t.dims(0) << " to " << dyn_dim();
      //t.ScaleDynamicDimension(dyn_dim());
    //} 
  //}
  for (auto* t : outputs_) {
    if (t->IsDynamicSize() && t->dims(0) != dyn_dim()) {
      VLOG(V_DEBUG) << t->name() << " [OUTPUT] first dimension change from "
                    << t->dims(0) << " to " << dyn_dim();
      t->ScaleDynamicDimension(dyn_dim());
    } 
  }
}

void OpContext::SetZero() {
  for (auto* t : outputs_) {
    if (t->ZeroInitEnforced()) {
      t->InitWithZero(round());
    }
  }
}

string OpContext::debug_info() const {
  string info;
  for (unsigned i = 0; i < inputs_.size(); i++) {
    info += "input tensor[" + std::to_string(i)
            + "]:\t" + inputs_[i]->name();
    info += "\n";
  }
  for (unsigned i = 0; i < outputs_.size(); i++) {
    info += "output tensor[" + std::to_string(i)
            + "]:\t" + outputs_[i]->name();
    info += "\n";
  }
  return info;
}

} //namespace midend
