#include "tiger/regalloc/regalloc.h"

#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/liveness/liveness.h"
#include "tiger/output/logger.h"
#include "tiger/regalloc/color.h"
#include <iostream>
#include <memory>

extern frame::RegManager *reg_manager;
// 可以用于获取Function Frame信息
extern std::map<std::string, std::pair<int, int>> frame_info_map;

namespace ra {
/* TODO: Put your lab6 code here */
RegAllocator::RegAllocator(const std::string &proc_name,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : proc_name_(proc_name), assem_instr_(std::move(assem_instr)) {
  precolored_ = new live::INodeList();
  initial_ = new live::INodeList();
  simplify_worklist_ = new live::INodeList();
  freeze_worklist_ = new live::INodeList();
  spill_worklist_ = new live::INodeList();
  spilled_nodes_ = new live::INodeList();
  coalesced_nodes_ = new live::INodeList();
  colored_nodes_ = new live::INodeList();
  select_stack_ = new live::INodeList();
  worklist_moves_ = new live::MoveList();
  active_moves_ = new live::MoveList();
  move_list_ = std::make_unique<tab::Table<live::INode, live::MoveList>>();
  degree_ = std::make_unique<tab::Table<live::INode, int>>();
  interference_graph_ = nullptr;
  moves_ = nullptr;
  result_ = nullptr;
}

void RegAllocator::RegAlloc() {
  // procedure Main()
  // LivenessAnalysis()
  // Build()

  // MakeWorklist()
  // repeat
  //     if simplifyWorklist != {} then Simplify()
  //     else if worklistMoves != {} then Coalesce()
  //     else if freezeWorklist != {} then Freeze()
  //     else if spillWorklist != {} then SelectSpill()
  // until simplifyWorklist = {} && worklistMove={} &&
  //          freezeWorklist={} && spillWorklist={}
  // AssignColor()
  // if spilledNodes != {} then
  //     RewriteProgram(spilledNodes)
  //     Main()

  // Build a FlowGraph

  fg::FlowGraphFactory flowgraph_factory(assem_instr_->GetInstrList());
  flowgraph_factory.AssemFlowGraph();
  fg::FGraphPtr flowgraph = flowgraph_factory.GetFlowGraph();

  // Liveness Analysis
  live::LiveGraphFactory live_graph_factory(flowgraph);
  live_graph_factory.Liveness();
  live::LiveGraph live_graph = live_graph_factory.GetLiveGraph();

  // Build Interference Graph
  // 从Live Graph中初始化
  interference_graph_ = live_graph.interf_graph;
  moves_ = live_graph.moves;

  // 获取temp_node_map
  temp_node_map_ = live_graph_factory.GetTempNodeMap();

  Build();
  MakeWorklist();
  while (!simplify_worklist_->GetList().empty() ||
         !worklist_moves_->GetList().empty() ||
         !freeze_worklist_->GetList().empty() ||
         !spill_worklist_->GetList().empty()) {
    if (!simplify_worklist_->GetList().empty()) {
      Simplify();
    } else if (!worklist_moves_->GetList().empty()) {
      Coalesce();
    } else if (!freeze_worklist_->GetList().empty()) {
      Freeze();
    } else if (!spill_worklist_->GetList().empty()) {
      SelectSpill();
    }
  }
  AssignColors();

  if (!spilled_nodes_->GetList().empty()) {
    RewriteProgram();
    RegAlloc();
  } else {
    // Create a temp::Map coloring map
    auto coloring = temp::Map::Empty();
    for (auto node : colored_nodes_->GetList()) {
      // TODO: Coloring 是个什么东西？
      // 临时变量到寄存器字符串的映射，如temp1 到 "%eax" 字符串的映射

      // 查询分配到的机器寄存器
      auto machine_reg = reg_manager->GetRegister(color_.at(node));
      auto name = reg_manager->temp_map_->Look(machine_reg);
      coloring->Enter(node->NodeInfo(), name);
    }
    // Prepare the instruction list
    result_ =
        std::make_unique<Result>(coloring, this->assem_instr_->GetInstrList());
  }

  // FIXME
  // 需要初始化的变量
  // active_moves_: 是虚线边，就是初始的集合
  // move_list_：是实线边，是node和move的映射
  // 把机器寄存器的Color上色
  // precolored_
  // 机器寄存器的Temp号是内定的，从0x100开始
  // initial_
  // degree_
  // adj_list
  // adj_set
}

/**!SECTION
 * Build Interference Graph
 *
 * moveList
 * workListMoves
 * AddEdge
 */
void RegAllocator::Build() {
  // Use Information Obtained From Liveness Analysis to initialize the data
  // structures

  // RegManager contains all temporaries for registers
  temp::Map *temp_map = reg_manager->temp_map_;

  for (live::INodePtr node : interference_graph_->Nodes()->GetList()) {
    // Initialize the degree of each node
    degree_->Enter(node, new int(node->OutDegree()));

    // Initiallize the moveList
    live::MoveList *node_move_list = new live::MoveList();
    for (auto move : worklist_moves_->GetList()) {
      // FIXME: 有可能合并的传送指令集合？
      auto src = move.first;
      auto dst = move.second;
      if (src == node || src == node) {
        node_move_list->Append(src, dst);
      }
    }
    move_list_->Enter(node, node_move_list);

    // Initialize precolored & initial
    if (temp_map->Look(node->NodeInfo())) {
      precolored_->Append(node);
    } else {
      initial_->Append(node);
    }
  }

  // Initialize Machine reg node colors
  for (auto reg : reg_manager->Registers()->GetList()) {
    // 找到机器寄存器对应的Node;
    // 获取机器寄存器的编号;
    auto node = temp_node_map_->Look(reg);
    color_[node] = reg->Int();
  }
}

void RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (!u->Adj(v) && u != v) {
    if (!precolored_->Contain(u)) {
      interference_graph_->AddEdge(u, v);
      ++(*degree_->Look(u));
    }
    if (!precolored_->Contain(v)) {
      interference_graph_->AddEdge(v, u);
      ++(*degree_->Look(v));
    }
  }
}

/**
 * @brief  将节点从initial中移动到各个Worklist中
 * @note
 * @retval None
 */
void RegAllocator::MakeWorklist() {
  // 初始化工作列表，将干涉图中的节点分类到不同的工作列表中，根据节点的度数（Degree，表示与其他节点的连接数量）以及其他特性分配到以下几类工作列表：
  // simplifyWorklist：低度数节点，可以直接简化。
  // worklistMoves：涉及变量移动的节点。
  // freezeWorklist：需要冻结的节点。
  // spillWorklist：高风险会导致溢出的节点。
  for (live::INodePtr node : interference_graph_->Nodes()->GetList()) {
    initial_->DeleteNode(node);
    if (node->Degree() >= K_) {
      spill_worklist_->Append(node);
    } else if (MoveRelated(node)) {
      freeze_worklist_->Append(node);
    } else {
      simplify_worklist_->Append(node);
    }
  }
}

live::INodeListPtr RegAllocator::Adjacent(live::INodePtr node) {
  live::INodeListPtr adj_list = node->Succ();
  return adj_list->Diff(select_stack_->Union(coalesced_nodes_));
}

live::MoveList *RegAllocator::NodeMoves(live::INodePtr node) {
  return move_list_->Look(node)->Intersect(
      active_moves_->Union(worklist_moves_));
}

bool RegAllocator::MoveRelated(live::INodePtr node) {
  return NodeMoves(node)->GetList().size() > 0;
}

/**
 * 将simplofyWorklist中的节点简化
 * 内容压入selectStack
 * 将相邻边节点度数降低
 */
void RegAllocator::Simplify() {
  if (simplify_worklist_->GetList().empty()) {
    return;
  }
  // 从simplifyWorklist中取出一个节点，将其压入selectStack中
  live::INodePtr node = simplify_worklist_->GetList().front();
  simplify_worklist_->DeleteNode(node);
  select_stack_->Prepend(node);

  // 将node的相邻节点的度数减1
  for (live::INodePtr adj_node : Adjacent(node)->GetList()) {
    DecrementDegree(adj_node);
  }
}

void RegAllocator::DecrementDegree(live::INodePtr m) {
  int *d = degree_->Look(m);
  // 降度数
  --(*d);
  if (*d == K_ - 1) {
    // 当邻节点的度数从K变到K-1时，
    // 他的领结点相关的传送指令将有可能变成可合并的
    // 将其从spillWorklist中移动到simplifyWorklist中
    live::INodeListPtr m_set = new live::INodeList();
    m_set->Append(m);
    EnableMoves(m_set->Union(Adjacent(m)));
    spill_worklist_->DeleteNode(m);
    if (MoveRelated(m)) {
      // 低度数传送有关节点表
      freeze_worklist_->Union(m_set);
    } else {
      // 低度数传送无关节点表
      simplify_worklist_->Union(m_set);
    }
  }
}

void RegAllocator::EnableMoves(live::INodeListPtr nodes) {
  for (live::INodePtr node : nodes->GetList()) {
    for (auto move : NodeMoves(node)->GetList()) {
      // 与node相关的传送指令
      // 全部变为做好准备的(加入worklistMoves)
      if (active_moves_->Contain(move.first, move.second)) {
        // 将activeMoves变为worklistMoves
        active_moves_->Delete(move.first, move.second);
        // TODO: Union?
        worklist_moves_->Append(move.first, move.second);
      }
    }
  }
}

void RegAllocator::Coalesce() {
  // 从worklistMoves中取出一个传送指令
  // 如果传送指令的源节点和目标节点都在同一个集合中
  // 则将传送指令合并
  // 否则，将传送指令加入冻结列表
  if (worklist_moves_->GetList().empty()) {
    return;
  }
  live::INodePtr x, y;
  std::tie(x, y) = worklist_moves_->GetList().front();

  live::INodePtr x_alias = GetAlias(x);
  live::INodePtr y_alias = GetAlias(y);

  live::INodePtr u = x_alias;
  live::INodePtr v = y_alias;

  if (precolored_->Contain(y_alias)) {
    std::swap(u, v);
  }

  // 确保v是机器寄存器当且仅当二者都是机器寄存器
  worklist_moves_->Delete(x, y);

  if (u == v) {
    // 合并传送指令
    coalesced_moves_->Union(x, y);
    AddWorkList(u);
    return;
  }

  if (precolored_->Contain(v) || u->Adj(v)) {
    // 如果二者冲突（即都是寄存器或者是相互冲突的临时变量）
    // 冻结传送指令
    constrained_moves_->Union(u, v);
    AddWorkList(u);
    AddWorkList(v);
    return;
  }

  if (precolored_->Contain(u)) {
    // 对于机器寄存器和临时变量的情况
    // 采取Georage方法
    // 对任意属于Adjacent(v)的节点t, Ok(t,u)
    auto adj_v = Adjacent(v);
    bool ok = true;
    for (live::INodePtr t : adj_v->GetList()) {
      if (!Ok(t, u)) {
        ok = false;
        break;
      }
    }
    // 进行合并
    if (ok) {
      coalesced_moves_->Union(x, y);
      Combine(u, v);
      AddWorkList(u);
      return;
    }
    // 不OK，放回没准备好的传送指令
  } else {
    // 对于两个临时变量的情况
    // 采取Briggs方法
    if (Conservative(Adjacent(u)->Union(Adjacent(v)))) {
      coalesced_moves_->Union(x, y);
      Combine(u, v);
      AddWorkList(u);
      return;
    }
  }

  active_moves_->Union(u, v);
}

void RegAllocator::AddWorkList(live::INodePtr u) {
  if (!precolored_->Contain(u) && !MoveRelated(u) && u->Degree() < K_) {
    freeze_worklist_->DeleteNode(u);
    simplify_worklist_->Union(u);
  }
}

bool RegAllocator::Ok(live::INodePtr t, live::INodePtr u) {
  return t->Degree() < K_ || precolored_->Contain(t) || t->Adj(u);
}

bool RegAllocator::Conservative(live::INodeListPtr nodes) {
  int k = 0;
  for (live::INodePtr node : nodes->GetList()) {
    if (node->Degree() >= K_) {
      ++k;
    }
  }
  return k < K_;
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr n) {
  if (coalesced_nodes_->Contain(n)) {
    return GetAlias(alias_->Look(n));
  }
  return n;
}

void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
  if (freeze_worklist_->Contain(v)) {
    freeze_worklist_->DeleteNode(v);
  } else {
    spill_worklist_->DeleteNode(v);
  }
  coalesced_nodes_->Union(v);
  alias_->Enter(v, u);
  move_list_->Set(u, move_list_->Look(u)->Union(move_list_->Look(v)));
  for (live::INodePtr t : Adjacent(v)->GetList()) {
    AddEdge(t, u);
    DecrementDegree(t);
  }
  if (u->Degree() >= K_ && freeze_worklist_->Contain(u)) {
    freeze_worklist_->DeleteNode(u);
    spill_worklist_->Union(u);
  }
}

