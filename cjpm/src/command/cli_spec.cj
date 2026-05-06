// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import std.convert.*
import std.regex.*
import cjpm.config.*
import cjpm.util.Result

public enum Action {
    | Set
    | SetTrue
    | SetFalse
    | Append
    | Help
}

private const HELP_PADDING: Int64 = 4

public func CliOption<T>(
    // Long option name
    name!: String,
    // Short option alias. If `name == short`, only short option exists
    short!: String = "",
    action!: Action = Action.Set,
    // `--name` without value will get treated as `--name=defaultValue`
    // limits option syntax. Only setting value via `=` is allowed (`--name=value`)
    defaultValue!: String = "", 
    // option description
    help!: String = "",
    // When unset, <value> will be used
    helpValue!: ?String = None,
    // Takes effect only when `action == Action.Append`
    // `--name=a --name=b --name=c` can be shortened to `--name=a,b,c` if delimiter `,` is specified
    delimiter!: ?String = None,
    // Used for grouping options to display them together in generated `--help` message
    category!: String = "",
    // Whether `--name=`, `--name=""` makes sense
    allowEmpty!: Bool = false,
    // An arbitrary function to do checks and type conversions
    // For `Action.Append` the check is performed for a single value
    validator!: (String) -> Result<T, String>
): TypedCliOption<T> {
    TypedCliOption<T>(
        name: name,
        short: short,
        action: action,
        defaultValue: defaultValue,
        help: help,
        helpValue: helpValue,
        delimiter: delimiter,
        category: category,
        allowEmpty: allowEmpty,
        validator: validator
    )
}

// An overloaded version of same function to "default" generic parameter to type `String` if no validator is given
public func CliOption(
    name!: String,
    short!: String = "",
    action!: Action = Action.Set,
    defaultValue!: String = "",
    help!: String = "",
    helpValue!: ?String = None,
    delimiter!: ?String = None,
    category!: String = "",
    allowEmpty!: Bool = false,
): TypedCliOption<String> {
    TypedCliOption<String>(
        name: name,
        short: short,
        action: action,
        defaultValue: defaultValue,
        help: help,
        helpValue: helpValue,
        delimiter: delimiter,
        category: category,
        allowEmpty: allowEmpty,
        validator: idValidator<String>
    )
}

sealed abstract class CliOptionBase {
    protected CliOptionBase(
        public let name: String,
        public let short: String,
        public let action: Action,
        public let defaultValue: String,
        public let help: String,
        public let helpValue: ?String,
        public let delimiter: ?String,
        public let category: String,
        public let allowEmpty: Bool,
        protected let _validator: (String) -> Result<Any, String>
    ) { }

    public prop isValueOption: Bool {
        get() {
            match (action) {
                case Action.Set | Action.Append => true
                case Action.SetTrue | Action.SetFalse | Action.Help => false
            }
        }
    }
}

public class TypedCliOption<T> <: CliOptionBase {
    private var _typedValidator: (String) -> Result<T, String>

    public TypedCliOption(
        name!: String,
        short!: String = "",
        action!: Action = Action.Set,
        defaultValue!: String = "",
        help!: String = "",
        helpValue!: ?String = None,
        delimiter!: ?String = None,
        category!: String = "",
        allowEmpty!: Bool = false,
        validator!: (String) -> Result<T, String>
    ) { 
        super(name, short, action, defaultValue, help, helpValue, delimiter, category, allowEmpty) { it => 
            validator(it).map { it => let x: Any = it; x }
        }
        this._typedValidator = validator
    }

    public prop validator: (String) -> Result<T, String> {
        get() { _typedValidator }
    }
}

private func idValidator<T>(it: T): Result<T, String> { Ok(it) }

public func jobsValidator(s: String): Result<Int32, String> {
    posInt32Validator(s, "-j/--jobs")
}

private func posInt32Validator(s: String, name: String): Result<Int32, String> {
    try {
        let v = Int32.parse(s)
        if (v > 0) { Ok(v) } else { Err("the '${name}' option requires a positive integer argument, its valid range is (0, 2147483647]") }
    } catch (_: Exception) {
        Err("the '${name}' option requires a positive integer argument, its valid range is (0, 2147483647]")
    }
}

