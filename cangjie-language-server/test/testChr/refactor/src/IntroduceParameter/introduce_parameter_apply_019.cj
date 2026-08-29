package ohos_app_cangjie_entry.IntroduceParameter

func tokenBoundarySample(base: Int64): Int64 {
    let baseValue = 2
    return base + baseValue
}

func callTokenBoundarySample(): Unit {
    let result = tokenBoundarySample(10)
}
