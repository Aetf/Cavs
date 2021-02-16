#include "cavs/util/op_util.h"
#include "cavs/util/logging.h"

#include <functional>

using std::string;
using std::vector;

string GetGradientName(const string& op) {
  return op+"_grad";
}

string GetOriginName(const string& op) {
  CHECK(op.length() > 5);
  CHECK(op.substr(op.length()-5, 5) == "_grad");
  return op.substr(0, op.length()-5);
}

const char* DeviceTypeToString(DeviceType type) {
  if (type == GPU)
    return "GPU";
  else if (type == CPU)
    return "CPU";
  return "CPU";
}

size_t GetHash(const OpDef& op_def) {
  std::hash<string> hash_fn;
  string s;
  for (auto& input : op_def.input())
    s += input;
  for (auto& output : op_def.output())
    s+= output;
  return hash_fn(s);
}

bool IsVariableName(const string& edge) {
  return (edge.length() >= 8 && edge.substr(0, 8) == "Variable")
      || (edge.length() >= 3 && edge.substr(0, 3) == "DDV" );
}

bool IsGradientName(const string& edge) {
  return (edge.length() > 5 && edge.substr(edge.length()-5, 5) == "_grad");
}

bool IsStatefulName(const string& node) {
  return (node == "Accumulate" || node == "PartialAccumulate");
}

#define INSTANTIATE_GETSINGLEARG(T, fieldname)      \
  template<>                                        \
  T GetSingleArg<T>(const OpDef& op_def,            \
      const string& key) {                          \
    for (auto& attr : op_def.attr()){               \
      if (attr.name() == key) {                     \
        /*CHECK(attr.value().has_##fieldname());*/  \
        return attr.value().fieldname();            \
      }                                             \
    }                                               \
    LOG(FATAL) << key << " NOT FOUND\n"             \
               << op_def.DebugString();             \
  }                                                 \
  template<>                                        \
  T GetSingleArg<T>(const OpDef& op_def,            \
      const string& key, T value) {                 \
    for (auto& attr : op_def.attr()){               \
      if (attr.name() == key) {                     \
        return attr.value().fieldname();            \
      }                                             \
    }                                               \
    return value;                                   \
  }                                                 \
  template<>                                             \
  vector<T> GetListArg<T>(const OpDef& op_def,           \
      const string& key) {                               \
    vector<T> ret;                                       \
    for (auto& attr : op_def.attr()) {                   \
      if (attr.name() == key) {                          \
        for (auto& m : attr.value().list().fieldname())  \
          ret.push_back(m);                              \
      }                                                  \
    }                                                    \
    return ret;                                          \
  }                                                       

INSTANTIATE_GETSINGLEARG(float, f)
INSTANTIATE_GETSINGLEARG(int, i)
INSTANTIATE_GETSINGLEARG(string, s)
INSTANTIATE_GETSINGLEARG(bool, b)
