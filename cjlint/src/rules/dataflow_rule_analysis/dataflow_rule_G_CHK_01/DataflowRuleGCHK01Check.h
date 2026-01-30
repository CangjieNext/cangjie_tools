// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef DATAFLOW_RULE_G_CHK_01_CHECK_H
#define DATAFLOW_RULE_G_CHK_01_CHECK_H

#include <map>
#include <set>
#include <string>
#include "common/DiagnosticEngine.h"
#include "common/TaintData.h"
#include "../DataflowRule.h"

namespace Cangjie::CodeCheck {

using TaintedVarSet = std::set<std::string>;

class DataflowRuleGCHK01Check : public DataflowRule {
public:
    struct NodeInfo {
        bool isRoot = false;
        bool isHarmful = true;
        NodeInfo() = default;
        NodeInfo(bool isRoot, bool isHarmful) : isRoot(isRoot), isHarmful(isHarmful) {}
        NodeInfo(const NodeInfo&) = default;
        NodeInfo& operator=(const NodeInfo&) = default;
    };

    DataflowRuleGCHK01Check() : DataflowRule(nullptr) {}
    explicit DataflowRuleGCHK01Check(CodeCheckDiagnosticEngine* diagEngine) : DataflowRule(diagEngine) {}
    ~DataflowRuleGCHK01Check() override = default;

protected:
    void CheckBasedOnCHIR(CHIR::Package& package) override;

private:
    std::map<std::string, std::set<CHIR::Block*>> varInblockWithoutTainted;
    TaintedVarSet perilousGlobalVars;

    void CheckFuncBody(CHIR::Block& entryBlock);
    void CheckBlock(CHIR::Block* block);
    void CheckExpr(CHIR::Value* value, CHIR::LocalVar* result, TaintedVarSet& taintedVars);
    void CheckStore(CHIR::Store* store, TaintedVarSet& taintedVars);
    void CheckTuple(CHIR::Tuple* tuple, TaintedVarSet& taintedVars);
    void CheckField(CHIR::Field* field, TaintedVarSet& taintedVars);
    void CheckStoreInUsers(CHIR::Store* store, TaintedVarSet& taintedVars);
    void CheckStoreElementRef(CHIR::StoreElementRef* storeElementRef, TaintedVarSet& taintedVars);
    void CheckApplyUser(CHIR::Expression* user, CHIR::Value* result);

    template <typename T>
    bool IsAlarmNotNeeded(T* apply);
    template <typename T>
    void CheckApply(T* apply, TaintedVarSet& taintedVars);
    template <typename T>
    void CheckApplyInUsers(T* apply, CHIR::Value* arg);
    template <typename T>
    void CheckInvokeInUsers(T* invoke, CHIR::Value* arg);
    template <typename T>
    void CheckApplyUsers(T* apply, TaintedVarSet& taintedVars);

    void GetPerilousGlobalVar(CHIR::GlobalVar* globalVar);
};
} // namespace Cangjie::CodeCheck

#endif // DATAFLOW_RULE_G_CHK_01_CHECK_H
