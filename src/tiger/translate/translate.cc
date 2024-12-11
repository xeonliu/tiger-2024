#include "tiger/translate/translate.h"

// #include <llvm-14/llvm/ADT/APInt.h>
// #include <llvm-14/llvm/IR/BasicBlock.h>
// #include <llvm-14/llvm/IR/Constants.h>
// #include <llvm-14/llvm/IR/DerivedTypes.h>
#include <cstdint>
#include <list>
#include <llvm-14/llvm/ADT/APInt.h>
#include <llvm-14/llvm/IR/BasicBlock.h>
#include <llvm-14/llvm/IR/Constant.h>
#include <llvm-14/llvm/IR/Constants.h>
#include <llvm-14/llvm/IR/DataLayout.h>
#include <llvm-14/llvm/IR/DerivedTypes.h>
#include <llvm-14/llvm/IR/GlobalVariable.h>
#include <llvm-14/llvm/IR/Instructions.h>
#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/x64frame.h"
#include "tiger/semant/types.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <stack>

/**
  TODO: 这些全局变量是干什么用的？
 */
extern frame::Frags *frags;
extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;
/**
  这两个Stack用作什么？
 */
// 当翻译一个Func定义时，添加进来，翻译完返回之后弹出
// 尤其对于Let表达式，需要知道in后面的式子现在位于哪一个函数
std::stack<llvm::Function *> func_stack;
// 给Break用的
std::stack<llvm::BasicBlock *> loop_stack;

// ANCHOR[id=C]: 外部的C函数
// LINK: Defined in src/tiger/runtime/runtime_llvm.c
llvm::Function *alloc_record;
/*
  creates an n-word area
  initialize all the element with the same value
  returns an i64, can be turned into a pointer
  */
llvm::Function *init_array;
llvm::Function *string_equal;
// NOTE: For Storing Frame Information
// LINK: in test_translate_llvm.cc
std::vector<std::pair<std::string, frame::Frame *>> frame_info;

bool CheckBBTerminatorIsBranch(llvm::BasicBlock *bb) {
  auto inst = bb->getTerminator();
  if (inst) {
    llvm::BranchInst *branchInst = llvm::dyn_cast<llvm::BranchInst>(inst);
    if (branchInst && !branchInst->isConditional()) {
      return true;
    }
  }
  return false;
}

int getActualFramesize(tr::Level *level) {
  return level->frame_->calculateActualFramesize();
}

namespace tr {

/**
  Returns the Stack Top (Frame Pointer) of the current level.
*/
llvm::Value *Level::get_fp(llvm::Value *sp) {
  llvm::Value *frame_size = ir_builder->CreateLoad(ir_builder->getInt64Ty(),
                                                   frame_->framesize_global);
  // fp = sp + func_global_offset
  llvm::Value *fp = ir_builder->CreateAdd(sp, frame_size);
  return fp;
}

/**
  Returns the Stack Pointer of a Target Level
  Caller must make sure THIS Level's sp is accessable.

  For CallExp Statck Link Arg
  `this` refers the level of the caller
  target refers to the Frame in which the callee is defined

  For Simple Var
  target refers to the Frame of the function in which the variable is defined
  Needs fp for Variable Access.
  追溯次数可以静态确定
  然而取值过程需要生成汇编代码以动态执行
 */
llvm::Value *Level::gen_sp(tr::Level *target) {

  // The Caller Should Make Sure this is accessable under current context.
  llvm::Value *sp = this->get_sp();

  for (tr::Level *level = this; level != target; level = level->parent_) {
    auto st_access = level->frame_->Formals()->front();

    // Address of the static link
    // Address of the static link
    llvm::Value *st_addr = st_access->ToLLVMVal(level->get_fp(sp));
    llvm::Value *st_ptr = ir_builder->CreateIntToPtr(
        st_addr, llvm::PointerType::get(ir_builder->getInt64Ty(), 0));

    // Load the static link and Update sp
    sp = ir_builder->CreateLoad(ir_builder->getInt64Ty(), st_ptr);
  }

  return sp;
}

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

class ValAndTy {
public:
  type::Ty *ty_;
  llvm::Value *val_;
  llvm::BasicBlock *last_bb_;

