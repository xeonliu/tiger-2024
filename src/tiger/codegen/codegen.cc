#include "tiger/codegen/codegen.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"

#include <cassert>
#include <iostream>
#include <llvm-14/llvm/IR/BasicBlock.h>
#include <llvm-14/llvm/IR/Constants.h>
#include <llvm-14/llvm/IR/GlobalVariable.h>
#include <llvm-14/llvm/IR/Instruction.h>
#include <llvm-14/llvm/IR/Instructions.h>
#include <llvm-14/llvm/IR/Value.h>
#include <llvm-14/llvm/Support/Casting.h>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

int GenerateRandomNumber() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(1, 1000000000);
  return dis(gen);
}

void CodeGen::Codegen() {
  // 初始化，创建`temp_map_`和`bb_map_`
  // 将llvm::Value* 映射到临时变量
  temp_map_ = new std::unordered_map<llvm::Value *, temp::Temp *>();
  // 将Basic Block 映射到整数
  // NOTE: 用于 PHI 指令，跳转前将当前基本块的索引存入临时变量
  bb_map_ = new std::unordered_map<llvm::BasicBlock *, int>();
  // 初始化汇编指令列表，这也就是最终要生成的汇编代码
  auto *list = new assem::InstrList();
  // 处理全局字符串片段，将他们的地址加载到临时变量中，并生成相应的汇编指令。
  // NOTE: Load the addresses of all strings into the temporary register in
  // advance firstly get all global string's location
  for (auto &&frag : frags->GetList()) {
    if (auto *str_frag = dynamic_cast<frame::StringFrag *>(frag)) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "leaq " + std::string(str_frag->str_val_->getName()) + "(%rip),`d0",
          new temp::TempList(tmp), new temp::TempList(), nullptr));
      // NOTE: temp_map_ now maps the global string to the temporary register
      std::cerr << "Key: " << str_frag->str_val_ << "; Value: " << tmp
                << std::endl;
      temp_map_->insert({str_frag->str_val_, tmp});
    }
  }
  // 将函数参数移动到临时变量中
  // REVIEW - When was `traces_` initialized?
  // move arguments to temp
  auto arg_iter = traces_->GetBody()->arg_begin();
  auto regs = reg_manager->ArgRegs();
  auto tmp_iter = regs->GetList().begin();

  // first arguement is rsp, we need to skip it
  ++arg_iter;

  for (; arg_iter != traces_->GetBody()->arg_end() &&
         tmp_iter != regs->GetList().end();
       ++arg_iter, ++tmp_iter) {
    auto tmp = temp::TempFactory::NewTemp();
    list->Append(new assem::OperInstr("movq `s0,`d0", new temp::TempList(tmp),
                                      new temp::TempList(*tmp_iter), nullptr));
    std::cerr << "Key: " << static_cast<llvm::Value *>(arg_iter)
              << "; Value: " << tmp << std::endl;
    temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
  }

  // pass-by-stack parameters
  if (arg_iter != traces_->GetBody()->arg_end()) {
    auto last_sp = temp::TempFactory::NewTemp();
    list->Append(
        new assem::OperInstr("movq %rsp,`d0", new temp::TempList(last_sp),
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RSP)),
                             nullptr));
    list->Append(new assem::OperInstr(
        "addq $" + std::string(traces_->GetFunctionName()) +
            "_framesize_local,`s0",
        new temp::TempList(last_sp),
        new temp::TempList({last_sp, reg_manager->GetRegister(
                                         frame::X64RegManager::Reg::RSP)}),
        nullptr));
    while (arg_iter != traces_->GetBody()->arg_end()) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "movq " +
              std::to_string(8 * (arg_iter - traces_->GetBody()->arg_begin())) +
              "(`s0),`d0",
          new temp::TempList(tmp), new temp::TempList(last_sp), nullptr));
      temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
      ++arg_iter;
    }
  }
  // 构建 Basic Block 和 Index 的映射
  // construct bb_map
  int bb_index = 0;
  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    bb_map_->insert({bb, bb_index++});
  }
  // 记录每个LLVM指令的返回值
  // NOTE: All the return values of LLVM instructions are recorded in temp_map
  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // record every return value from llvm instruction
    for (auto &&inst : bb->getInstList()) {

      std::cerr << "Record Return Value of each inst" << std::endl;
      inst.print(llvm::errs());
      std::cerr << std::endl;

      temp_map_->insert({&inst, temp::TempFactory::NewTemp()});
    }
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // 遍历基本块列表，为每个基本块生成标签指令
    // Generate label for basic block
    list->Append(new assem::LabelInstr(std::string(bb->getName())));
    // 为每个基本块中的每个LLVM指令生成相应的汇编指令
    // Generate instructions for basic block
    for (auto &&inst : bb->getInstList())
      InstrSel(list, inst, traces_->GetFunctionName(), bb);
  }
  // 生成前序和后序代码？
  // TODO： ProcEntryExit1待实现
  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(
      frame::ProcEntryExit1(traces_->GetFunctionName(), list)));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}

