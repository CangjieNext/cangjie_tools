// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Protocol.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace ark;

namespace {
constexpr int32_t TEST_VERSION_1 = 1;
constexpr int32_t TEST_VERSION_2 = 2;
constexpr int32_t TEST_LINE_0 = 0;
constexpr int32_t TEST_LINE_1 = 1;
constexpr int32_t TEST_LINE_2 = 2;
constexpr int32_t TEST_LINE_3 = 3;
constexpr int32_t TEST_LINE_4 = 4;
constexpr int32_t TEST_LINE_5 = 5;
constexpr int32_t TEST_LINE_6 = 6;
constexpr int32_t TEST_LINE_7 = 7;
constexpr int32_t TEST_LINE_10 = 10;
constexpr int32_t TEST_LINE_20 = 20;
constexpr int32_t TEST_COL_0 = 0;
constexpr int32_t TEST_COL_5 = 5;
constexpr int32_t TEST_COL_7 = 7;
constexpr int32_t TEST_COL_10 = 10;
constexpr int32_t TEST_COL_13 = 13;
constexpr int32_t TEST_COL_15 = 15;
constexpr int32_t TEST_COL_20 = 20;
constexpr int32_t TEST_COL_25 = 25;
constexpr int32_t TEST_RANGE_LEN_10 = 10;
constexpr int32_t INVALID_TRIGGER_KIND = -1;
constexpr int32_t TRIGGER_KIND_INVOKED = 1;
constexpr int32_t TRIGGER_KIND_CHARACTER = 2;
constexpr uint64_t TEST_SYMBOL_ID_1 = 12345ull;
constexpr uint64_t TEST_SYMBOL_ID_2 = 67890ull;
constexpr int32_t SYMBOL_KIND_CLASS = 5;
constexpr int32_t SYMBOL_KIND_METHOD = 6;
constexpr int32_t FILE_CHANGE_CREATED = 1;
constexpr int32_t TEST_DIAG_CODE = 1;
constexpr int32_t TEST_DIAG_CAT = 1;
constexpr int32_t TEST_TAG_1 = 1;
constexpr int32_t TEST_TAG_2 = 2;
constexpr uint32_t EXPECTED_SIZE_1 = 1u;
constexpr uint32_t EXPECTED_SIZE_2 = 2u;
constexpr int32_t TEST_FILE_ID_1 = 1;
constexpr int32_t TEST_FILE_ID_2 = 2;
constexpr int32_t INVALID_POS_VALUE = 123;
}

class ProtocolTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        MessageHeaderEndOfLine::SetIsDeveco(false);
    }
};

TEST_F(ProtocolTest, FromJSON_DidOpenTextDocumentParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "languageId": "Cangjie",
            "version": 1,
            "text": "fn main() {}"
        }
    })"_json;

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.textDocument.languageId, "Cangjie");
    EXPECT_EQ(reply.textDocument.version, TEST_VERSION_1);
    EXPECT_EQ(reply.textDocument.text, "fn main() {}");
}

TEST_F(ProtocolTest, FromJSON_DidOpenTextDocumentParams_MissingFields)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "languageId": "Cangjie"
        }
    })"_json;

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_TextDocumentPositionParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": 10,
            "character": 5
        }
    })"_json;

    TextDocumentPositionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.position.line, TEST_LINE_10);
    EXPECT_EQ(reply.position.column, TEST_COL_5);
}

TEST_F(ProtocolTest, FromJSON_TextDocumentPositionParams_InvalidStructure)
{
    json params = R"({
        "textDocument": "invalid",
        "position": {
            "line": 10,
            "character": 5
        }
    })"_json;

    TextDocumentPositionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_SignatureHelpContext_InvalidTriggerKind)
{
    json params = R"({
        "triggerKind": -1
    })"_json;

    SignatureHelpContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_SignatureHelpParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": 10,
            "character": 5
        },
        "context": {
            "triggerKind": 1
        }
    })"_json;

    SignatureHelpParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.position.line, TEST_LINE_10);
    EXPECT_EQ(reply.position.column, TEST_COL_5);
}

TEST_F(ProtocolTest, FromJSON_InitializeParams_ValidInput)
{
    json params = R"({
        "rootUri": "file:///workspace",
        "capabilities": {
            "textDocument": {
                "documentHighlight": {},
                "typeHierarchy": {},
                "publishDiagnostics": {
                    "versionSupport": true
                },
                "hover": {},
                "documentLink": {}
            }
        },
        "initializationOptions": {
            "cangjieRootUri": "file:///custom_root"
        }
    })"_json;

    InitializeParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.rootUri.file, "file:///custom_root");
    EXPECT_TRUE(MessageHeaderEndOfLine::GetIsDeveco());
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.documentHighlightClientCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.typeHierarchyCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.diagnosticVersionSupport);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.hoverClientCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.documentLinkClientCapabilities);
}

TEST_F(ProtocolTest, FromJSON_DidCloseTextDocumentParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        }
    })"_json;

    DidCloseTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
}

TEST_F(ProtocolTest, FromJSON_TrackCompletionParams_ValidInput)
{
    json params = R"({
        "label": "myFunction"
    })"_json;

    TrackCompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.label, "myFunction");
}

TEST_F(ProtocolTest, FromJSON_TrackCompletionParams_MissingLabel)
{
    json params = R"({
        "otherField": "value"
    })"_json;

    TrackCompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_CompletionContext_ValidInput)
{
    json params = R"({
        "triggerKind": 2,
        "triggerCharacter": "."
    })"_json;

    CompletionContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(static_cast<int>(reply.triggerKind), TRIGGER_KIND_CHARACTER);
    EXPECT_EQ(reply.triggerCharacter, ".");
}

TEST_F(ProtocolTest, FromJSON_CompletionParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": 10,
            "character": 5
        },
        "context": {
            "triggerKind": 1
        }
    })"_json;

    CompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.position.line, TEST_LINE_10);
    EXPECT_EQ(reply.position.column, TEST_COL_5);
    EXPECT_EQ(static_cast<int>(reply.context.triggerKind), TRIGGER_KIND_INVOKED);
}

TEST_F(ProtocolTest, FromJSON_SemanticTokensParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        }
    })"_json;

    SemanticTokensParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
}

TEST_F(ProtocolTest, FromJSON_DidChangeTextDocumentParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "version": 2
        },
        "contentChanges": [
            {
                "text": "updated text",
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 10}
                },
                "rangeLength": 10
            }
        ]
    })"_json;

    DidChangeTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.textDocument.version, TEST_VERSION_2);
    ASSERT_EQ(reply.contentChanges.size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].text, "updated text");
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].range->start.line, TEST_LINE_0);
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].range->start.column, TEST_COL_0);
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].range->end.line, TEST_LINE_0);
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].range->end.column, TEST_COL_10);
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].rangeLength, TEST_RANGE_LEN_10);
}

