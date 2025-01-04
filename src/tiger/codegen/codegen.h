#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/canon/canon.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"
#include <cstdio>
#include <llvm-14/llvm/IR/Value.h>

// Forward Declarations
namespace frame {
class RegManager;
class Frame;
} // namespace frame

namespace assem {
class Instr;
class InstrList;
} // namespace assem

namespace canon {
class Traces;
} // namespace canon

namespace cg {
/**!SECTION
管理汇编指令列表
提供方法将汇编指令列表打印到文件中
 */
class AssemInstr {
public:
  AssemInstr() = delete;
  explicit AssemInstr(assem::InstrList *instr_list) : instr_list_(instr_list) {}

  void Print(FILE *out, temp::Map *map) const;

  [[nodiscard]] assem::InstrList *GetInstrList() const { return instr_list_; }

private:
  assem::InstrList *instr_list_;
};
/*!SECTION
将LLVM IR 转换为汇编指令
管理LLVM IR 值到临时变量的映射
记录基本快到索引的映射
生成会变指令列表？
*/
class CodeGen {
public:
  CodeGen(std::unique_ptr<canon::Traces> traces)
      : traces_(std::move(traces)), phi_temp_(temp::TempFactory::NewTemp()) {}

  void Codegen();

  // check if the value is %sp in llvm
  bool IsRsp(llvm::Value *val, std::string_view function_name) const {
    // TODO: your lab5 code here

    // Return true if val name == function_name_sp
    if (val->getName().str() == std::string(function_name) + "_sp") {
      return true;
    }

    return false;
  }

  // bb is to add move instruction to record which block it jumps from
  // function_name can be used to construct return or exit label
  void InstrSel(assem::InstrList *instr_list, llvm::Instruction &inst,
                std::string_view function_name, llvm::BasicBlock *bb);

  std::unique_ptr<AssemInstr> TransferAssemInstr() {
    return std::move(assem_instr_);
  }

private:
  // 记录LLVM IR值到临时变量的映射
  // record mapping from llvm value to temp
  std::unordered_map<llvm::Value *, temp::Temp *> *temp_map_;
  // for phi node, record mapping from llvm basic block to index of the block,
  // to check which block it jumps from
  std::unordered_map<llvm::BasicBlock *, int> *bb_map_;

  // Before every jump, move bb index into %rax
  // for phi node, use a temp to record which block it jumps from
  temp::Temp *phi_temp_;
  // 储存函数的基本块和相关信息
  std::unique_ptr<canon::Traces> traces_;
  // 储存生成的汇编指令
  std::unique_ptr<AssemInstr> assem_instr_;

  // 太愚蠢了，我不要为每个指令都写一对相同的的东西
  /**
   * @brief  Convert llvm::Value* params to $inst or `s0, `s1, ...
   * @note
   * @param  sources: A List of llvm::Value* sources.
   * @retval
   */
  static std::vector<std::string>
  convertToStrings(std::vector<llvm::Value *> sources) {
    std::vector<std::string> result;
    for (int i = 0; i < sources.size(); ++i) {
      if (auto const_int = llvm::dyn_cast<llvm::ConstantInt>(sources[i])) {
        result.push_back("$" + std::to_string(const_int->getSExtValue()));
      } else {
        result.push_back("`s" + std::to_string(i));
      }
    }
    return result;
  }

  temp::TempList *convertToSrcTempList(std::vector<llvm::Value *> srcs) {
    temp::TempList *src_temp_list = new temp::TempList();
    for (auto src : srcs) {
      auto it = temp_map_->find(src);
      if (it != temp_map_->end()) {
        src_temp_list->Append(it->second);
      } else {
        src_temp_list->Append(nullptr);
      }
    }
    return src_temp_list;
  }
};

} // namespace cg
#endif