/**
 * @brief  将LLVM IR 指令转换为汇编指令插入instr_list
 * @note
 * @param  *instr_list: 修改并传出
 * @param  &inst: 传入LLVM IR 指令
 * @param  function_name: 函数名 // function_name can be used to construct
 * return or exit label
 * @param  *bb: 该指令从属的 Basic Block // bb is to add move instruction to
 * record which block it jumps from
 * @retval None
 */
void CodeGen::InstrSel(assem::InstrList *instr_list, llvm::Instruction &inst,
                       std::string_view function_name, llvm::BasicBlock *bb) {
  // TODO: your lab5 code here

  /**
    1. llvm::Instruction::Load
    2. llvm::Instruction::Add/Sub/Mul/SDiv
    3. llvm::Instruction::PtrToInt/IntToPtr
    4. llvm::Instruction::GetElementPtr
    5. llvm::Instruction::Store
    6. llvm::Instruction::BitCast/ZExt
    7. llvm::Instruction::Call
    8. llvm::Instruction::Ret
    9. llvm::Instruction::Br
    10. llvm::Instruction::Icmp
    11. llvm::Instruction::Phi
  */

  /**
   We only support following instructions now:
   movq, addq, subq, imulq, idivq, leaq, callq, cmpq, jmp, je, jne, jg, jge,
   jl, jle, retq, cqto, sete, setne, setg, setge, setl, setle
   */

  // 操纵对象以寄存器为主？不止
  llvm::errs() << "Generating code for instruction: ";
  inst.print(llvm::errs());
  llvm::errs() << "\n";

  auto opcode = inst.getOpcode();

  switch (opcode) {
  case llvm::Instruction::Load: {

    llvm::LoadInst &load_inst = llvm::cast<llvm::LoadInst>(inst);

    // %10 = load i32, i32* %N_ptr, align 4
    // Get pointer value in load (%N_ptr)
    // Get the corresponding temp from temp_map
    // Key: %N_ptr; value: t102;
    // Key: %10; value: t105
    // Load from address value of t102 to t105

    llvm::Value *ptr = load_inst.getPointerOperand();
    llvm::Value *result = &inst;

    temp::Temp *result_temp = temp_map_->at(result);

    // Map every LLVM instruction’s generated value to a temporary register

    // NOTE: For global variable
    // %2 = load i64, i64* @tigermain_framesize_global, align 4
    if (llvm::GlobalVariable *global_var =
            llvm::dyn_cast<llvm::GlobalVariable>(ptr)) {
      std::string assem =
          "movq " + std::string(global_var->getName()) + "(%rip)" + ", `d0";
      instr_list->Append(new assem::OperInstr(
          assem, new temp::TempList(result_temp), nullptr, nullptr));

    } else {
      temp::Temp *ptr_temp = temp_map_->at(ptr);
      std::string assem = "movq (`s0), `d0";
      instr_list->Append(new assem::MoveInstr(assem,
                                              new temp::TempList(result_temp),
                                              new temp::TempList(ptr_temp)));
    }

    break;
  }
  case llvm::Instruction::Add:
  case llvm::Instruction::Sub: {
    // Use addq, subq, leaq
    llvm::Value *lhs = inst.getOperand(0);
    llvm::Value *rhs = inst.getOperand(1);
    llvm::Value *result = &inst;

    // NOTE: 我们需要跳过使用（也即Parent Stack Pointer）%0
    // 用于获取当前函数的栈指针的指令
    // 比如 %tigermain_sp = sub i64 %0, %2

    auto parent_sp =
        static_cast<llvm::Value *>(traces_->GetBody()->arg_begin());
    if (parent_sp == lhs) {
      break;
    }

    // lhs and rhs can be a constant int
    temp::Temp *lhs_temp = nullptr;
    temp::Temp *rhs_temp = nullptr;
    temp::Temp *result_temp = temp_map_->at(result);

    auto it = temp_map_->find(lhs);
    if (it != temp_map_->end()) {
      lhs_temp = it->second;
    }

    it = temp_map_->find(rhs);
    if (it != temp_map_->end()) {
      rhs_temp = it->second;
    }

    // lhs or rhs can be rsp
    if (IsRsp(lhs, function_name)) {
      lhs_temp = reg_manager->GetRegister(frame::X64RegManager::Reg::RSP);
    }

    if (IsRsp(rhs, function_name)) {
      rhs_temp = reg_manager->GetRegister(frame::X64RegManager::Reg::RSP);
    }

    // If Both temp exists

    if (lhs_temp && rhs_temp) {
      // %9 = add i64 %g_sp, %8
      // 翻译为一条 addq 和一条 moveq 指令
      // movq %rsp, t143
      // addq t142, t143

      // 1. movq lhs, result
      instr_list->Append(new assem::MoveInstr("movq `s0, `d0",
                                              new temp::TempList({result_temp}),
                                              new temp::TempList({lhs_temp})));

      // 2. addq/subq rhs, result
      std::string assem =
          opcode == llvm::Instruction::Add ? "addq `s0, `d0" : "subq `s0, `d0";
      instr_list->Append(
          new assem::OperInstr(assem, new temp::TempList({result_temp}),
                               new temp::TempList({rhs_temp}), nullptr));
      break;
    }

    // If there is at least one register
    temp::Temp *reg = nullptr;
    llvm::ConstantInt *const_int = nullptr;

    // If lhs is a register
    if (lhs_temp) {
      // Check if rhs is a constant int
      // %5 = add i64 %4, 8
      reg = lhs_temp;
      const_int = llvm::dyn_cast<llvm::ConstantInt>(rhs);
    } else if (rhs_temp) {
      // Check if lhs is a constant int
      // %16 = add i32 1, %15
      reg = rhs_temp;
      const_int = llvm::dyn_cast<llvm::ConstantInt>(lhs);
    } else {
      throw std::runtime_error("Unknown operand type");
    }

    // 常数加法可以使用leaq实现
    // leaq 8(%rsp), %rax

    std::string assem;
    if (opcode == llvm::Instruction::Add) {
      assem =
          "leaq " + std::to_string(const_int->getSExtValue()) + "(`s0), `d0";
    } else {
      assem =
          "leaq " + std::to_string(-const_int->getSExtValue()) + "(`s0), `d0";
    }

    instr_list->Append(
        new assem::OperInstr(assem, new temp::TempList({result_temp}),
                             new temp::TempList({reg}), nullptr));

    break;
  }
  case llvm::Instruction::Mul: {
    llvm::Value *lhs = inst.getOperand(0);
    llvm::Value *rhs = inst.getOperand(1);
    llvm::Value *result = &inst;

    temp::Temp *lhs_temp = nullptr;
    temp::Temp *rhs_temp = nullptr;
    temp::Temp *result_temp = temp_map_->at(result);

    auto it = temp_map_->find(rhs);
    if (it != temp_map_->end()) {
      rhs_temp = it->second;
    }

    it = temp_map_->find(lhs);
    if (it != temp_map_->end()) {
      lhs_temp = it->second;
    }

    if (lhs_temp && rhs_temp) {
      // 使用 imulq 指令
      // NOTE: 测评的模拟器只支持两操作数形式！

      // 1. movq lhs, result
      instr_list->Append(new assem::MoveInstr("movq `s0, `d0",
                                              new temp::TempList({result_temp}),
                                              new temp::TempList({lhs_temp})));

      // 2. imulq rhs, result
      instr_list->Append(new assem::OperInstr(
          "imulq `s0, `d0", new temp::TempList({result_temp}),
          new temp::TempList({rhs_temp}), nullptr));
      break;
    }

    // If there is at least one register
    temp::Temp *reg = nullptr;
    llvm::ConstantInt *const_int = nullptr;

    // If lhs is a register
    if (lhs_temp) {
      // Check if rhs is a constant int
      // %5 = add i64 %4, 8
      reg = lhs_temp;
      const_int = llvm::dyn_cast<llvm::ConstantInt>(rhs);
    } else if (rhs_temp) {
      // Check if lhs is a constant int
      // %16 = add i32 1, %15
      reg = rhs_temp;
      const_int = llvm::dyn_cast<llvm::ConstantInt>(lhs);
    } else {
      throw std::runtime_error("Unknown operand type");
    }

    // movq reg, result
    instr_list->Append(new assem::MoveInstr("movq `s0, `d0",
                                            new temp::TempList({result_temp}),
                                            new temp::TempList({reg})));
    // imulq const_int, result
    instr_list->Append(new assem::OperInstr(
        "imulq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
        new temp::TempList({result_temp}), nullptr, nullptr));

    break;
  }
  case llvm::Instruction::SDiv: {
    llvm::Value *lhs = inst.getOperand(0);
    llvm::Value *rhs = inst.getOperand(1);
    llvm::Value *result = &inst;

    temp::Temp *lhs_temp = temp_map_->at(lhs);
    temp::Temp *rhs_temp = nullptr;
    temp::Temp *result_temp = temp_map_->at(result);

    auto it = temp_map_->find(rhs);
    if (it != temp_map_->end()) {
      rhs_temp = it->second;
    }

    // 使用 idivq 指令
    // 需要将被除数放入 RAX 和 RDX 中
    instr_list->Append(new assem::OperInstr(
        "movq `s0, %rax", nullptr, new temp::TempList({lhs_temp}), nullptr));
    instr_list->Append(new assem::OperInstr("cqto", nullptr, nullptr,
                                            nullptr)); // 扩展 RAX 到 RDX:RAX

    if (rhs_temp) {
      // rhs_temp already exists, do nothing
    } else if (llvm::ConstantInt *const_int =
                   llvm::dyn_cast<llvm::ConstantInt>(rhs)) {
      rhs_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
          new temp::TempList({rhs_temp}), nullptr, nullptr));
    } else {
      throw std::runtime_error("Unknown operand type");
    }

    instr_list->Append(new assem::OperInstr(
        "idivq `s0", nullptr, new temp::TempList({rhs_temp}), nullptr));
    instr_list->Append(new assem::OperInstr(
        "movq %rax, `d0", new temp::TempList({result_temp}), nullptr, nullptr));

    break;
  }
  case llvm::Instruction::PtrToInt:
  case llvm::Instruction::IntToPtr: {
    // Use MoveInstr
    llvm::Value *src = inst.getOperand(0);
    llvm::Value *result = &inst;

    temp::Temp *src_temp = temp_map_->at(src);
    temp::Temp *result_temp = temp_map_->at(result);

    std::string assem = "movq `s0, `d0";
    instr_list->Append(new assem::MoveInstr(assem,
                                            new temp::TempList({result_temp}),
                                            new temp::TempList({src_temp})));
    break;
  }
  case llvm::Instruction::GetElementPtr: {
    // 处理 GetElementPtr 指令，GetElementPointer有三个参数
    // 测试集中仅有三参数（其中第二个参数为0）的取Record
    // 和两参数（两个均为寄存器）的取Array

    // Convert to GetElement Ptr
    llvm::GetElementPtrInst &gep_inst =
        llvm::cast<llvm::GetElementPtrInst>(inst);

    // 获取gep_inst的三个操作数
    llvm::Value *ptr = gep_inst.getPointerOperand();
    // Check Operand Number

    llvm::Value *index_1 = gep_inst.getOperand(1);
    llvm::Value *index_2 = nullptr;
    if (gep_inst.getNumOperands() == 3) {
      index_2 = gep_inst.getOperand(2);
    }
    llvm::Value *result = &gep_inst;

    temp::Temp *ptr_temp = temp_map_->at(ptr);
    temp::Temp *result_temp = temp_map_->at(result);

    // 关于Leaq指令的格式
    // leaq 8(%rbp, %rdi, 4), %rax

    if (!index_2) {
      // 两参数，存取数组
      // 判断index_1是否为常数
      temp::Temp *index_temp = nullptr;
      if (llvm::ConstantInt *const_int =
              llvm::dyn_cast<llvm::ConstantInt>(index_1)) {
        // 如果是常数，先移动到寄存器
        index_temp = temp::TempFactory::NewTemp();
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
            new temp::TempList({index_temp}), nullptr, nullptr));
      } else {
        // 如果不是常数，直接使用
        index_temp = temp_map_->at(index_1);
      }

      std::string assem = "leaq (`s0,`s1,8), `d0";
      instr_list->Append(new assem::OperInstr(
          assem, new temp::TempList({result_temp}),
          new temp::TempList({ptr_temp, index_temp}), nullptr));
      break;
    } else {
      // 三参数，存取Record
      // %5 = getelementptr %MyStruct, %MyStruct* %4, i32 0, i32 0

      // FIXME: 处理GEP第二个参数不为0的情况
      if (llvm::ConstantInt *const_int =
              llvm::dyn_cast<llvm::ConstantInt>(index_1)) {
        if (const_int->getSExtValue() != 0) {
          throw std::runtime_error(
              "Second parameter of GetElementPtr is not 0");
        }
      }

      temp::Temp *index_temp_2 = nullptr;
      if (llvm::ConstantInt *const_int =
              llvm::dyn_cast<llvm::ConstantInt>(index_2)) {
        // 如果是常数，先移动到寄存器
        index_temp_2 = temp::TempFactory::NewTemp();
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
            new temp::TempList(index_temp_2), nullptr, nullptr));
      } else {
        // 如果不是常数，直接使用
        index_temp_2 = temp_map_->at(index_2);
      }

      std::string assem = "leaq (`s0,`s1,8), `d0";
      instr_list->Append(new assem::OperInstr(
          assem, new temp::TempList({result_temp}),
          new temp::TempList({ptr_temp, index_temp_2}), nullptr));
      break;
    }
    break;
  }
  // 5. llvm::Instruction::Store
  case llvm::Instruction::Store: {
    llvm::Value *value = inst.getOperand(0);
    llvm::Value *ptr = inst.getOperand(1);

    // value can be constant int
    temp::Temp *value_temp = nullptr;
    temp::Temp *ptr_temp = temp_map_->at(ptr);

    auto it = temp_map_->find(value);
    if (it != temp_map_->end()) {
      value_temp = it->second;
    }
    // FIXME: What is source and what is destination?
    if (value_temp) {
      std::string assem = "movq `s0, (`d0)";
      instr_list->Append(
          new assem::MoveInstr(assem, new temp::TempList{ptr_temp},
                               new temp::TempList({value_temp})));
    } else if (llvm::ConstantInt *const_int =
                   llvm::dyn_cast<llvm::ConstantInt>(value)) {
      value_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ", (`d0)",
          new temp::TempList(ptr_temp), nullptr, nullptr));
    }

    break;
  }
  //   6. llvm::Instruction::BitCast/ZExt
  case llvm::Instruction::BitCast:
  case llvm::Instruction::ZExt: {
    llvm::Value *src = inst.getOperand(0);
    llvm::Value *result = &inst;

    temp::Temp *src_temp = temp_map_->at(src);
    temp::Temp *result_temp = temp_map_->at(result);

    std::string assem = "movq `s0, `d0";
    instr_list->Append(new assem::MoveInstr(assem,
                                            new temp::TempList({result_temp}),
                                            new temp::TempList({src_temp})));
    break;
  }
  //   7. llvm::Instruction::Call
  case llvm::Instruction::Call: {
    llvm::CallInst *call_inst = llvm::dyn_cast<llvm::CallInst>(&inst);
    if (!call_inst) {
      throw std::runtime_error("Failed to cast to CallInst");
    }

    llvm::Function *callee = call_inst->getCalledFunction();
    std::vector<llvm::Value *> args(call_inst->arg_begin(),
                                    call_inst->arg_end());

    /*
    * Skip the first %bsearch_sp parameter
      FIXME: Move all pass-by-stack values into stack
      Call the target function
      Get return value from %rax
    */

    for (int i = 0, r = 0; i < args.size(); i++, r++) {

      if (i == 0 && IsRsp(args[i], function_name)) {
        r--;
        continue;
      }

      // NOTE: 对于C函数，不应该跳过第一个参数
      llvm::Value *arg = args[i];
      temp::Temp *arg_temp = nullptr;

      llvm::errs() << "Argument type: ";
      arg->getType()->print(llvm::errs());
      llvm::errs() << "\n";

      llvm::errs() << "Argument value: ";
      arg->print(llvm::errs());
      llvm::errs() << "\n";

      auto it = temp_map_->find(arg);
      if (it != temp_map_->end()) {
        arg_temp = it->second;
      }

      if (IsRsp(arg, function_name)) {
        arg_temp = reg_manager->GetRegister(frame::X64RegManager::Reg::RSP);
      }

      // FIXME: 仅仅使用寄存器传参？
      // NOTE: 使用Round标记当前应使用的寄存器
      auto dst_temp = reg_manager->ArgRegs()->NthTemp(r);

      if (arg_temp) {
        instr_list->Append(new assem::MoveInstr(
            "movq `s0, `d0", new temp::TempList({dst_temp}),
            new temp::TempList({arg_temp}))); // Move argument to register
        // instr_list->Append(new assem::OperInstr(
        //     "movq `s0, " + std::to_string(8 * i) + "(%rsp)",
        //     new temp::TempList(arg_temp),
        //     new temp::TempList({arg_temp, reg_manager->GetRegister(
        //                                       frame::X64RegManager::Reg::RSP)}),
        //     nullptr));
      } else if (llvm::ConstantInt *const_int =
                     llvm::dyn_cast<llvm::ConstantInt>(arg)) {
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
            new temp::TempList(dst_temp), nullptr, nullptr));
        // instr_list->Append(new assem::OperInstr(
        //     "movq $" + std::to_string(const_int->getSExtValue()) + ", " +
        //         std::to_string(8 * i) + "(%rsp)",
        //     new temp::TempList(),
        //     new temp::TempList(
        //         {reg_manager->GetRegister(frame::X64RegManager::Reg::RSP)}),
        //     nullptr));
      } else {
        throw std::runtime_error("Unknown argument type");
      }
    }

    instr_list->Append(
        new assem::OperInstr("call " + callee->getName().str(),
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RAX)),
                             new temp::TempList(), nullptr));

    temp::Temp *result_temp = temp_map_->at(&inst);
    instr_list->Append(
        new assem::OperInstr("movq `s0, `d0", new temp::TempList({result_temp}),
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RAX)),
                             nullptr));
    break;
  }
  //   8. llvm::Instruction::Ret
  case llvm::Instruction::Ret: {
    // ret i32 %47
    // Move the value to %rax
    // Jump to the exit label
    // Hint: A part of return-related tasks will be done in ProcEntryExit3. You
    // may consider building a label in ProcEntryExit3

    llvm::ReturnInst *ret_inst = llvm::dyn_cast<llvm::ReturnInst>(&inst);
    if (!ret_inst) {
      throw std::runtime_error("Failed to cast to ReturnInst");
    }

    // Check if RetInst has param
    llvm::Value *ret_val = ret_inst->getReturnValue();

    // NOTE: For ret void
    if (!ret_val) {
      instr_list->Append(
          new assem::OperInstr("jmp " + std::string(function_name) + "_exit",
                               nullptr, nullptr, nullptr));
      break;
    }

    // Check if the return value is a constant int
    temp::Temp *ret_val_temp = nullptr;
    auto it = temp_map_->find(ret_val);
    if (it != temp_map_->end()) {
      ret_val_temp = it->second;
      // FIXME: what are jumps for?
      instr_list->Append(
          new assem::OperInstr("movq `s0, `d0",
                               new temp::TempList(reg_manager->GetRegister(
                                   frame::X64RegManager::Reg::RAX)),
                               new temp::TempList({ret_val_temp}), nullptr));

    } else if (llvm::ConstantInt *const_int =
                   llvm::dyn_cast<llvm::ConstantInt>(ret_val)) {
      ret_val_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
          new temp::TempList(
              reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          nullptr, nullptr));
    }

    instr_list->Append(
        new assem::OperInstr("jmp " + std::string(function_name) + "_exit",
                             nullptr, nullptr, nullptr));

    break;
  }
  //   9. llvm::Instruction::Br
  case llvm::Instruction::Br: {
    llvm::BranchInst *br_inst = llvm::dyn_cast<llvm::BranchInst>(&inst);
    if (!br_inst) {
      throw std::runtime_error("Failed to cast to BranchInst");
    }

    // br label %if_test
    if (br_inst->isUnconditional()) {
      llvm::BasicBlock *uncond_bb = br_inst->getSuccessor(0);

      // TODO: Before jump, move bb index to %rax?
      instr_list->Append(new assem::MoveInstr(
          "movq $" + std::to_string(bb_map_->at(bb)) + ", `d0",
          new temp::TempList(phi_temp_), nullptr));

      instr_list->Append(new assem::OperInstr(
          "jmp " + std::string(uncond_bb->getName()), new temp::TempList(),
          new temp::TempList(), nullptr));
      break;
    }

    // br i1 %20, label %if_then, label %if_else

    // Get the value of %20 (which is set in icmp instruction)
    // Compare with 1 or 0, and jump to the corresponding label

    llvm::Value *cond = br_inst->getCondition();
    llvm::BasicBlock *true_bb = br_inst->getSuccessor(0);
    llvm::BasicBlock *false_bb = br_inst->getSuccessor(1);

    temp::Temp *cond_temp = temp_map_->at(cond);

    // Compare with 1 or 0
    // FIXME: Maybe check 0 is better?
    instr_list->Append(new assem::OperInstr(
        "cmpq $1, `s0", nullptr, new temp::TempList({cond_temp}), nullptr));

    // Jump to the corresponding label
    // TODO: Before jump, move bb index to %rax?
    instr_list->Append(new assem::MoveInstr(
        "movq $" + std::to_string(bb_map_->at(bb)) + ", `d0",
        new temp::TempList(phi_temp_), nullptr));

    instr_list->Append(new assem::OperInstr(
        "je " + std::string(true_bb->getName()), nullptr, nullptr, nullptr));

    instr_list->Append(new assem::OperInstr(
        "jmp " + std::string(false_bb->getName()), nullptr, nullptr, nullptr));

    break;
  }
    //   10. llvm::Instruction::Icmp
  case llvm::Instruction::ICmp: {
    // Get all Operands
    // Compare them according to the Icmp condition
    // Using `set` assembly instruction to move the compare result into a
    // temporary register

    llvm::ICmpInst *icmp_inst = llvm::dyn_cast<llvm::ICmpInst>(&inst);
    if (!icmp_inst) {
      throw std::runtime_error("Failed to cast to ICmpInst");
    }

    llvm::Value *lhs = icmp_inst->getOperand(0);
    llvm::Value *rhs = icmp_inst->getOperand(1);
    llvm::Value *result = &inst;

    temp::Temp *lhs_temp = temp_map_->at(lhs);
    // rhs can be constant int
    temp::Temp *rhs_temp = nullptr;
    auto it = temp_map_->find(rhs);
    if (it != temp_map_->end()) {
      rhs_temp = it->second;
    }

    temp::Temp *result_temp = temp_map_->at(result);

    std::string assem;
    switch (icmp_inst->getPredicate()) {
    case llvm::ICmpInst::ICMP_EQ:
      assem = "sete";
      break;
    case llvm::ICmpInst::ICMP_NE:
      assem = "setne";
      break;
    case llvm::ICmpInst::ICMP_SGT:
      assem = "setg";
      break;
    case llvm::ICmpInst::ICMP_SGE:
      assem = "setge";
      break;
    case llvm::ICmpInst::ICMP_SLT:
      assem = "setl";
      break;
    case llvm::ICmpInst::ICMP_SLE:
      assem = "setle";
      break;
    default:
      throw std::runtime_error("Unknown ICmp predicate");
    }

    if (rhs_temp) {
      instr_list->Append(new assem::OperInstr(
          "cmpq `s1, `s0", nullptr, new temp::TempList({lhs_temp, rhs_temp}),
          nullptr));
    } else if (llvm::ConstantInt *const_int =
                   llvm::dyn_cast<llvm::ConstantInt>(rhs)) {
      instr_list->Append(new assem::OperInstr(
          "cmpq $" + std::to_string(const_int->getSExtValue()) + ", `s0",
          nullptr, new temp::TempList({lhs_temp}), nullptr));
    }

    instr_list->Append(new assem::OperInstr(
        assem + " `d0", new temp::TempList({result_temp}),
        new temp::TempList({lhs_temp, rhs_temp}), nullptr));

    break;
  }
  // 11. llvm::Instruction::Phi
  case llvm::Instruction::PHI: {
    // Get the value of the phi node
    // Move it to the result temp

    // opand_next:                 ; preds = %opand_right_test, %isdigit
    //   %42 = phi i1 [ false, %isdigit ], [ %41, %opand_right_test ]
    // %47 = phi %MyStruct.0* [ %34, %if_then ], [ null, %if_else ]
    // Review
    // The value of %42 is defined as follows:
    // If it jumps from %opand_right_test, it is %41
    // If it jumps from %isdigit, it is false

    // How can we know where it jumps from?

    // In every unique label:
    // Just move right value into the temporary register of PHI node
    // And jump to the end label

    llvm::PHINode *phi_inst = llvm::dyn_cast<llvm::PHINode>(&inst);
    if (!phi_inst) {
      throw std::runtime_error("Failed to cast to PHINode");
    }

    temp::Temp *inst_temp = temp_map_->at(phi_inst);
    llvm::BasicBlock *phi_bb = phi_inst->getParent();
    std::string phi_bb_name = phi_bb->getName().str();

    // Generate labels for each incoming value
    std::vector<std::string> labels;
    for (unsigned i = 0; i < phi_inst->getNumIncomingValues(); ++i) {
      // TODO: Use non conflicting random numbers as labels

      // Generate random number

      std::string label = phi_bb_name + "_phi_" + std::to_string(i) + "_" +
                          std::to_string(GenerateRandomNumber());
      labels.push_back(label);
    }
    std::string end_label = phi_bb_name + "_phi_end";

    // TODO: Before every jump, move bb index into %rax
    // FIXME: move into phi_temp_?

    // In PHI, generate compare instructions:
    // Compare between index and %rax (phi_temp_)?
    // Jump to the right labels
    for (unsigned i = 0; i < phi_inst->getNumIncomingValues(); ++i) {
      llvm::BasicBlock *incoming_bb = phi_inst->getIncomingBlock(i);
      int block_num = bb_map_->at(incoming_bb);
      instr_list->Append(new assem::OperInstr(
          "cmpq $" + std::to_string(block_num) + ", `s0", nullptr,
          new temp::TempList(phi_temp_), nullptr));

      instr_list->Append(
          new assem::OperInstr("je " + labels[i], nullptr, nullptr, nullptr));
    }

    // Generate compare instructions and jump to the right labels
    for (unsigned i = 0; i < phi_inst->getNumIncomingValues(); ++i) {
      // In PHI, generate labels:
      // Every condition will have a unique label
      instr_list->Append(new assem::LabelInstr(labels[i]));
      // incoming value can be int or register
      llvm::Value *incoming_value = phi_inst->getIncomingValue(i);

      temp::Temp *incoming_temp = nullptr;
      auto it = temp_map_->find(incoming_value);
      if (it != temp_map_->end()) {
        incoming_temp = it->second;
      }
      llvm::errs() << "SHIT";
      if (incoming_temp) {
        instr_list->Append(new assem::OperInstr(
            "movq `s0, `d0", new temp::TempList({inst_temp}),
            new temp::TempList({incoming_temp}), nullptr));
      }

      else if (llvm::ConstantInt *const_int =
                   llvm::dyn_cast<llvm::ConstantInt>(incoming_value)) {
        if (const_int->getType()->isIntegerTy(1)) {
          // FIXME: 为何True被翻译成-1？
          // Boolean true is represented as -1 in LLVM IR
          int64_t bool_value = const_int->getSExtValue() ? 1 : 0;
          instr_list->Append(new assem::OperInstr(
              "movq $" + std::to_string(bool_value) + ", `d0",
              new temp::TempList({inst_temp}), nullptr, nullptr));
        } else {
          instr_list->Append(new assem::OperInstr(
              "movq $" + std::to_string(const_int->getSExtValue()) + ", `d0",
              new temp::TempList({inst_temp}), nullptr, nullptr));
        }
      } else if (llvm::ConstantPointerNull *const_ptr =
                     llvm::dyn_cast<llvm::ConstantPointerNull>(
                         incoming_value)) {
        // if incoming_value is null
        // merge.tig.ll: %47 = phi %MyStruct.0* [ %34, %if_then ], [ null,
        // %if_else ]
        instr_list->Append(new assem::MoveInstr(
            "movq $0, `d0", new temp::TempList({inst_temp}), nullptr));
      } else {
        throw std::runtime_error("Unknown incoming value type");
      }

      instr_list->Append(
          new assem::OperInstr("jmp " + end_label, nullptr, nullptr, nullptr));
    }

    // And an end label just shows the end of PHI node
    instr_list->Append(new assem::LabelInstr(end_label));

    break;
  }
  default:
    throw std::runtime_error(std::string("Unknown instruction: ") +
                             inst.getOpcodeName());
  }
}

} // namespace cg