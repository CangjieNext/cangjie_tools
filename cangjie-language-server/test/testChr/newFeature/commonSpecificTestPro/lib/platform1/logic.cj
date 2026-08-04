features {os.platform1}

package ohos_app_cangjie_entry

public specific func logicTest(): String {
    let nativeResult: CString = unsafe { FfiLogicTest() }
    let result = nativeResult.toString()
    unsafe { FfiFreeString(nativeResult) }
    result
}

foreign func FfiLogicTest(): CString
foreign func FfiFreeString(str: CString): Unit
