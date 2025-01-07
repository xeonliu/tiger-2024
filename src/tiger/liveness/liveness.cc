#include "tiger/liveness/liveness.h"
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

  bool change = true;

  while (change) {
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

      if (in->Equal(new_in) && out->Equal(new_out)) {
        change = false;
        continue;
      } else {
        change = true;
      }

      in_->Enter(n, new_in);
      out_->Enter(n, new_out);
    }
    // Check if there is any change
  }

  // No Change. Finish
}

/**
 * @brief
 * 用于构建干涉图。干涉图是一个无向图，其中每个节点对应一个临时变量，如果两个节点之间有边，则表示这两个临时变量之间有干涉。
 * @note
 * @retval None
 */
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

  // 首先添加所有的临时变量
  for (auto n : flowgraph_->Nodes()->GetList()) {
    auto def = n->NodeInfo()->Def();
    for (auto d : def->GetList()) {
      auto temp_node = live_graph_.interf_graph->NewNode(d);
      temp_node_map_->Enter(d, temp_node);
    }
  }

  // 添加干涉边
  // 对于流图中的每一个节点n，如果该节点有新定值的临时变量d属于def[n]，
  // 并且有属于liveMap的临时变量{t1, t2, ...}，
  // 则为d和{t1, t2, ...}添加干涉边
  for (auto n : flowgraph_->Nodes()->GetList()) {
    bool is_move_inst = false;

    // 检查指令是否是移动指令
    // Try to cast to MoveInstr
    if (auto move_inst = dynamic_cast<assem::MoveInstr *>(n->NodeInfo())) {
      is_move_inst = true;

      // 确定src和dst
      temp::Temp *src = nullptr;
      temp::Temp *dst = nullptr;

      // Check Null Ptr
      if (move_inst->src_) {
        src = move_inst->src_->NthTemp(0);
      }

      // 显然目标寄存器必须存在
      assert(move_inst->dst_);
      dst = move_inst->dst_->NthTemp(0);
      auto dst_node = temp_node_map_->Look(dst);

      // move的目标应当与def相同
      auto def_list = n->NodeInfo()->Def()->GetList();
      assert(def_list.size() == 1);
      assert(def_list.front() == dst);

      // 对于传送指令a<-c，
      // 如果变量b1,...,bj在该指令处是出口活跃的，
      // 则对每一个不同于c的bi添加冲突边
      auto out = out_->Look(n);
      for (auto t : out->GetList()) {
        // 忽略src临时变量
        if (t == src) {
          continue;
        }

        auto t_node = temp_node_map_->Look(t);

        if (dst != t) {
          /* 添加冲突边 */
          live_graph_.interf_graph->AddEdge(dst_node, t_node);
        }
      }

      /* 添加移动边 */
      // TODO: If and only if src and dst both exists?
      if (src) {
        auto src_node = temp_node_map_->Look(src);
        live_graph_.moves->Append(src_node, dst_node);
      }

    } else {
      // 对于任何对变量 a define 的非传送指令，
      // 以及在该指令处是出口活跃的变量b1, ..., bj,
      // 添加冲突边(a,b1), ..., (a, bj).
      auto out = out_->Look(n);
      auto def = n->NodeInfo()->Def();
      auto def_list = def->GetList();
      for (auto d : def_list) {
        auto d_node = temp_node_map_->Look(d);
        for (auto t : out->GetList()) {
          // 出口活跃的变量
          auto t_node = temp_node_map_->Look(t);
          if (d != t) {
            live_graph_.interf_graph->AddEdge(d_node, t_node);
          }
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