private func posInt64Validator(s: String, name: String): Result<Int64, String> {
    try {
        let v = Int64.parse(s)
        if (v > 0) { Ok(v) } else { Err("the '${name}' option requires a positive integer argument, its valid range is (0, 9223372036854775807]") }
    } catch (_: Exception) {
        Err("the '${name}' option requires a positive integer argument, its valid range is (0, 9223372036854775807]")
    }
}

public func featureValidator(s: String): Result<Feature, String> {
    Feature.fromString(s)
}

@When[os == "Windows"]
let EXAMPLE_OUTPUT_NAME = "main.exe"
@When[os != "Windows"]
let EXAMPLE_OUTPUT_NAME = "main"
public func outputValidator(name: String): Result<String, String> {
    if (Regex("^([a-zA-Z]+[-._a-zA-Z0-9]*)$").find(name).isSome()) {
        return Ok(name)
    } else {
        return Err("the output name '${name}' is wrong, the example is '${EXAMPLE_OUTPUT_NAME}'")
    }
}

public func targetValidator(target: String): Result<String, String> {
    if (Regex("^([-_a-zA-Z0-9]+)$").find(target).isSome()) {
        return Ok(target)
    } else {
        return Err("the cross-compile target '${target}' is wrong, the example is 'aarch64-hm-gnu'")
    }
}

public func targetDirValidator(name: String): Result<String, String> {
    safeCheckResult(name, path: "target-dir")
}

public class CliScope {
    public let options: ArrayList<CliOptionBase> = ArrayList<CliOptionBase>()
    public let subScopes: HashMap<String, CliScope> = HashMap<String, CliScope>()

    public CliScope(
        // Name of the subcommand
        public let name!: String,
        // Description of subcommand in `--help`
        public let description!: String = "",
        // Perform `Action.Help` if no are given
        public let helpNoArgs!: Bool = false,
        // Automatically generate help message and help action. None - false. Some(cat) - category name of generated option
        public let autoHelp!: ?String = name,
        // Positional arguments before `--` are highly discourage to avoid ambiguities
        // However, there's already existing API, which utilize positionals (`cjpm test`, `cjpm bench`)
        public let allowPositionals!: Bool = false,
        // Customized options are not known until resolution of `cjpm.toml` files is performed
        // In this case, unknown options should not be treated as errors, but can be resolved dynamically
        public let allowLateBinding!: Bool = false,
        // Description for arguments after `--`, None should mean that `--` and all options after are ignored
        public let finalArgsDescription!: ?String = None,
    ) { 
        if (let Some(cat) <- autoHelp) {
            options.add(CliOption(name: "help", short: "h", action: Action.Help, help: "display help", category: cat))
        }
    }

    public func addOption(opt: CliOptionBase): CliScope {
        options.add(opt)
        return this
    }

    public func addOptions(opts: Iterable<CliOptionBase>): CliScope {
        for (opt in opts) {
            addOption(opt)
        }
        return this
    }

    public func addSubScope(scope: CliScope): CliScope {
        subScopes[scope.name] = scope
        return this
    }

    public func findScope(parts: Array<String>): ?CliScope {
        if (parts.isEmpty()) {
            return this
        }
        let first = parts[0]
        if (let Some(sub) <- subScopes.get(first)) {
            return sub.findScope(parts[1..])
        }
        None
    }

