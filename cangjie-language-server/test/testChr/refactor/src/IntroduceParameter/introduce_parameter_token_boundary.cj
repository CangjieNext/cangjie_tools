package ohos_app_cangjie_entry.IntroduceParameter

let baseValue = 2

func tokenBoundarySample(base: Int64): Int64 {
    return base + baseValue
}

func callTokenBoundarySample(): Unit {
    let result = tokenBoundarySample(10)
}
