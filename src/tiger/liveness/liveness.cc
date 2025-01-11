#include "tiger/liveness/liveness.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include <cassert>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

/*
 * 用于生成每个流图节点的活跃信息
 * */
void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */

  // 初始化in_和out_表
  for (auto n : flowgraph_->Nodes()->GetList()) {
    in_->Enter(n, new temp::TempList());
    out_->Enter(n, new temp::TempList());
  }

  // 每轮要记录一个列表的是否变化
  // 每个列表对应一个节点
  // 如果某个节点的in和out没有变化，说明已经收敛
  // 每次清空
  bool change = true;
  while (change) {
    // Assume no change
    change = false;
    // Repeat for each n until no change
    for (auto n : flowgraph_->Nodes()->GetList()) {

      auto in = in_->Look(n);
      auto out = out_->Look(n);

      // in[n] = use[n] U (out[n] - def[n])
      auto use = n->NodeInfo()->Use();
      auto def = n->NodeInfo()->Def();
      auto new_in = use->Union(out->Diff(def));

      // out[n] = U (in[s]) s is a succ of n
      auto succs = n->Succ();
      auto new_out = new temp::TempList();
      for (auto s : succs->GetList()) {
        new_out = new_out->Union(in_->Look(s));
      }

      // Check if there is any change
      if (!new_in->Equal(in) || !new_out->Equal(out)) {
        change = true;
        in_->Set(n, new_in);
        out_->Set(n, new_out);
      }
    }
  }

  // No Change. Finish
}

/**
 * @brief
 * 用于构建干涉图。干涉图是一个无向图，其中每个节点对应一个临时变量，如果两个节点之间有边，则表示这两个临时变量之间有干涉。
 * @note
 * @retval None
 */
// FIXME： 存入机器寄存器
// 单独加入RSP
void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  // Takes a flow graph to produce a live::LiveGraph
  // An interference graph
  // Using liveness information at the exit of each node
  // A list of node-pairs representing MOVE instructions

  // If a variable is defined but never used, we still need to generate
  // interference edges from this variable because this instruction needs to be
  // executed Unless we can remove it via DCE

  // NOTE: 冲突图是一个由temp::Temp*组成的Graph
  // NOTE: 使用temp_node_map_记录每个临时变量对应的节点

  // TODO: 添加机器寄存器
  std::list<temp::Temp *> temps = reg_manager->Registers()->GetList();
  // TODO: 添加RSP（RSP不在List中）
  temps.emplace_back(reg_manager->FramePointer());

  // 任意两个机器寄存器之间都有冲突边
  for (temp::Temp *temp1 : temps) {
    for (temp::Temp *temp2 : temps) {
      if (temp1 != temp2) {
        auto temp_node1 = temp_node_map_->Look(temp1);
        auto temp_node2 = temp_node_map_->Look(temp2);
        live_graph_.interf_graph->AddEdge(temp_node1, temp_node2);
      }
    }
  }

  // 添加所有的临时变量
  for (auto n : flowgraph_->Nodes()->GetList()) {
    // 加入所有的Def和Use
    // FIXME: NullPtr
    assem::Instr *instr = n->NodeInfo();
    for (temp::Temp *temp : instr->Use()->GetList()) {
      if (!temp_node_map_->Look(temp)) {
        auto temp_node = live_graph_.interf_graph->NewNode(temp);
        temp_node_map_->Enter(temp, temp_node);
      }
    }
    for (temp::Temp *temp : instr->Def()->GetList()) {
      if (!temp_node_map_->Look(temp)) {
        auto temp_node = live_graph_.interf_graph->NewNode(temp);
        temp_node_map_->Enter(temp, temp_node);
      }
    }
  }

  // 添加干涉边
  // 对于流图中的每一个节点n，如果该节点有新定值的临时变量d属于def[n]，
  // 并且有属于liveMap的临时变量{t1, t2, ...}，
  // 则为d和{t1, t2, ...}添加干涉边
  for (auto n : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = n->NodeInfo();
    temp::TempList *out = out_->Look(n);

    // 对于任何对变量 a define 的非传送指令，
    // 以及在该指令处是出口活跃的变量b1, ..., bj,
    // 添加冲突边(a,b1), ..., (a, bj).

    for (temp::Temp *def : instr->Def()->GetList()) {
      INodePtr def_node = temp_node_map_->Look(def);
      if (typeid(*instr) == typeid(assem::MoveInstr)) {
        for (temp::Temp *b : out->GetList()) {
          // 对于不同于Use的变量，添加冲突边
          INodePtr b_node = temp_node_map_->Look(b);
          if (!instr->Use()->Contain(b)) {
            // 添加双向边
            live_graph_.interf_graph->AddEdge(def_node, b_node);
            live_graph_.interf_graph->AddEdge(b_node, def_node);
          }
        }
        // 添加Use和Def之间的传送边
        for (temp::Temp *use : instr->Use()->GetList()) {
          INodePtr use_node = temp_node_map_->Look(use);
          live_graph_.moves->Union(use_node, def_node);
        }
      } else {
        for (temp::Temp *b : out->GetList()) {
          INodePtr b_node = temp_node_map_->Look(b);
          live_graph_.interf_graph->AddEdge(def_node, b_node);
          live_graph_.interf_graph->AddEdge(b_node, def_node);
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live