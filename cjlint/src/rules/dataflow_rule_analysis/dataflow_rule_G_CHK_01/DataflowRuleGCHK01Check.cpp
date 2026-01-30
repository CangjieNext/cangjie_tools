// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "DataflowRuleGCHK01Check.h"
#include <algorithm>
#include "cangjie/AST/Match.h"
#include "cangjie/Utils/Utils.h"
#include "common/CommonFunc.h"

namespace Cangjie::CodeCheck {

using namespace Cangjie::CHIR;

using namespace Cangjie::CodeCheck::TaintData;

// Check whether an exit node exists in a block in focus.
static bool IsAnyReturnInBlock(CHIR::Block* block)
{
    auto exprs = block->GetExpressions();
    auto ret = std::find_if(
        exprs.begin(), exprs.end(), [](auto& expr) { return expr->GetExprKind() == CHIR::ExprKind::EXIT; });
    return ret != exprs.end();
}

static void GetDecontaminatBlockHelper(
    CHIR::Block* block, std::set<Cangjie::CHIR::Block*>& blockSet, std::set<CHIR::Block*>& visited)
{
    visited.insert(block);
    blockSet.insert(block);
    auto exprs = block->GetExpressions();
    for (auto& expr : exprs) {
        if (!expr->IsTerminator()) {
            continue;
        }
        auto terminator = StaticCast<CHIR::Terminator*>(expr);
        for (auto newBlock : terminator->GetSuccessors()) {
            if (visited.count(newBlock) == 0) {
                blockSet.insert(newBlock);
                GetDecontaminatBlockHelper(newBlock, blockSet, visited);
            }
        }
    }
}

// Obtains the block after decontamination.
static void GetDecontaminatBlock(
    CHIR::Value* value, std::set<Cangjie::CHIR::Block*>& blockSet, std::set<Cangjie::CHIR::Expression*>& visitedExpr)
{
    for (auto user : value->GetUsers()) {
        if (visitedExpr.count(user) != 0) {
            continue;
        }
        visitedExpr.insert(user);
        if (user->GetExprKind() == CHIR::ExprKind::BRANCH) {
            auto branch = StaticCast<CHIR::Branch*>(user);
            auto trueBlock = branch->GetTrueBlock();
            auto falseBlock = branch->GetFalseBlock();
            if (branch->GetCondition() == value && (IsAnyReturnInBlock(trueBlock) || IsAnyReturnInBlock(falseBlock))) {
                std::set<Cangjie::CHIR::Block*> visited;
                if (IsAnyReturnInBlock(trueBlock)) {
                    GetDecontaminatBlockHelper(falseBlock, blockSet, visited);
                }
                if (IsAnyReturnInBlock(falseBlock)) {
                    GetDecontaminatBlockHelper(trueBlock, blockSet, visited);
                }
            }
        } else {
            GetDecontaminatBlock(user->GetResult(), blockSet, visitedExpr);
        }
    }
}

static std::set<Cangjie::CHIR::Block*> GetDecontaminatBlock(CHIR::Value* value)
{
    std::set<Cangjie::CHIR::Block*> blockSet;
    std::set<Cangjie::CHIR::Expression*> visitedExpr;
    GetDecontaminatBlock(value, blockSet, visitedExpr);
    return blockSet;
}

// Returns true if the argument is untrusted in the given block (not in decontaminated set).
static bool IsArgUntrustedInBlock(
    const std::map<std::string, std::set<CHIR::Block*>>& varInblockWithoutTainted,
    const std::string& identifier, CHIR::Block* block)
{
    auto it = varInblockWithoutTainted.find(identifier);
    return it == varInblockWithoutTainted.end() || it->second.count(block) == 0;
}

static std::string OrdinalSuffix(uint32_t n)
{
    if (n >= 11 && n <= 13) {
        return std::to_string(n) + "th";
    }
    switch (n % 10) {
        case 1: return std::to_string(n) + "st";
        case 2: return std::to_string(n) + "nd";
        case 3: return std::to_string(n) + "rd";
        default: return std::to_string(n) + "th";
    }
}

static bool IsMemberFunc(CHIR::Value* value)
{
    if (value->IsFuncWithBody()) {
        auto func = VirtualCast<CHIR::Func*>(value);
        if (func->TestAttr(Attribute::STATIC)) {
            return false;
        }
        if (func->GetParentCustomTypeDef() || func->GetFuncKind() == CHIR::FuncKind::LAMBDA) {
            return true;
        }
    }
    return false;
}

static std::string GetArgName(CHIR::Value* value)
{
    if (value->IsLocalVar()) {
        auto localVar = StaticCast<CHIR::LocalVar*>(value);
        auto debug = localVar->GetDebugExpr();
        if (debug) {
            return debug->GetSrcCodeIdentifier();
        }
        auto expr = localVar->GetExpr();
        if (expr->GetExprKind() == CHIR::ExprKind::LOAD) {
            auto load = StaticCast<CHIR::Load*>(expr);
            return GetArgName(load->GetLocation());
        }
        return localVar->GetIdentifier();
    }
    return value->GetSrcCodeIdentifier();
}

template <typename T>
bool DataflowRuleGCHK01Check::IsAlarmNotNeeded(T* apply)
{
    auto funcName = apply->GetCallee()->GetSrcCodeIdentifier();
    return std::any_of(taintPropagate.begin(), taintPropagate.end(),
        [&funcName](const auto& taintedFunc) { return taintedFunc.first.funcName == funcName; });
}

template <typename T>
void DataflowRuleGCHK01Check::CheckApplyInUsers(T* apply, CHIR::Value* arg)
{
    if (IsAlarmNotNeeded<T>(apply)) {
        return;
    }
    auto args = apply->GetArgs();
    auto block = apply->GetParentBlock();
    auto argIdentifier = GetArgName(arg);
    for (uint32_t i = 0; i < args.size(); ++i) {
        if (args[i] != arg || !IsArgUntrustedInBlock(varInblockWithoutTainted, argIdentifier, block)) {
            continue;
        }
        auto index = IsMemberFunc(apply->GetCallee()) ? i : i + 1;
        std::string tips = "the " + OrdinalSuffix(index) + " argument is untrusted, we can't deliver it outer.";
        std::string calleeName;
        auto callee = apply->GetCallee();
        if (callee->IsLocalVar()) {
            auto lambda = StaticCast<const CHIR::LocalVar*>(callee)->GetExpr();
            CJC_ASSERT(lambda->GetExprKind() == CHIR::ExprKind::LAMBDA);
            calleeName = StaticCast<const CHIR::Lambda*>(lambda)->GetSrcCodeIdentifier();
        } else {
            calleeName = callee->GetSrcCodeIdentifier();
        }
        auto applyLoc = CommonFunc::GetCodePosition(apply);
        diagEngine->Diagnose(
            applyLoc.first, applyLoc.second, CodeCheckDiagKind::G_CHK_01_illegal_func_arg, calleeName, tips);
    }
}

template <typename T>
void DataflowRuleGCHK01Check::CheckInvokeInUsers(T* invoke, CHIR::Value* arg)
{
    auto args = invoke->GetArgs();
    auto block = invoke->GetParentBlock();
    auto argIdentifier = GetArgName(arg);
    for (uint32_t i = 0; i < args.size(); ++i) {
        if (args[i] != arg || !IsArgUntrustedInBlock(varInblockWithoutTainted, argIdentifier, block)) {
            continue;
        }
        auto index = IsMemberFunc(invoke->GetOperand(0)) ? i : i + 1;
        std::string tips = "the " + OrdinalSuffix(index) + " argument is untrusted, we can't deliver it outer.";
        auto invokeLoc = CommonFunc::GetCodePosition(invoke);
        diagEngine->Diagnose(invokeLoc.first, invokeLoc.second, CodeCheckDiagKind::G_CHK_01_illegal_func_arg,
            invoke->GetMethodName(), tips);
    }
}

static CommonFunc::PositionPair GetCorrectCodeLoc(CHIR::Store* store)
{
    auto pos = CommonFunc::GetCodePosition(store);
    if (pos.first == Cangjie::Position{0, 0, 0}) {
        auto block = store->GetParentBlock();
        return CommonFunc::GetCodePosition(block->GetTerminator());
    }
    return pos;
}

void DataflowRuleGCHK01Check::CheckStoreInUsers(CHIR::Store* store, TaintedVarSet& taintedVars)
{
    auto localVar = DynamicCast<CHIR::LocalVar*>(store->GetLocation());
    if (localVar && localVar->IsRetValue()) {
        auto storeLoc = GetCorrectCodeLoc(store);
        diagEngine->Diagnose(storeLoc.first, storeLoc.second, CodeCheckDiagKind::G_CHK_01_illegal_return);
    } else {
        auto identifier = store->GetLocation()->GetSrcCodeIdentifier();
        taintedVars.insert(identifier);
        auto blockSet = GetDecontaminatBlock(store->GetLocation());
        varInblockWithoutTainted[identifier] = blockSet;
    }
}

void DataflowRuleGCHK01Check::CheckApplyUser(CHIR::Expression* user, CHIR::Value* result)
{
    switch (user->GetExprKind()) {
        case CHIR::ExprKind::APPLY:
            CheckApplyInUsers<CHIR::Apply>(StaticCast<CHIR::Apply*>(user), result);
            break;
        case CHIR::ExprKind::APPLY_WITH_EXCEPTION:
            CheckApplyInUsers<CHIR::ApplyWithException>(StaticCast<CHIR::ApplyWithException*>(user), result);
            break;
        case CHIR::ExprKind::INVOKE:
            CheckInvokeInUsers<CHIR::Invoke>(StaticCast<CHIR::Invoke*>(user), result);
            break;
        case CHIR::ExprKind::INVOKESTATIC:
            CheckInvokeInUsers<CHIR::InvokeStatic>(StaticCast<CHIR::InvokeStatic*>(user), result);
            break;
        case CHIR::ExprKind::INVOKE_WITH_EXCEPTION:
            CheckInvokeInUsers<CHIR::InvokeWithException>(StaticCast<CHIR::InvokeWithException*>(user), result);
            break;
        default:
            break;
    }
}

template <typename T>
void DataflowRuleGCHK01Check::CheckApplyUsers(T* apply, TaintedVarSet& taintedVars)
{
    auto users = apply->GetResult()->GetUsers();
    for (auto user : users) {
        if (user->GetExprKind() == CHIR::ExprKind::STORE) {
            auto store = StaticCast<CHIR::Store*>(user);
            CheckStoreInUsers(store, taintedVars);
            continue;
        }
        // Let-type Warning
        CheckApplyUser(user, apply->GetResult());
    }
}

template <typename T>
void DataflowRuleGCHK01Check::CheckApply(T* apply, TaintedVarSet& taintedVars)
{
    for (auto taintedFunc : taintSource) {
        if (CommonFunc::FindCHIRFunction(apply->GetCallee(), taintedFunc.first) && apply->GetResult()) {
            auto loc = CommonFunc::GetCodePosition(apply);
            auto identifier = apply->GetResult()->GetIdentifier();
            taintedVars.insert(identifier);
            auto blockSet = GetDecontaminatBlock(apply->GetResult());
            varInblockWithoutTainted[identifier] = blockSet;
            CheckApplyUsers(apply, taintedVars);
            return;
        }
    }
    for (auto taintedFunc : taintPropagate) {
        if (CommonFunc::FindCHIRFunction(apply->GetCallee(), taintedFunc.first)) {
            auto args = apply->GetArgs();
            auto arg = std::find_if(args.begin(), args.end(), [&taintedVars](auto& arg) {
                auto identifier = GetArgName(arg);
                return taintedVars.count(identifier) > 0;
            });
            if (arg != args.end()) {
                auto result = taintedFunc.second.dst == RET_VAL ? apply->GetResult() : args[taintedFunc.second.dst - 1];
                auto identifier = GetArgName(result);
                taintedVars.insert(identifier);
                auto blockSet = GetDecontaminatBlock(result);
                varInblockWithoutTainted[identifier] = blockSet;
                auto users = result->GetUsers();
                auto user = std::find_if(users.begin(), users.end(),
                    [](auto& user) { return user->GetExprKind() == CHIR::ExprKind::STORE; });
                if (user != users.end()) {
                    auto store = StaticCast<CHIR::Store*>(*user);
                    CheckStoreInUsers(store, taintedVars);
                }
                return;
            }
        }
    }
}

void DataflowRuleGCHK01Check::CheckExpr(
    CHIR::Value* value, CHIR::LocalVar* result, TaintedVarSet& taintedVars)
{
    auto identifier = value->GetSrcCodeIdentifier().empty() ? value->GetIdentifier() : value->GetSrcCodeIdentifier();
    if (taintedVars.count(identifier) == 0) {
        return;
    }
    taintedVars.insert(result->GetIdentifier());
    auto users = result->GetUsers();
    for (auto user : users) {
        if (user->GetExprKind() != CHIR::ExprKind::STORE) {
            CheckApplyUser(user, result);
            continue;
        }
        auto store = StaticCast<CHIR::Store*>(user);
        auto storeLoc = store->GetLocation();
        if (auto localVar = DynamicCast<CHIR::LocalVar*>(storeLoc); localVar && localVar->IsRetValue()) {
            auto loc = GetCorrectCodeLoc(store);
            diagEngine->Diagnose(loc.first, loc.second, CodeCheckDiagKind::G_CHK_01_illegal_return);
            continue;
        }
        if (store->GetLocation() == result) {
            taintedVars.erase(identifier);
        } else {
            auto newVar = store->GetLocation()->GetSrcCodeIdentifier();
            taintedVars.insert(newVar);
            auto blockSet = GetDecontaminatBlock(store->GetLocation());
            varInblockWithoutTainted[newVar] = blockSet;
        }
    }
}

void DataflowRuleGCHK01Check::CheckStore(CHIR::Store* store, TaintedVarSet& taintedVars)
{
    auto value = store->GetValue();
    auto identifier = GetArgName(value);
    if (taintedVars.count(identifier) > 0) {
        auto storeLoc = store->GetLocation();
        auto localVar = DynamicCast<CHIR::LocalVar*>(storeLoc);
        if (localVar && localVar->IsRetValue()) {
            auto loc = GetCorrectCodeLoc(store);
            diagEngine->Diagnose(loc.first, loc.second, CodeCheckDiagKind::G_CHK_01_illegal_return);
        }
        taintedVars.insert(GetArgName(storeLoc));
    }
}

/**
 * Added support for tuple type data checking, for example
 * '''
 * var a1: (String,String,String,String) = (getArgs()[0],getArgs()[1],getArgs()[2],getArgs()[3])
 * var a2 = a1[0]
 * return a2
 * '''
 */
void DataflowRuleGCHK01Check::CheckTuple(CHIR::Tuple* tuple, TaintedVarSet& taintedVars)
{
    auto operands = tuple->GetOperands();
    auto iter = std::find_if(operands.begin(), operands.end(), [&taintedVars](auto& operand) {
        auto identifier = GetArgName(operand);
        return taintedVars.count(identifier) > 0;
    });
    if (iter != operands.end()) {
        auto users = tuple->GetResult()->GetUsers();
        if (users.size() == 1 && users[0]->GetExprKind() == CHIR::ExprKind::STORE) {
            auto store = StaticCast<CHIR::Store*>(users[0]);
            CheckStoreInUsers(store, taintedVars);
        }
    }
}

void DataflowRuleGCHK01Check::CheckStoreElementRef(
    CHIR::StoreElementRef* storeElementRef, TaintedVarSet& taintedVars)
{
    auto base = storeElementRef->GetValue();
    auto identifier = GetArgName(base);
    if (taintedVars.count(identifier) > 0) {
        taintedVars.insert(GetArgName(storeElementRef->GetLocation()));
    }
}

void DataflowRuleGCHK01Check::CheckField(CHIR::Field* field, TaintedVarSet& taintedVars)
{
    auto identifier = GetArgName(field->GetBase());
    if (taintedVars.count(identifier) > 0) {
        taintedVars.insert(GetArgName(field->GetResult()));
    }
}

void DataflowRuleGCHK01Check::CheckFuncBody(CHIR::Block& entryBlock)
{
    std::unordered_set<Block*> visited;
    std::function<void(CHIR::Block*, TaintedVarSet)> visitBlock =
        [this, &visitBlock, &visited](CHIR::Block* block, TaintedVarSet taintedVars) {
            if (!visited.emplace(block).second) {
                return;
            }
            for (auto expr : block->GetExpressions()) {
                switch (expr->GetExprKind()) {
                    case CHIR::ExprKind::LAMBDA:
                        CheckFuncBody(*StaticCast<CHIR::Lambda*>(expr)->GetEntryBlock());
                        break;
                    case CHIR::ExprKind::APPLY:
                        CheckApply<CHIR::Apply>(StaticCast<CHIR::Apply*>(expr), taintedVars);
                        break;
                    case CHIR::ExprKind::LOAD: {
                        auto load = StaticCast<CHIR::Load*>(expr);
                        CheckExpr(load->GetLocation(), load->GetResult(), taintedVars);
                        break;
                    }
                    case CHIR::ExprKind::STORE:
                        CheckStore(StaticCast<CHIR::Store*>(expr), taintedVars);
                        break;
                    case CHIR::ExprKind::TYPECAST: {
                        auto typeCast = StaticCast<CHIR::TypeCast*>(expr);
                        CheckExpr(typeCast->GetSourceValue(), typeCast->GetResult(), taintedVars);
                        break;
                    }
                    case CHIR::ExprKind::BOX: {
                        auto box = StaticCast<CHIR::Box*>(expr);
                        CheckExpr(box->GetSourceValue(), box->GetResult(), taintedVars);
                        break;
                    }
                    case CHIR::ExprKind::TUPLE:
                        CheckTuple(StaticCast<CHIR::Tuple*>(expr), taintedVars);
                        break;
                    case CHIR::ExprKind::STORE_ELEMENT_REF:
                        CheckStoreElementRef(StaticCast<CHIR::StoreElementRef*>(expr), taintedVars);
                        break;
                    case CHIR::ExprKind::FIELD:
                        CheckField(StaticCast<CHIR::Field*>(expr), taintedVars);
                        break;
                    case CHIR::ExprKind::APPLY_WITH_EXCEPTION:
                        CheckApply<CHIR::ApplyWithException>(
                            StaticCast<CHIR::ApplyWithException*>(expr), taintedVars);
                        [[fallthrough]];
                    default:
                        if (expr->IsTerminator()) {
                            auto term = StaticCast<CHIR::Terminator*>(expr);
                            for (auto newblock : term->GetSuccessors()) {
                                visitBlock(newblock, taintedVars);
                            }
                        }
                        break;
                }
            }
        };
    TaintedVarSet taintedVars = perilousGlobalVars;
    visitBlock(&entryBlock, taintedVars);
}

void DataflowRuleGCHK01Check::CheckBlock(CHIR::Block* block)
{
    if (!block) {
        return;
    }
    for (auto expr : block->GetExpressions()) {
        if (expr->GetExprKind() == CHIR::ExprKind::APPLY) {
            auto apply = StaticCast<CHIR::Apply*>(expr);
            CheckApply<CHIR::Apply>(apply, perilousGlobalVars);
        }
    }
}

void DataflowRuleGCHK01Check::GetPerilousGlobalVar(CHIR::GlobalVar* globalVar)
{
    auto func = globalVar->GetInitFunc();
    if (func) {
        CheckBlock(func->GetEntryBlock());
    }
}

void DataflowRuleGCHK01Check::CheckBasedOnCHIR(CHIR::Package& package)
{
    auto globalVars = package.GetGlobalVars();
    for (auto var : globalVars) {
        GetPerilousGlobalVar(var);
    }
    auto funcs = package.GetGlobalFuncs();
    for (auto& func : funcs) {
        if (func->TestAttr(CHIR::Attribute::IMPORTED)) {
            continue;
        }
        auto identifier = func->GetIdentifier();
        // Check whether the function is the initialization function of global variables.
        // If yes, skip this step.
        // 5 means the length of the string "@_CGV$"
        if (identifier.size() > 5 && identifier.compare(0, 5, "@_CGV")) {
            CheckFuncBody(*func->GetEntryBlock());
        }
    }
}

} // namespace Cangjie::CodeCheck
