package ohos_app_cangjie_entry.IntroduceParameter

open class SyncBase {
    public open func merge(a: Int64, b: Int64, c: Int64): Int64 {
        return a + b + c
    }
}

class SyncChild <: SyncBase {
    public override func merge(a: Int64, b: Int64, c: Int64): Int64 {
        return a + b + c
    }
}

func callSyncBase(): Unit {
    let base = SyncBase()
    let child = SyncChild()
    let r1 = base.merge(1, 2, 3)
    let r2 = child.merge(4, 5, 6)
}
