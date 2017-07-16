#include "cavs/backend/op_impl.h"
#include "cavs/midend/graph_scheduler.h"
#include "cavs/midend/tensor.h"
#include "cavs/util/macros_gpu.h"
#include "cavs/util/op_util.h"

using ::midend::Tensor;
using ::midend::GraphScheduler;

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
    GraphScheduler* gs = context->graph_scheduler();
    //int job_id = gs->GetCurrentJobId();
    if (gs->HasChild()) {
      CHECK(gs->child_id().size() > child_offset_);
      int child_id = gs->child_id().at(child_offset_);
      const Tensor& inp = gs->GetMessagePasser(child_id);
      CHECK(inp.count() == count_);
      CHECK(out->count() == inp.count())
            << "Input count:\t" << inp.count()
            << "\t" << inp.debug_size() << "Bytes\n"
            << "Output count:\t" << out->count() 
            << "\t" << out->debug_size() << "Bytes";
      checkCudaError(cudaMemcpy(out->mutable_data<T>(),
                                inp.data<T>(),
                                inp.count()*sizeof(T),
                                cudaMemcpyDeviceToDevice));
    }else {
      checkCudaError(cudaMemset(out->mutable_data<T>(),
                                0,
                                out->count()*sizeof(T)));
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
    GraphScheduler* gs = context->graph_scheduler();
    gs->SetMessagePasser(*out);
  }
};

template <typename T>
class GraphPushOp : public OpImpl {
 public:
  explicit GraphPushOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "Push Operator needs further runtime support";
    GraphScheduler* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    const Tensor& inp = context->Input(0);
    Tensor* out = context->Output(0);
    CHECK(out->count() >= inp.count())
          << "Input count:\t" << inp.count()
          << "\t" << inp.debug_size() << "Bytes\n"
          << "Output count:\t" << out->count() 
          << "\t" << out->debug_size() << "Bytes";

    T* out_ptr = out->mutable_data<T>();
    //if out tensor is a global tensor(in the backward of pull)
    //in tensor must be local
    if (out->IsFullShape()) {
      out_ptr += inp.count()*gs->GetJobId();
    }
    checkCudaError(cudaMemcpy(out_ptr,
                              inp.data<T>(),
                              inp.count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
    gs->SetMessagePusher(*out);
  }
};

template <typename T>
class GraphPullOp : public OpImpl {
 public:
  explicit GraphPullOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "Pull Operator needs further runtime support";
    GraphScheduler* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    const Tensor& inp = context->Input(0);
    Tensor* out = context->Output(0);
    CHECK(inp.count() >= out->count())
          << "Input count:\t" << inp.count()
          << "\t" << inp.debug_size() << "Bytes\n"
          << "Output count:\t" << out->count() 
          << "\t" << out->debug_size() << "Bytes";

    //out tensor must be local
    //if in tensor is a global tensor(in the backward of pull)
    const T* inp_ptr = inp.data<T>();
    if (inp.IsFullShape()) {
      inp_ptr += out->count()*gs->GetJobId();
    }
    checkCudaError(cudaMemcpy(out->mutable_data<T>(),
                              inp_ptr,
                              out->count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
    inp.DebugNumerical<T>();
    out->DebugNumerical<T>();
  }
};

template <typename T>
class FetchUpperGradOp : public OpImpl {
 public:
  explicit FetchUpperGradOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "here";
    GraphScheduler* gs = context->graph_scheduler();
    const Tensor& inp = gs->GetMessagePusher();
    Tensor* out = context->Output(0);
    CHECK(out->count() < inp.count());
    CHECK(out->debug_size() == inp.debug_size());
    VLOG(V_DEBUG) << "here";
    CHECK(inp.IsFullShape());
    VLOG(V_DEBUG) << "here";
    CHECK(!out->IsFullShape());
    VLOG(V_DEBUG) << "here";
    out->SetOffsetWithId(0);
    VLOG(V_DEBUG) << "here";
    checkCudaError(cudaMemcpy(out->mutable_data<T>(),
                              inp.data<T>(),
                              inp.count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
  }
};

template <typename T>
class GraphOutputOp : public OpImpl {
 public:
  explicit GraphOutputOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "graphoutput Operator needs further runtime support";
    GraphScheduler* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    const Tensor& inp = gs->GetMessagePusher();
    Tensor* out = context->Output(0);
    CHECK(out->debug_size() == inp.debug_size())
          << "Input count:\t" << inp.count()
          << "\t" << inp.debug_size() << "Bytes\n"
          << "Output count:\t" << out->count() 
          << "\t" << out->debug_size() << "Bytes";
    checkCudaError(cudaMemcpy(out->mutable_data<T>(),
                              inp.data<T>(),
                              out->count()*sizeof(T),
                              cudaMemcpyDeviceToDevice));
  }
};

template <typename T>
class GraphOutputGradOp : public OpImpl {
 public:
  explicit GraphOutputGradOp(const OpDef& def) : OpImpl(def) {}
  void Compute(OpContext* context) override {
    //LOG(FATAL) << "graphoutputgrad Operator needs further runtime support";
    const Tensor& dy = context->Input(0);
    GraphScheduler* gs = context->graph_scheduler();
    CHECK_NOTNULL(gs);
    gs->SetMessagePusher(dy);
  }
};

REGISTER_OP_IMPL_BUILDER(Key("GraphOutput").Device("GPU"), GraphOutputOp<float>);
REGISTER_OP_IMPL_BUILDER(Key(GetGradientName("GraphOutput")).Device("GPU"), GraphOutputGradOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Pull").Device("GPU"),    GraphPullOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Push").Device("GPU"),    GraphPushOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Scatter").Device("GPU"), GraphScatterOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("Gather").Device("GPU"),  GraphGatherOp<float>);
REGISTER_OP_IMPL_BUILDER(Key("FetchUpperGrad").Device("GPU"), FetchUpperGradOp<float>);

} //namespace backend