TEST_F(ProtocolTest, FromJSON_RenameParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": 10,
            "character": 5
        },
        "newName": "newVarName"
    })"_json;

    RenameParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.position.line, TEST_LINE_10);
    EXPECT_EQ(reply.position.column, TEST_COL_5);
    EXPECT_EQ(reply.newName, "newVarName");
}

TEST_F(ProtocolTest, FromJSON_TextDocumentIdentifier_ValidInput)
{
    json params = R"({
        "uri": "file:///test.cj"
    })"_json;

    TextDocumentIdentifier reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.uri.file, "file:///test.cj");
}

TEST_F(ProtocolTest, FromJSON_TextDocumentParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        }
    })"_json;

    TextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
}

TEST_F(ProtocolTest, FromJSON_TypeHierarchyItem_ValidInput)
{
    json params = R"({
        "item": {
            "name": "MyClass",
            "kind": 5,
            "uri": "file:///test.cj",
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 10, "character": 20}
            },
            "selectionRange": {
                "start": {"line": 2, "character": 5},
                "end": {"line": 2, "character": 15}
            },
            "data": {
                "isKernel": true,
                "isChildOrSuper": false,
                "symbolId": "12345"
            }
        }
    })"_json;

    TypeHierarchyItem reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.name, "MyClass");
    EXPECT_EQ(static_cast<int>(reply.kind), SYMBOL_KIND_CLASS);
    EXPECT_EQ(reply.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.range.start.line, TEST_LINE_0);
    EXPECT_EQ(reply.range.start.column, TEST_COL_0);
    EXPECT_EQ(reply.range.end.line, TEST_LINE_10);
    EXPECT_EQ(reply.range.end.column, TEST_COL_20);
    EXPECT_EQ(reply.selectionRange.start.line, TEST_LINE_2);
    EXPECT_EQ(reply.selectionRange.start.column, TEST_COL_5);
    EXPECT_EQ(reply.selectionRange.end.line, TEST_LINE_2);
    EXPECT_EQ(reply.selectionRange.end.column, TEST_COL_15);
    EXPECT_TRUE(reply.isKernel);
    EXPECT_FALSE(reply.isChildOrSuper);
    EXPECT_EQ(reply.symbolId, TEST_SYMBOL_ID_1);
}

TEST_F(ProtocolTest, FromJSON_CallHierarchyItem_ValidInput)
{
    json params = R"({
        "item": {
            "name": "myMethod",
            "kind": 6,
            "uri": "file:///test.cj",
            "range": {
                "start": {"line": 5, "character": 10},
                "end": {"line": 7, "character": 20}
            },
            "selectionRange": {
                "start": {"line": 6, "character": 15},
                "end": {"line": 6, "character": 25}
            },
            "detail": "This is a method",
            "data": {
                "isKernel": false,
                "symbolId": "67890"
            }
        }
    })"_json;

    CallHierarchyItem reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.name, "myMethod");
    EXPECT_EQ(static_cast<int>(reply.kind), SYMBOL_KIND_METHOD);
    EXPECT_EQ(reply.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.range.start.line, TEST_LINE_5);
    EXPECT_EQ(reply.range.start.column, TEST_COL_10);
    EXPECT_EQ(reply.range.end.line, TEST_LINE_7);
    EXPECT_EQ(reply.range.end.column, TEST_COL_20);
    EXPECT_EQ(reply.selectionRange.start.line, TEST_LINE_6);
    EXPECT_EQ(reply.selectionRange.start.column, TEST_COL_15);
    EXPECT_EQ(reply.selectionRange.end.line, TEST_LINE_6);
    EXPECT_EQ(reply.selectionRange.end.column, TEST_COL_25);
    EXPECT_EQ(reply.detail, "This is a method");
    EXPECT_FALSE(reply.isKernel);
    EXPECT_EQ(reply.symbolId, TEST_SYMBOL_ID_2);
}

TEST_F(ProtocolTest, FromJSON_DocumentLinkParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        }
    })"_json;

    DocumentLinkParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
}

TEST_F(ProtocolTest, FromJSON_DidChangeWatchedFilesParam_ValidInput)
{
    json params = R"({
        "changes": [
            {
                "uri": "file:///test.cj",
                "type": 1
            }
        ]
    })"_json;

    DidChangeWatchedFilesParam reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    ASSERT_EQ(reply.changes.size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply.changes[TEST_COL_0].textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(static_cast<int>(reply.changes[TEST_COL_0].type), FILE_CHANGE_CREATED);
}

TEST_F(ProtocolTest, FromJSON_DocumentSymbolParams_ValidInput)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        }
    })"_json;

    DocumentSymbolParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
}

