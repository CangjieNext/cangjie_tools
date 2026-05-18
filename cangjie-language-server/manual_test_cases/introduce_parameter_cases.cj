package manual_refactor.introduce_parameter

func literalInt(base: Int64): Int64 {
    return 1 + base
}

func literalFloat(radius: Float64): Float64 {
    return 0.9 + radius
}

func literalBool(flag: Bool): Bool {
    return true && flag
}

func literalString(name: String): String {
    return "hello" + name
}

func literalRune(): Rune {
    return r'a'
}

func literalUnit(): Unit {
    return ()
}

func replaceOldParameter(base: Int64): Int64 {
    let delta = 2
    return base + delta
}

func replaceTwoParameters(left: Int64, right: Int64): Int64 {
    return left + right
}

func keepUsedOldParameter(base: Int64): Int64 {
    let delta = 2
    let copied = base
    return base + delta + copied
}

func localOnlyExpression(): Int64 {
    let left = 1
    let right = 2
    return left + right
}

func parameterLiteral(base: Int64): Int64 {
    return base + 1
}

func localLiteral(): Int64 {
    let local = 1
    return local + 2
}

func parameterLocalLiteral(base: Int64): Int64 {
    let delta = 2
    return base + delta + 1
}

func boolParameterLocal(flag: Bool): Bool {
    let local = true
    return flag && local
}

func stringParameterLocal(name: String): String {
    let prefix = "hi"
    return prefix + name
}

func targetForCallUpdate(base: Int64): Int64 {
    return base + 1
}

func callTargetForCallUpdate(): Int64 {
    let value = 10
    return targetForCallUpdate(value)
}

func targetForLiteralCall(base: Int64): Int64 {
    return 1 + base
}

func callTargetForLiteralCall(): Int64 {
    return targetForLiteralCall(9)
}

class ParameterClassCases {
    func memberLiteral(radius: Float64): Float64 {
        return 0.9 + radius
    }

    func replaceMemberParameter(base: Int64): Int64 {
        let delta = 2
        return base + delta
    }

    func memberLocalOnly(): Int64 {
        let left = 1
        let right = 2
        return left + right
    }

    func memberParameterLocalLiteral(base: Int64): Int64 {
        let delta = 2
        return base + delta + 1
    }
}

struct ParameterStructCases {
    func structLiteral(count: Int64): Int64 {
        return 12 + count
    }

    func structMixed(base: Int64): Int64 {
        let delta = 3
        return base + delta
    }
}

interface UnsupportedInterfaceParameter {
    func body(): Int64 {
        return 1 + 2
    }
}

