#ifndef TIGER_ENV_ENV_H_
#define TIGER_ENV_ENV_H_

#include "tiger/frame/temp.h"
#include "tiger/semant/types.h"
#include "tiger/symbol/symbol.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

// Forward Declarations
namespace tr {
class Access;
class Level;
} // namespace tr

namespace env {
/**
  在Declaration时创建Entry
  在计算Expression时使用Entry
  */
class EnvEntry {
public:
  bool readonly_;

  explicit EnvEntry(bool readonly = true) : readonly_(readonly) {}
  virtual ~EnvEntry() = default;
};

class VarEntry : public EnvEntry {
public:
  tr::Access *access_;
  type::Ty *ty_;

  // For lab4(semantic analysis) only
  explicit VarEntry(type::Ty *ty, bool readonly = false)
      : EnvEntry(readonly), ty_(ty), access_(nullptr){};

  // For lab5(translate IR tree)
  VarEntry(tr::Access *access, type::Ty *ty, bool readonly = false)
      : EnvEntry(readonly), ty_(ty), access_(access){};
};

/**
  FunEntry Contains type of formals & result
 */
class FunEntry : public EnvEntry {
public:
  tr::Level *level_;
  type::TyList *formals_;
  type::Ty *result_;

  llvm::FunctionType *func_type_;
  llvm::Function *func_;

  // For lab4(semantic analysis) only
  FunEntry(type::TyList *formals, type::Ty *result)
      : formals_(formals), result_(result), level_(nullptr),
        func_type_(nullptr), func_(nullptr) {}

  // This piece of shit is also for lab5.
  // Generate build-in functions such as `printi`
  FunEntry(tr::Level *level, type::TyList *formals, type::Ty *result,
           std::string str);

  // For lab5(translate IR tree)
  FunEntry(tr::Level *level, type::TyList *formals, type::Ty *result,
           llvm::FunctionType *func_type, llvm::Function *func)
      : formals_(formals), result_(result), level_(level),
        func_type_(func_type), func_(func) {}
};

using VEnv = sym::Table<env::EnvEntry>;
using TEnv = sym::Table<type::Ty>;
using VEnvPtr = sym::Table<env::EnvEntry> *;
using TEnvPtr = sym::Table<type::Ty> *;
} // namespace env

#endif // TIGER_ENV_ENV_H_
