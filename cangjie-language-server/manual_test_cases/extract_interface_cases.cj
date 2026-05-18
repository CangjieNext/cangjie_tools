package manual_refactor.extract_interface

public interface PrintableBase {
    public func printLabel(): String
}

public interface LocatableBase {
    public func location(): String
}

public interface NamedBaseInterface {
    public func nameText(): String
}

open class OpenParent {
    public func parentText(): String {
        return "parent"
    }
}

public class ClassAllMembers <: OpenParent & PrintableBase {
    var name: String

    init(name: String) {
        this.name = name
    }

    public func printLabel(): String {
        return this.name
    }

    func defaultVisible(): String {
        return this.name
    }

    protected func protectedVisible(): String {
        return this.name
    }

    private func privateVisible(): String {
        return "hidden"
    }

    static func staticFactory(): ClassAllMembers {
        return ClassAllMembers("static")
    }
}

public open class OpenClassAllMembers {
    public func publicApi(): String {
        return "public"
    }

    func defaultApi(): String {
        return "default"
    }
}

public struct StructAllMembers <: PrintableBase {
    var x: Int64 = 0

    public func printLabel(): String {
        return "point"
    }

    func defaultVisible(): Int64 {
        return this.x
    }

    public operator func <(right: StructAllMembers): Bool {
        return this.x < right.x
    }

    static func origin(): StructAllMembers {
        return StructAllMembers()
    }
}

public enum EnumAllMembers <: PrintableBase {
    Ready | Done

    public func printLabel(): String {
        return "state"
    }

    func code(): Int64 {
        return 1
    }

    static func defaultState(): EnumAllMembers {
        return Ready
    }
}

public interface InterfaceAllMembers <: PrintableBase {
    public func printLabel(): String

    func defaultBody(): String {
        return "body"
    }
}

extend ClassAllMembers <: LocatableBase {
    public func location(): String {
        return "earth"
    }

    func extendDefault(): String {
        return "extend"
    }
}

public class AlreadyOneInterface <: PrintableBase {
    public func printLabel(): String {
        return "one"
    }

    func nextApi(): String {
        return "next"
    }
}

public class AlreadyMultiInterface <: PrintableBase & LocatableBase {
    public func printLabel(): String {
        return "multi"
    }

    public func location(): String {
        return "mars"
    }

    func nextApi(): String {
        return "next"
    }
}

public class ParentAndInterface <: OpenParent & PrintableBase {
    public func printLabel(): String {
        return "mixed"
    }

    func ownApi(): String {
        return "own"
    }
}

public interface InterfaceAlreadyExtends <: PrintableBase {
    public func printLabel(): String

    func extra(): String {
        return "extra"
    }
}

public class TypeReferenceCases {
    var cachedClass: ClassAllMembers = ClassAllMembers("cached")
    var cachedStruct: StructAllMembers = StructAllMembers()
    var cachedEnum: EnumAllMembers = EnumAllMembers.Ready

    func acceptClass(value: ClassAllMembers): ClassAllMembers {
        var local: ClassAllMembers = value
        return local
    }

    func acceptStruct(value: StructAllMembers): StructAllMembers {
        var local: StructAllMembers = value
        return local
    }

    func acceptEnum(value: EnumAllMembers): EnumAllMembers {
        var local: EnumAllMembers = value
        return local
    }
}

public class CommentMemberCases {
    // line comment copied to interface
    public func lineComment(): String {
        return "line"
    }

    /* block comment copied to interface */
    public func blockComment(): String {
        return "block"
    }

    /** doc comment is not copied */
    public func docComment(): String {
        return "doc"
    }
}