/**
 * @brief 将相关的传送指令冻结，重新从free_worklist送入simplify_worklist
 * @note
 * @retval None
 */
void RegAllocator::Freeze() {
  if (freeze_worklist_->GetList().empty()) {
    return;
  }
  live::INodePtr u = freeze_worklist_->GetList().front();
  freeze_worklist_->DeleteNode(u);
  simplify_worklist_->Union(u);
  FreezeMoves(u);
}

void RegAllocator::FreezeMoves(live::INodePtr u) {
  for (auto move : NodeMoves(u)->GetList()) {
    live::INodePtr x = move.first;
    live::INodePtr y = move.second;
    live::INodePtr v;
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    active_moves_->Delete(x, y);
    frozen_moves_->Union(x, y);
    if (NodeMoves(v)->GetList().empty() && v->Degree() < K_) {
      freeze_worklist_->DeleteNode(v);
      simplify_worklist_->Append(v);
    }
  }
}

/**
 * @brief
 * 如果没有低度数的节点，选择一个潜在可能溢出的高度数节点并将他压入栈（进行简化）
 * @note
 * @retval None
 */
void RegAllocator::SelectSpill() {
  // 选择一个溢出风险最高的节点
  // 将其从spillWorklist中移动到simplifyWorklist中
  live::INodePtr m = spill_worklist_->GetList().front();
  spill_worklist_->DeleteNode(m);
  simplify_worklist_->Append(m);
  FreezeMoves(m);
}

