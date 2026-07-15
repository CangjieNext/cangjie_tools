package demo

func ifLetSample(option: ?Int64): Unit {
    if (let Some(value) <- option) {
        println(value)
    }
}
