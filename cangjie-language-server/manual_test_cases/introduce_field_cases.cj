package manual_refactor.introduce_field

class LiteralFieldCases {

    func floatLiteral(radius: Float64): Float64 {
        return 0.9 + radius
    }

    func intLiteral(count: Int64): Int64 {
        return 12 + count
    }

    func boolLiteral(flag: Bool): Bool {
        return true && flag
    }

    func stringLiteral(name: String): String {
        return "hello" + name
    }

    func runeLiteral(): Rune {
        return r'a'
    }

    func unitLiteral(): Unit {
        return ()
    }
}

class ExistingFieldCases {
    var first: Int64 = 1

    func beforeLaterField(count: Int64): Int64 {
        return 2 + count
    }

    var later: Int64 = 3

    func afterLaterField(count: Int64): Int64 {
        return 4 + count
    }
}

class MemberReferenceCases {
    var base: Float64 = 2.0

    func selectThisBase(): Float64 {
        return this.base + 1.0
    }

    func selectOnlyThisBase(): Float64 {
        return this.base
    }
}

class ComplexExpressionCases {

    func parameterAndLiteral(radius: Float64): Float64 {
        return radius + 1.0
    }

    func localAndLiteral(): Int64 {
        let local = 1
        return local + 2
    }

    func parameterLocalLiteral(base: Int64): Int64 {
        let delta = 2
        return base + delta + 1
    }

    func boolExpression(flag: Bool): Bool {
        let local = true
        return flag && local
    }

    func stringExpression(name: String): String {
        let prefix = "hi"
        return prefix + name
    }
}

class StaticMethodCases {

    static func staticLiteral(count: Int64): Int64 {
        return 7 + count
    }

    static func staticComplex(count: Int64): Int64 {
        let delta = 2
        return count + delta
    }
}

class CommentBeforeMethodCases {

    // keep on method
    func commentedMethod(): Int64 {
        return 1 * 2
    }
}

struct StructLiteralCases {

    func intLiteral(count: Int64): Int64 {
        return 100 + count
    }

    func complex(base: Int64): Int64 {
        let delta = 3
        return base + delta
    }
}

func topLevelLiteral(flag: Bool): Bool {
    return false || flag
}

func topLevelComplex(base: Int64): Int64 {
    let delta = 2
    return base + delta
}

func topLevelDependency(): Int64 {
    return globalBase + 1
}

var globalBase: Int64 = 10

interface UnsupportedInterfaceField {
    func notSupported(): Int64 {
        return 1 + 2
    }
}

extend LiteralFieldCases {
    func notSupportedExtend(): Int64 {
        return 1 + 2
    }
}