TEST_F(ProtocolTest, ToJSON_BreakpointLocation_ValidInput)
{
    BreakpointLocation params;
    params.uri = "file:///test.cj";
    params.range.start.line = TEST_LINE_5;
    params.range.start.column = TEST_COL_10;
    params.range.end.line = TEST_LINE_5;
    params.range.end.column = TEST_COL_20;

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["uri"], "file:///test.cj");
    EXPECT_EQ(reply["range"]["start"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["range"]["start"]["character"], TEST_COL_10);
    EXPECT_EQ(reply["range"]["end"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["range"]["end"]["character"], TEST_COL_20);
}

TEST_F(ProtocolTest, ToJSON_ExecutableRange_ValidInput)
{
    ExecutableRange params;
    params.uri = "file:///test.cj";
    params.projectName = "MyProject";
    params.packageName = "com.example";
    params.className = "MyClass";
    params.functionName = "main";
    params.range.start.line = TEST_LINE_0;
    params.range.start.column = TEST_COL_0;
    params.range.end.line = TEST_LINE_10;
    params.range.end.column = TEST_COL_20;

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["uri"], "file:///test.cj");
    EXPECT_EQ(reply["projectName"], "MyProject");
    EXPECT_EQ(reply["packageName"], "com.example");
    EXPECT_EQ(reply["className"], "MyClass");
    EXPECT_EQ(reply["functionName"], "main");
    EXPECT_EQ(reply["range"]["start"]["line"], TEST_LINE_0);
    EXPECT_EQ(reply["range"]["start"]["character"], TEST_COL_0);
    EXPECT_EQ(reply["range"]["end"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["range"]["end"]["character"], TEST_COL_20);
}

TEST_F(ProtocolTest, ToJSON_CodeLens_ValidInput)
{
    CodeLens params;
    params.range.start.line = TEST_LINE_5;
    params.range.start.column = TEST_COL_10;
    params.range.end.line = TEST_LINE_5;
    params.range.end.column = TEST_COL_20;

    params.command.title = "Run Test";
    params.command.command = "run.test";

    ExecutableRange arg;
    arg.uri = "file:///test.cj";
    arg.range.start.line = TEST_LINE_0;
    arg.range.start.column = TEST_COL_0;
    arg.range.end.line = TEST_LINE_10;
    arg.range.end.column = TEST_COL_20;
    params.command.arguments.insert(arg);

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["range"]["start"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["range"]["start"]["character"], TEST_COL_10);
    EXPECT_EQ(reply["range"]["end"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["range"]["end"]["character"], TEST_COL_20);
    EXPECT_EQ(reply["command"]["title"], "Run Test");
    EXPECT_EQ(reply["command"]["command"], "run.test");
    ASSERT_EQ(reply["command"]["arguments"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["command"]["arguments"][TEST_COL_0]["uri"], "file:///test.cj");
}

TEST_F(ProtocolTest, ToJSON_TypeHierarchyItem_ValidInput)
{
    TypeHierarchyItem iter;
    iter.name = "MyClass";
    iter.kind = SymbolKind::CLASS;
    iter.uri.file = "file:///test.cj";
    iter.range.start.line = TEST_LINE_0;
    iter.range.start.column = TEST_COL_0;
    iter.range.end.line = TEST_LINE_10;
    iter.range.end.column = TEST_COL_20;
    iter.selectionRange.start.line = TEST_LINE_2;
    iter.selectionRange.start.column = TEST_COL_5;
    iter.selectionRange.end.line = TEST_LINE_2;
    iter.selectionRange.end.column = TEST_COL_15;
    iter.isKernel = true;
    iter.isChildOrSuper = false;
    iter.symbolId = TEST_SYMBOL_ID_1;

    json replyTH;
    bool result = ToJSON(iter, replyTH);

    EXPECT_TRUE(result);
    EXPECT_EQ(replyTH["name"], "MyClass");
    EXPECT_EQ(replyTH["kind"], static_cast<int>(SymbolKind::CLASS));
    EXPECT_EQ(replyTH["uri"], "file:///test.cj");
    EXPECT_EQ(replyTH["range"]["start"]["line"], TEST_LINE_0);
    EXPECT_EQ(replyTH["range"]["start"]["character"], TEST_COL_0);
    EXPECT_EQ(replyTH["range"]["end"]["line"], TEST_LINE_10);
    EXPECT_EQ(replyTH["range"]["end"]["character"], TEST_COL_20);
    EXPECT_EQ(replyTH["selectionRange"]["start"]["line"], TEST_LINE_2);
    EXPECT_EQ(replyTH["selectionRange"]["start"]["character"], TEST_COL_5);
    EXPECT_EQ(replyTH["selectionRange"]["end"]["line"], TEST_LINE_2);
    EXPECT_EQ(replyTH["selectionRange"]["end"]["character"], TEST_COL_15);
    EXPECT_TRUE(replyTH["data"]["isKernel"]);
    EXPECT_FALSE(replyTH["data"]["isChildOrSuper"]);
    EXPECT_EQ(replyTH["data"]["symbolId"], "12345");
}

TEST_F(ProtocolTest, ToJSON_CallHierarchyItem_ValidInput)
{
    CallHierarchyItem iter;
    iter.name = "myMethod";
    iter.kind = SymbolKind::FUNCTION;
    iter.uri.file = "file:///test.cj";
    iter.range.start.line = TEST_LINE_5;
    iter.range.start.column = TEST_COL_10;
    iter.range.end.line = TEST_LINE_7;
    iter.range.end.column = TEST_COL_20;
    iter.selectionRange.start.line = TEST_LINE_6;
    iter.selectionRange.start.column = TEST_COL_15;
    iter.selectionRange.end.line = TEST_LINE_6;
    iter.selectionRange.end.column = TEST_COL_25;
    iter.detail = "This is a method";
    iter.isKernel = false;
    iter.symbolId = TEST_SYMBOL_ID_2;

    json replyCH;
    bool result = ToJSON(iter, replyCH);

    EXPECT_TRUE(result);
    EXPECT_EQ(replyCH["name"], "myMethod");
    EXPECT_EQ(replyCH["kind"], static_cast<int>(SymbolKind::FUNCTION));
    EXPECT_EQ(replyCH["uri"], "file:///test.cj");
    EXPECT_EQ(replyCH["range"]["start"]["line"], TEST_LINE_5);
    EXPECT_EQ(replyCH["range"]["start"]["character"], TEST_COL_10);
    EXPECT_EQ(replyCH["range"]["end"]["line"], TEST_LINE_7);
    EXPECT_EQ(replyCH["range"]["end"]["character"], TEST_COL_20);
    EXPECT_EQ(replyCH["selectionRange"]["start"]["line"], TEST_LINE_6);
    EXPECT_EQ(replyCH["selectionRange"]["start"]["character"], TEST_COL_15);
    EXPECT_EQ(replyCH["selectionRange"]["end"]["line"], TEST_LINE_6);
    EXPECT_EQ(replyCH["selectionRange"]["end"]["character"], TEST_COL_25);
    EXPECT_EQ(replyCH["detail"], "This is a method");
    EXPECT_FALSE(replyCH["data"]["isKernel"]);
    EXPECT_EQ(replyCH["data"]["symbolId"], "67890");
}

TEST_F(ProtocolTest, ToJSON_CompletionItem_ValidInput)
{
    CompletionItem iter;
    iter.label = "myFunction";
    iter.kind = CompletionItemKind::CIK_FUNCTION;
    iter.detail = "A sample function";
    iter.documentation = "Does something useful";
    iter.sortText = "a";
    iter.filterText = "myFunc";
    iter.insertText = "myFunction()";
    iter.insertTextFormat = InsertTextFormat::SNIPPET;
    iter.deprecated = false;

    TextEdit edit;
    edit.range.start.line = TEST_LINE_0;
    edit.range.start.column = TEST_COL_0;
    edit.range.end.line = TEST_LINE_0;
    edit.range.end.column = TEST_COL_10;
    edit.newText = "replacement";
    iter.additionalTextEdits = std::vector{edit};

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["label"], "myFunction");
    EXPECT_EQ(reply["kind"], static_cast<int>(CompletionItemKind::CIK_FUNCTION));
    EXPECT_EQ(reply["detail"], "A sample function");
    EXPECT_EQ(reply["documentation"], "Does something useful");
    EXPECT_EQ(reply["sortText"], "a");
    EXPECT_EQ(reply["filterText"], "myFunc");
    EXPECT_EQ(reply["insertText"], "myFunction()");
    EXPECT_EQ(reply["insertTextFormat"], static_cast<int>(InsertTextFormat::SNIPPET));
    EXPECT_FALSE(reply["deprecated"]);
    ASSERT_TRUE(reply.contains("additionalTextEdits"));
    ASSERT_EQ(reply["additionalTextEdits"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["additionalTextEdits"][TEST_COL_0]["newText"], "replacement");
}

TEST_F(ProtocolTest, ToJSON_DiagnosticRelatedInformation_ValidInput)
{
    DiagnosticRelatedInformation info;
    info.message = "Defined here";
    info.location.uri.file = "file:///definition.cj";
    info.location.range.start.line = TEST_LINE_10;
    info.location.range.start.column = TEST_COL_5;
    info.location.range.end.line = TEST_LINE_10;
    info.location.range.end.column = TEST_COL_15;

    json reply;
    bool result = ToJSON(info, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["message"], "Defined here");
    EXPECT_EQ(reply["location"]["uri"], "file:///definition.cj");
    EXPECT_EQ(reply["location"]["range"]["start"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["location"]["range"]["start"]["character"], TEST_COL_5);
    EXPECT_EQ(reply["location"]["range"]["end"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["location"]["range"]["end"]["character"], TEST_COL_15);
}

TEST_F(ProtocolTest, ToJSON_PublishDiagnosticsParams_ValidInput)
{
    PublishDiagnosticsParams params;
    params.uri.file = "file:///test.cj";
    params.version = TEST_VERSION_1;

    DiagnosticToken diag;
    diag.range.start.line = TEST_LINE_5;
    diag.range.start.column = TEST_COL_10;
    diag.range.end.line = TEST_LINE_5;
    diag.range.end.column = TEST_COL_20;
    diag.severity = 1;
    diag.source = "compiler";
    diag.message = "Undefined variable 'x'";
    params.diagnostics.push_back(diag);

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["uri"], "file:///test.cj");
    EXPECT_EQ(reply["version"], TEST_VERSION_1);
    ASSERT_TRUE(reply.contains("diagnostics"));
    ASSERT_EQ(reply["diagnostics"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["diagnostics"][TEST_COL_0]["message"], "Undefined variable 'x'");
}

TEST_F(ProtocolTest, ToJSON_WorkspaceEdit_ValidInput)
{
    WorkspaceEdit params;

    TextEdit edit;
    edit.range.start.line = TEST_LINE_0;
    edit.range.start.column = TEST_COL_0;
    edit.range.end.line = TEST_LINE_0;
    edit.range.end.column = TEST_COL_10;
    edit.newText = "new content";

    params.changes["file:///test.cj"] = std::vector{edit};

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    ASSERT_TRUE(reply.contains("changes"));
    ASSERT_TRUE(reply["changes"].contains("file:///test.cj"));
    ASSERT_EQ(reply["changes"]["file:///test.cj"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["changes"]["file:///test.cj"][TEST_COL_0]["newText"], "new content");
}

TEST_F(ProtocolTest, ToJSON_TextDocumentEdit_ValidInput)
{
    TextDocumentEdit params;
    params.textDocument.uri.file = "file:///test.cj";
    params.textDocument.version = TEST_VERSION_1;

    TextEdit edit;
    edit.range.start.line = TEST_LINE_0;
    edit.range.start.column = TEST_COL_0;
    edit.range.end.line = TEST_LINE_0;
    edit.range.end.column = TEST_COL_10;
    edit.newText = "new content";
    params.textEdits.push_back(edit);

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["textDocument"]["uri"], "file:///test.cj");
    EXPECT_EQ(reply["textDocument"]["version"], TEST_VERSION_1);
    ASSERT_EQ(reply["edits"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["edits"][TEST_COL_0]["newText"], "new content");
}

TEST_F(ProtocolTest, ToJSON_DocumentSymbol_ValidInput)
{
    DocumentSymbol item;
    item.name = "MyClass";
    item.kind = SymbolKind::CLASS;
    item.detail = "A sample class";
    item.range.start.line = TEST_LINE_0;
    item.range.start.column = TEST_COL_0;
    item.range.end.line = TEST_LINE_10;
    item.range.end.column = TEST_COL_20;
    item.selectionRange.start.line = TEST_LINE_2;
    item.selectionRange.start.column = TEST_COL_5;
    item.selectionRange.end.line = TEST_LINE_2;
    item.selectionRange.end.column = TEST_COL_15;

    DocumentSymbol child;
    child.name = "myMethod";
    child.kind = SymbolKind::FUNCTION;
    child.range.start.line = TEST_LINE_3;
    child.range.start.column = TEST_COL_5;
    child.range.end.line = TEST_LINE_5;
    child.range.end.column = TEST_COL_15;
    child.selectionRange.start.line = TEST_LINE_4;
    child.selectionRange.start.column = TEST_COL_7;
    child.selectionRange.end.line = TEST_LINE_4;
    child.selectionRange.end.column = TEST_COL_13;
    item.children.push_back(child);

    json result;
    bool success = ToJSON(item, result);

    EXPECT_TRUE(success);
    EXPECT_EQ(result["name"], "MyClass");
    EXPECT_EQ(result["kind"], static_cast<int>(SymbolKind::CLASS));
    EXPECT_EQ(result["detail"], "A sample class");
    EXPECT_EQ(result["range"]["start"]["line"], TEST_LINE_0);
    EXPECT_EQ(result["range"]["start"]["character"], TEST_COL_0);
    EXPECT_EQ(result["range"]["end"]["line"], TEST_LINE_10);
    EXPECT_EQ(result["range"]["end"]["character"], TEST_COL_20);
    EXPECT_EQ(result["selectionRange"]["start"]["line"], TEST_LINE_2);
    EXPECT_EQ(result["selectionRange"]["start"]["character"], TEST_COL_5);
    EXPECT_EQ(result["selectionRange"]["end"]["line"], TEST_LINE_2);
    EXPECT_EQ(result["selectionRange"]["end"]["character"], TEST_COL_15);
    ASSERT_TRUE(result.contains("children"));
    ASSERT_EQ(result["children"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(result["children"][TEST_COL_0]["name"], "myMethod");
}

TEST_F(ProtocolTest, ToJSON_CallHierarchyOutgoingCall_ValidInput)
{
    CallHierarchyOutgoingCall iter;

    CallHierarchyItem toItem;
    toItem.name = "calledFunction";
    toItem.kind = SymbolKind::FUNCTION;
    toItem.uri.file = "file:///callee.cj";
    toItem.range.start.line = TEST_LINE_0;
    toItem.range.start.column = TEST_COL_0;
    toItem.range.end.line = TEST_LINE_5;
    toItem.range.end.column = TEST_COL_20;
    toItem.selectionRange.start.line = TEST_LINE_1;
    toItem.selectionRange.start.column = TEST_COL_5;
    toItem.selectionRange.end.line = TEST_LINE_1;
    toItem.selectionRange.end.column = TEST_COL_15;
    iter.to = toItem;

    Range fromRange;
    fromRange.start.line = TEST_LINE_10;
    fromRange.start.column = TEST_COL_5;
    fromRange.end.line = TEST_LINE_10;
    fromRange.end.column = TEST_COL_15;
    iter.fromRanges.push_back(fromRange);

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["to"]["name"], "calledFunction");
    ASSERT_EQ(reply["fromRanges"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["character"], TEST_COL_5);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["end"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["end"]["character"], TEST_COL_15);
}

TEST_F(ProtocolTest, ToJSON_CallHierarchyIncomingCall_ValidInput)
{
    CallHierarchyIncomingCall iter;

    CallHierarchyItem fromItem;
    fromItem.name = "callingFunction";
    fromItem.kind = SymbolKind::FUNCTION;
    fromItem.uri.file = "file:///caller.cj";
    fromItem.range.start.line = TEST_LINE_0;
    fromItem.range.start.column = TEST_COL_0;
    fromItem.range.end.line = TEST_LINE_5;
    fromItem.range.end.column = TEST_COL_20;
    fromItem.selectionRange.start.line = TEST_LINE_1;
    fromItem.selectionRange.start.column = TEST_COL_5;
    fromItem.selectionRange.end.line = TEST_LINE_1;
    fromItem.selectionRange.end.column = TEST_COL_15;
    iter.from = fromItem;

    Range fromRange;
    fromRange.start.fileID = TEST_FILE_ID_1;
    fromRange.start.line = TEST_LINE_10;
    fromRange.start.column = TEST_COL_5;
    fromRange.end.fileID = TEST_FILE_ID_1;
    fromRange.end.line = TEST_LINE_10;
    fromRange.end.column = TEST_COL_15;
    iter.fromRanges.push_back(fromRange);

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["from"]["name"], "callingFunction");
    ASSERT_EQ(reply["fromRanges"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["fileID"], TEST_FILE_ID_1);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["character"], TEST_COL_5);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["end"]["fileID"], TEST_FILE_ID_1);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["end"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["end"]["character"], TEST_COL_15);
}

TEST_F(ProtocolTest, FromJSON_DidOpenTextDocumentParams_InvalidLanguageId)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "languageId": "Java",
            "version": 1,
            "text": "fn main() {}"
        }
    })"_json;

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_DidOpenTextDocumentParams_NullFields)
{
    json params = R"({
        "textDocument": {
            "uri": null,
            "languageId": "Cangjie",
            "version": 1,
            "text": "fn main() {}"
        }
    })"_json;

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_TextDocumentPositionParams_InvalidPosition)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": null,
            "character": 5
        }
    })"_json;

    TextDocumentPositionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_SignatureHelpParams_WithoutContext)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": 10,
            "character": 5
        }
    })"_json;

    SignatureHelpParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.position.line, TEST_LINE_10);
    EXPECT_EQ(reply.position.column, TEST_COL_5);
}

