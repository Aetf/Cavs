#include "cavs/backend/op_decl.h"
#include "cavs/util/op_def_builder.h"

using std::vector;

namespace backend {

class ReshapeOpDecl : public OpDecl {
 public:
  ReshapeOpDecl(const OpDef& def) : OpDecl(def) {
    CHECK(GetSingleArg<bool>(op_def_, "ShareMemory"));
  }
  
  void MakeGradient(vector<OpDef>* grad) override {
    CHECK(grad->size() == 0);
    OpDef reshape;
    OpDefBuilder("ReshapeLike")
      .Input(GetGradientName(op_def_.output(0)))
      .Input(op_def_.input(0))
      .Output(GetGradientName(op_def_.input(0)))
      .AttrSingle<bool>("ShareMemory", true)
      .Device(op_def_)
      .Finalize(&reshape);
    grad->push_back(std::move(reshape));
  }

  void ShapeInference(vector<TensorShapeDef>* out_shape,
    const vector<TensorShapeDef>& inputs) override {
    CHECK(inputs.size() == 1); 
    CHECK(out_shape->empty());
    out_shape->push_back(op_def_.shape(0));
  }
};

class ReshapeLikeOpDecl : public ReshapeOpDecl {
 public:
  ReshapeLikeOpDecl(const OpDef& def) : ReshapeOpDecl(def) {
    CHECK(GetSingleArg<bool>(op_def_, "ShareMemory"));
  }
  void ShapeInference(vector<TensorShapeDef>* out_shape,
    const vector<TensorShapeDef>& inputs) override {
    CHECK(inputs.size() == 2);
    CHECK(out_shape->size() == 0);
    out_shape->push_back(inputs[1]);
  }
};

class FlattenOpDecl : public ReshapeOpDecl {
 public:
  FlattenOpDecl(const OpDef& def) : ReshapeOpDecl(def) {}
  void ShapeInference(vector<TensorShapeDef>* out_shape,
    const vector<TensorShapeDef>& inputs) override {
    CHECK(inputs.size() == 1);
    CHECK(inputs[0].dim_size() == 4);
    CHECK(out_shape->size() == 0);
    TensorShapeDef shape;
    int N = inputs[0].dim(0);
    int C = inputs[0].dim(1);
    int H = inputs[0].dim(2);
    int W = inputs[0].dim(3);
    shape.add_dim(N);
    shape.add_dim(C*H*W);
    out_shape->push_back(std::move(shape));
  }
};

REGISTER_OP_DECL_BUILDER("Reshape", ReshapeOpDecl);
REGISTER_OP_DECL_BUILDER("Flatten", FlattenOpDecl);
REGISTER_OP_DECL_BUILDER("ReshapeLike", ReshapeLikeOpDecl);

} //namespace backend
