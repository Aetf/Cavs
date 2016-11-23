#include "cavs/core/macros_gpu.h"
#include "cavs/kernels/elementwise_ops.h"

namespace cavs {

template <typename OP, typename T> 
__global__ void UnaryKernel(T* out, const T* inp, size_t n) {
    CUDA_1D_KERNEL_LOOP(i, n) { 
        out[i] = OP::Run(inp[i]); 
    } 
}

template <typename OP, typename T> 
__global__ void BinaryKernel(T* out, const T* inp0, const T* inp1, size_t n) {
    CUDA_1D_KERNEL_LOOP(i, n) { 
        out[i] = OP::Run(inp0[i], inp1[i]); 
    } 
}

template <typename OP, typename T>
struct CUDAUnaryFunctor {
    static void Run(T* out, const T* inp, size_t n) {
        UnaryKernel<OP, T><<<THREADS_PER_BLOCK, BLOCKS_PER_GRID(n)>>>(
            out, inp, n);
    }
};

template <typename OP, typename T>
struct CUDABinaryFunctor {
    static void Run(T* out, const T* inp0, const T* inp1, size_t n) {
        BinaryKernel<OP, T><<<THREADS_PER_BLOCK, BLOCKS_PER_GRID(n)>>>(
            out, inp0, inp1, n);
    }
};

#define CudaUnaryOpInstance(math, dtype)    \
    UnaryOp<CUDAUnaryFunctor<math<dtype>, dtype>, dtype> 
#define CudaBinaryOpInstance(math, dtype)   \
    BinaryOp<CUDABinaryFunctor<math<dtype>, dtype>, dtype> 

/*REGISTER_OP_BUILDER(Key("Add").Device("GPU"), UnaryOp<CUDAUnaryFunctor<Math::Abs<float>, float>, float>);*/
REGISTER_OP_BUILDER(Key("Abs").Device("GPU"), CudaUnaryOpInstance(Math::Abs, float));
REGISTER_OP_BUILDER(Key("Add").Device("GPU"), CudaBinaryOpInstance(Math::Add, float));

/*string namemm = "mmm";*/
/*static op_factory::OpRegister rrr(namemm, */
        /*[](const  OpDef& def, Session* s) -> Op*{*/
            /*return new CudaBinaryOpInstance(Math::Add, float)(def, s);*/
         /*});*/
string namemm = "mmm";
static op_factory::OpRegister rrr1(namemm, 
        [](const  OpDef& def, Session* s) -> Op*{
            return new testxsz(def, s);
         });

} //namespace cavs
