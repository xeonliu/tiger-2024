/**
  这些类和函数主要用于管理编译过程中生成的临时变量（Temp）和标签（Label）。
  这些临时变量和标签在编译器的中间表示（IR）中起着重要作用，用于标识和管理中间代码中的变量和跳转目标。
 */

#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"

#include <list>

namespace temp {

using Label = sym::Symbol;

class LabelFactory {
public:
  static Label *NewLabel();
  static Label *NamedLabel(std::string_view name);
  static std::string LabelString(Label *s);

private:
  int label_id_ = 0;
  static LabelFactory label_factory;
};

class Temp {
  friend class TempFactory;

public:
  [[nodiscard]] int Int() const;

private:
  int num_;
  explicit Temp(int num) : num_(num) {}
};

/**
  用于生成和管理临时变量
 */
class TempFactory {
public:
  // 获取新的Temp对象，静态的temp_factory成员其temp_id_自增
  static Temp *NewTemp();

private:
  int temp_id_ = 100;
  static TempFactory temp_factory;
};

/**
  用于管理临时变量到字符串的映射
 */
class Map {
public:
  void Enter(Temp *t, std::string *s);
  std::string *Look(Temp *t);
  void DumpMap(FILE *out);

  // 返回一个空的映射
  static Map *Empty();
  // 返回一个全局的映射实例
  static Map *Name();
  // 创建一个新的映射层
  static Map *LayerMap(Map *over, Map *under);

private:
  tab::Table<Temp, std::string> *tab_;
  Map *under_;

  Map() : tab_(new tab::Table<Temp, std::string>()), under_(nullptr) {}
  Map(tab::Table<Temp, std::string> *tab, Map *under)
      : tab_(tab), under_(under) {}
};

class TempList {
public:
  explicit TempList(Temp *t) : temp_list_({t}) {}
  TempList(std::initializer_list<Temp *> list) : temp_list_(list) {}
  TempList() = default;

  bool Contain(Temp *a) const;
  bool Equal(TempList *tl) const;
  void Replace(Temp *old_temp, Temp *new_temp);
  void Delete(Temp *t);
  // 接到末尾
  void CatList(temp::TempList *tl);
  void Append(Temp *t) { temp_list_.push_back(t); }
  void Prepend(Temp *t) { temp_list_.push_front(t); }
  // 并集
  TempList *Union(temp::TempList *tl) const;
  // 差集
  TempList *Diff(temp::TempList *tl) const;
  [[nodiscard]] Temp *NthTemp(int i) const;
  [[nodiscard]] const std::list<Temp *> &GetList() const { return temp_list_; }

private:
  std::list<Temp *> temp_list_;
};

} // namespace temp

#endif