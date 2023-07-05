#!/bin/bash

# 格式化
clang-format -i `find ./ -type f -name '*.h'`
clang-format -i `find ./ -type f -name '*.c'`

# 统计代码行数
cloc --git `git branch --show-current`