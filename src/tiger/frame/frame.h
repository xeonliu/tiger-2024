#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/frame/temp.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  temp::Map *temp_map_;

protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
public:
  /* TODO: Put your lab5-part1 code here */

  virtual ~Access() = default;
  /* Base addr is frame pointer (stack top) */
  virtual llvm::Value *ToLLVMVal(llvm::Value *fp) const = 0;
};

/**
  foo():
    dec X, Y, Z
    bar(X, Y+Z)
  
  ---------------------- foo's stack top
    | Z           |
    | Y           |       local vars in foo
    | X           |
    -------------------- (-offset)
    | arg2        |
    | arg1        |       args for bar. Belong to Foo's Allocation Actual Frame Size
    | Static Link |
    -------------------- (outgo_size_)
    | Return Address |
    -------------------- bar's stack top
    | k |
    | j |
    | i |

 */
class Frame {
public:
  // 是一个整数，表示函数调用时需要为传递给其他函数的参数预留的栈空间的大小
  // 给传参和Static Link 预留的
  // 在构造时初始化，在AllocOutgoSpace中修改
  int outgo_size_;
  // offset_是一个整数，表示当前Frame中局部变量和参数的偏移量，是一个负数
  // 在构造时初始化，在AllocLocal中再次修改
  // 表示下一次分配InFrameAccess时其Access的值？
  // 同时体现了已经分配的本地变量区域的尺寸
  int offset_;
  // 函数名？
  temp::Label *name_;
  // 这个Formals是给哪一层的？
  // 形式参数 & Static Link
  std::list<frame::Access *> *formals_;
  llvm::GlobalVariable *framesize_global;
  // 栈顶指针
  llvm::Value *sp;

  Frame(int outgo_size, int offset, temp::Label *name,
        std::list<frame::Access *> *formals)
      : outgo_size_(outgo_size), offset_(offset), name_(name),
        formals_(formals) {}

  [[nodiscard]] virtual std::string GetLabel() const = 0;
  [[nodiscard]] virtual temp::Label *Name() const = 0;
  [[nodiscard]] virtual std::list<frame::Access *> *Formals() const = 0;
  virtual frame::Access *AllocLocal(bool escape) = 0;
  virtual void AllocOutgoSpace(int size) = 0;
  /**
    offset_ 负数，局部变量和参数的偏移量
    outgo_size_ 预留的栈空间的大小
    8 返回地址的大小
   */
  int calculateActualFramesize() {
    return (-offset_ + outgo_size_) + 8;
  }
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
    FrameSize,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase,
                           bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  llvm::Value *str_val_;
  std::string str_;

  StringFrag(llvm::Value *str_val, std::string str)
      : str_val_(str_val), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class FrameSizeFrag : public Frag {
public:
  llvm::Value *framesize_val_;
  int framesize_;

  FrameSizeFrag(llvm::Value *framesize_val, int framesize)
      : framesize_val_(framesize_val), framesize_(framesize) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  llvm::Function *body_;

  ProcFrag(llvm::Function *body) : body_(body) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag);
  const std::list<Frag *> &GetList() { return frags_; }

private:
  std::list<Frag *> frags_;
};

/**
  接受一个函数名和一个布尔列表，表示参数是否需要逃逸（在堆上分配）
  @param name 函数名称
  @param formals 函数参数是否需要逃逸
 */
frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals);

} // namespace frame

#endif