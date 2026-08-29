package ohos_app_cangjie_entry.IntroduceParameter

class NestedCallTarget {
    func calc(base: Int64): Int64 {
        return base + 2
    }
}

func makeValue(seed: Int64): Int64 {
    return seed
}

func callNestedCallTarget(): Unit {
    let target = NestedCallTarget()
    let result = target.calc(makeValue(1))
}
