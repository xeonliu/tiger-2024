#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/semant/types.h"
#include <cstddef>

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // Call Different Analyser
  // Report Error using: errormsg->Error(int pos, std::string_view message, ...)
  // TODO: What Does Label Count mean?
  this->root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg,
                                bool readonly) const {
  // Determine Type of var and return it.
  // Singeleton
  // Have Access to this->sym_

  // 1. Identifier is in the environment
  // 2. It binds to a VarEntry (not FunEntry)

  env::VarEntry *var_entry =
      dynamic_cast<env::VarEntry *>(venv->Look(this->sym_));

  if (var_entry == nullptr) {

    // Is std::string in C++ \0 terminated?
    // Nope. c_str() method returns a zero-terminated C String.
    errormsg->Error(this->pos_, "undefined variable %s",
                    this->sym_->Name().c_str());

    // TODO: What should I return?
    return nullptr;
  }

  // Check if the var is required to be readonly
  if (!readonly && var_entry->readonly_) {
    errormsg->Error(this->pos_, "loop variable can't be assigned");
  }

  // 3. Sometimes VarEntry may have type::NameTy type.
  // However we should return an "Actual Type", which traces up to their final
  // definition.
  return var_entry->ty_->ActualTy();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg,
                               bool readonly) const {
  /* TODO: Put your lab4 code here */
  // How to get the type of var.sym?

  // The type of var should be RecordTy?
  // TODO: Use `Ty::IsSameType()`?
  type::RecordTy *record_ty = dynamic_cast<type::RecordTy *>(
      this->var_->SemAnalyze(venv, tenv, labelcount, errormsg));

  if (record_ty == nullptr) {
    errormsg->Error(this->pos_, "not a record type");
  }

  // Get the type of sim.
  // Find the field in fieldlist
  // absyn::FieldList -> type::FieldList -> std::list<type::Field *>
  // Find the type of this->sim_ in field_list
  std::list<type::Field *> field_list = record_ty->fields_->GetList();

  for (auto field : field_list) {
    if (field->name_ == this->sym_) {
      return field->ty_;
    }
  }

  errormsg->Error(this->pos_, "field %s doesn't exist",
                  this->sym_->Name().c_str());

  return nullptr;
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount, err::ErrorMsg *errormsg,
                                   bool readonly) const {
  /* TODO: Put your lab4 code here */
  type::ArrayTy *array_ty = dynamic_cast<type::ArrayTy *>(
      this->var_->SemAnalyze(venv, tenv, labelcount, errormsg));

  if (array_ty == nullptr) {
    errormsg->Error(this->pos_, "array type required");
  }

  return array_ty->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // 1. Identifier is in the environment
  env::FunEntry *fun_entry =
      dynamic_cast<env::FunEntry *>(venv->Look(this->func_));

  if (fun_entry == nullptr) {
    errormsg->Error(this->pos_, "undefined function %s",
                    this->func_->Name().c_str());
    return nullptr;
  }

  // 2. Check whether arguments are defined and whether their type matches
  auto param_list = fun_entry->formals_->GetList();
  auto arg_list = this->args_->GetList();

  auto param_it = param_list.begin();
  auto arg_it = arg_list.begin();

  while (param_it != param_list.end() && arg_it != arg_list.end()) {
    auto arg_type = (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!arg_type->IsSameType(*param_it)) {
      errormsg->Error(this->pos_, "para type mismatch");
    }
    ++param_it;
    ++arg_it;
  }

  if (arg_it != arg_list.end()) {
    errormsg->Error(this->pos_, "too many params in function %s",
                    this->func_->Name().c_str());
  }

  return fun_entry->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto oper = this->oper_;
  auto left = this->left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto right = this->right_->SemAnalyze(venv, tenv, labelcount, errormsg);

  // Must be same type. Either both int or both string.
  // For Test 16
  switch (oper) {
  // For Test 13 & 14. Comparison.
  case Oper::GE_OP:
  case Oper::GT_OP:
  case Oper::LE_OP:
  case Oper::LT_OP:
  case Oper::NEQ_OP:
    if (!left->IsSameType(right)) {
      errormsg->Error(this->pos_, "same type required");
    }
    break;
  // For Test 16. Alrithmetic
  case Oper::PLUS_OP:
  case Oper::MINUS_OP:
  case Oper::TIMES_OP:
  case Oper::DIVIDE_OP:
    using type::IntTy;
    if (!left->IsSameType(IntTy::Instance())) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (!right->IsSameType(IntTy::Instance())) {
      errormsg->Error(left_->pos_, "integer required");
    }
    break;
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // Get the type of this->typ_(Symbol)
  // 1. Get the Record Type
  auto record_type = dynamic_cast<type::RecordTy *>(tenv->Look(this->typ_));

  if (record_type == nullptr) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->typ_->Name().c_str());
  }

  // 2. Verify name & type
  const auto exp_fields = this->fields_->GetList();
  const auto type_fields = record_type->fields_->GetList();

  if (exp_fields.size() != type_fields.size()) {
    errormsg->Error(this->pos_, "Field number mismatch");
  }

  auto exp_it = exp_fields.begin();
  auto type_it = type_fields.begin();

  while (exp_it != exp_fields.end() && type_it != type_fields.end()) {
    auto exp_field = *exp_it;
    auto type_field = *type_it;

    if (exp_field->name_ != type_field->name_) {
      errormsg->Error(this->pos_, "field name mismatch");
    }

    auto exp_field_type =
        exp_field->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!exp_field_type->IsSameType(type_field->ty_)) {
      errormsg->Error(this->pos_, "field type mismatch");
    }

    ++exp_it;
    ++type_it;
  }

  return record_type;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // Doing semantic analysis on sub exps?
  type::Ty *ty = nullptr;

  for (auto exp : this->seq_->GetList()) {
    ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  return ty->ActualTy();
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */

  // TODO: Check if the var_ can be assaigned!
  // test11.tig:3.12:loop variable can't be assigned
  // Well. All Vars should be able to be assigned?
  // Maybe we should handle it at var.
  // But what about varexp?
  // I think I have to change the function signature!
  type::Ty *var_type =
      this->var_->SemAnalyze(venv, tenv, labelcount, errormsg, false);
  type::Ty *exp_type = this->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!var_type->IsSameType(exp_type)) {
    errormsg->Error(this->pos_, "unmatched assign exp");
  }

  // The assignment expression produces **no value**
  return type::VoidTy::Instance();
}

