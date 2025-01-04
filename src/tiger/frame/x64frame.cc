#include "tiger/frame/x64frame.h"
#include "tiger/codegen/assem.h"
#include "tiger/env/env.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"

#include <iostream>
#include <list>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
  for (int i = 0; i < REG_COUNT; i++)
    regs_.push_back(temp::TempFactory::NewTemp());

  // Note: no frame pointer in tiger compiler
  std::array<std::string_view, REG_COUNT> reg_name{
      "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
      "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
  int reg = RAX;
  for (auto &name : reg_name) {
    temp_map_->Enter(regs_[reg], new std::string(name));
    reg++;
  }
}

temp::TempList *X64RegManager::Registers() {
  const std::array reg_array{
      RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15,
  };
  auto *temp_list = new temp::TempList();
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  const std::array reg_array{RDI, RSI, RDX, RCX, R8, R9};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  std::array reg_array{RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  std::array reg_array{RBP, RBX, R12, R13, R14, R15};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

/**
  TODO: What is Return Sink??
  在函数返回时，这些寄存器的值需要进行恢复？？
 */
temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temp_list = CalleeSaves();
  // Stack Pointer
  temp_list->Append(regs_[SP]);
  // Return Value，返回值储存在 %rax 中
  temp_list->Append(regs_[RV]);
  return temp_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[FP]; }

/**
  表示对当前Frame中变量的访问。
  包括本地变量和函数参数
 */
class InFrameAccess : public Access {
public:
  // `offset`表示对相对当前 `栈顶` 的 Offset
  // 对于变量是创建该变量时的Offset，是一个负数
  // 对于参数而言是一个正数
  int offset;
  // `parent_frame`指向当前的 Frame
  frame::Frame *parent_frame;

  explicit InFrameAccess(int offset, frame::Frame *parent)
      : offset(offset), parent_frame(parent) {}

  /* TODO: Put your lab5-part1 code here */
  // Calculate the address of the access
  // Needed to convert into ptr later
  llvm::Value *ToLLVMVal(llvm::Value *fp) const override {
    return ir_builder->CreateAdd(
        fp, llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(ir_module->getContext()), offset));
  }
};

class X64Frame : public Frame {
public:
  X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
      : Frame(8, 0, name, formals) {}

  [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override {
    return formals_;
  }

  /**
    在当前 Frame 中分配一个局部变量，并返回一个表示该局部变量的frame::Access对象
   */
  frame::Access *AllocLocal(bool escape) override {
    frame::Access *access;

    offset_ -= reg_manager->WordSize();
    access = new InFrameAccess(offset_, this);

    return access;
  }

  /**
    修改outgo_size_，处理Call指令时使用。
    拷贝参数由Callee实现。
    >
    使用llvm的调用命令传递实参，再由callee将接收到的参数拷贝到outgo空间里对应access的位置
   */
  void AllocOutgoSpace(int size) override {
    if (size > outgo_size_)
      outgo_size_ = size;
  }
};

/**
  通过变量是否逃逸生成Frame
 */
frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  /* TODO: Put your lab5-part1 code here */
  // Construct Access for formals
  Frame *frame = new X64Frame(name, nullptr);
  std::list<frame::Access *> *formal_accesses =
      new std::list<frame::Access *>();

  int word_size = reg_manager->WordSize();
  int offset = word_size;
  // TMD SB 这Access根本就不再当前Frame中

  // Static Link
  Access *access = new InFrameAccess(offset, frame);
  formal_accesses->emplace_back(access);

  for (auto escape : formals) {
    offset += word_size;
    access = new InFrameAccess(offset, frame);
    formal_accesses->emplace_back(access);
  }

  frame->formals_ = formal_accesses;

  return frame;
}

/**
 * Moving incoming formal parameters, the saving and restoring of callee-save
 * Registers
 * @param frame curruent frame
 * @param stm statements
 * @return statements with saving, restoring and view shift
 */
assem::InstrList *ProcEntryExit1(std::string_view function_name,
                                 assem::InstrList *body) {
  // TODO: your lab5 code here

  body->Append(new assem::LabelInstr(std::string(function_name) + "_exit"));

  for (auto reg : reg_manager->CalleeSaves()->GetList()) {
    // Create a new temp
    auto temp = temp::TempFactory::NewTemp();
    auto iter = body->GetList().begin();
    /*
     * 1. Store instructions to save any **callee-saved registers**- including
     * the return address register – used within the function
     */
    body->Insert(
        iter, new assem::OperInstr("movq `s0, `d0", new temp::TempList({temp}),
                                   new temp::TempList({reg}), nullptr));
    /*
     * 2. Load instructions to restore the **callee-save registers**
     */
    body->Append(new assem::OperInstr("movq `s0, `d0",
                                      new temp::TempList({reg}),
                                      new temp::TempList({temp}), nullptr));
  }

  return body;
}

/**
 * Appends a “sink” instruction to the function body to tell the register
 * allocator that certain registers are live at procedure exit
 * @param body function body
 * @return instructions with sink instruction
 */
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  // 添加在InstrList之后
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

/**
 * The procedure entry/exit sequences
 * @param frame the frame of current func
 * @param body current function body
 * @return whole instruction list with prolog_ end epilog_
 */
assem::Proc *ProcEntryExit3(std::string_view function_name,
                            assem::InstrList *body) {
  std::string prologue = "";
  std::string epilogue = "";

  // TODO: your lab5 code here
  // 1. Pseudo-instructions to announce the beginning of a function;
  // 2. A label definition of the function name
  prologue += std::string(function_name) + std::string(":\n");
  // 3. An instruction to adjust the stack pointer.
  prologue +=
      "movq " + std::string(function_name) + "_framesize_global(%rip), %rax\n";
  prologue += "subq %rax, %rsp\n";

  // 4. An instruction to reset the stack pointer (to deallocate the frame)
  // FIXME: Why %rdi?
  epilogue +=
      "movq " + std::string(function_name) + "_framesize_global(%rip), %rdi\n";
  epilogue += "addq %rdi, %rsp\n";
  // 5. A return instruction (Jump to the return address)
  epilogue += "retq\n";
  // 6. Pseduo-instructions, as needed, to announce the end of a function

  return new assem::Proc(prologue, body, epilogue);
}

void Frags::PushBack(Frag *frag) { frags_.emplace_back(frag); }

} // namespace frame