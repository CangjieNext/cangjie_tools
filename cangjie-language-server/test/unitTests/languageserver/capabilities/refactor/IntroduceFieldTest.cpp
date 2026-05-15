// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/IntroduceField.h"
#include "gtest/gtest.h"

using namespace ark;
using namespace Cangjie::AST;

TEST(IntroduceFieldTest, TargetKinds)
{
    FuncDecl topLevelFunc;
    EXPECT_TRUE(IntroduceField::IsSupportedTargetFunc(topLevelFunc));
    EXPECT_FALSE(IntroduceField::IsMemberFieldTarget(topLevelFunc));
    EXPECT_FALSE(IntroduceField::IsStaticFieldTarget(topLevelFunc));

    auto classDecl = OwnedPtr<ClassDecl>(new ClassDecl());
    FuncDecl classFunc;
    classFunc.outerDecl = classDecl.get();
    EXPECT_TRUE(IntroduceField::IsSupportedTargetFunc(classFunc));
    EXPECT_TRUE(IntroduceField::IsMemberFieldTarget(classFunc));
    EXPECT_FALSE(IntroduceField::IsStaticFieldTarget(classFunc));
    classFunc.EnableAttr(Attribute::STATIC);
    EXPECT_TRUE(IntroduceField::IsStaticFieldTarget(classFunc));

    auto structDecl = OwnedPtr<StructDecl>(new StructDecl());
    FuncDecl structFunc;
    structFunc.outerDecl = structDecl.get();
    EXPECT_TRUE(IntroduceField::IsSupportedTargetFunc(structFunc));
    EXPECT_TRUE(IntroduceField::IsMemberFieldTarget(structFunc));

    auto interfaceDecl = OwnedPtr<InterfaceDecl>(new InterfaceDecl());
    FuncDecl interfaceFunc;
    interfaceFunc.outerDecl = interfaceDecl.get();
    EXPECT_FALSE(IntroduceField::IsSupportedTargetFunc(interfaceFunc));
    EXPECT_FALSE(IntroduceField::IsMemberFieldTarget(interfaceFunc));

    auto enumDecl = OwnedPtr<EnumDecl>(new EnumDecl());
    FuncDecl enumFunc;
    enumFunc.outerDecl = enumDecl.get();
    EXPECT_FALSE(IntroduceField::IsSupportedTargetFunc(enumFunc));
    EXPECT_FALSE(IntroduceField::IsMemberFieldTarget(enumFunc));

    auto extendDecl = OwnedPtr<ExtendDecl>(new ExtendDecl());
    FuncDecl extendFunc;
    extendFunc.outerDecl = extendDecl.get();
    EXPECT_FALSE(IntroduceField::IsSupportedTargetFunc(extendFunc));
    EXPECT_FALSE(IntroduceField::IsMemberFieldTarget(extendFunc));
}
