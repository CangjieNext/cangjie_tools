// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import cjpm.util.*
import std.regex.*

public class CliNamespace {
    private let storage: HashMap<String, Any> = HashMap<String, Any>()

    public var helpText: ?String = None
    
    public func get<T>(key!: String): ?T {
        match (storage.get(key)) {
            case Some(v: T) => Some(v)
            case _ => None<T>
        }
    }

    public func set<T>(key!: String, value!: T): Unit {
        storage[key] = value
    }

    public func getArray<T>(key!: String): ?Array<T> {
        match (storage.get(key)) {
            case Some(v: ArrayList<Any>) => 
                try {
                    Some(v.iterator().map { it => (it as T).getOrThrow() } |> collectArray)
                } catch (_: Exception) {
                    return None<Array<T>>
                }
            case _ => None<Array<T>>
        }
    }

    public func append<T>(key!: String, value!: T): Unit {
        if (let Some(arr: ArrayList<Any>) <- storage.get(key)) {
            arr.add(value)
        } else {
            storage[key] = ArrayList<Any>([value])
        }
    }
}

public enum ParseError {
    // name, isShort, hint
    | UnknownOption(String, Bool, String)
    // name, hint
    | UnknownSubcommand(String, String)
    // name
    | InvalidToken(String)
    // name, isShort
    | MissingValue(String, Bool)
    // name, isShort, value, hint
    | AmbiguousValue(String, Bool, String, String)
    // name, isShort
    | BooleanOptionWithEq(String, Bool)
    // name, isShort, error message
    | InvalidValue(String, Bool, String)
    // name, isShort, error message
    | MalformedOptionName(String, Bool, String)

    public prop name: String { 
        get() { match(this) {
            case ParseError.UnknownOption(n, _, _) => n
            case ParseError.UnknownSubcommand(n, _) => n
            case ParseError.InvalidToken(n) => n
            case ParseError.MissingValue(n, _) => n
            case ParseError.AmbiguousValue(n, _, _, _) => n
            case ParseError.BooleanOptionWithEq(n, _) => n
            case ParseError.InvalidValue(n, _, _) => n
            case ParseError.MalformedOptionName(n, _, _) => n
        } }
    }

    public prop isShort: Bool { 
        get() { match(this) {
            case ParseError.UnknownOption(_, s, _) => s
            case ParseError.UnknownSubcommand(_, _) => false
            case ParseError.InvalidToken(_) => false
            case ParseError.MissingValue(_, s) => s
            case ParseError.AmbiguousValue(_, s, _, _) => s
            case ParseError.BooleanOptionWithEq(_, s) => s
            case ParseError.InvalidValue(_, s, _) => s
            case ParseError.MalformedOptionName(_, s, _) => s
        } }
    }

    public prop hint: String { 
        get() { match(this) {
            case ParseError.UnknownOption(_, _, h) => h
            case ParseError.UnknownSubcommand(_, h) => h
            case ParseError.InvalidToken(_) => ""
            case ParseError.MissingValue(_, _) => ""
            case ParseError.AmbiguousValue(_, _, _, h) => h
            case ParseError.BooleanOptionWithEq(_, _) => ""
            case ParseError.InvalidValue(_, _, _) => ""
            case ParseError.MalformedOptionName(_, _, _) => ""
        } }
    }
}

public class OptionSpec {
    public OptionSpec(
        public let option: CliOptionBase,
        public var isShort!: Bool = false,
        public var hasEq!: Bool = false,
        public var value!: ?String = None,
    ) { }
}

public class ParseResult {
    public ParseResult(
        // subcommand relatively to which option resolution occured
        public let scope: Array<String>,
        // resolved name-value bindings for options
        public let namespace: CliNamespace,
        // `values which were not consumed by options`
        public let positionals: Array<String>,
        // if `allowLateBinding == true`, contains unresolved `--long` options
        public let unresolvedOptions: Array<String>,
        // after `--`
        public let finalArgs: Array<String>,
        public let errors: ArrayList<ParseError>,
    ) { }
}

