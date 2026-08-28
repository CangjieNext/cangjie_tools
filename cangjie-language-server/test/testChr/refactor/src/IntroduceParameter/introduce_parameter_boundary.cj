package ohos_app_cangjie_entry.IntroduceParameter

func boundarySample(left: Int64, right: Int64): Int64 {
    return left + right + 1
}

func callBoundarySample(): Unit {
    let result = boundarySample(10, 20)
}
