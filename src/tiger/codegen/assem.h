#ifndef TIGER_CODEGEN_ASSEM_H_
#define TIGER_CODEGEN_ASSEM_H_

#include <cstdio>
#include <string>
#include <vector>

#include "tiger/frame/temp.h"

namespace assem {

/**!SECTION
跳转目标的集合
包含一个标签的列表
 */
class Targets {
public:
  std::vector<temp::Label *> *labels_;

  explicit Targets(std::vector<temp::Label *> *labels) : labels_(labels) {}
};

/**!SECTION
表示汇编指令
 */
class Instr {
public:
  virtual ~Instr() = default;

  /**
   * @brief  Formats an assembly instruction as a string
   * @note   Prints it into the file `out`
   * @param  *out: file
   * @param  *m: tells the register assignment of every temp
   * @retval None
   */
  virtual void Print(FILE *out, temp::Map *m) const = 0;
  // Obtain a list of temporary registers defined by this instruction
  // e.g. 对于指令`movq (t200), t201`
  // 该指令定义的临时变量即为t201,使用的即为t200
  [[nodiscard]] virtual temp::TempList *Def() const = 0;
  // Obtain a list of temporary registers used by this instruction
  [[nodiscard]] virtual temp::TempList *Use() const = 0;
};
/**
 * @brief  An OperInstr holds an assembly-language instruction assem_
A list of operand registers src_
A list of result registers dst_
 * @note
 * @retval None
  表示操作指令
 */
class OperInstr : public Instr {
public:
  // 汇编字符串
  std::string assem_;
  // 目标临时变量、源临时变量
  temp::TempList *dst_, *src_;

  // 跳转目标
  /**
    + Operations always fall through to the next instruction have jumps_=
    nullptr

    + Other operations have a list of “target” labels
     + to which they may jump
     + it must explicitly include the next instruction if it is possible to
   fall through to it
  */
  Targets *jumps_;

  OperInstr(std::string assem, temp::TempList *dst, temp::TempList *src,
            Targets *jumps)
      : assem_(std::move(assem)), dst_(dst), src_(src), jumps_(jumps) {}

  void Print(FILE *out, temp::Map *m) const override;
  [[nodiscard]] temp::TempList *Def() const override;
  [[nodiscard]] temp::TempList *Use() const override;
};

/**
 * An LabelInstr is a point in a program to which jumps may go
An assem_ component showing how the label will look in the assembly-language program
(eg. In x86, a “:” and a “\n” follows the label.)
A label_ component identifying which label-symbol was used
 */
class LabelInstr : public Instr {
public:
  std::string assem_;
  temp::Label *label_;
  LabelInstr(std::string assem)
      : assem_(assem), label_(temp::LabelFactory::NamedLabel(assem)) {}

  void Print(FILE *out, temp::Map *m) const override;
  [[nodiscard]] temp::TempList *Def() const override;
  [[nodiscard]] temp::TempList *Use() const override;
};

/**
 * @brief  An MoveInstr is like OperInstr, but must perform only data transfer
if dst_ and src_ temporaries are assigned to the same register, the MOVE can later be deleted

 * @note   
 * @retval None
 */
class MoveInstr : public Instr {
public:
  std::string assem_;
  temp::TempList *dst_, *src_;

  MoveInstr(std::string assem, temp::TempList *dst, temp::TempList *src)
      : assem_(std::move(assem)), dst_(dst), src_(src) {}

  void Print(FILE *out, temp::Map *m) const override;
  [[nodiscard]] temp::TempList *Def() const override;
  [[nodiscard]] temp::TempList *Use() const override;
};

class InstrList {
public:
  InstrList() = default;

  void Print(FILE *out, temp::Map *m) const;
  void Append(assem::Instr *instr) { instr_list_.push_back(instr); }
  void Remove(assem::Instr *instr) { instr_list_.remove(instr); }
  void Insert(std::list<Instr *>::const_iterator pos, assem::Instr *instr) {
    instr_list_.insert(pos, instr);
  }
  [[nodiscard]] const std::list<Instr *> &GetList() const {
    return instr_list_;
  }

private:
  std::list<Instr *> instr_list_;
};

class Proc {
public:
  std::string prolog_;
  InstrList *body_;
  std::string epilog_;

  Proc(std::string prolog, InstrList *body, std::string epilog)
      : prolog_(std::move(prolog)), body_(body), epilog_(std::move(epilog)) {}
};

} // namespace assem

#endif