public class CliParser {
    private var args = Array<String>()
    private var pos = 0
    private var currentScope: CliScope
    private var currentPath = ArrayList<String>()
    private var specs = ArrayList<OptionSpec>()
    private var positionals = ArrayList<String>()
    private var unresolvedOptions = ArrayList<String>()
    private var errors = ArrayList<ParseError>()

    public CliParser(
        private let rootScope: CliScope,
    ) { 
        this.currentScope = rootScope
        this.currentPath.add(rootScope.name)
    }

    public func parse(args: Array<String>): ParseResult {
        this.args = args

        resolveScope()

        if (errors.size == 0) {
            parseOptions()
        }

        let result = ParseResult(
            currentPath.toArray(),
            applyOptions(),
            this.positionals.toArray(),
            this.unresolvedOptions.toArray(),
            args[pos..],
            this.errors
        )

        reinit()
        return result
    }

    private func reinit(): Unit {
        this.args = []
        this.pos = 0
        this.specs = ArrayList()
        this.positionals = ArrayList()
        this.unresolvedOptions = ArrayList()
        this.currentScope = rootScope
        this.currentPath = ArrayList()
        this.errors = ArrayList()
    }
    
    private func atEnd(): Bool {
        pos >= args.size
    }

    private func peek(): ?String {
        if (atEnd()) { return None }
        args[pos]
    }

    private func next(): ?String {
        let result = peek()
        pos += 1
        result
    }

    private func peekPositional(): ?String {
        peek().filter { it => it == "-" || !it.startsWith("-") }
    }

    private func nextPositional(): ?String {
        peekPositional()
            .map { it => next(); it }
    }

    private func peekValue(): ?String {
        peek().filter { it => it != "--" }
    }

    private func nextFinal(): ?String {
        peek()
            .filter { it => it == "--" }.map { it => next(); it }
    }

    private func nextShortOptions(): ?String {
        peek()
            .filter { it => it.startsWith("-") && !it.startsWith("--") }
            .filter { it => it != "-" && it != "-=" }
            .map { it => next(); it }
    }

    private func nextLongOption(): ?String {
        peek()
            .filter { it => it.startsWith("--") }
            .filter { it => it != "--" && it != "--=" }
            .map { it => next(); it }
    }

    private func resolveScope(): Unit {
        // Root scope name can be ommitted
        // Both `["cjpm", "build", "--verbose"]` and `["build", "--verbose"]` will work.
        if (peekPositional() == rootScope.name) { next() }
        
        // All positional arguments need to be provided after final `--`.
        // 
        // Currently, scope resoltion does not support syntax like:
        // `["root", "--rflag", "subcommand", "--sflag"]` // [scope: root]
        // 
        // Only:
        // `["root", "subcommand", "--sflag"]` // [scope: root-subcommand]
        //
        // However, anticipating that such API might emerge, 
        // The option parsing after scope resolution will consider it possible
        while (let Some(subscope) <- nextPositional()) {
            match (currentScope.subScopes.get(subscope)) {
                case Some(sub) =>
                    currentPath.add(subscope)
                    currentScope = sub
                case None =>
                    if (currentScope.allowPositionals) {
                        positionals.add(subscope)
                    } else {
                        let hint = ""
                        errors.add(ParseError.UnknownSubcommand(subscope, hint))
                    }
                    break
            }
        }
    }
    
    private func parseOptions(): Unit {
        if (atEnd() && currentScope.helpNoArgs) {
            specs.add(findOption("help", isShort: false) ?? return ())
        }

        while (!atEnd()) {
            if (nextFinal().isSome()) { break }
            var validToken: ?Unit =
            nextPositional().map { it =>
                if (currentScope.allowPositionals) {
                    positionals.add(it)
                } else {
                    let hint = ""
                    errors.add(ParseError.UnknownSubcommand(it, hint))
                }
            } ??
            nextLongOption().map { it => parseLongOption(it) } ??
            nextShortOptions().map { it => parseShortOptions(it) }
            if (validToken.isNone()) {
                errors.add(ParseError.InvalidToken(next() ?? ""))
            }
        }
    }

