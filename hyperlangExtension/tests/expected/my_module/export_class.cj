// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package my_module

import ohos.ark_interop.*
import ohos.ark_interop_helper.*
import ohos.base.*


/*interface AutoGenType7 {
    }*/

public open class AutoGenType7 {
    
    protected AutoGenType7() { }
    
    
    public open func toJSValue(context: JSContext): JSValue {
        let obj = context.object()
        
        obj.toJSValue()
    }
    
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType7 {
        let obj = input.asObject()
        AutoGenType7(
        
        )
    }
    
}

/*class ExportKeyword DeclareKeyword UserInfoManager {
    userPreferences?: { [key: String]: String | number | boolean };
    }*/

public class UserInfoManager {
    
    protected UserInfoManager(var arkts_object: JSObject) { }
    
    
    public mut prop userPreferences: ?AutoGenType7 {
        get() {
            checkThreadAndCall < ?AutoGenType7 >(getMainContext()) {
                ctx: JSContext =>
                if(arkts_object["userPreferences"].isNull()) {
                    None < AutoGenType7 >
                } else {
                    AutoGenType7.fromJSValue(ctx, arkts_object["userPreferences"])
                }
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["userPreferences"] = match(v) {
                    case None => ctx.undefined().toJSValue()
                    case _ => v.getOrThrow().toJSValue(ctx)
                }
            }
        }
        
    }
    
    
    func toJSValue(context: JSContext): JSValue {
        arkts_object.toJSValue()
    }
    
    static func fromJSValue(context: JSContext, input: JSValue): UserInfoManager {
        UserInfoManager(input.asObject())
    }
}

