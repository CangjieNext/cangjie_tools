# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

file(GLOB FORMAT_SRC "*.cpp" "NodeFomatter/Decl/*.cpp" "NodeFomatter/Node/*.cpp" "NodeFomatter/Pattern/*.cpp" "NodeFomatter/Expr/*.cpp" "NodeFomatter/Type/*.cpp"  "DocProcessor/*.cpp")
add_library(CangjieFormat OBJECT ${FORMAT_SRC} ${AUTODIFF_SRC})