TEST_F(ProtocolTest, FromJSON_InitializeParams_WithoutInitializationOptions)
{
    json params = R"({
        "rootUri": "file:///workspace",
        "capabilities": {
            "textDocument": {
                "documentHighlight": {},
                "typeHierarchy": {},
                "publishDiagnostics": {
                    "versionSupport": true
                },
                "hover": {},
                "documentLink": {}
            }
        }
    })"_json;

    InitializeParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.rootUri.file, "file:///workspace");
    EXPECT_FALSE(MessageHeaderEndOfLine::GetIsDeveco());
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.documentHighlightClientCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.typeHierarchyCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.diagnosticVersionSupport);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.hoverClientCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.documentLinkClientCapabilities);
}

TEST_F(ProtocolTest, FromJSON_InitializeParams_EmptyTextDocumentCapabilities)
{
    json params = R"({
        "rootUri": "file:///workspace",
        "capabilities": {
            "textDocument": {}
        }
    })"_json;

    InitializeParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_FALSE(reply.capabilities.textDocumentClientCapabilities.documentHighlightClientCapabilities);
    EXPECT_FALSE(reply.capabilities.textDocumentClientCapabilities.typeHierarchyCapabilities);
    EXPECT_FALSE(reply.capabilities.textDocumentClientCapabilities.diagnosticVersionSupport);
    EXPECT_FALSE(reply.capabilities.textDocumentClientCapabilities.hoverClientCapabilities);
    EXPECT_FALSE(reply.capabilities.textDocumentClientCapabilities.documentLinkClientCapabilities);
}

