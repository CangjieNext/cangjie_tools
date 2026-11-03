#!/usr/bin/env python3
# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

"""Check filesystem events and automatic internal-import diagnostic refresh.

Run in WSL with the Cangjie SDK environment loaded:
    python3 test/integration/internal_import_child_lifecycle.py output/bin/LSPServer

Each event is synchronized with a protocol response or expected publication.
No parent edit/request is sent after deleting the last child package.
"""

import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import tempfile
import threading
import time


class Client:
    def __init__(self, server, stderr):
        self.process = subprocess.Popen(
            [str(server), "--test"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr
        )
        self.incoming = queue.Queue()
        self.messages = []
        self.request_id = 0
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        try:
            while True:
                line = self.process.stdout.readline()
                if not line:
                    self.incoming.put(EOFError("LSP server closed stdout"))
                    return
                if not line.lower().startswith(b"content-length:"):
                    continue
                length = int(line.split(b":", 1)[1])
                while self.process.stdout.readline().strip():
                    pass
                self.incoming.put(json.loads(self.process.stdout.read(length)))
        except Exception as error:
            self.incoming.put(error)

    def send(self, method, params, request=False):
        message = {"jsonrpc": "2.0", "method": method, "params": params}
        if request:
            self.request_id += 1
            message["id"] = self.request_id
        data = json.dumps(message).encode("utf-8")
        self.process.stdin.write(f"Content-Length: {len(data)}\r\n\r\n".encode() + data)
        self.process.stdin.flush()
        return message.get("id")

    def wait(self, predicate, timeout=30):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                message = self.incoming.get(timeout=max(0.01, deadline - time.monotonic()))
            except queue.Empty:
                break
            if isinstance(message, Exception):
                raise message
            self.messages.append(message)
            if predicate(message):
                return message
        raise AssertionError(f"Expected LSP message missing (server status {self.process.poll()})")

    def request(self, method, params):
        request_id = self.send(method, params, request=True)
        response = self.wait(lambda message: message.get("id") == request_id)
        assert "error" not in response, response
        return response

    def open(self, path, contents):
        self.send("textDocument/didOpen", {"textDocument": {
            "uri": path.as_uri(), "languageId": "Cangjie", "version": 1, "text": contents
        }})

    def semantic(self, path):
        self.request("textDocument/semanticTokens/full", {"textDocument": {"uri": path.as_uri()}})

    def watched(self, path, kind):
        self.send("workspace/didChangeWatchedFiles", {"changes": [{"uri": path.as_uri(), "type": kind}]})

    def diagnostics(self, path):
        for message in reversed(self.messages):
            if is_diagnostic(message, path):
                return message["params"]["diagnostics"]
        raise AssertionError(f"No diagnostics for {path}")

    def close(self):
        try:
            self.request("shutdown", {})
            self.send("exit", {})
            self.process.wait(timeout=5)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()


def is_diagnostic(message, path):
    return (message.get("method") == "textDocument/publishDiagnostics"
            and message["params"]["uri"] == path.as_uri())


def unused_imports(diagnostics):
    return [item for item in diagnostics if item.get("message", "").startswith("unused import 'std.fs.File'")]


def assert_diagnostics(diagnostics, line=None):
    assert not [item for item in diagnostics if item.get("severity") == 1], diagnostics
    imports = unused_imports(diagnostics)
    if line is None:
        assert not imports, diagnostics
    else:
        assert len(imports) == 1, diagnostics
        assert imports[0]["tags"] == [1], imports
        assert imports[0]["severity"] == 4, imports
        assert imports[0]["range"] == {
            "start": {"line": line, "character": 0}, "end": {"line": line, "character": 27}
        }, imports


def run(server, project, stderr):
    src = project / "src"
    src.mkdir()
    main_file = src / "main.cj"
    helper = src / "helper.cj"
    main_contents = "package import_lifecycle\ninternal import std.fs.File\nmain(): Int64 { 0 }\n"
    helper_contents = "package import_lifecycle\ninternal import std.fs.File\npublic func marker(): Int64 { 2 }\n"
    main_file.write_text(main_contents, encoding="utf-8")
    helper.write_text(helper_contents, encoding="utf-8")
    parents = {main_file: 1, helper: 1}
    client = Client(server, stderr)

    def check_parents(gray):
        for path, line in parents.items():
            client.semantic(path)
            assert_diagnostics(client.diagnostics(path), line if gray else None)

    def await_parent_refresh():
        pending = set(parents)

        def refreshed(message):
            for path in tuple(pending):
                if is_diagnostic(message, path) and unused_imports(message["params"]["diagnostics"]):
                    assert_diagnostics(message["params"]["diagnostics"], parents[path])
                    pending.remove(path)
            return not pending

        client.wait(refreshed, timeout=15)

    def add_file(path, function):
        path.parent.mkdir(parents=True, exist_ok=True)
        contents = f"package import_lifecycle.{path.parent.name}\npublic func {function}(): Int64 {{ 1 }}\n"
        path.write_text(contents, encoding="utf-8")
        client.watched(path, 1)
        client.open(path, contents)
        client.semantic(path)

    def delete_file(path, directory_event=False):
        # Keep the deleted child as the edited document so the active-file fallback cannot refresh a parent.
        client.request("textDocument/documentLink", {"textDocument": {"uri": path.as_uri()}})
        path.unlink()
        client.send("textDocument/didClose", {"textDocument": {"uri": path.as_uri()}})
        if directory_event:
            path.parent.rmdir()
        client.watched(path.parent if directory_event else path, 3)

    try:
        client.request("initialize", {
            "processId": os.getpid(), "rootUri": project.as_uri(), "rootPath": str(project),
            "capabilities": {"workspace": {}, "textDocument": {
                "documentLink": {}, "semanticTokens": {"requests": {"full": True},
                    "tokenTypes": [], "tokenModifiers": [], "formats": ["relative"]}}},
            "initializationOptions": {"multiModuleOption": {project.as_uri(): {
                "name": "import_lifecycle", "requires": {}}}},
            "workspaceFolders": [{"uri": project.as_uri(), "name": "import_lifecycle"}]
        })
        client.send("initialized", {})
        client.open(main_file, main_contents)
        client.open(helper, helper_contents)
        check_parents(gray=True)
        print("PASS: unused internal import is gray without child packages", flush=True)

        child = src / "child" / "child.cj"
        add_file(child, "childValue")
        check_parents(gray=False)
        delete_file(child, directory_event=True)
        await_parent_refresh()
        print("PASS: deleting the child directory automatically refreshes both parent files", flush=True)

        second = child.parent / "second.cj"
        sibling = src / "sibling" / "sibling.cj"
        add_file(child, "childValue")
        add_file(second, "secondValue")
        add_file(sibling, "siblingValue")
        check_parents(gray=False)
        delete_file(second)
        check_parents(gray=False)
        print("PASS: deleting one file of a child package keeps imports exported", flush=True)
        delete_file(child, directory_event=True)
        check_parents(gray=False)
        print("PASS: deleting one child package keeps imports exported while a sibling remains", flush=True)
        delete_file(sibling)
        await_parent_refresh()
        assert sibling.parent.is_dir()
        print("PASS: deleting the last child file refreshes parents even when its directory remains", flush=True)

        add_file(child, "childValue")
        check_parents(gray=False)
        unsaved = main_contents.replace("internal import", "\ninternal import")
        parents[main_file] = 2
        client.send("textDocument/didChange", {"textDocument": {"uri": main_file.as_uri(), "version": 2},
            "contentChanges": [{"text": unsaved}]})
        delete_file(child)
        await_parent_refresh()
        assert main_file.read_text(encoding="utf-8") == main_contents
        print("PASS: repeated add/delete uses unsaved parent buffers and preserves diagnostic ranges", flush=True)
    finally:
        client.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server", type=Path, help="Path to the built LSPServer executable")
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="lsp-internal-import-") as temporary:
        project = Path(temporary)
        with tempfile.TemporaryFile() as stderr:
            try:
                run(server, project, stderr)
            except Exception:
                stderr.seek(0)
                print(stderr.read().decode("utf-8", errors="replace"))
                raise


if __name__ == "__main__":
    main()
