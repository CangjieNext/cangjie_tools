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

namespace {
const int EXPECTED_VERSION = 1;
const int UPDATED_VERSION = 2;
const int TEST_LINE = 10;
const int TEST_COLUMN = 5;
const int START_POS_ZERO = 0;
const int END_POS_TEN = 10;
const int RANGE_LEN_TEN = 10;
const int TRIGGER_KIND_INVOKED = 1;
const int TRIGGER_KIND_CHARACTER = 2;
const int INVALID_TRIGGER_KIND = -1;

const char* const TEST_URI = "file:///test.cj";
const char* const WORKSPACE_URI = "file:///workspace";
const char* const CUSTOM_ROOT_URI = "file:///custom_root";
const char* const LANG_ID_CANGJIE = "Cangjie";
}

class ProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        MessageHeaderEndOfLine::SetIsDeveco(false);
    }
};

// Test case for DidOpenTextDocumentParams FromJSON
TEST_F(ProtocolTest, FromJSON_DidOpenTextDocumentParams_ValidInput) {
    json params = {
        {"textDocument", {
            {"uri", TEST_URI},
            {"languageId", LANG_ID_CANGJIE},
            {"version", EXPECTED_VERSION},
            {"text", "fn main() {}"}
        }}
    };

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
    EXPECT_EQ(reply.textDocument.languageId, LANG_ID_CANGJIE);
    EXPECT_EQ(reply.textDocument.version, EXPECTED_VERSION);
    EXPECT_EQ(reply.textDocument.text, "fn main() {}");
}

TEST_F(ProtocolTest, FromJSON_DidOpenTextDocumentParams_MissingFields) {
    json params = {
        {"textDocument", {
            {"uri", TEST_URI},
            {"languageId", LANG_ID_CANGJIE}
        }}
    };

    DidOpenTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_TextDocumentPositionParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}},
        {"position", {
            {"line", TEST_LINE},
            {"character", TEST_COLUMN}
        }}
    };

    TextDocumentPositionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
}

TEST_F(ProtocolTest, FromJSON_TextDocumentPositionParams_InvalidStructure) {
    json params = {
        {"textDocument", "invalid"},
        {"position", {
            {"line", TEST_LINE},
            {"character", TEST_COLUMN}
        }}
    };

    TextDocumentPositionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_SignatureHelpContext_InvalidTriggerKind) {
    json params = {
        {"triggerKind", INVALID_TRIGGER_KIND}
    };

    SignatureHelpContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_SignatureHelpParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}},
        {"position", {
            {"line", TEST_LINE},
            {"character", TEST_COLUMN}
        }},
        {"context", {
            {"triggerKind", TRIGGER_KIND_INVOKED}
        }}
    };

    SignatureHelpParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
}

TEST_F(ProtocolTest, FromJSON_InitializeParams_ValidInput) {
    json params = {
        {"rootUri", WORKSPACE_URI},
        {"capabilities", {
            {"textDocument", {
                {"documentHighlight", json::object()},
                {"typeHierarchy", json::object()},
                {"publishDiagnostics", {{"versionSupport", true}}},
                {"hover", json::object()},
                {"documentLink", json::object()}
            }}
        }},
        {"initializationOptions", {
            {"cangjieRootUri", CUSTOM_ROOT_URI}
        }}
    };

    InitializeParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.rootUri.file, CUSTOM_ROOT_URI);
    EXPECT_TRUE(MessageHeaderEndOfLine::GetIsDeveco());
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.documentHighlightClientCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.typeHierarchyCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.diagnosticVersionSupport);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.hoverClientCapabilities);
    EXPECT_TRUE(reply.capabilities.textDocumentClientCapabilities.documentLinkClientCapabilities);
}

TEST_F(ProtocolTest, FromJSON_DidCloseTextDocumentParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}}
    };

    DidCloseTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
}

TEST_F(ProtocolTest, FromJSON_TrackCompletionParams_ValidInput) {
    json params = {{"label", "myFunction"}};

    TrackCompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.label, "myFunction");
}

TEST_F(ProtocolTest, FromJSON_TrackCompletionParams_MissingLabel) {
    json params = {{"otherField", "value"}};

    TrackCompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_FALSE(result);
}

TEST_F(ProtocolTest, FromJSON_CompletionContext_ValidInput) {
    json params = {
        {"triggerKind", TRIGGER_KIND_CHARACTER},
        {"triggerCharacter", "."}
    };

    CompletionContext reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(static_cast<int>(reply.triggerKind), TRIGGER_KIND_CHARACTER);
    EXPECT_EQ(reply.triggerCharacter, ".");
}

TEST_F(ProtocolTest, FromJSON_CompletionParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}},
        {"position", {
            {"line", TEST_LINE},
            {"character", TEST_COLUMN}
        }},
        {"context", {
            {"triggerKind", TRIGGER_KIND_INVOKED}
        }}
    };

    CompletionParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
    EXPECT_EQ(static_cast<int>(reply.context.triggerKind), TRIGGER_KIND_INVOKED);
}

TEST_F(ProtocolTest, FromJSON_SemanticTokensParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}}
    };

    SemanticTokensParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
}

TEST_F(ProtocolTest, FromJSON_DidChangeTextDocumentParams_ValidInput) {
    json params = {
        {"textDocument", {
            {"uri", TEST_URI},
            {"version", UPDATED_VERSION}
        }},
        {"contentChanges", {{
            {"text", "updated text"},
            {"range", {
                {"start", {{"line", START_POS_ZERO}, {"character", START_POS_ZERO}}},
                {"end", {{"line", START_POS_ZERO}, {"character", END_POS_TEN}}}
            }},
            {"rangeLength", RANGE_LEN_TEN}
        }}}
    };

    DidChangeTextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
    EXPECT_EQ(reply.textDocument.version, UPDATED_VERSION);
    ASSERT_EQ(reply.contentChanges.size(), 1u);
    EXPECT_EQ(reply.contentChanges[0].text, "updated text");
    EXPECT_EQ(reply.contentChanges[0].range->start.line, START_POS_ZERO);
    EXPECT_EQ(reply.contentChanges[0].range->start.column, START_POS_ZERO);
    EXPECT_EQ(reply.contentChanges[0].range->end.line, START_POS_ZERO);
    EXPECT_EQ(reply.contentChanges[0].range->end.column, END_POS_TEN);
    EXPECT_EQ(reply.contentChanges[0].rangeLength, RANGE_LEN_TEN);
}

TEST_F(ProtocolTest, FromJSON_RenameParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}},
        {"position", {
            {"line", TEST_LINE},
            {"character", TEST_COLUMN}
        }},
        {"newName", "newVarName"}
    };

    RenameParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
    EXPECT_EQ(reply.position.line, TEST_LINE);
    EXPECT_EQ(reply.position.column, TEST_COLUMN);
    EXPECT_EQ(reply.newName, "newVarName");
}

TEST_F(ProtocolTest, FromJSON_TextDocumentIdentifier_ValidInput) {
    json params = {{"uri", TEST_URI}};

    TextDocumentIdentifier reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.uri.file, TEST_URI);
}

TEST_F(ProtocolTest, FromJSON_TextDocumentParams_ValidInput) {
    json params = {
        {"textDocument", {{"uri", TEST_URI}}}
    };

    TextDocumentParams reply;
    bool result = FromJSON(params, reply);

    EXPECT_TRUE(result);
    EXPECT_EQ(reply.textDocument.uri.file, TEST_URI);
}