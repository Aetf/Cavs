#include "cavs/midend/tensor.h"
#include "cavs/util/types.h"
#include "cavs/util/logging.h"
#include "cavs/util/macros_gpu.h"

#include <iomanip>

using std::string;
using std::vector;

namespace midend {

#define CASE(TYPE, STMTS)                             \
  case DataTypeToEnum<TYPE>::value: {                 \
    typedef TYPE T;                                   \
    STMTS;                                            \
    break;                                            \
  }

#define CASES(TYPE_ENUM, STMTS)                       \
  switch (TYPE_ENUM) {                                \
    CASE(float, STMTS)                                \
    CASE(double, STMTS)                               \
    CASE(int, STMTS)                                  \
    default:                                          \
      LOG(FATAL) << "Unsupported type:" << TYPE_ENUM; \
      break;                                          \
  }

template <typename T>
class TensorBuffer : public TensorBufferBase {
 public:
  TensorBuffer(Allocator* alloc, size_t elem) 
      : TensorBufferBase(alloc), data_(NULL), elem_(elem) {
    if (elem_ > 0)
      data_ = alloc->Allocate<T>(elem_);   
  }
  ~TensorBuffer() override { alloc_->Deallocate<T>(data_); }
  FORCE_INLINE void* data() const override  { return data_; }
  FORCE_INLINE size_t size() const override { return elem_*sizeof(T); }
  FORCE_INLINE void InitWithZero() override {
    alloc_->InitWithZero(data(), size());
  }
  FORCE_INLINE void Resize(size_t size) override {
    CHECK(size % sizeof(T) == 0);
    CHECK(size != elem_*sizeof(T));
    if (data_) { alloc_->Deallocate<T>(data_); }
    data_ = alloc_->Allocate<T>(size/sizeof(T));   
    elem_ = size/sizeof(T);
  }

 private:
  T* data_;
  int elem_;

