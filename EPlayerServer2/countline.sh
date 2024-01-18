#!/bin/bash

# 统计 .cpp 和 .h 文件总行数的 Bash 脚本

# 初始化行数计数器
total_lines=0

# 查找当前文件夹下的所有 .cpp 和 .h 文件
files=$(find . -type f \( -name "*.cpp" -o -name "*.h" \))

# 遍历文件并统计行数
for file in $files; do
    lines=$(wc -l < "$file")
    echo "$file: $lines lines"
    ((total_lines += lines))
done

# 输出总行数
echo "Total lines: $total_lines"
