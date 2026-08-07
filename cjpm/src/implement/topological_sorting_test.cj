// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.unittest.*
import std.unittest.testmacro.*

@Test
func formatMissingPackageDiagnosticIncludesSortedImporters() {
    let importers = ArrayList<String>(["dependency_demo", "missing_import_demo"])
    let got = formatMissingPackageDiagnostic("missing.nonexistent", importers)
    let expected = "missing.nonexistent (imported by: dependency_demo, missing_import_demo)"
    if (got != expected) {
        throw Exception("Expected '${expected}', got '${got}'")
    }
}
