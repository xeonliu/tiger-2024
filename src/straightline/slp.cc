#include "straightline/slp.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int x1 = stm1->MaxArgs();
  int x2 = stm2->MaxArgs();
  return x1 > x2 ? x1 : x2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  t = stm1->Interp(t);
  t = stm2->Interp(t);
  return t;
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  if (t == nullptr) {
  }
  auto result = exp->Interp(t);
  t = result.t;
  if (t == nullptr) {
    t = new Table(id, result.i, nullptr);
  } else {
    t = t->Update(id, result.i);
  }
  return t;
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  // Calculate the size of its ExpList
  // Compare it with those inside it.
  return std::max(exps->NumExps(), exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  // Evaluate Each Expression in the ExpList.
  ExpList *exp_node = exps;
  while (exp_node != nullptr) {
    auto result = exp_node->Interp(t);
    std::cout << result.i << ' ';
    t = result.t;
    exp_node = exp_node->next();
  }
  std::cout << std::endl;
  return t;
}

int A::IdExp::MaxArgs() const { return 0; }

IntAndTable A::IdExp::Interp(Table *t) const {
  return IntAndTable(t->Lookup(id), t);
}

int A::NumExp::MaxArgs() const { return 0; }

IntAndTable A::NumExp::Interp(Table *t) const { return IntAndTable(num, t); }

int A::OpExp::MaxArgs() const {
  return std::max(left->MaxArgs(), right->MaxArgs());
}

IntAndTable A::OpExp::Interp(Table *t) const {
  // Interp Left
  auto left_result = left->Interp(t);
  t = left_result.t;
  // Interp Right
  auto right_result = right->Interp(t);
  t = right_result.t;

  switch (oper) {
  case PLUS:
    return IntAndTable(left_result.i + right_result.i, t);
  case MINUS:
    return IntAndTable(left_result.i - right_result.i, t);
  case TIMES:
    return IntAndTable(left_result.i * right_result.i, t);
  case DIV:
    return IntAndTable(left_result.i / right_result.i, t);
  }
  assert(false);
}

int A::EseqExp::MaxArgs() const {
  return std::max(stm->MaxArgs(), exp->MaxArgs());
}

IntAndTable A::EseqExp::Interp(Table *t) const {
  t = stm->Interp(t);
  return exp->Interp(t);
}

int A::PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}

int A::PairExpList::NumExps() const { return 1 + tail->NumExps(); }

/* Only Interpret this node */
IntAndTable A::PairExpList::Interp(Table *t) const { return exp->Interp(t); }

ExpList *A::PairExpList::next() const { return tail; }

int A::LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int A::LastExpList::NumExps() const { return 1; }

IntAndTable A::LastExpList::Interp(Table *t) const { return exp->Interp(t); }

ExpList *A::LastExpList::next() const { return nullptr; }

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