  ValAndTy(llvm::Value *val, type::Ty *ty) : val_(val), ty_(ty) {}
};

void ProgTr::OutputIR(std::string_view filename) {
  std::string llvmfile = std::string(filename) + ".ll";
  std::error_code ec;
  llvm::raw_fd_ostream out(llvmfile, ec, llvm::sys::fs::OpenFlags::OF_Text);
  ir_module->print(out, nullptr);
}

void ProgTr::Translate() {
  /* C Functions are included Here */
  FillBaseVEnv();
  FillBaseTEnv();
  /* TODO: Put your lab5-part1 code here */
  /** Bind the C Functions */

  // init_array
  llvm::FunctionType *init_array_func_type = llvm::FunctionType::get(
      ir_builder->getInt64Ty(),
      {ir_builder->getInt32Ty(), ir_builder->getInt64Ty()}, false);
  init_array = llvm::Function::Create(init_array_func_type,
                                      llvm::Function::ExternalLinkage,
                                      "init_array", ir_module);

  // alloc_record
  llvm::FunctionType *alloc_record_type = llvm::FunctionType::get(
      ir_builder->getInt64Ty(), {ir_builder->getInt32Ty()}, false);
  alloc_record =
      llvm::Function::Create(alloc_record_type, llvm::Function::ExternalLinkage,
                             "alloc_record", ir_module);

  // tiger_main
  llvm::FunctionType *tiger_main_type = llvm::FunctionType::get(
      ir_builder->getInt32Ty(),
      {ir_builder->getInt64Ty(), ir_builder->getInt64Ty()}, false);

  llvm::Function *tiger_main_func = llvm::Function::Create(
      tiger_main_type, llvm::Function::ExternalLinkage, "tigermain", ir_module);

  // @tigermain_framesize_global
  llvm::GlobalVariable *framesize_global = new llvm::GlobalVariable(
      *ir_module, ir_builder->getInt64Ty(), false,
      llvm::GlobalValue::PrivateLinkage,
      llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0),
      "tigermain_framesize_global");

  // Init FrameSizeGlobal
  main_level_->frame_->framesize_global = framesize_global;

  // tiger_main:
  llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(
      ir_builder->getContext(), "tigermain", tiger_main_func);
  ir_builder->SetInsertPoint(entry_bb);

  llvm::Function::arg_iterator args = tiger_main_func->arg_begin();
  llvm::Value *sp_arg = args++;
  llvm::Value *sl_arg = args++;

  llvm::Value *framesize_val =
      ir_builder->CreateLoad(ir_builder->getInt64Ty(), framesize_global);
  llvm::Value *new_sp =
      ir_builder->CreateSub(sp_arg, framesize_val, "tiger_main_sp");

  // NOTE: Initialize the top level's sp.
  // Otherwise segfault when storing arguments
  main_level_->set_sp(new_sp);

  func_stack.push(tiger_main_func);

  this->absyn_tree_->Translate(this->venv_.get(), this->tenv_.get(),
                               this->main_level_.get(), this->errormsg_.get());
  ir_builder->CreateRet(ir_builder->getInt32(0));
  // TODO: How are they binded?
  // FIXME： PPT or calculateActualFramesize()??
  int64_t framesize = main_level_->frame_->calculateActualFramesize();

  framesize_global->setInitializer(
      llvm::ConstantInt::get(ir_builder->getInt64Ty(), framesize));
}

} // namespace tr

namespace absyn {

tr::ValAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return this->root_->Translate(venv, tenv, level, errormsg);
}

void TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  for (auto tydec : this->types_->GetList()) {
    /* tydec -> type type-id = ty */
    tenv->Enter(tydec->name_, new type::NameTy(tydec->name_, nullptr));
    auto type = tydec->ty_->Translate(tenv, errormsg);
    tenv->Enter(tydec->name_, type);
  }
}

void FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */

  /**
    Stage 1: For (Mutuallly) Recursive Functions
    Functions are globally defined.
    Send the Partial Defined Functions to venv
   */
  for (auto fundec : this->functions_->GetList()) {
    // 1. Result Type
    type::Ty *result_ty = fundec->result_ ? tenv->Look(fundec->result_)
                                          : type::VoidTy::Instance();
    // 2. Param Type
    type::TyList *formal_tys =
        fundec->params_->MakeFormalTyList(tenv, errormsg);
    auto formal_ty_list = formal_tys->GetList();

    // 3. LLVM Function Type (Result & Param)
    // init for sp & static link
    std::vector<llvm::Type *> llvm_param_types{ir_builder->getInt64Ty(),
                                               ir_builder->getInt64Ty()};
    std::transform(formal_ty_list.begin(), formal_ty_list.end(),
                   std::back_inserter(llvm_param_types),
                   [](type::Ty *ty) { return ty->GetLLVMType(); });
    llvm::FunctionType *func_type = llvm::FunctionType::get(
        result_ty->GetLLVMType(), llvm_param_types, false);

    // 4. LLVM Function
    llvm::Function *func =
        llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                               fundec->name_->Name().c_str(), ir_module);

    // 5. Function Entry
    // FunEntry(tr::Level *level, type::TyList *formals, type::Ty *result,
    //       llvm::FunctionType *func_type, llvm::Function *func)

    /* Frame */
    std::list<bool> escape_formals(formal_ty_list.size(), true);

    //  NOTE - Param Access constructed here.
    auto frame = frame::NewFrame(fundec->name_, escape_formals);

    /* Level */
    tr::Level *fun_level = new tr::Level(frame, level);

    /* Entry */
    venv->Enter(fundec->name_, new env::FunEntry(fun_level, formal_tys,
                                                 result_ty, func_type, func));

    // 6. Init Level with Global Framesize Variable
    llvm::GlobalVariable *framesize_global = new llvm::GlobalVariable(
        *ir_module, ir_builder->getInt64Ty(), false,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0),
        fundec->name_->Name() + "_framesize_global");
    fun_level->frame_->framesize_global = framesize_global;
  }

  /**
    Stage 2: BeginScope & Send params to venv
    Generate the body for Certain Functions.
   */
  for (auto fundec : this->functions_->GetList()) {
    venv->BeginScope();

    /* 1. Find the function entry */
    env::FunEntry *func_entry =
        dynamic_cast<env::FunEntry *>(venv->Look(fundec->name_));
    // Obtain the level and thus frame
    auto func = func_entry->func_;
    auto func_level = func_entry->level_;
    auto framesize_global = func_level->frame_->framesize_global;
    auto result_ty = func_entry->result_;

    func_stack.push(func);

    /* 2. Generate VarEntry for params */
    std::list<frame::Access *> accesses =
        *func_entry->level_->frame_->Formals();
    std::list<type::Field *> fields =
        fundec->params_->MakeFieldList(tenv, errormsg)->GetList();

    auto access_iter = accesses.begin();
    access_iter++; // The first one is static link.

    for (auto field_iter = fields.begin(); field_iter != fields.end();
         field_iter++) {
      tr::Access *tr_access = new tr::Access(func_entry->level_, *access_iter);
      env::VarEntry *var_entry =
          new env::VarEntry(tr_access, (*field_iter)->ty_);
      venv->Enter((*field_iter)->name_, var_entry);
    }

    /* 3. Get the sp */
    // 函数的第一个参数是 Parent Stack Pointer.
    // %g_sp = sub i64 %0, %g_local_framesize
    // 第二个参数是 Static Link?
    // store i64 %1, i64* %sl_ptr, align 4
    llvm::Function::arg_iterator args = func->arg_begin();
    llvm::Value *sp_arg = args++;

    /* 4. Build the Body */

    // Build a body_bb. Insert with parent specified (Otherwise SegFault)
    llvm::BasicBlock *func_bb = llvm::BasicBlock::Create(
        ir_builder->getContext(), fundec->name_->Name(), func);
    ir_builder->SetInsertPoint(func_bb);

    // Calculate current sp.
    llvm::Value *func_framesize =
        ir_builder->CreateLoad(ir_builder->getInt64Ty(), framesize_global,
                               fundec->name_->Name() + "_local_framesize");
    llvm::Value *new_sp = ir_builder->CreateSub(sp_arg, func_framesize,
                                                fundec->name_->Name() + "_sp");
    func_level->set_sp(new_sp);

    // Store Static Link & others
    for (auto access : *(func_level->frame_->Formals())) {
      llvm::Value *arg = args++;
      // NOTE: 这是一个优化，可以不从当前的sp进行复杂计算以取出fp
      // Call ToLLVMVal with the callee's runtime Stack Top (Frame Pointer)
      llvm::Value *arg_addr = access->ToLLVMVal(sp_arg);
      llvm::Value *arg_ptr = ir_builder->CreateIntToPtr(
          arg_addr, llvm::PointerType::get(arg->getType(), 0));
      ir_builder->CreateStore(arg, arg_ptr);
    }

    // Translate the body
    tr::ValAndTy *body_val_ty =
        fundec->body_->Translate(venv, tenv, func_level, errormsg);

    // Return
    if (result_ty->IsSameType(type::VoidTy::Instance())) {
      ir_builder->CreateRetVoid();
    } else {
      ir_builder->CreateRet(body_val_ty->val_);
    }

    func_stack.pop();

    // FIXME： PPT or calculateActualFramesize()??
    int64_t framesize = func_level->frame_->calculateActualFramesize();

    framesize_global->setInitializer(
        llvm::ConstantInt::get(ir_builder->getInt64Ty(), framesize));
  }
}

