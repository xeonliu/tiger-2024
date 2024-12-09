#include "tiger/canon/canon.h"
#include "tiger/frame/temp.h"

namespace canon {

canon::BasicBlockList *Canon::BasicBlocks() {
  auto *basicblock_list = new BasicBlockList();
  for (auto &&basicblock : body_->getBasicBlockList()) {
    // basicblock.setName(temp::LabelFactory::NewLabel()->Name());
    basicblock.setName(body_->getName() + "_" + basicblock.getName());
    basicblock_list->Append(&basicblock);
  }
  bbs_ = basicblock_list;
  traces_ = std::make_unique<Traces>(body_, bbs_, body_->getName());
  return basicblock_list;
}

Traces::~Traces() {}
} // namespace canon