TEST_F(ProtocolTest, FromJSON_CompletionContext_WithTriggerCharacter)
{
    json params = R"({
        "triggerKind": 1,
        "triggerCharacter": "."
    })"_json;

    CompletionContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(static_cast<int>(reply.triggerKind), TRIGGER_KIND_INVOKED);
    EXPECT_EQ(reply.triggerCharacter, "");
}

TEST_F(ProtocolTest, FromJSON_CompletionContext_WithoutTriggerCharacter)
{
    json params = R"({
        "triggerKind": 2
    })"_json;

    CompletionContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(static_cast<int>(reply.triggerKind), TRIGGER_KIND_CHARACTER);
    EXPECT_TRUE(reply.triggerCharacter.empty());
}

TEST_F(ProtocolTest, FromJSON_CompletionParams_WithoutContext)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": 10,
            "character": 5
        }
    })"_json;

    CompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.position.line, TEST_LINE_10);
    EXPECT_EQ(reply.position.column, TEST_COL_5);
}

TEST_F(ProtocolTest, FromJSON_DidChangeTextDocumentParams_MultipleChanges)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "version": 2
        },
        "contentChanges": [
            {
                "text": "first change",
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 5}
                }
            },
            {
                "text": "second change"
            }
        ]
    })"_json;

    DidChangeTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test.cj");
    EXPECT_EQ(reply.textDocument.version, TEST_VERSION_2);
    ASSERT_EQ(reply.contentChanges.size(), EXPECTED_SIZE_2);
    EXPECT_EQ(reply.contentChanges[TEST_COL_0].text, "first change");
    EXPECT_TRUE(reply.contentChanges[TEST_COL_0].range.has_value());
    EXPECT_EQ(reply.contentChanges[TEST_LINE_1].text, "second change");
    EXPECT_FALSE(reply.contentChanges[TEST_LINE_1].range.has_value());
}

TEST_F(ProtocolTest, FromJSON_DidChangeTextDocumentParams_InvalidRange)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "version": 2
        },
        "contentChanges": [
            {
                "text": "change",
                "range": {
                    "start": {"line": null, "character": 0},
                    "end": {"line": 0, "character": 5}
                }
            }
        ]
    })"_json;

    DidChangeTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
    EXPECT_TRUE(reply.contentChanges.empty());
}

