package ohos_app_cangjie_entry.IntroduceParameter

class CompoundAssignOwner {
    var offset: Int64 = 3

    func shift(base: Int64): Unit {
        this.offset <<= 2
    }
}

func callCompoundAssignOwner(): Unit {
    let owner = CompoundAssignOwner()
    owner.shift(10)
}