  DISALLOW_COPY_AND_ASSIGN(TensorBuffer);
};

string TensorShape::debug_info() const {
  string ret; 
  for (auto& s : shape_)
    ret += std::to_string(s) + ",";
  return ret;
}

string Tensor::debug_info() const {
  string ret; 
  ret += "\nname: " + name_;
  ret += "\nshape: " + shape_.debug_info();
  ret += "\ntype: " + std::to_string(params_->type);
  return ret;
}

template <>
void Tensor::DebugNumerical<float>() const {
  if (VLOG_IS_ON(V_EXHAUSTIVE_DEBUG)) {
    vector<float> res(count());
    if (device_type() == GPU) {
      checkCudaError(cudaMemcpy(res.data(), data<float>(),
            count()*sizeof(float), cudaMemcpyDeviceToHost));
    }else {
      checkCudaError(cudaMemcpy(res.data(), data<float>(),
            count()*sizeof(float), cudaMemcpyHostToHost));
    }
    VLOG(V_EXHAUSTIVE_DEBUG) << debug_info();
    float L2_norm = 0;
    float checksum = 0;
    for (int i = 0; i < count(); i++) {
      L2_norm += res[i]*res[i]; 
      checksum += res[i];
    }
    float L2_norm_unit = 0;
    float checksum_unit = 0;
    for (int i = 0; i < count()/dims(0); i++) {
      L2_norm_unit += res[i]*res[i]; 
      checksum_unit += res[i];
    }
    VLOG(V_DEBUG) << "pointer-addr:\t" << buf_.get();
    VLOG(V_DEBUG) << "dynamic: " << params_->dynamic;
    VLOG(V_DEBUG) << "offset: " << params_->offset;
    VLOG(V_EXHAUSTIVE_DEBUG) << name() << std::setprecision(15)
      << "\tL2 Norm:\t" << L2_norm
      << "\t checksum:\t" << checksum;
    VLOG(V_EXHAUSTIVE_DEBUG) << name() << std::setprecision(15)
      << "\tL2 Norm Unit:\t" << L2_norm_unit
      << "\t checksum Unit:\t" << checksum_unit;
    for (int i = 0; i < 10 && i < count(); i++)
      VLOG(V_EXHAUSTIVE_DEBUG) << name() << "[" << i << "]: "
                << std::setprecision(15) << res[i];
    L2_norm = 0;
    checksum  = 0;
  }
}

Tensor::Tensor() : buf_(nullptr), name_(""), params_(nullptr) {}

Tensor::Tensor(const string& name, Allocator *a, 
               DataType type, const TensorShape& shape) 
    : buf_(nullptr), name_(name) {
  params_.reset(new Params());
  params_->type = type;
  CHECK(shape.dim() > 0);
  if (shape.dim(0) == -1) {
    params_->dynamic = true;
    shape_ = shape;
    CASES(params_->type, buf_.reset(new TensorBuffer<T>(a, 0)));
  }else {
    CHECK(shape.n_elements() > 0);
    Rebase(a, params_->type, shape);
  }
}

Tensor::Tensor(const string& name, Allocator *a, 
               DataType type, TensorShape&& shape) 
    : buf_(nullptr), name_(name) {
  params_.reset(new Params());
  params_->type = type;
  CHECK(shape.dim() > 0);
  if (shape.dim(0) == -1) {
    params_->dynamic = true;
    shape_ = std::move(shape);
    CASES(params_->type, buf_.reset(new TensorBuffer<T>(a, 0)));
  }else {
    CHECK(shape.n_elements() > 0);
    Rebase(a, params_->type, std::move(shape));
  }
}

//for sharing buffer operators like reshape
Tensor::Tensor(const std::string& name, const Tensor& t) {
  buf_ = t.buf_;
  name_ = name;
  shape_  = t.shape_;
  params_.reset(new Params());
  params_->type = t.params_->type;
  //I don't know whether it is necessary for the followings
  //After one-day debugging, I found assign operator(share memory)
  //lead to one buffer initialized twice. It is a fatal bug.
  //share memory is an optimization to avoid redudant memory copy/allocation
  //but the output tensor should have its own behavior.
  //So the following configuration should not be copied
  //Actually, the reset shape is called 
  //params_->offset = t.params_->offset;
  //params_->dynamic = t.params_->dynamic;
  //params_->zero_init_enforced = t.params_->zero_init_enforced;
  //params_->iteration = t.params_->iteration;
} 

Tensor& Tensor::operator =(const Tensor& t) {
  buf_    = t.buf_;
  shape_  = t.shape_;
  name_   = t.name_;
  params_ = t.params_;
  return *this;
}

void Tensor::Rebase(Allocator *a, 
        DataType type, const TensorShape& shape) {
  //CHECK_NOTNULL(params_.get());
  if (!params_)  params_.reset(new Params());
  params_->type = type;
  shape_ = shape;
  //CASES(type, buf_.reset(new TensorBuffer<T>(a, shape_.n_elements())));
  CASES(params_->type, buf_.reset(new TensorBuffer<T>(a, shape_.n_elements())));
}

void Tensor::Rebase(Allocator *a, 
        DataType type, TensorShape&& shape) {
  //CHECK_NOTNULL(params_.get());
  if (!params_)  params_.reset(new Params());
  params_->type = type;
  shape_ = std::move(shape);
  //CASES(type, buf_.reset(new TensorBuffer<T>(a, shape_.n_elements())));
  CASES(params_->type, buf_.reset(new TensorBuffer<T>(a, shape_.n_elements())));
}

void Tensor::Rebase(Allocator *a, const Tensor& t) {
  CHECK_NOTNULL(t.params_.get());
  Rebase(a, t.params_->type, t.shape_);
}

void Tensor::Reshape(const TensorShapeDef& shape) {
  CHECK(shape.dim_size() > 0);
  CHECK_NOTNULL(params_.get());
  int new_counts = 1;
  for (auto& dim : shape.dim())
    new_counts *= dim;
  if (shape.dim(0) == -1) {
    params_->dynamic = true;
  }else {
    CHECK(new_counts == count())
      << new_counts << "\t" << count() << name();
  }
  //if (new_counts != count()) {
    //CHECK(shape.dim(0) == -1) << new_counts << "\tvs\t" << count();
    ////dynamic_ = true;
    //params_->dynamic = true;
  //}
  shape_ = TensorShape(shape);
}

void Tensor::Reshape(const vector<int>& dims) {
  CHECK(!dims.empty());
  CHECK_NOTNULL(params_.get());
  int new_counts = 1;
  for (auto& dim : dims)
    new_counts *= dim;
  if (dims[0] == -1) {
    params_->dynamic = true;
  }else {
    CHECK(new_counts == count());
  }
  //CHECK(new_counts == count() || dims[0] == -1)
       //<< new_counts << "\tvs\t" << count();
  shape_ = TensorShape(dims);
}

void Tensor::Reshape(const Tensor& t) {
  CHECK_NOTNULL(params_.get());
  if (t.dims(0) == -1) {
    params_->dynamic = true;
  }else {
    CHECK(t.count() == count());
  }
  shape_ = t.shape_;
}

void Tensor::Resize(const TensorShape& shape) {
  CHECK_NOTNULL(params_.get());
  if (shape.n_elements() > count()) {
    size_t new_size = shape.n_elements();
    CASES(params_->type, new_size *= sizeof(T));
    CHECK_NOTNULL(buf_.get());
    buf_->Resize(new_size);
  }
  shape_ = shape;
}

void Tensor::ScaleDynamicDimension(int new_dim) {
  CHECK_NOTNULL(params_.get());
  CHECK(params_->dynamic);
  int old_dim = shape_.dim(0);
  shape_.SetDim(0, new_dim);   
  size_t new_size = shape_.n_elements();
  CASES(params_->type, new_size *= sizeof(T));
  if (old_dim < new_dim && buf_->size() < new_size) {
    CHECK_NOTNULL(buf_.get());
    //VLOG(V_DEBUG) << "Resizing " << new_size << " Bytes";
    buf_->Resize(new_size);
  }
}

void Tensor::SetZeroInitEnforced() {
  CHECK_NOTNULL(params_.get());
  params_->zero_init_enforced = true;
}

bool Tensor::ZeroInitEnforced() const {
  CHECK_NOTNULL(params_.get());
  return params_->zero_init_enforced;
}

bool Tensor::InitWithZero(int iteration) {
  CHECK_NOTNULL(params_.get());
  CHECK(ZeroInitEnforced());
  size_t visable_size = count();
  CASES(params_->type, visable_size *= sizeof(T));
  if (params_->iteration == iteration-1) {
    buf_->InitWithZero();
    params_->iteration++;
    VLOG(V_DEBUG) << "Setting Zero for " << name() << " in round " << params_->iteration;
    return true;
  }else if (params_->iteration == iteration) {
    VLOG(V_DEBUG) << "Has been Set Zero " << name() << " in round " << params_->iteration;
    return false;
  }else {
    LOG(FATAL) <<  "Illegal iteration: "
               << params_->iteration << " vs " << iteration;
  }
}

bool Tensor::IsFullShape() const {
  CHECK_NOTNULL(params_.get());
  size_t visable_size = count();
  CASES(params_->type, visable_size *= sizeof(T));
  CHECK_NOTNULL(buf_.get());
  //CHECK(buf_->size() % visable_size == 0);
  return buf_->size() == visable_size;
}

void Tensor::SetOffsetWithId(int id) {
  //CHECK(!IsFullShape());
  CHECK(IsDynamicShape());
  CHECK(dims() > 1);
  size_t unit = count()/dims(0);
  CASES(params_->type, unit *= sizeof(T));
  size_t offset = unit*id; 
  CHECK(offset < buf_->size())
    << id  << "\t" << unit << "\t" << buf_->size() << debug_info();
  params_->offset = offset;
}


void Tensor::SyncWith(const Tensor& t) {
  //CHECK(t.device_type() != device_type());
  CHECK(t.buf_ && buf_);
  CHECK(t.shape_.n_elements() > 0 && shape_.n_elements() > 0);
  size_t size = count();
  CHECK_NOTNULL(params_.get());
  CASES(params_->type, size*= sizeof(T));
  CHECK(size <= t.buf_->size());
  //cudaMemcpyDefault can remove such a complexity
  //but for development, specified it clearly is better.
  if (t.device_type() == CPU && device_type() == GPU) {
    //checkCudaError(cudaMemcpy(buf_->data(), t.buf_->data(), 
                   //t.buf_->size(), cudaMemcpyHostToDevice));
    checkCudaError(cudaMemcpy(buf_->data(), t.buf_->data(), 
                   size, cudaMemcpyHostToDevice));
  }else if (t.device_type() == GPU && device_type() == CPU) {
    checkCudaError(cudaMemcpy(buf_->data(), t.buf_->data(), 
                   size, cudaMemcpyDeviceToHost));
  }else if (t.device_type() == CPU && device_type() == CPU) {
    checkCudaError(cudaMemcpy(buf_->data(), t.buf_->data(), 
                   size, cudaMemcpyHostToHost));
  }else if (t.device_type() == GPU && device_type() == GPU) {
    checkCudaError(cudaMemcpy(buf_->data(), t.buf_->data(), 
                   size, cudaMemcpyDeviceToDevice));
  }else{
    LOG(FATAL) << "which device on earth?";
  }
}

} //namespace midend
