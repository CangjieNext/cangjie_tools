// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CJCPL_H
#define CANGJIE_CJCPL_H

#include <string>
#include <vector>

#include "cangjie/AST/PrintNode.h"

#include "cjcpl/Option/Option.h"
#include "cjcpl/CheckersAndRules/Checker.h"
#include "cjcpl/CjoLoader/CjoLoader.h"
#include "cjcpl/Diff/Diff.h"
#include "cjcpl/Dsl/Dsl.h"

using namespace CjComplienceChecker;
namespace CjComplienceChecker {
int CJcpl(const CjComplienceChecker::Options &options);
}


#endif