TEST_F(ProtocolTest, ToJSON_CompletionItem_WithoutOptionalFields)
{
    CompletionItem iter;
    iter.label = "myFunction";
    iter.kind = CompletionItemKind::CIK_FUNCTION;
    iter.detail = "A sample function";

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["label"], "myFunction");
    EXPECT_EQ(reply["kind"], static_cast<int>(CompletionItemKind::CIK_FUNCTION));
    EXPECT_EQ(reply["detail"], "A sample function");
    EXPECT_FALSE(reply.contains("additionalTextEdits"));
}

TEST_F(ProtocolTest, ToJSON_DiagnosticToken_WithAllOptionalFields)
{
    DiagnosticToken iter;
    iter.range.start.line = TEST_LINE_5;
    iter.range.start.column = TEST_COL_10;
    iter.range.end.line = TEST_LINE_5;
    iter.range.end.column = TEST_COL_20;
    iter.severity = 1;
    iter.code = TEST_DIAG_CODE;
    iter.source = "compiler";
    iter.message = "Undefined variable";
    iter.category = TEST_DIAG_CAT;
    iter.tags = {TEST_TAG_1, TEST_TAG_2};

    DiagnosticRelatedInformation relatedInfo;
    relatedInfo.message = "Defined here";
    relatedInfo.location.uri.file = "file:///def.cj";
    relatedInfo.location.range.start.line = TEST_LINE_10;
    relatedInfo.location.range.start.column = TEST_COL_5;
    relatedInfo.location.range.end.line = TEST_LINE_10;
    relatedInfo.location.range.end.column = TEST_COL_15;
    iter.relatedInformation = std::vector{relatedInfo};

    CodeAction action;
    action.title = "Fix import";
    action.kind = "quickfix";
    iter.codeActions = std::vector{action};

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["range"]["start"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["range"]["start"]["character"], TEST_COL_10);
    EXPECT_EQ(reply["range"]["end"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["range"]["end"]["character"], TEST_COL_20);
    EXPECT_EQ(reply["code"], TEST_DIAG_CODE);
    EXPECT_EQ(reply["source"], "compiler");
    EXPECT_EQ(reply["message"], "Undefined variable");
    EXPECT_EQ(reply["category"], TEST_DIAG_CAT);
    ASSERT_TRUE(reply.contains("tags"));
    ASSERT_EQ(reply["tags"].size(), EXPECTED_SIZE_2);
    ASSERT_TRUE(reply.contains("relatedInformation"));
    ASSERT_EQ(reply["relatedInformation"].size(), EXPECTED_SIZE_1);
    ASSERT_TRUE(reply.contains("codeActions"));
    ASSERT_EQ(reply["codeActions"].size(), EXPECTED_SIZE_1);
}

TEST_F(ProtocolTest, ToJSON_PublishDiagnosticsParams_EmptyDiagnostics)
{
    PublishDiagnosticsParams params;
    params.uri.file = "file:///test.cj";
    params.version = TEST_VERSION_1;

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["uri"], "file:///test.cj");
    EXPECT_EQ(reply["version"], TEST_VERSION_1);
    ASSERT_TRUE(reply.contains("diagnostics"));
    EXPECT_TRUE(reply["diagnostics"].is_array());
    EXPECT_TRUE(reply["diagnostics"].empty());
}

TEST_F(ProtocolTest, ToJSON_PublishDiagnosticsParams_WithoutVersion)
{
    PublishDiagnosticsParams params;
    params.uri.file = "file:///test.cj";

    DiagnosticToken diag;
    diag.range.start.line = TEST_LINE_5;
    diag.range.start.column = TEST_COL_10;
    diag.range.end.line = TEST_LINE_5;
    diag.range.end.column = TEST_COL_20;
    diag.severity = 1;
    diag.message = "Error";
    params.diagnostics.push_back(diag);

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["uri"], "file:///test.cj");
    EXPECT_FALSE(reply.contains("version"));
    ASSERT_TRUE(reply.contains("diagnostics"));
    ASSERT_EQ(reply["diagnostics"].size(), EXPECTED_SIZE_1);
}

TEST_F(ProtocolTest, ToJSON_WorkspaceEdit_MultipleChanges)
{
    WorkspaceEdit params;

    TextEdit edit1;
    edit1.range.start.line = TEST_LINE_0;
    edit1.range.start.column = TEST_COL_0;
    edit1.range.end.line = TEST_LINE_0;
    edit1.range.end.column = TEST_COL_10;
    edit1.newText = "new content 1";

    TextEdit edit2;
    edit2.range.start.line = TEST_LINE_1;
    edit2.range.start.column = TEST_COL_0;
    edit2.range.end.line = TEST_LINE_1;
    edit2.range.end.column = TEST_COL_5;
    edit2.newText = "new content 2";

    params.changes["file:///test1.cj"] = std::vector{edit1};
    params.changes["file:///test2.cj"] = std::vector{edit1, edit2};

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    ASSERT_TRUE(reply.contains("changes"));
    ASSERT_TRUE(reply["changes"].contains("file:///test1.cj"));
    ASSERT_TRUE(reply["changes"].contains("file:///test2.cj"));
    EXPECT_EQ(reply["changes"]["file:///test1.cj"].size(), EXPECTED_SIZE_1);
    EXPECT_EQ(reply["changes"]["file:///test2.cj"].size(), EXPECTED_SIZE_2);
}


TEST_F(ProtocolTest, ToJSON_DocumentSymbol_WithoutChildren)
{
    DocumentSymbol item;
    item.name = "MyFunction";
    item.kind = SymbolKind::FUNCTION;
    item.detail = "A function";
    item.range.start.line = TEST_LINE_0;
    item.range.start.column = TEST_COL_0;
    item.range.end.line = TEST_LINE_5;
    item.range.end.column = TEST_COL_20;
    item.selectionRange.start.line = TEST_LINE_1;
    item.selectionRange.start.column = TEST_COL_5;
    item.selectionRange.end.line = TEST_LINE_1;
    item.selectionRange.end.column = TEST_COL_15;

    json result;
    bool success = ToJSON(item, result);

    EXPECT_TRUE(success);
    EXPECT_EQ(result["name"], "MyFunction");
    EXPECT_EQ(result["kind"], static_cast<int>(SymbolKind::FUNCTION));
    EXPECT_EQ(result["detail"], "A function");
    EXPECT_FALSE(result.contains("children"));
}