    public func printHelp(fullPath: String): String {
        let sb = StringBuilder()
        if (!description.isEmpty()) {
            sb.append(description)
            sb.append("\n")
        }
        sb.append("\n")
        
        let optionsUsage = if (!options.isEmpty()) { " [option]" } else { "" }
        let subcommandUsage = if (!subScopes.isEmpty()) { " <subcommand>" } else { "" }
        let finalArgsUsage = match (finalArgsDescription) {
            case Some(s) => " [-- ${s}]"
            case None => ""
        }
        sb.append("Usage:\n")
        sb.append("  ${fullPath}${optionsUsage}${subcommandUsage}${finalArgsUsage}\n")

        if (!options.isEmpty()) {
            sb.append("\n")
            let categoryOptions = HashMap<String, ArrayList<CliOptionBase>>()
            let categoryOrder = ArrayList<String>()
            for (opt in options) {
                if (!categoryOptions.contains(opt.category)) {
                    categoryOptions[opt.category] = ArrayList<CliOptionBase>()
                    categoryOrder.add(opt.category)
                }
                let cat = if (opt.category.isEmpty()) {
                    categoryOrder.first ?? opt.category
                } else {
                    opt.category
                }
                categoryOptions[cat].add(opt)
            }
            
            let firstCat = categoryOrder[0]
            let header = if (categoryOrder.size == 1) { 
                "Available options:" 
            } else {
                "Available '${firstCat}' options:" 
            }
            sb.append("${header}\n")
            let opts = StringBuilder()
            appendOptionsTable(opts, categoryOptions[categoryOrder[0]])
            sb.append(appendTableAligned(opts, HELP_PADDING))
            
            if (categoryOrder.size > 1) {
                for (i in 1..categoryOrder.size) {
                    let cat = categoryOrder[i]
                    sb.append("\n")
                    let header = "Available '${cat}' options:"
                    sb.append("${header}\n")
                    let opts = StringBuilder()
                    appendOptionsTable(opts, categoryOptions[cat])
                    sb.append(appendTableAligned(opts, HELP_PADDING))
                }
            }
        }

        if (!subScopes.isEmpty()) {
            sb.append("\n")
            sb.append("Available subcommands:\n")
            let sub = StringBuilder()
            appendSubcommandTable(sub, subScopes)
            sb.append(appendTableAligned(sub, HELP_PADDING))
            sb.append("\n")
            sb.append("Use '${fullPath} <subcommand> --help' for more information about a command.\n")
        }
        
        sb.toString()
    }
}

private type Table = ArrayList<(String, String)>

private func appendTableAligned(sb: StringBuilder, padding: Int64): StringBuilder {
    let lines = sb.toString().split("\n")
    var firstCol = 0
    for (line in lines) {
        if (line.isEmpty()) { continue }
        let parts = line.split("|")
        let first = parts[0]
        if (first.size > firstCol) { firstCol = first.size }
    }
    let result = StringBuilder()
    for (line in lines) {
        if (line.isEmpty()) { continue }
        let parts = line.split("|")
        let first = parts[0]
        let second = if (parts.size > 1) { parts[1] } else { "" }
        let pad = firstCol - first.size + padding
        var paddingStr = ""
        for (_ in 0..pad) { paddingStr += " " }
        result.append("  ${first}${paddingStr}${second}\n")
    }
    result
}

private func appendOptionsTable(sb: StringBuilder, opts: ArrayList<CliOptionBase>): StringBuilder {
    for (opt in opts) {
        let shortStr = if (opt.short.isEmpty()) { "" } else { "-${opt.short}" }
        let hasDefault = !opt.defaultValue.isEmpty()
        let valueName = match (opt.action) {
            case Action.SetTrue | Action.SetFalse | Action.Help => ""
            case Action.Append | Action.Set where hasDefault => "[=<value>]"
            case Action.Append | Action.Set where !hasDefault => " <value>"
            case _ => ""
        }

        let valuePart = match ((opt.helpValue, opt.action)) {
            case (None, Action.Append) => "${valueName}${opt.delimiter ?? ""}..."
            case (None, _) => valueName 
            case (Some(value), _) => " ${value}"
        } 

        let delim = if (opt.short == opt.name || opt.short.isEmpty() ) { "" } else { ", "}
        let longStr = if (opt.short == opt.name) { "" } else {"--${opt.name}${valuePart}"}

        let first = "${shortStr}${delim}${longStr}"
        let helpSuffix = if (hasDefault) { " (default: ${opt.defaultValue})" } else { "" }
        let second = if (opt.help.isEmpty()) { "" } else { "${opt.help}${helpSuffix}" }
        sb.append("${first}|${second}\n")
    }
    sb
}

private func appendSubcommandTable(sb: StringBuilder, subScopes: HashMap<String, CliScope>): StringBuilder {
    for ((name, scope) in subScopes) {
        sb.append("${name}|${scope.description}\n")
    }
    sb
}
