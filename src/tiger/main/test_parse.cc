#include <cstdio>
#include <fstream>

#include "tiger/absyn/absyn.h"
#include "tiger/parse/parser.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

// define here to parse compilation
llvm::IRBuilder<> *ir_builder;
llvm::Module *ir_module;
frame::RegManager *reg_manager;
frame::Frags frags;

int main(int argc, char **argv) {
  std::unique_ptr<absyn::AbsynTree> absyn_tree;

  if (argc < 2) {
    fprintf(stderr, "usage: a.out filename\n");
    exit(1);
  }

  Parser parser(argv[1], std::cerr);
  parser.parse();
  absyn_tree = parser.TransferAbsynTree();
  absyn_tree->Print(stderr);
  fprintf(stderr, "\n");
  return 0;
}