TEST_F(ProtocolTest, ToJSON_CallHierarchyOutgoingCall_MultipleFromRanges)
{
    CallHierarchyOutgoingCall iter;

    CallHierarchyItem toItem;
    toItem.name = "callee";
    iter.to = toItem;

    Range range1;
    range1.start.line = TEST_LINE_10;
    range1.start.column = TEST_COL_5;
    range1.end.line = TEST_LINE_10;
    range1.end.column = TEST_COL_15;

    Range range2;
    range2.start.line = TEST_LINE_20;
    range2.start.column = TEST_COL_0;
    range2.end.line = TEST_LINE_20;
    range2.end.column = TEST_COL_10;

    iter.fromRanges.push_back(range1);
    iter.fromRanges.push_back(range2);

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["to"]["name"], "callee");
    ASSERT_EQ(reply["fromRanges"].size(), EXPECTED_SIZE_2);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["line"], TEST_LINE_10);
    EXPECT_EQ(reply["fromRanges"][TEST_LINE_1]["start"]["line"], TEST_LINE_20);
}

TEST_F(ProtocolTest, ToJSON_CallHierarchyIncomingCall_MultipleFromRanges)
{
    CallHierarchyIncomingCall iter;

    CallHierarchyItem fromItem;
    fromItem.name = "caller";
    iter.from = fromItem;

    Range range1;
    range1.start.fileID = TEST_FILE_ID_1;
    range1.start.line = TEST_LINE_10;
    range1.start.column = TEST_COL_5;
    range1.end.fileID = TEST_FILE_ID_1;
    range1.end.line = TEST_LINE_10;
    range1.end.column = TEST_COL_15;

    Range range2;
    range2.start.fileID = TEST_FILE_ID_2;
    range2.start.line = TEST_LINE_20;
    range2.start.column = TEST_COL_0;
    range2.end.fileID = TEST_FILE_ID_2;
    range2.end.line = TEST_LINE_20;
    range2.end.column = TEST_COL_10;

    iter.fromRanges.push_back(range1);
    iter.fromRanges.push_back(range2);

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["from"]["name"], "caller");
    ASSERT_EQ(reply["fromRanges"].size(), EXPECTED_SIZE_2);
    EXPECT_EQ(reply["fromRanges"][TEST_COL_0]["start"]["fileID"], TEST_FILE_ID_1);
    EXPECT_EQ(reply["fromRanges"][TEST_LINE_1]["start"]["fileID"], TEST_FILE_ID_2);
}

TEST_F(ProtocolTest, ToJSON_CodeLens_EmptyArguments)
{
    CodeLens params;
    params.range.start.line = TEST_LINE_5;
    params.range.start.column = TEST_COL_10;
    params.range.end.line = TEST_LINE_5;
    params.range.end.column = TEST_COL_20;

    params.command.title = "Run Test";
    params.command.command = "run.test";

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["range"]["start"]["line"], TEST_LINE_5);
    EXPECT_EQ(reply["command"]["title"], "Run Test");
    EXPECT_EQ(reply["command"]["command"], "run.test");
    ASSERT_TRUE(reply["command"].contains("arguments"));
    EXPECT_TRUE(reply["command"]["arguments"].empty());
}

TEST_F(ProtocolTest, ToJSON_TextDocumentEdit_EmptyTextEdits)
{
    TextDocumentEdit params;
    params.textDocument.uri.file = "file:///test.cj";
    params.textDocument.version = TEST_VERSION_1;

    json reply;
    bool result = ToJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["textDocument"]["uri"], "file:///test.cj");
    EXPECT_EQ(reply["textDocument"]["version"], TEST_VERSION_1);
    ASSERT_TRUE(reply.contains("edits"));
    EXPECT_TRUE(reply["edits"].empty());
}

TEST_F(ProtocolTest, FromJSON_SignatureHelpContext_EmptySignatures)
{
    json params = R"({
        "triggerKind": 1,
        "activeSignatureHelp": {
            "activeSignature": 0,
            "activeParameter": 1,
            "signatures": []
        }
    })"_json;

    SignatureHelpContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.activeSignatureHelp.activeSignature, TEST_LINE_0);
    EXPECT_EQ(reply.activeSignatureHelp.activeParameter, TEST_LINE_1);
    EXPECT_TRUE(reply.activeSignatureHelp.signatures.empty());
}

TEST_F(ProtocolTest, FromJSON_InitializeParams_EmptyCangjieRootUri)
{
    json params = R"({
        "rootUri": "",
        "capabilities": {
            "textDocument": {}
        },
        "initializationOptions": {
            "cangjieRootUri": ""
        }
    })"_json;

    InitializeParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.rootUri.file, "");
    EXPECT_TRUE(MessageHeaderEndOfLine::GetIsDeveco());
}

TEST_F(ProtocolTest, FromJSON_DidChangeTextDocumentParams_EmptyContentChanges)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "version": 2
        },
        "contentChanges": []
    })"_json;

    DidChangeTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_DidChangeTextDocumentParams_InvalidContentChanges)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj",
            "version": 2
        },
        "contentChanges": [
            {
                "text": null
            }
        ]
    })"_json;

    DidChangeTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_RenameParams_InvalidPosition)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": null,
            "character": 5
        },
        "newName": "newName"
    })"_json;

    RenameParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_TypeHierarchyItem_EmptySymbolId)
{
    json params = R"({
        "item": {
            "name": "MyClass",
            "kind": 5,
            "uri": "file:///test.cj",
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 10, "character": 20}
            },
            "selectionRange": {
                "start": {"line": 2, "character": 5},
                "end": {"line": 2, "character": 15}
            },
            "data": {
                "isKernel": true,
                "isChildOrSuper": false,
                "symbolId": ""
            }
        }
    })"_json;

    TypeHierarchyItem reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.name, "MyClass");
    EXPECT_EQ(reply.symbolId, 0ull);
}

TEST_F(ProtocolTest, FromJSON_CallHierarchyItem_EmptySymbolId)
{
    json params = R"({
        "item": {
            "name": "myMethod",
            "kind": 6,
            "uri": "file:///test.cj",
            "range": {
                "start": {"line": 5, "character": 10},
                "end": {"line": 7, "character": 20}
            },
            "selectionRange": {
                "start": {"line": 6, "character": 15},
                "end": {"line": 6, "character": 25}
            },
            "detail": "This is a method",
            "data": {
                "isKernel": false,
                "symbolId": ""
            }
        }
    })"_json;

    CallHierarchyItem reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.name, "myMethod");
    EXPECT_EQ(reply.symbolId, 0ull);
}

TEST_F(ProtocolTest, FromJSON_DidChangeWatchedFilesParam_EmptyChanges)
{
    json params = R"({
        "changes": []
    })"_json;

    DidChangeWatchedFilesParam reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_TRUE(reply.changes.empty());
}

