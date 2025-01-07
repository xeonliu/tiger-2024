#include "tiger/liveness/flowgraph.h"
#include <iostream>
#include <unordered_map>
namespace fg {

/**
 * @brief  Assem will construct a flow graph given instructions in instr_list_
 * and store into flowgraph_
 * @note   Info of each graph::Node can be a pointer to an assem::Instr
 *         jump fileds of the instrs are used to create control flow edges
 * @retval None
 */
void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  // using FNode = graph::Node<assem::Instr>;
  // using FNodePtr = graph::Node<assem::Instr> *;
  // using FNodeListPtr = graph::NodeList<assem::Instr> *;
  // using FGraph = graph::Graph<assem::Instr>;
  // using FGraphPtr = graph::Graph<assem::Instr> *;

  /* Construct the flow graph */

  // Graph API:
  // 1. NewNode(T *info) -> Node<T> *
  // 2. AddEdge(Node<T> *from, Node<T> *to) -> void
  // 3. Nodes() -> NodeList<T> *
  // 4. Show(FILE *out, NodeList<T> *p, std::function<void(T *)> show_info) ->
  // void

  // Deal with nodes one at a time
  // Each Node contains information of an assem::Instr

  auto instr_list = instr_list_->GetList();
  // Create a map between instr and node
  std::map<assem::Instr *, FNodePtr> instr_node_map;

  for (auto inst : instr_list) {

    // Add Each Instrcution as a Node.
    FNodePtr node = flowgraph_->NewNode(inst);

    // TODO: Label是否用作节点？
    // NOTE: 目前仍用作节点
    // Identify the Labels and add them to the label_map_
    // Check the type of instructions
    // 1. OperInstr
    // 2. LabelInstr
    // 3. MoveInstr

    if (auto label_instr = dynamic_cast<assem::LabelInstr *>(inst)) {
      // Add the label to the label_map_
      label_map_->insert({label_instr->label_->Name(), node});
    }

    // Add the node to the map
    instr_node_map.insert({inst, node});
  }

  // Connect the nodes with edges

  for (auto [inst, node] : instr_node_map) {
    // Check the type of instructions

    // 1. OperInstr
    if (auto oper_instr = dynamic_cast<assem::OperInstr *>(inst)) {
      // Check if the instruction is a jump
      if (oper_instr->jumps_) {
        // Get the label
        auto label_temps = oper_instr->jumps_->labels_;
        for (auto label_temp : *label_temps) {
          

          // FIXME: 不跳转Label，跳转到下一条指令
          if (label_temp == nullptr) {
            // Deal with the case where the next instruction is the one
            // following it.
            auto next_inst = std::next(instr_node_map.find(inst))->first;
            // Get the node corresponding to the next instruction
            auto next_node = instr_node_map.at(next_inst);
            // Add an edge between the current node and the next node
            flowgraph_->AddEdge(node, next_node);
            continue;
          }

          // Get the label
          auto label = label_temp->Name();
          // Get the node corresponding to the label
          auto label_node = label_map_->at(label);
          // Add an edge between the current node and the label node
          flowgraph_->AddEdge(node, label_node);
        }
        break;
      }
    }

    // 2. LabelInstr
    // 3. MoveInstr
    // Get the next instruction
    auto next_inst = std::next(instr_node_map.find(inst))->first;
    // Get the node corresponding to the next instruction
    auto next_node = instr_node_map.at(next_inst);
    // Add an edge between the current node and the next node
    flowgraph_->AddEdge(node, next_node);
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const { return dst_; }

temp::TempList *OperInstr::Def() const { return dst_; }

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  return (src_) ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  return (src_) ? src_ : new temp::TempList();
}
} // namespace assem
