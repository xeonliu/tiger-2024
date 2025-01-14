#include "tiger/codegen/assem.h"

#include <cassert>
#include <iostream>
#include <map>
namespace temp {

Temp *TempList::NthTemp(int i) const {
  for (auto it : temp_list_)
    if (i-- == 0)
      return it;
  assert(0);
}
} // namespace temp
static std::map<std::string, std::string> reg_map = {
    {"%rax", "%al"},   {"%rbx", "%bl"},   {"%rcx", "%cl"},   {"%rdx", "%dl"},
    {"%rsi", "%sil"},  {"%rdi", "%dil"},  {"%rbp", "%bpl"},  {"%rsp", "%spl"},
    {"%r8", "%r8b"},   {"%r9", "%r9b"},   {"%r10", "%r10b"}, {"%r11", "%r11b"},
    {"%r12", "%r12b"}, {"%r13", "%r13b"}, {"%r14", "%r14b"}, {"%r15", "%r15b"}};

namespace assem {
/**
 * First param is string created by this function by reading 'assem' string
 * and replacing `d `s and `j stuff.
 * Last param is function to use to determine what to do with each temp.
 这段代码用于格式化汇编指令字符串？？通过读取特殊标记`d` `s` `j`将他们替换为相应的寄存器名称或标签字符串。
 比如说输入assem为 "mov `d0, `s0"
 dst 中 有 temp1
 src 中 有 temp2
 map 中 含有临时变量 temp1 到 "%eax" 字符串的映射
 可以转换为"mov %eax, %ebx"
 Lab5-part2 似乎还不需要这样分配？
 * Lab5-Part 2 中，这里是转换成 t131 t130 之类的临时变量。
 * @param assem assembly string
 * @param dst dst_ temp
 * @param src src temp
 * @param jumps jump labels_
 * @param m temp map: 临时变量到寄存器字符串的映射
 * @return formatted assembly string
 */
static std::string Format(std::string_view assem, temp::TempList *dst,
                          temp::TempList *src, Targets *jumps, temp::Map *m) {
  std::string result;
  for (std::string::size_type i = 0; i < assem.size(); i++) {
    char ch = assem.at(i);
    if (ch == '`') {
      i++;
      switch (assem.at(i)) {
      // TODO: s代表源寄存器？ 
      case 's': {
        i++;
        int n = assem.at(i) - '0';
        std::string *s = m->Look(src->NthTemp(n));
        result += *s;
      } break;
      // d 代表目标寄存器？
      case 'd': {
        i++;
        int n = assem.at(i) - '0';
        std::string *s = m->Look(dst->NthTemp(n));
        if (assem.find("set") != std::string::npos && (*s)[1] == 'r') {
          s = new std::string(reg_map[*s]);
        }
        result += *s;
      } break;
      // j 代表跳转目标？？
      case 'j': {
        i++;
        assert(jumps);
        std::string::size_type n = assem.at(i) - '0';
        std::string s = temp::LabelFactory::LabelString(jumps->labels_->at(n));
        result += s;
      } break;
      case '`': {
        result += '`';
      } break;
      default:
        assert(0);
      }
    } else {
      result += ch;
    }
  }
  return result;
}

void OperInstr::Print(FILE *out, temp::Map *m) const {
  std::string result = Format(assem_, dst_, src_, jumps_, m);
  fprintf(out, "%s\n", result.data());
}

void LabelInstr::Print(FILE *out, temp::Map *m) const {
  std::string result = Format(assem_, nullptr, nullptr, nullptr, m);
  fprintf(out, "%s:\n", result.data());
}

void MoveInstr::Print(FILE *out, temp::Map *m) const {
  if (!dst_ && !src_) {
    std::size_t srcpos = assem_.find_first_of('%');
    if (srcpos != std::string::npos) {
      std::size_t dstpos = assem_.find_first_of('%', srcpos + 1);
      if (dstpos != std::string::npos) {
        if ((assem_[srcpos + 1] == assem_[dstpos + 1]) &&
            (assem_[srcpos + 2] == assem_[dstpos + 2]) &&
            (assem_[srcpos + 3] == assem_[dstpos + 3]))
          return;
      }
    }
  }
  std::string result = Format(assem_, dst_, src_, nullptr, m);
  fprintf(out, "%s\n", result.data());
}

void InstrList::Print(FILE *out, temp::Map *m) const {
  for (auto instr : instr_list_)
    instr->Print(out, m);
  fprintf(out, "\n");
}

} // namespace assem