TEST_F(ProtocolTest, ToJSON_CompletionItem_EmptyOptionalVectors)
{
    CompletionItem iter;
    iter.label = "test";
    iter.kind = CompletionItemKind::CIK_FUNCTION;

    iter.additionalTextEdits = std::vector<TextEdit>{};

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["label"], "test");
    EXPECT_TRUE(reply.contains("additionalTextEdits"));
}

TEST_F(ProtocolTest, ToJSON_DiagnosticToken_EmptyOptionalVectors)
{
    DiagnosticToken iter;
    iter.range.start.line = TEST_LINE_5;
    iter.range.start.column = TEST_COL_10;
    iter.range.end.line = TEST_LINE_5;
    iter.range.end.column = TEST_COL_20;
    iter.severity = 1;
    iter.message = "test";

    iter.tags = {};
    iter.relatedInformation = std::vector<DiagnosticRelatedInformation>{};
    iter.codeActions = std::vector<CodeAction>{};

    json reply;
    bool result = ToJSON(iter, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply["message"], "test");
    EXPECT_FALSE(reply.contains("tags"));
    EXPECT_FALSE(reply.contains("relatedInformation"));
    EXPECT_TRUE(reply.contains("codeActions"));
}

TEST_F(ProtocolTest, MessageHeaderEndOfLine_StaticMethods)
{
    EXPECT_FALSE(MessageHeaderEndOfLine::GetIsDeveco());

    MessageHeaderEndOfLine::SetIsDeveco(true);
    EXPECT_TRUE(MessageHeaderEndOfLine::GetIsDeveco());

    MessageHeaderEndOfLine::SetIsDeveco(false);
    EXPECT_FALSE(MessageHeaderEndOfLine::GetIsDeveco());
}

TEST_F(ProtocolTest, FromJSON_TextDocumentIdentifier_NullUri)
{
    json params = R"({
        "uri": null
    })"_json;

    TextDocumentIdentifier reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FetchTextDocument_NullCompletionItem)
{
    json textDocument = R"({
        "completion": {
            "completionItem": null
        }
    })"_json;

    InitializeParams params;
    bool result = FetchTextDocument(textDocument, params);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FetchTextDocument_EmptyCapabilities)
{
    json textDocument = json::object();

    InitializeParams params;
    bool result = FetchTextDocument(textDocument, params);

    EXPECT_TRUE(result);
    EXPECT_FALSE(params.capabilities.textDocumentClientCapabilities.documentHighlightClientCapabilities);
    EXPECT_FALSE(params.capabilities.textDocumentClientCapabilities.typeHierarchyCapabilities);
    EXPECT_FALSE(params.capabilities.textDocumentClientCapabilities.diagnosticVersionSupport);
    EXPECT_FALSE(params.capabilities.textDocumentClientCapabilities.hoverClientCapabilities);
    EXPECT_FALSE(params.capabilities.textDocumentClientCapabilities.documentLinkClientCapabilities);
}

TEST_F(ProtocolTest, FromJSON_EdgeCaseNumericValues)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test.cj"
        },
        "position": {
            "line": -1,
            "character": 0
        }
    })"_json;

    TextDocumentPositionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.position.line, INVALID_TRIGGER_KIND);
    EXPECT_EQ(reply.position.column, TEST_COL_0);
}

TEST_F(ProtocolTest, FromJSON_SpecialCharacters)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///test with spaces.cj",
            "languageId": "Cangjie",
            "version": 1,
            "text": "line1\nline2\tline3\"quoted\""
        }
    })"_json;

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///test with spaces.cj");
    EXPECT_EQ(reply.textDocument.text, "line1\nline2\tline3\"quoted\"");
}

TEST_F(ProtocolTest, FromJSON_UnicodeCharacters)
{
    json params = R"({
        "textDocument": {
            "uri": "file:///测试.cj",
            "languageId": "Cangjie",
            "version": 1,
            "text": "中文测试 🚀"
        }
    })"_json;

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, "file:///测试.cj");
    EXPECT_EQ(reply.textDocument.text, "中文测试 🚀");
}

TEST_F(ProtocolTest, FromJSON_DidOpen_NotObject)
{
    json params = R"({"textDocument": []})"_json;
    DidOpenTextDocumentParams reply;
    EXPECT_FALSE(FromJSON(params, reply));
}

TEST_F(ProtocolTest, FromJSON_DidOpen_NullFields)
{
    DidOpenTextDocumentParams reply;
    EXPECT_FALSE(
        FromJSON(R"({"textDocument": {"uri":null, "languageId":"Cangjie", "version":1, "text":""}})"_json, reply));
    EXPECT_FALSE(FromJSON(R"({"textDocument": {"uri":"a", "languageId":null, "version":1, "text":""}})"_json, reply));
    EXPECT_FALSE(
        FromJSON(R"({"textDocument": {"uri":"a", "languageId":"Cangjie", "version":null, "text":""}})"_json, reply));
    EXPECT_FALSE(
        FromJSON(R"({"textDocument": {"uri":"a", "languageId":"Cangjie", "version":1, "text":null}})"_json, reply));
}

TEST_F(ProtocolTest, FromJSON_DidOpen_WrongLanguage)
{
    json params = R"({"textDocument": {"uri":"a", "languageId":"java", "version":1, "text":""}})"_json;
    DidOpenTextDocumentParams reply;
    EXPECT_FALSE(FromJSON(params, reply));
}

TEST_F(ProtocolTest, FromJSON_Position_Invalid)
{
    TextDocumentPositionParams reply;
    EXPECT_FALSE(FromJSON(R"({"textDocument":{"uri":"a"}, "position":123})"_json, reply));
    EXPECT_FALSE(FromJSON(R"({"textDocument":{"uri":"a"}, "position":{"line":null, "character":1}})"_json, reply));
    EXPECT_FALSE(FromJSON(R"({"textDocument":{"uri":"a"}, "position":{"line":1, "character":null}})"_json, reply));
}

TEST_F(ProtocolTest, FromJSON_Initialize_MissingFields)
{
    InitializeParams reply;
    EXPECT_FALSE(FromJSON(R"({"rootUri":"a"})"_json, reply));

    EXPECT_TRUE(FromJSON(R"({"rootUri":"a", "capabilities":{"textDocument":null}})"_json, reply));

    json js = R"({
        "rootUri":"a",
        "capabilities":{
            "textDocument":{
                "completion":{"completionItem": null}
            }
        }
    })"_json;
    EXPECT_FALSE(FromJSON(js, reply));
}

TEST_F(ProtocolTest, FromJSON_DidChange_EmptyChanges)
{
    json params = R"({
        "textDocument": {"uri":"a", "version":1},
        "contentChanges": []
    })"_json;
    DidChangeTextDocumentParams reply;
    EXPECT_FALSE(FromJSON(params, reply));
}
