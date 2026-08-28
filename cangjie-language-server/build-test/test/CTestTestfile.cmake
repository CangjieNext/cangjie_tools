# CMake generated Testfile for 
# Source directory: D:/CppProjects/main/cangjie_tools/cangjie-language-server/test
# Build directory: D:/CppProjects/main/cangjie_tools/cangjie-language-server/build-test/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(gtest_simple_test "D:/CppProjects/main/cangjie_tools/cangjie-language-server/output/bin/gtest_LSPServer_test.exe")
set_tests_properties(gtest_simple_test PROPERTIES  _BACKTRACE_TRIPLES "D:/CppProjects/main/cangjie_tools/cangjie-language-server/test/CMakeLists.txt;103;add_test;D:/CppProjects/main/cangjie_tools/cangjie-language-server/test/CMakeLists.txt;0;")
subdirs("common")
subdirs("apiTest")
subdirs("featureTest/completion")
subdirs("featureTest/documentHighlight")
subdirs("featureTest/goToDefinition")
subdirs("featureTest/references")
subdirs("featureTest/semanticHighlight")
subdirs("featureTest/hover")
subdirs("featureTest/diagnostic")
subdirs("featureTest/codeCov")
subdirs("featureTest/workSpaceSymbol")
subdirs("featureTest/documentSymbol")
subdirs("featureTest/signatureHelp")
subdirs("featureTest/rename")
subdirs("featureTest/typeHierarchy")
subdirs("featureTest/breakpoints")
subdirs("featureTest/callHierarchy")
subdirs("featureTest/codeLens")
subdirs("featureTest/overrideMethods")
subdirs("featureTest/findFileReference")
subdirs("featureTest/applyEdit")
subdirs("featureTest/codeAction")
subdirs("featureTest/crossLanguageDefinition")
subdirs("unitTests")