/**
  Update venv
  An assignment for init.
 */
void VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                       err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  tr::ValAndTy *init_val_ty =
      this->init_->Translate(venv, tenv, level, errormsg);

  // 在当前Level为变量分配空间
  tr::Access *access = tr::Access::AllocLocal(level, true); // 变量默认逃逸

  // 将初始化值储存到变量地址
  // NOTE: ToLLVM应该传入fp!!
  llvm::Value *fp = level->get_fp(level->get_sp());
  llvm::Value *var_addr = access->access_->ToLLVMVal(fp); // i64 here
  llvm::Value *var_ptr = ir_builder->CreateIntToPtr(
      var_addr, llvm::PointerType::get(init_val_ty->ty_->GetLLVMType(), 0));

  ir_builder->CreateStore(init_val_ty->val_, var_ptr);

  venv->Enter(this->var_, new env::VarEntry(access, init_val_ty->ty_));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return tenv->Look(this->name_);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new type::RecordTy(this->record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto element_type = tenv->Look(this->array_);
  return new type::ArrayTy(element_type);
}

/**
    Here are some vars.
    All the vars are **lvalue**.
    You should return the address of them in llvm::Value *
    The type::Ty* returned does not contain the pointer info?
 */

/**
  通过Static Link找到正确的Level
  以llvm::Value* 返回变量的地址
  以type::Ty* 返回变量的类型
  llvm::Vaule* 中暗含了当前返回的是llvm::PointerType
  TODO: 可以用于后续检验？
 */
tr::ValAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  /* Fetch the entry */
  env::VarEntry *var_entry =
      dynamic_cast<env::VarEntry *>(venv->Look(this->sym_));

  // Get the Type of the variable
  type::Ty *ty = var_entry->ty_->ActualTy();

  auto target_level = var_entry->access_->level_;

  /* Get Stack Pointer of the function in which this variable is defined*/
  llvm::Value *sp = level->gen_sp(var_entry->access_->level_);
  
  /* Calculate the address of the variable */
  // NOTE: Offset 是针对 Frame Pointer 的
  llvm::Value *var_addr =
      var_entry->access_->access_->ToLLVMVal(target_level->get_fp(sp));

  /* Transfer the address to pointer */
  // %x_ptr = inttoptr i64 %x_addr to i32*
  llvm::Value *val = ir_builder->CreateIntToPtr(
      var_addr, llvm::PointerType::get(ty->GetLLVMType(), 0),
      this->sym_->Name().c_str());

  return new tr::ValAndTy(val, ty);
}

/**
  Get the pointer to the element?
 */