/*
  Return a value if and only if has else
  Otherwise return void
*/
type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  this->test_->SemAnalyze(venv, tenv, labelcount, errormsg);

  auto exp2_type = this->then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (this->elsee_) {
    auto exp3_type = this->elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!exp2_type->IsSameType(exp3_type)) {
      errormsg->Error(this->pos_, "then exp and else exp type mismatch");
    }
    return exp2_type;
  } else {
    if (!exp2_type->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(this->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  this->test_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!this->body_->SemAnalyze(venv, tenv, labelcount, errormsg)
           ->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(this->pos_, "while body must produce no value");
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // Contains declaration!

  // exp1 to exp2
  auto exp1_type = this->lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto exp2_type = this->hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!exp1_type->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->lo_->pos_, "for exp's range type is not integer");
  }

  if (!exp2_type->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->hi_->pos_, "for exp's range type is not integer");
  }

  // id :=
  venv->BeginScope();
  // Scope within exp3, cannot assign in exp3.
  venv->Enter(this->var_, new env::VarEntry(exp1_type, true));

  // exp3 must produce no value
  if (!this->body_->SemAnalyze(venv, tenv, labelcount, errormsg)
           ->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(this->pos_, "Exp3 Must Produce no value");
  }
  venv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // 1. Evaluates the declarations
  // DecList
  auto decs = this->decs_->GetList();
  // BeginScope?
  venv->BeginScope();
  for (auto dec : decs) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  // 2. Evaluate the declarations, binding types, variables, and functions to
  // the scope of the expression sequence Should be absyn::SeqExp?
  auto type = this->body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  venv->EndScope();

  return type;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // The expression type-id [expr] of expr creates a new arrtay of type type-id
  // whose size is given by the expression in brackets.
  // Initially, the array is filled with elements whose values are given by the
  // expression after the `of`

  // Example:
  // type intArray = array of int
  // var row := intArray [N] of 0

  // 1. evaluate type
  type::ArrayTy *array_ty =
      dynamic_cast<type::ArrayTy *>(tenv->Look(this->typ_));

  auto element_ty = array_ty->ty_;

  // 2. Evaluate expression in the brackets
  auto size_type = this->size_->SemAnalyze(venv, tenv, labelcount, errormsg);

  // Check Size type
  if (!size_type->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(this->pos_, "size must be int");
  }

  auto init_type = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);

  // Check init type
  if (!init_type->IsSameType(element_ty)) {
    errormsg->Error(this->pos_, "type mismatch");
  }

  return array_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // There can be a list of function declarations.

  env::VEnvPtr declared_functions = new env::VEnv();

  for (auto fundec : this->functions_->GetList()) {
    // Fundec: name, params, result

    // 1. Result Type
    // NOTE: fundec->result_ can be nullptr.
    type::Ty *result_ty = fundec->result_ ? tenv->Look(fundec->result_)
                                          : type::VoidTy::Instance();

    // //   Field(int pos, sym::Symbol *name, sym::Symbol *typ)
    // //  : pos_(pos), name_(name), typ_(typ), escape_(true) {}
    // auto fields = fundec->params_->MakeFieldList(tenv, errormsg);

    // Convert Fields to Param Types.
    // 2. Param Type
    type::TyList *formal_tys = fundec->params_->MakeFormalTyList(
        tenv, errormsg); // Generate Formal TyList.

    // Bind Funcdef in outer
    // Bind name with result type and param types.
    auto fun_entry = new env::FunEntry(formal_tys, result_ty);
    venv->Enter(fundec->name_, fun_entry);

    // There can't be two functions with same name! Test 39.
    if(auto entry = declared_functions->Look(fundec->name_)) {
      errormsg->Error(this->pos_, "two functions have the same name");
    }

    declared_functions->Enter(fundec->name_, fun_entry);
  }

  // Stage 2: For Mutually Recursive defs.
  for (auto fundec : this->functions_->GetList()) {
    venv->BeginScope();
    // Bind each param with its type
    std::list<type::Field *> fields =
        fundec->params_->MakeFieldList(tenv, errormsg)->GetList();
    for (auto field : fields) {
      // TODO: Is it read-only?
      venv->Enter(field->name_, new env::VarEntry(field->ty_));
    }
    // 3. Deal with Body
    auto body_type =
        fundec->body_->SemAnalyze(venv, tenv, labelcount, errormsg);

    env::FunEntry *func_entry =
        dynamic_cast<env::FunEntry *>(venv->Look(fundec->name_));

    if (func_entry->result_->IsSameType(type::VoidTy::Instance()) &&
        !body_type->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(this->pos_, "procedure returns value");
    }

    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto exp_type = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  // If the type is not specified, the variable’s type comes from the
  // expression.
  if (this->typ_ == nullptr) {
    // NOTE: var := RecordExp
    if(exp_type->ActualTy() == type::NilTy::Instance()) {
      errormsg->Error(this->pos_, "init should not be nil without type specified");
    }
    venv->Enter(this->var_, new env::VarEntry(exp_type));
  } else {
    auto typ_type = tenv->Look(this->typ_);
    if (!typ_type->IsSameType(exp_type)) {
      errormsg->Error(this->pos_, "type mismatch");
    } else {
      venv->Enter(this->var_, new env::VarEntry(typ_type));
    }
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // Multiple Tydec on multiple lines.
  // Example: Test 14
  // type arrtype = array of int
  // type rectype = {name:string, id: int}

  env::TEnvPtr declared_types = new env::TEnv();

  for (auto tydec : this->types_->GetList()) {
    /* tydec -> type type-id = ty */

    // For Mutually Recursive Declarations.
    // TODO： Send type_id into tenv with type::NameTy(tydec->name, nullptr)?
    tenv->Enter(tydec->name_, new type::NameTy(tydec->name_, nullptr));

    auto type = tydec->ty_->SemAnalyze(tenv, errormsg);

    // TODO: HACK for test 16: If type equals NameTy, break.
    if (dynamic_cast<type::NameTy *>(type->ActualTy()) != nullptr) {
      errormsg->Error(tydec->ty_->pos_, "illegal type cycle");
      break;
    }

    // Bind Name With Type
    tenv->Enter(tydec->name_, type);
    if (auto old_type = declared_types->Look(tydec->name_)) {
      if (!old_type->IsSameType(type)) {
        errormsg->Error(tydec->ty_->pos_, "two types have the same name");
        continue;
      }
    }
    declared_types->Enter(tydec->name_, type);
  }
}

/**
  Should return NameTy(sym::Symbol *sym, Ty *ty).
  Never returns null.
 */
type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // ty -> type-id

  // TODO: 只要到达任何Ty_Name类型，transTy就应停止？
  auto type = tenv->Look(this->name_);
  if (type == nullptr) {
    type = new type::NameTy(this->name_, nullptr);
  }
  return type;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // ty -> { tyfields }
  // What's the type of Field List??
  return new type::RecordTy(this->record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // ty -> array of type-id
  auto element_type = tenv->Look(this->array_);
  return new type::ArrayTy(element_type);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