    private func checkMalformedLongOption(name: String): Bool {
        if (Regex("\\s").find(name).isSome()) {
            errors.add(ParseError.MalformedOptionName(name, false, "cannot contain whitespace characters"))
            return false
        }

        if (Regex("^_*[a-zA-Z][a-zA-Z0-9_-]*$").find(name).isNone()) {
            errors.add(ParseError.MalformedOptionName(name, false, "must start with a letter (a-z or A-Z) and may contain only letters, digits, hyphens (-), and underscores (_)."))
            return false
        }
        return true
    }
    
    private func parseLongOption(arg: String): Unit {
        let raw = arg[2..]
        let eq = raw.indexOf("=")
        let (optName, value) = match (eq) {
            case Some(idx) => (raw[0..idx], Some(raw[idx+1..]))
            case None => (raw, None<String>)
        }

        if (!checkMalformedLongOption(optName)) {
            return
        }

        match (findOption(optName, isShort: false)) {
            case Some(spec) =>
                spec.isShort = false
                spec.hasEq = eq.isSome()
                applyValue(spec, value)
            case None =>
                if (currentScope.allowLateBinding) {
                    unresolvedOptions.add(arg)
                } else {
                    let hint = ""
                    errors.add(ParseError.UnknownOption(optName, false, hint))
                }
        }
    }

    private func checkMalformedShortOptions(raw: String, name: String): Bool {
        if (Regex("\\s").find(name).isSome()) {
            errors.add(ParseError.MalformedOptionName(raw, true, "cannot contain whitespace characters"))
            return false
        }
        return true
    }

    private func parseShortOptions(arg: String): Unit {
        let raw = arg[1..]

        
        // if `valueIndex > 0`, then everything on the right should be consumed as value of last option
        var valueIndex = 0
        func helper(optName: String, idx: Int64): ?OptionSpec {
            if (!checkMalformedShortOptions(raw, optName)) {
                return None
            }
            match (findOption(optName, isShort: true)) {
                case Some(spec) =>
                    spec.isShort = true
                    if (spec.option.isValueOption) {
                        valueIndex = idx + 1
                    }
                    return spec
                case None =>
                    errors.add(ParseError.UnknownOption(optName, true, ""))
                    return None 
            }
        }

        var prevSpec: OptionSpec = helper(raw[0..1], 0) ?? return ()

        for (idx in 1..raw.size) {
            let optName = raw[idx..idx + 1]
            if (optName == "=") {
                prevSpec.hasEq = true
                valueIndex = idx + 1 
            }

            if (valueIndex > 0) { break } else { specs.add(prevSpec) }
            
            prevSpec = helper(optName, idx) ?? return
        }

        if (valueIndex > 0) {
            if (!prevSpec.hasEq) {
                applyValue(prevSpec, raw[valueIndex..].optionEmpty())
            } else {
                applyValue(prevSpec, raw[valueIndex..])
            }
        } else {
            specs.add(prevSpec)
        }
    }

    private func applyValue(spec: OptionSpec, value: ?String): Unit {
        match (value) {
            case Some(v) =>
                spec.value = Some(v)
                specs.add(spec)
            case None =>
                if (spec.option.isValueOption) {
                    lookaheadValue(spec)
                } else {
                    specs.add(spec)
                }
        }
    }

    private func lookaheadValue(spec: OptionSpec): Unit {
        let name = if (spec.isShort) { spec.option.short } else { spec.option.name }
        
        let default = spec.option.defaultValue
        if (!default.isEmpty()) {
            if (currentScope.allowPositionals && let Some(value) <- peekPositional()) {
                let hint = defaultOverrideHint(spec, value, default)
                errors.add(ParseError.AmbiguousValue(name, spec.isShort, value, hint))
                next()
            } else {
                specs.add(spec)
            }
        } else if (let Some(value) <- peekValue()) {
            spec.value = value
            specs.add(spec)
            next()
        } else {
            errors.add(ParseError.MissingValue(name, spec.isShort))
        }
    }
    
    private func findOption(name: String, isShort!: Bool): ?OptionSpec {
        if (name.isAsciiBlank()) {
            return None
        }

        if (!isShort && name.size == 1) {
            return None
        }

        for (opt in currentScope.options) {
            if (opt.name == name || opt.short == name) {
                return Some(OptionSpec(opt))
            }
        }
        return None
    }

