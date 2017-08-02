#include "cavs/backend/op_impl.h"
#include "cavs/midend/graph_scheduler.h"
#include "cavs/midend/tensor.h"
#include "cavs/util/macros_gpu.h"
#include "cavs/util/op_util.h"

using ::midend::Tensor;
using ::midend::GraphSchedulerBase;
using std::vector;

namespace backend {

template <typename T>
class GraphGatherOp : public OpImpl {
 public:
  explicit GraphGatherOp(const OpDef& def) : OpImpl(def), count_(1) {
    CHECK(def.input_size()  == 0);
    CHECK(def.output_size() == 1);
    CHECK(def.shape_size()  == 1);
    for (auto d : def.shape(0).dim())
      count_ *= d;
    child_offset_ = GetSingleArg<int>(def, "Child");
    CHECK(child_offset_ >= 0);
  }

  void Compute(OpContext* context) override {
    //LOG(FATAL) << "Gather Operator needs further runtime support";
    Tensor* out = context->Output(0);
    GraphSchedulerBase* gs = context->graph_scheduler();
    //int job_id = gs->GetCurrentJobId();
    const vector<int>& job_ids = gs->GetJobId();
    context->SetDynDim(job_ids.size());
    context->ScaleTensor();
    VLOG(V_DEBUG) << "Batching jobs of this round: " << job_ids.size();
    for (int local_id = 0; local_id < job_ids.size(); local_id++) {
      int gid = job_ids[local_id];
      if (gs->HasChild(gid)) {
        CHECK(gs->child_id(gid).size() > child_offset_);
        int child_gid = gs->child_id(gid).at(child_offset_);
        VLOG(V_DEBUG) << "Gathering Child_gid:" << child_gid;
        const Tensor& inp = gs->GetMessagePasser(gs->JobIdToTensorId(child_gid));
        CHECK(inp.count() == count_);
        CHECK(out->count() == inp.count())
              << "Input count:\t" << inp.count()
              << "\t" << inp.debug_size() << "Bytes\n"
              << "Output count:\t" << out->count() 
              << "\t" << out->debug_size() << "Bytes";
        checkCudaError(cudaMemcpy(out->mutable_data<T>()+local_id*inp.count(),
                                  inp.data<T>(),
                                  inp.count()*sizeof(T),
                                  cudaMemcpyDeviceToDevice));
      }else {
        VLOG(V_DEBUG) << "[Gathering] No Child_id, Setting Zero";
        checkCudaError(cudaMemset(out->mutable_data<T>()+local_id*out->count(),
                                  0,
                                  out->count()*sizeof(T)));
      }
    }
  }

 private:
  int count_;
  int child_offset_;
};

template <typename T>
class GraphScatterOp : public OpImpl {
 public:
  explicit GraphScatterOp(const OpDef& def) : OpImpl(def) {}

  void Compute(OpContext* context) override {
    //LOG(FATAL) << "Scatter Operator needs further runtime support";
    const Tensor& inp = context->Input(0);
    Tensor* out = context->Output(0);
    CHECK(out->count() == inp.count())
          << "Input count:\t" << inp.count()
          << "\t" << inp.debug_size() << "Bytes\n"
          << "Output count:\t" << out->count() 
          << "\t" << out->debug_size() << "Bytes";
    checkCudaError(cudaMemcpy(out->mutable_data<T>(),
                              inp.data<T>(),
                              inp.count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
    GraphSchedulerBase* gs = context->graph_scheduler();
    gs->SetMessagePasser(*out);
  }
};

template <typename T>
class GraphPushOp : public OpImpl {
 public:
  explicit GraphPushOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "Push Operator needs further runtime support";
    GraphSchedulerBase* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    const Tensor& inp = context->Input(0);
    Tensor* out = context->Output(0);
    CHECK(out->count() >= inp.count())
          << "Input count:\t" << inp.count()
          << "\t" << inp.debug_size() << "Bytes\n"
          << "Output count:\t" << out->count() 
          << "\t" << out->debug_size() << "Bytes";

    T* out_ptr = out->mutable_data<T>();
    CHECK(!out->IsFullShape());
    checkCudaError(cudaMemcpy(out_ptr,
                              inp.data<T>(),
                              inp.count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
    gs->SetFuncRet(*out);

    inp.DebugNumerical<T>();
    out->DebugNumerical<T>();
  }
};

template <typename T>
class GraphPullOp : public OpImpl {
 public:
  explicit GraphPullOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "Pull Operator needs further runtime support";
    GraphSchedulerBase* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    //const Tensor& inp = context->Input(0);
    const Tensor& inp = gs->GetFuncArg();
    Tensor* out = context->Output(0);
    CHECK(inp.count() >= out->count())
          << "Input count:\t" << inp.count()
          << "\t" << inp.debug_size() << "Bytes\n"
          << "Output count:\t" << out->count() 
          << "\t" << out->debug_size() << "Bytes";

    //out tensor must be local
    //if in tensor is a global tensor(in the backward of pull)
    CHECK(inp.IsFullShape());
    const vector<int>& job_ids = gs->GetJobId();
    for (int local_id = 0; local_id < job_ids.size(); local_id++) {
      int gid = job_ids[local_id];
      //const T* inp_ptr = inp.data<T>() + out->count()*gs->GetJobId();
      checkCudaError(cudaMemcpy(out->mutable_data<T>() + local_id*out->count(),
                                inp.data<T>() + gs->JobIdToTensorId(gid)*out->count(),
                                out->count()*sizeof(T),
                                cudaMemcpyDeviceToDevice));
    }
    inp.DebugNumerical<T>();
    out->DebugNumerical<T>();
  }
};

template <typename T>
class FunctionPushArgOp : public OpImpl {
 public:
  explicit FunctionPushArgOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "here";
    const Tensor& inp = context->Input(0);
    GraphSchedulerBase* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    gs->SetFuncArg(inp);
    inp.DebugNumerical<T>();
  }
};

template <typename T>
class FunctionPopRetOp : public OpImpl {
 public:
  explicit FunctionPopRetOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "here";
    GraphSchedulerBase* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    //checkCudaError(cudaDeviceSynchronize());
    //checkCudaError(cudaGetLastError());
    const Tensor& inp = gs->GetFuncRet();
    Tensor* out = context->Output(0);
    CHECK(inp.count() <= out->count());
    CHECK(inp.debug_size() == out->debug_size());
    checkCudaError(cudaMemcpy(out->mutable_data<T>(),
                              inp.data<T>(),
                              out->count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
    out->DebugNumerical<T>();
  }
};

//REGISTER_OP_IMPL_BUILDER(Key("GraphOutput").Device("GPU"), GraphOutputOp<float>);
//REGISTER_OP_IMPL_BUILDER(Key(GetGradientName("GraphOutput")).Device("GPU"), GraphOutputGradOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Pull").Device("GPU"),    GraphPullOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Push").Device("GPU"),    GraphPushOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Scatter").Device("GPU"), GraphScatterOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Gather").Device("GPU"),  GraphGatherOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("FunctionPushArg").Device("GPU"), FunctionPushArgOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("FunctionPopRet").Device("GPU"), FunctionPopRetOp<float>);

} //namespace backend