tr::ValAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  // Pointer to the record
  tr::ValAndTy *var_val = this->var_->Translate(venv, tenv, level, errormsg);
  // Semantic Analysis Ensure its type is RecordTy
  type::RecordTy *record_ty = dynamic_cast<type::RecordTy *>(var_val->ty_);

  // How do I access its members?
  // 1. Get the field list
  std::list<type::Field *> field_list = record_ty->fields_->GetList();
  // 2. Find the field (with its index)
  int index = 0;
  type::Ty *field_ty = nullptr;
  for (auto field : field_list) {
    if (field->name_ == this->sym_) {
      field_ty = field->ty_;
      break;
    }
    index++;
  }

  // Create GEP
  // %field_ptr = getelementptr (record_type), i64* %record_ptr, i32 0, i32
  // index
  llvm::Value *field_ptr = ir_builder->CreateGEP(
      record_ty->GetLLVMType()->getPointerElementType(), var_val->val_,
      {ir_builder->getInt32(0), ir_builder->getInt32(index)},
      this->sym_->Name().c_str());

  return new tr::ValAndTy(field_ptr, field_ty);
}

tr::ValAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  tr::ValAndTy *var_val = this->var_->Translate(venv, tenv, level, errormsg);
  type::ArrayTy *array_ty = dynamic_cast<type::ArrayTy *>(var_val->ty_);

  // Get the type of the subscript
  type::Ty *subscript_ty = array_ty->ty_;

  // How do I get the address of the subscript?
  llvm::Value *base_addr = var_val->val_;

  // Will it return an address or a value?
  llvm::Value *subscript_val =
      this->subscript_->Translate(venv, tenv, level, errormsg)->val_;

  return new tr::ValAndTy(subscript_val, subscript_ty);
}

/**
  TODO: 此处Var作为Exp使用，应该使用Load转换为右值后返回
 */
tr::ValAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  tr::ValAndTy *var_val_ty = var_->Translate(venv, tenv, level, errormsg);
  llvm::Value *var_addr = var_val_ty->val_;
  type::Ty *var_type = var_val_ty->ty_;

  /** Lecture 14 P38 Convert from Variable to Value */
  llvm::Value *loaded_val =
      ir_builder->CreateLoad(var_type->GetLLVMType(), var_addr);
  return new tr::ValAndTy(loaded_val, var_type);
}

/**
  Nil Represents a value that can be assigned to any record type.
  Accessing a field from a nil-valued record is a runtime error.
 */
tr::ValAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  // TODO: What should I return?
  return new tr::ValAndTy(ir_builder->getInt64(0), type::NilTy::Instance());
}

tr::ValAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new tr::ValAndTy(ir_builder->getInt32(this->val_),
                          type::IntTy::Instance());
}

tr::ValAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto val = type::StringTy::CreateGlobalStringStructPtr(this->str_);
  return new tr::ValAndTy(val, type::StringTy::Instance());
}

/**
  FIXME: Call Expression will change the outgo size of the current frame.
 */
tr::ValAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  env::FunEntry *fun_entry =
      dynamic_cast<env::FunEntry *>(venv->Look(this->func_));

  llvm::Function *llvm_func = fun_entry->func_;
  llvm::FunctionType *func_type = llvm_func->getFunctionType();

  std::vector<llvm::Value *> args;

  uint64_t required_outgo_size = 0;

  // TODO: Explicitly Deal with build-in functions?
  // Check args in llvm_func.
  // If the function needs sp and st.
  if (func_type->getNumParams() == this->args_->GetList().size() + 2) {
    // Stack Pointer
    args.emplace_back(level->get_sp());

    // Static Link (The Stack Pointer of the function in which it is defined)
    // 1. Deafult: Caller's sp
    // 2. The function in which it is defined is not the caller
    // use static link to find the correct sp
    // NOTE: The highest Static Link is the sp of tiger_main

    // For CallExp, to obtain Statck Link Arg
    // `this` refers the level of the caller
    // target refers to the Frame in which the callee is defined
    auto st = level->gen_sp(fun_entry->level_->parent_);

    args.emplace_back(st);

    required_outgo_size += reg_manager->WordSize();
  }

  for (auto arg_exp : this->args_->GetList()) {
    tr::ValAndTy *arg_val_ty = arg_exp->Translate(venv, tenv, level, errormsg);
    args.emplace_back(arg_val_ty->val_);
    required_outgo_size += reg_manager->WordSize();
  }

  llvm::Value *call = ir_builder->CreateCall(llvm_func, args);

  /* Adjuest the frame outgo size: sl + params */
  level->frame_->AllocOutgoSpace(required_outgo_size);

  return new tr::ValAndTy(call, fun_entry->result_);
}

