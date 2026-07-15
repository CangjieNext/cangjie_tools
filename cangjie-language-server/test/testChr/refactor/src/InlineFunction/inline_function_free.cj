// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * InlineFunction - free function scenarios.
 *
 * These cases verify that inlining a free (top-level, non-class-member)
 * function does NOT prefix references with `this.`. The caller is also a
 * free function so there is no `this` in scope.
 */
package ohos_app_cangjie_entry.InlineFunction

// TC1: user-reported bug. Select tobeInline1() inside caller1 and trigger
//      "Inline function". Expected result body: println("test inline")
//      (must NOT become this.println("test inline")).
func tobeInline1() {
    println("test inline")
}

func caller1() {
    tobeInline1()
}

// TC2: free function with parameters and return value.
//      Select tobeInline2(1, 2) inside caller2. Expected: a + b inlined as
//      1 + 2; no this. anywhere.
func tobeInline2(a: Int64, b: Int64): Int64 {
    return a + b
}

func caller2() {
    var r = tobeInline2(1, 2)
    println(r)
}

// TC3: free function body calls multiple global functions.
//      Select tobeInline3() inside caller3. Expected: print("a") and
//      println("b") stay unqualified (no this.).
func tobeInline3(): Int64 {
    print("a")
    println("b")
    return 1
}

func caller3() {
    var r = tobeInline3()
}