void RegAllocator::AssignColors() {
  while (!select_stack_->GetList().empty()) {
    // n = pop(select_stack)
    live::INodePtr n = select_stack_->GetList().front();
    select_stack_->DeleteNode(n);

    std::set<int> ok_colors;
    for (int i = 0; i < K_; ++i) {
      ok_colors.insert(i);
    }

    for (live::INodePtr w : Adjacent(n)->GetList()) {
      live::INodePtr alias_w = GetAlias(w);
      if (colored_nodes_->Contain(alias_w) || precolored_->Contain(alias_w)) {
        ok_colors.erase(color_.at(alias_w));
      }
    }

    if (ok_colors.empty()) {
      spilled_nodes_->Union(n);
    } else {
      colored_nodes_->Union(n);
      color_.insert({n, *ok_colors.begin()});
    }

    for (auto n : coalesced_nodes_->GetList()) {
      color_.insert({n, color_.at(GetAlias(n))});
    }
  }
}

/**
 * @brief
 * @note
 * @retval None
 */
void RegAllocator::RewriteProgram() {
  // 关于Frame
  // frame_info_map: 函数名 -> {Offset，FrameSize}
  // Frame Size 需要修改？

  // 为了溢出的临时变量分配栈空间
  // FIXME: How do I get the function names?????
  int offset = frame_info_map[proc_name_].second;
  int local_frame_size = frame_info_map[proc_name_].second;
  frame_info_map[proc_name_].second += spilled_nodes_->GetList().size() * 16;

  live::INodeListPtr new_temps = new live::INodeList();
  // 为每一个v \in spilledNodes分配一个储存单元
  for (live::INodePtr v : spilled_nodes_->GetList()) {
    // 创建一个新的temp，把这条指令中的temp替换为新的temp
    temp::Temp *new_temp = temp::TempFactory::NewTemp();

    // 遍历所有指令
    auto instr_iter = assem_instr_->GetInstrList()->GetList().begin();
    while (instr_iter != assem_instr_->GetInstrList()->GetList().end()) {
      auto instr = *instr_iter;
      if (instr->Use() && instr->Use()->Contain(v->NodeInfo())) {
        // v的每一个使用之前插入一条取数指令
        instr->Use()->Replace(v->NodeInfo(), new_temp);
        auto fetchInstr = new assem::OperInstr(
            "movq (" + proc_name_ + "_frame_size_local-" +
                std::to_string(offset) + ")(%rsp), `d0",
            new temp::TempList({new_temp}),
            new temp::TempList{reg_manager->FramePointer()}, nullptr);
        assem_instr_->GetInstrList()->Insert(instr_iter, fetchInstr);
      }
      if (instr->Def() && instr->Def()->Contain(v->NodeInfo())) {
        // v的每一个定值之后插入一条储存指令
        instr->Def()->Replace(v->NodeInfo(), new_temp);
        auto storeInstr = new assem::OperInstr(
            "movq `s0, (" + proc_name_ + "_frame_size_local-" +
                std::to_string(offset) + ")(%rsp)",
            nullptr, new temp::TempList({reg_manager->FramePointer()}),
            nullptr);
        assem_instr_->GetInstrList()->Insert(++instr_iter, storeInstr);
      }
      ++instr_iter;
    }
    // FIXME: WTF? What if in different frames?
    offset += 8;

    // 将所有的v_i放入newTemps中
    live::INodePtr new_node = interference_graph_->NewNode(new_temp);
    new_temps->Append(new_node);
  }

  // spilledNoeds <- {}
  // initial <- coloredNodes U coalescedNodes U newTemps
  // coloredNodes <- {}
  // coalescedNodes <- {}
  spilled_nodes_->Clear();
  initial_ = colored_nodes_->Union(coalesced_nodes_->Union(new_temps));
  colored_nodes_->Clear();
  coalesced_nodes_->Clear();
}

Result::~Result() {}
} // namespace ra