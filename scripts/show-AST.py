import graphviz
import sys


def parse_tree(lines):
    # lines = input_str.strip().splitlines()
    tree = {}
    stack = []
    idx = 0

    for line in lines:
        idx += 1
        depth = len(line) - len(line.lstrip())
        # print(f"Depth = {depth} in {line}")
        if(depth == 0):
            continue
        #     print(f"Depth = 0 in {line}")
        #     print(line)
        #     print(line.lstrip())
        #     continue

        node_name = line.strip()
        node_id = f"[L{idx}] {node_name}"  # 确保每个节点唯一

        # 如果当前深度为0，直接添加根节点
        if depth == 1:
            tree[node_id] = []
            stack = [(node_id, depth)]
        else:
            # 找到当前节点的父节点
            while stack and stack[-1][1] >= depth:
                stack.pop()
            if stack:
                parent_id = stack[-1][0]
                tree[parent_id].append(node_id)
                tree[node_id] = []
            stack.append((node_id, depth))

    return tree

def visualize_tree(tree, filename):
    filename+="-tree"
    dot = graphviz.Digraph()

    for parent, children in tree.items():
        for child in children:
            dot.node(parent)
            dot.node(child)
            dot.edge(parent, child)
    print(f"Saved at {filename}.png")
    dot.render(filename, format='png', cleanup=True)


if __name__ == "__main__":
    # 获取命令行参数
    file_name = sys.argv[1]  # sys.argv[0] 是脚本名称
    # print("传入的参数:", args)

    # file_name = '/home/stu/tiger-compiler/testdata/lab5or6/refs-part1/test_patch.out'
    # file_name = '/home/stu/tiger-compiler/testdata/lab5or6/refs-part1/tif.out'
    # /home/stu/tiger-compiler/testdata/lab5or6/refs-part1/tif.out

    # 使用 with 语句打开文件，确保文件在读取后正确关闭
    print(f"file_name = {file_name}")
    with open(file_name, 'r', encoding='utf-8') as file:
        # 逐行读取文件内容
        lines = file.readlines()

        # 解析树状结构
        tree = parse_tree(lines)

        # 可视化并保存为文件
        visualize_tree(tree, file_name)
