#!/bin/bash

# 检查输入参数
if [ "$#" -ne 1 ]; then
    echo "用法: \$0 <llvm_ir_file.ll>"
    exit 1
fi

LLVM_IR_FILE="$1"

# 检查文件是否存在
if [ ! -f "$LLVM_IR_FILE" ]; then
    echo "错误: 文件 '$LLVM_IR_FILE' 不存在."
    exit 1
fi

# 生成DOT文件
DOT_FILE="./test.dot"
opt -dot-cfg "$LLVM_IR_FILE" -o=$DOT_FILE

# 生成PNG文件
PNG_FILE="cfg.png"
dot -Tpng "$DOT_FILE" -o "$PNG_FILE"

# 输出结果
echo "控制流图已生成: $(realpath "$PNG_FILE")"
