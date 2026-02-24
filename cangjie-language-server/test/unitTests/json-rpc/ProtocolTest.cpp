// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "Protocol.h"

using json = nlohmann::json;
using namespace ark;

namespace
{
// Constants for magic numbers to improve readability
const int EXPECTED_VERSION = 1;
const int UPDATED_VERSION = 2;
const int TEST_LINE = 10;
const int TEST_COLUMN = 5;
const int RANGE_END_LINE = 10;
const int RANGE_END_COLUMN = 20;
const int SELECTION_START_LINE = 2;
const int SELECTION_START_COLUMN = 5;
const int SELECTION_END_LINE = 2;
const int SELECTION_END_COLUMN = 15;
const int DIAGNOSTIC_SEVERITY_ERROR = 1;
const int TRIGGER_KIND_INVOKED = 1;
const int TRIGGER_KIND_CHARACTER = 2;
const uint64_t TEST_SYMBOL_ID_1 = 12345ull;
const uint64_t TEST_SYMBOL_ID_2 = 67890ull;
const int FILE_ID_ONE = 1;
const int FILE_ID_TWO = 2;
const int SYMBOL_KIND_CLASS_VAL = 5;
const int SYMBOL_KIND_FUNCTION_VAL = 6;
}

class ProtocolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Reset global state before each test
        MessageHeaderEndOfLine::SetIsDeveco(false);
    }
};

// Test case for DidOpenTextDocumentParams FromJSON
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
    EXPECT_EQ(reply.textDocument.version, EXPECTED_VERSION);
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
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
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
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
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
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
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
    EXPECT_EQ(reply.textDocument.version, UPDATED_VERSION);
    ASSERT_EQ(reply.contentChanges.size(), 1u);
    EXPECT_EQ(reply.contentChanges[0].text, "updated text");
    EXPECT_EQ(reply.contentChanges[0].range->start.line, 0);
    EXPECT_EQ(reply.contentChanges[0].range->start.column, 0);
    EXPECT_EQ(reply.contentChanges[0].range->end.line, 0);
    EXPECT_EQ(reply.contentChanges[0].range->end.column, 10);
    EXPECT_EQ(reply.contentChanges[0].rangeLength, 10);
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
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
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