    private func applyOptions(): CliNamespace {
        let namespace = CliNamespace()

        for (spec in specs) {
            let opt = spec.option
            match (opt.action) {
                case Action.SetTrue =>
                    namespace.set(key: opt.name, value: true)
                case Action.SetFalse =>
                    namespace.set(key: opt.name, value: false)
                case Action.Help =>
                    if (namespace.helpText.isNone()) {
                        namespace.helpText = Some(currentScope.printHelp(currentPath |> collectString(delimiter: " ")))
                    }
                case rest where opt.isValueOption =>
                    let rawValue = spec.value.flatMap { it => it.optionEmpty() } ?? opt.defaultValue
                    let splitRawValue = opt.delimiter.map { it => rawValue.split(it) } ?? [ rawValue ]

                    if (!opt.allowEmpty && splitRawValue.size == 0) {
                        let prefix = if (spec.isShort) { "-" } else { "--" }
                        let name = if (spec.isShort) { opt.short } else { opt.name }
                        errors.add(ParseError.InvalidValue(name, spec.isShort, "the value of option '${prefix}${name}' can't be empty"))
                        continue
                    }

                    for (v in splitRawValue) {
                        if (!opt.allowEmpty && v.isEmpty()) {
                            let prefix = if (spec.isShort) { "-" } else { "--" }
                            let name = if (spec.isShort) { opt.short } else { opt.name }
                            errors.add(ParseError.InvalidValue(name, spec.isShort, "the value of option '${prefix}${name}' can't be empty"))
                            continue
                        }
                        opt._validator(v)
                        .inspectErr { e => 
                            let name = if (spec.isShort) { opt.short } else { opt.name }
                            errors.add(ParseError.InvalidValue(name, spec.isShort, e))
                        }
                        .inspect { value => 
                            match (rest) {
                                case Action.Set =>
                                    namespace.set(key: opt.name, value: value)
                                case Action.Append =>
                                    namespace.append(key: opt.name, value: value)
                                case _ => ()
                            }
                        }
                    }
                case _ => ()
            }
        }

        return namespace
    }  
}


private func defaultOverrideHint(spec: OptionSpec, value: String, default: String): String {
    let opt = spec.option
    if (spec.isShort) {
        "option '-${opt.short}' has default: `${default}`, use '-${opt.short}=${value}' or '-${opt.short}=${default}' to set value"
    } else {
        "option '${opt.name}' has default: `${default}`, use '--${opt.name}=${value}' or '--${opt.name}=${default}' to set value"
    }
}

extend String {
    func optionEmpty(): ?String {
        if (this.isEmpty()) {
            return None
        } else {
            return Some(this)
        }
    }
}

func printCliErrors(parseResult: ParseResult): Unit {
    for (err in parseResult.errors) {
        let errName = err.name
        let errHint = err.hint
        let prefix = if (err.isShort) { "-" } else { "--" }
        match (err) {
            case ParseError.UnknownOption(_, _, _) =>
                eprintln("Error: unknown option '${prefix}${errName}'")
            case ParseError.UnknownSubcommand(_, _) =>
                let scope = parseResult.scope |> collectString(delimiter: " ")
                eprintln("Error: unknown command '${errName}' for ${scope}")
            case ParseError.InvalidToken(_) =>
                eprintln("Error: invalid token '${errName}'")
            case ParseError.MissingValue(_, _) =>
                eprintln("Error: the '${prefix}${errName}' option needs a value")
            case ParseError.BooleanOptionWithEq(_, _) =>
                eprintln("Error: the '${prefix}${errName}' option does not take a value, remove '='")
            case ParseError.AmbiguousValue(_, _, value, _) =>
                eprintln("Error: ambiguous value '${value}' after '${prefix}${errName}' option")
            case ParseError.InvalidValue(_, _, msg) =>
                eprintln("Error: ${msg}")
            case ParseError.MalformedOptionName(_, _, desc) =>
                let optText = if (err.isShort) { "short option" } else { "long option" }
                eprintln("Error: malformed ${optText} '${prefix}${errName}': ${desc}")
        }
        if (errHint != "") { eprintln("Hint: ${errHint}") }
    }
}