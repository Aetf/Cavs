#include "cavs/backend/op_impl.h"
#include "cavs/backend/cuda_common.h"
#include "cavs/backend/cublas_wrapper.h"
#include "cavs/proto/tensor_shape.pb.h"
#include "cavs/util/macros_gpu.h"

namespace backend {

using ::midend::Tensor;

template <typename T>
class EmbeddingLookupOp: public OpImpl {
 public:
  explicit EmbeddingLookupOp(const OpDef& def)
    : OpImpl(def), stream_(cudaStreamDefault) {}
  void Compute(OpContext* context) override;

 private:
  cudaStream_t stream_;
};

template <typename T>
__global__ void BatchedCopy(T *embedding,
    const T* data, const T* matrix,
    int embedding_size) {
  int output_offset = blockIdx.x*embedding_size;
  int matrix_offset = data[blockIdx.x]*embedding_size;
  for (int round = 0; round < (embedding_size+blockDim.x-1)/blockDim.x; round++) {
    int offset_within_vec = threadIdx.x + round*blockDim.x;
    if (offset_within_vec < embedding_size) {  
      embedding[output_offset+offset_within_vec] =
        matrix[matrix_offset+offset_within_vec];
    }
  }
}

template <typename T>
void EmbeddingLookupOp<T>::Compute(OpContext* context) {
  const Tensor& input = context->Input(0);
  const Tensor& embedding_matrix = context->Input(1);
  Tensor* embedding = context->Output(0);

  CHECK(embedding_matrix.dims() == 2);
  int vocabulary_size = embedding_matrix.dims(0);
  int embedding_size  = embedding_matrix.dims(1);
  CHECK(vocabulary_size >= embedding_size);
  //we loose this constraint for the batching,
  //in which we will add a dimension(1) in the 0th dimension
  CHECK(embedding->dims() == input.dims()+1 ||
      (embedding->dims() == input.dims() && input.IsDynamicShape()));
  //we loose this constraint for the batching,
  //1==>1, 100 before batching 
  //1, 1==>1, 100 after batching
  //for input, we add a dimension(1)
  //for output, we modify the 0th dimension to batch_size
  /*for (int i = 0; i < input.dims(); i++)*/
    /*CHECK(embedding->dims(i) == input.dims(i));*/
  CHECK(embedding->dims(embedding->dims()-1) == embedding_size);
  if (!stream_ && context->GetStreamID() != -1) {
    stream_ = StreamEventHandlePool::GetCudaStream(context->GetStreamID());
    VLOG(V_DEBUG) << "[Unary] Assign new stream with ID " << context->GetStreamID();
  }

  int slices = input.count();
  const int MAX_THREADS_IN_BLOCK = 1 << 10;
  int threadsPerBlock = (MAX_THREADS_IN_BLOCK > embedding_size) ?
                         embedding_size : MAX_THREADS_IN_BLOCK;
  int blocksPerGrid = slices;
  BatchedCopy<<<blocksPerGrid, threadsPerBlock, 0, stream_>>>(
      embedding->mutable_data<T>(),
      input.data<T>(), embedding_matrix.data<T>(),
      embedding_size);

  input.DebugNumerical<T>();
  embedding_matrix.DebugNumerical<T>();
  embedding->DebugNumerical<T>();
}

template <typename T>
class EmbeddingLookupGradOp: public OpImpl {
 public:
  explicit EmbeddingLookupGradOp(const OpDef& def)
    : OpImpl(def), stream_(cudaStreamDefault) {}
  void Compute(OpContext* context) override;

 private:
  cudaStream_t stream_;
};

template <typename T>
__global__ void BatchedSparseUpdate(T *dMatrix,
    const T* data, const T* dY,
    int embedding_size) {
  int dY_offset = blockIdx.x*embedding_size;
  int dMatrix_offset = data[blockIdx.x]*embedding_size;
  for (int round = 0; round < (embedding_size+blockDim.x-1)/blockDim.x; round++) {
    int offset_within_vec = threadIdx.x + round*blockDim.x;
    if (offset_within_vec < embedding_size) {  
      atomicAdd(&(dMatrix[dMatrix_offset+offset_within_vec]), 
        dY[dY_offset+offset_within_vec]);
    }
  }
}

template <typename T>
void EmbeddingLookupGradOp<T>::Compute(OpContext* context) {
  const Tensor& dY = context->Input(0);
  const Tensor& input = context->Input(1);
  Tensor* dMatrix= context->Output(0);
  //we don't calculate the dX, because dX is not passed backward

  CHECK(dMatrix->dims() == 2);
  int vocabulary_size = dMatrix->dims(0);
  int embedding_size  = dMatrix->dims(1);
  CHECK(vocabulary_size >= embedding_size);
  //we loose this constraint for the batching,
  //1==>1, 100 before batching 
  //1, 1==>1, 100 after batching
  //for input, we add a dimension(1)
  //for output, we modify the 0th dimension to batch_size
  CHECK(dY.dims() == input.dims()+1 ||
       (dY.dims() == input.dims() && input.IsDynamicShape()));
  /*for (int i = 0; i < input.dims(); i++)*/
    /*CHECK(dY.dims(i) == input.dims(i));*/
  CHECK(dY.dims(dY.dims()-1) == embedding_size);

  if (!stream_ && context->GetStreamID() != -1) {
    stream_ = StreamEventHandlePool::GetCudaStream(context->GetStreamID());
    VLOG(V_DEBUG) << "[Unary] Assign new stream with ID " << context->GetStreamID();
  }

  /*checkCudaError(cudaMemsetAsync(dMatrix->mutable_data<T>(), 0, */
                                 /*dMatrix->count()*sizeof(T), stream_));*/
  int slices = input.count();
  const int MAX_THREADS_IN_BLOCK = 1 << 10;
  int threadsPerBlock = (MAX_THREADS_IN_BLOCK > embedding_size) ?
                         embedding_size : MAX_THREADS_IN_BLOCK;
  int blocksPerGrid = slices;

  BatchedSparseUpdate<<<blocksPerGrid, threadsPerBlock, 0, stream_>>>(
      dMatrix->mutable_data<T>(),
      input.data<T>(), dY.data<T>(),
      embedding_size);

  dY.DebugNumerical<T>();
  input.DebugNumerical<T>();
  dMatrix->DebugNumerical<T>();
}

REGISTER_OP_IMPL_BUILDER(Key("EmbeddingLookup").Device("GPU"), EmbeddingLookupOp<float>);
REGISTER_OP_IMPL_BUILDER(Key(GetGradientName("EmbeddingLookup")).Device("GPU"), EmbeddingLookupGradOp<float>);

} //namespace backend

