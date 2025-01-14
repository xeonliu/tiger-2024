#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"
#include <memory>

namespace ra {

/**
 * Register Allocation
 * Takes a flow graph to produce a live::LiveGraph
 * An interference graph
 * Using liveness information at the exit of each node
 * A list of node-pairs representing MOVE instructions
 */

/** Two ways to represent edges
Adjacent list
Only for normal temporaries
Because the adjacent list for machine registers may be too large
Machine registers cannot be selected
We only coalesce temporary nodes to machine registers using George method
Bitmap
Both for temporaries and machine registers
Move-related node
associated with a count number of moves
easy to maintain and test if it is no longer move-related
* All nodes
Associated with a count of the number of neighbors currently in the graph
This number is used to determine whether
a node is of significant degree during coalescing
a node can be removed from the graph during simplification
 */

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result();
};

/**
 * @brief  Build
Simplify
Remove non-move-related nodes of low-degree
Coalesce
Resulting node may become non-move-related node
Freeze
Freeze the moves node of low-degree
Potential Spill
Select
If failed, rewrite code to implement actual spill and rebuild the RIG

 * @note
 * @retval None
 */
class RegAllocator {
  /* TODO: Put your lab6 code here */
  // LINK - src/tiger/output/output.cc
  //  ra::RegAllocator reg_allocator(body_->getName().str(),
  //                                 std::move(assem_instr));
  //  reg_allocator.RegAlloc();
  //  allocation = reg_allocator.TransferResult();
  //  il = allocation->il_;
  //  color = temp::Map::LayerMap(reg_manager->temp_map_,
  //  allocation->coloring_);
public:
  /**
   * @brief
   * @note
   * @param  &proc_name:
   * @param  assem_instr: Lab5 生成的汇编指令
   * @retval
   */
  RegAllocator(const std::string &proc_name,
               std::unique_ptr<cg::AssemInstr> assem_instr);

  void RegAlloc();

  std::unique_ptr<Result> TransferResult() { return std::move(result_); };

  std::unique_ptr<Result> result_;

private:
  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  void MakeWorklist();
  live::INodeListPtr Adjacent(live::INodePtr node);
  live::MoveList *NodeMoves(live::INodePtr node);
  bool MoveRelated(live::INodePtr node);
  void Simplify();
  void DecrementDegree(live::INodePtr m);
  void EnableMoves(live::INodeListPtr nodes);
  void Coalesce();
  void AddWorkList(live::INodePtr u);
  bool Ok(live::INodePtr t, live::INodePtr u);
  bool Conservative(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr n);
  void Combine(live::INodePtr u, live::INodePtr v);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  void SelectSpill();
  void AssignColors();
  void RewriteProgram();

  std::string proc_name_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  int K_;

  // Obtained From Liveness Analysis
  // FIXME: Adjacent list: Only for normal temporaries??
  live::IGraphPtr interference_graph_;
  live::MoveList *moves_;
  // Used in Build.
  tab::Table<temp::Temp, live::INode> *temp_node_map_;

  // 机器寄存器集合
  live::INodeListPtr precolored_;

  // P41
  // initial: Temporary registers, not precolored and not yet processed
  // 临时寄存器集合
  live::INodeListPtr initial_;

  /* Data Structures for Codegen */
  // P39: A mapping from a node to the list of moves it is associated with
  std::unique_ptr<tab::Table<live::INode, live::MoveList>> move_list_;

  // FIXME: P40
  // adjSet: The bit-matrix of interference graph
  // adjList: The adjacent nodes list of interference graph
  /**
   * @brief
        All nodes
        Associated with a count of the number of neighbors currently in the
   graph This number is used to determine whether a node is of significant
   degree during coalescing a node can be removed from the graph during
   simplification
   */
  // An array containing the current degree of each node
  std::unique_ptr<tab::Table<live::INode, int>> degree_;

  // alias.并查集
  std::unique_ptr<tab::Table<live::INode, live::INode>> alias_;

  // color.颜色表
  // 从中获取染色结果
  std::map<live::INodePtr, int> color_;

  // P37
  // low-degree non-move-related nodes
  live::INodeListPtr simplify_worklist_;
  // low-degree move-related nodes
  live::INodeListPtr freeze_worklist_;
  // high-degree nodes
  live::INodeListPtr spill_worklist_;

  // nodes marked for spilling
  live::INodeListPtr spilled_nodes_;
  // 已合并的寄存器集合
  // 当合并u<-v时，将加入到这个集合中，u则被放回到某个工作表中（或反之）
  live::INodeListPtr coalesced_nodes_;
  live::INodeListPtr colored_nodes_;

  // P176 存放move指令的集合
  // 一共五个，所有的move指令在同一时刻分布在不同的集合中，互不重叠
  // 记录move指令是否在Coalesce过程中被处理过
  // 1. 已经合并的move指令
  live::MoveList *coalesced_moves_;
  // 2. src和dst存在冲突的move指令
  live::MoveList *constrained_moves_;
  // 3. 不再考虑合并的move指令
  live::MoveList *frozen_moves_;
  // 4. 有可能合并的move指令（等待在Colaesce()中处理）
  live::MoveList *worklist_moves_;
  // 5.
  // 尚未做好准备进行合并的move指令（Colasece()不处理这些，在EnableMoves()中启用）
  live::MoveList *active_moves_;

  // 一个包含从图中删除的临时变量的栈
  live::INodeListPtr select_stack_;
};

} // namespace ra

#endif