tr::ValAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto oper = this->oper_;
  auto left = this->left_->Translate(venv, tenv, level, errormsg);
  auto right = this->right_->Translate(venv, tenv, level, errormsg);

  switch (oper) {
  case Oper::AND_OP:
    return new tr::ValAndTy(ir_builder->CreateAnd(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::OR_OP:
    return new tr::ValAndTy(ir_builder->CreateOr(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::PLUS_OP:
    return new tr::ValAndTy(ir_builder->CreateAdd(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::MINUS_OP:
    return new tr::ValAndTy(ir_builder->CreateSub(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::TIMES_OP:
    return new tr::ValAndTy(ir_builder->CreateMul(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::DIVIDE_OP:
    return new tr::ValAndTy(ir_builder->CreateSDiv(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::EQ_OP:
    return new tr::ValAndTy(ir_builder->CreateICmpEQ(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::NEQ_OP:
    return new tr::ValAndTy(ir_builder->CreateICmpNE(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::LT_OP:
    return new tr::ValAndTy(ir_builder->CreateICmpSLT(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::LE_OP:
    return new tr::ValAndTy(ir_builder->CreateICmpSLE(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::GT_OP:
    return new tr::ValAndTy(ir_builder->CreateICmpSGT(left->val_, right->val_),
                            type::IntTy::Instance());
  case Oper::GE_OP:
    return new tr::ValAndTy(ir_builder->CreateICmpSGE(left->val_, right->val_),
                            type::IntTy::Instance());
  // TODO: What is ABSYN_OPER_COUNT?
  default:
    errormsg->Error(this->pos_, "unknown operator");
    return nullptr;
  }
}

/**
  Creare a new instance of type Record and return its address.
 */
tr::ValAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  // Get the type of the record
  auto record_type = dynamic_cast<type::RecordTy *>(tenv->Look(this->typ_));

  // TODO: Craete a New Instance of RecordExp
  // and return its address
  // Get the size of record
  llvm::Type *llvm_record_type = record_type->GetLLVMType();
  const llvm::DataLayout &data_layout = ir_module->getDataLayout();
  int record_size = data_layout.getTypeAllocSize(llvm_record_type);
  llvm::Value *record_size_val = ir_builder->getInt32(record_size);

  // Allocate memory for the record
  llvm::Value *record_addr =
      ir_builder->CreateCall(alloc_record, {record_size_val});
  // ir_builder->CreateAlloca(record_type->GetLLVMType(), nullptr);

  // Initialize each field
  int index = 0;
  for (auto field : this->fields_->GetList()) {
    llvm::Value *field_val =
        field->exp_->Translate(venv, tenv, level, errormsg)->val_;
    llvm::Value *field_addr = ir_builder->CreateGEP(
        record_type->GetLLVMType()->getPointerElementType(), record_addr,
        {ir_builder->getInt32(0), ir_builder->getInt32(index)},
        field->name_->Name().c_str());
    ir_builder->CreateStore(field_val, field_addr);
    index++;
  }

  return new tr::ValAndTy(record_addr, record_type);
}

/**
  Evaluate each sub expression and return the value of the last one.
 */
tr::ValAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  tr::ValAndTy *val_and_ty = nullptr;

  for (auto exp : this->seq_->GetList()) {
    val_and_ty = exp->Translate(venv, tenv, level, errormsg);
  }

  return val_and_ty;
}

/**
  var_ as a lvalue (ptr)
  exp_ as a rvalue (value / need conversion)
 */
tr::ValAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */

  // Treat var_ as lvalue
  tr::ValAndTy *var_val_and_ty = var_->Translate(venv, tenv, level, errormsg);
  llvm::Value *lvalue_addr = var_val_and_ty->val_;
  type::Ty *lvalue_ty = var_val_and_ty->ty_;

  // lvalue_addr is the address to some var or a pointer (to the baseaddr of
  // Array or Record).

  // Evaluate exp_
  tr::ValAndTy *exp_val_and_ty = exp_->Translate(venv, tenv, level, errormsg);

  // Can be val, can be addr to val?
  auto exp_val = exp_val_and_ty->val_;
  // Get the type of exp_
  auto exp_ty = exp_val_and_ty->ty_;

  // For Record Type and Array Type
  // Assign the address of the variable directly
  // exp_val is the value of the pointer (a.k.a an address)
  // lvalue_val is the address of the pointer.
  // *lvalue_addr = exp_val

  // Assign the exp_ to var.
  // TODO: Size of store?
  ir_builder->CreateStore(exp_val, lvalue_addr);
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */

  /* Get Parent Function */
  llvm::Function *func = ir_builder->GetInsertBlock()->getParent();
  llvm::LLVMContext &context = ir_module->getContext();

  /* Create Basic Block */
  llvm::BasicBlock *test_bb =
      llvm::BasicBlock::Create(context, "if_test", func);
  llvm::BasicBlock *then_bb =
      llvm::BasicBlock::Create(context, "if_then", func);
  llvm::BasicBlock *else_bb = nullptr;
  if (this->elsee_) {
    else_bb = llvm::BasicBlock::Create(context, "if_else", func);
  }
  llvm::BasicBlock *next_bb =
      llvm::BasicBlock::Create(context, "if_next", func);
  if (!this->elsee_) {
    else_bb = next_bb;
  }

  ir_builder->CreateBr(test_bb);

  // if_test:
  ir_builder->SetInsertPoint(test_bb);
  tr::ValAndTy *test_val_ty =
      this->test_->Translate(venv, tenv, level, errormsg);
  llvm::Value *test_val = test_val_ty->val_;

  // br i1 %23, label %if_then, label %if_else
  ir_builder->CreateCondBr(test_val, then_bb, else_bb);

  // if_then:
  ir_builder->SetInsertPoint(then_bb);
  tr::ValAndTy *then_val_ty =
      this->then_->Translate(venv, tenv, level, errormsg);
  llvm::Value *then_val = then_val_ty->val_;

  ir_builder->CreateBr(next_bb);

  // if_else:
  if (this->elsee_) {
    ir_builder->SetInsertPoint(else_bb);
    tr::ValAndTy *else_val_ty = elsee_->Translate(venv, tenv, level, errormsg);
    llvm::Value *else_val = else_val_ty->val_;
    ir_builder->CreateBr(next_bb);

    /* Following Instructions should be in next_bb */
    ir_builder->SetInsertPoint(next_bb);

    // Void
    if (else_val_ty->ty_->IsSameType(type::VoidTy::Instance())) {
      return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
    }

    // Non-Void, using phi
    llvm::PHINode *phi_node =
        ir_builder->CreatePHI(then_val_ty->ty_->GetLLVMType(), 2, "iftmp");
    phi_node->addIncoming(then_val, then_bb);
    phi_node->addIncoming(else_val, else_bb);
    return new tr::ValAndTy(phi_node, then_val_ty->ty_);
  } else {
    /* Following Instructions should be in next_bb */
    ir_builder->SetInsertPoint(next_bb);

    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  }
}

tr::ValAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  // What is this?
  llvm::Function *func = ir_builder->GetInsertBlock()->getParent();

  llvm::LLVMContext &context = ir_module->getContext();

  llvm::BasicBlock *testBB =
      llvm::BasicBlock::Create(context, "while_test", func);
  llvm::BasicBlock *bodyBB =
      llvm::BasicBlock::Create(context, "while_body", func);
  llvm::BasicBlock *doneBB =
      llvm::BasicBlock::Create(context, "while_next", func);

  ir_builder->CreateBr(testBB);
  ir_builder->SetInsertPoint(testBB);

  tr::ValAndTy *cond_val_ty = test_->Translate(venv, tenv, level, errormsg);
  llvm::Value *cond_val = cond_val_ty->val_;

  // Branch base on cond value
  ir_builder->CreateCondBr(cond_val, bodyBB, doneBB);
  ir_builder->SetInsertPoint(bodyBB);

  // Excute the body
  /** There may be break exp in body_, It will use the doneBB */
  loop_stack.push(doneBB);
  body_->Translate(venv, tenv, level, errormsg);
  loop_stack.pop();

  // jump to test
  ir_builder->CreateBr(testBB);

  // done:
  ir_builder->SetInsertPoint(doneBB);

  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  llvm::Function *func = ir_builder->GetInsertBlock()->getParent();
  llvm::LLVMContext &context = ir_module->getContext();

  llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context, "cond", func);
  llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context, "body", func);
  llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(context, "after", func);

  // Initialize hi and lo
  tr::ValAndTy *lo_val_ty = lo_->Translate(venv, tenv, level, errormsg);
  llvm::Value *lo_val = lo_val_ty->val_;
  tr::ValAndTy *hi_val_ty = hi_->Translate(venv, tenv, level, errormsg);
  llvm::Value *end_val = hi_val_ty->val_;

  /* Copied from VarDec::Translate */
  // 创建循环变量
  tr::Access *var_access = tr::Access::AllocLocal(level, true);

  // 初始化变量 (必须为IntTy)
  llvm::Value *fp = level->get_fp(level->get_sp());
  llvm::Value *var_addr = var_access->access_->ToLLVMVal(fp);
  llvm::Value *var_ptr = ir_builder->CreateIntToPtr(
      var_addr, llvm::PointerType::get(lo_val_ty->ty_->GetLLVMType(), 0));

  ir_builder->CreateStore(lo_val, var_ptr);

  // 将循环变量送入环境！
  venv->BeginScope();
  venv->Enter(this->var_, new env::VarEntry(var_access, lo_val_ty->ty_, true));

  // cond:
  ir_builder->CreateBr(condBB);
  ir_builder->SetInsertPoint(condBB);

  llvm::Value *var_val =
      ir_builder->CreateLoad(ir_builder->getInt32Ty(), var_ptr);
  llvm::Value *cond = ir_builder->CreateICmpSLE(var_val, end_val, "cond");

  // Check Condition. jump to afterBB
  ir_builder->CreateCondBr(cond, bodyBB, afterBB);

  // body:
  ir_builder->SetInsertPoint(bodyBB);
  loop_stack.push(afterBB);
  body_->Translate(venv, tenv, level, errormsg);
  loop_stack.pop();

  // 循环变量离开作用域
  venv->EndScope();
  llvm::Value *next_val = ir_builder->CreateAdd(
      var_val, llvm::ConstantInt::get(context, llvm::APInt(32, 1)), "next_val");
  ir_builder->CreateStore(next_val, var_ptr);

  // goto cond
  ir_builder->CreateBr(condBB);

  // after:
  ir_builder->SetInsertPoint(afterBB);

  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto afterBB = loop_stack.top();
  ir_builder->CreateBr(afterBB);
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto decs = this->decs_->GetList();
  venv->BeginScope();
  for (auto dec : decs) {
    dec->Translate(venv, tenv, level, errormsg);
  }
  // FIXME: Where does these expressions belong?
  ir_builder->SetInsertPoint(&func_stack.top()->getEntryBlock());
  auto val_ty = this->body_->Translate(venv, tenv, level, errormsg);
  venv->EndScope();
  return val_ty;
}

/**
    The expression type-id [expr] of expr creates a new arrtay of type type-id
  whose size is given by the expression in brackets.
  Initially, the array is filled with elements whose values are given by the
  expression after the `of`
 */
tr::ValAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */

  // Calculate the type of array?
  type::ArrayTy *array_ty =
      dynamic_cast<type::ArrayTy *>(tenv->Look(this->typ_));

  // Calculate len_val & init_val
  llvm::Value *len_val =
      this->size_->Translate(venv, tenv, level, errormsg)->val_;
  llvm::Value *init_val =
      this->init_->Translate(venv, tenv, level, errormsg)->val_;

  /* LINK: #C The C Functions */
  // Call an external initArray function
  llvm::Value *array_addr =
      ir_builder->CreateCall(init_array, {len_val, init_val});

  return new tr::ValAndTy(array_addr, array_ty);
}

tr::ValAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

} // namespace absyn