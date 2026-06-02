package script_dep_root

import script_dep.scriptOnlyFunc

public func useScriptDepInRoot(): Int64 {
    scriptOnlyFunc()
    return 0
}
