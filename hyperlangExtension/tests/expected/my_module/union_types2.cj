// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package my_module

import ohos.ark_interop.*
import ohos.ark_interop_helper.*
import ohos.base.*

public enum AutoGenType35 {
    | NUMBER(Float64)
    | STRING(String)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType35 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        }  
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType36 {
    | BOOLEAN(Bool)
    | STRING(String)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case BOOLEAN(x) => context.boolean(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType36 {
        
        if (input.isBoolean()) {
            return BOOLEAN(Bool.fromJSValue(context, input))
        } 
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType37 {
    | NUMBER(Float64)
    | _BOOLEAN_STRING___(JSValue/* FIXME: `(boolean | string)[]` */)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case _BOOLEAN_STRING___(x) => x
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType37 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        } 
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType38 {
    | NUMBER(Float64)
    | BOOLEAN__(JSValue/* FIXME: `boolean[]` */)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case BOOLEAN__(x) => x
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType38 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        } 
        throw Exception("unsupport type!")
    }
}


/*interface AutoGenType39 {
    city: string;
    zipCode: string;
    }*/

public open class AutoGenType39 {
    
    protected AutoGenType39(public var city: String,
    public var zipCode: String) { }
    
    
    public open func toJSValue(context: JSContext): JSValue {
        let obj = context.object()
        obj["city"] = city.toJSValue(context)
        obj["zipCode"] = zipCode.toJSValue(context)
        obj.toJSValue()
    }
    
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType39 {
        let obj = input.asObject()
        AutoGenType39(
        String.fromJSValue(context, obj["city"]),
        String.fromJSValue(context, obj["zipCode"])
        )
    }
    
}
/***********OBJECT***********/

/*interface UserProfile {
    array: (number | String)[];
    test: (num: number | String) => String;
    test1: number|(boolean | String)[];
    test2: number| boolean[];
    name: String;
    address: {
      city: String;
      zipCode: String;
    };
    id?: number|String;
    }*/

public open class UserProfile {
    
    protected UserProfile(public var array: Array<AutoGenType35>,
    public var test: (num: AutoGenType35) -> String,
    public var test1: AutoGenType37,
    public var test2: AutoGenType38,
    public var name: String,
    public var address: AutoGenType39,
    public var id!: Option<AutoGenType35> = None) { }
    
    
    public open func toJSValue(context: JSContext): JSValue {
        let obj = context.object()
        obj["array"] = toJSArray < AutoGenType35 >(context, array, { ctx: JSContext, val: AutoGenType35 => val.toJSValue(ctx) })
        obj["test"] = context.function({ ctx, info =>
            let p0 = AutoGenType35.fromJSValue(ctx, info[0])
            let ret = test(p0)
            ret.toJSValue(ctx)
        }).toJSValue()
        obj["test1"] = test1.toJSValue(context)
        obj["test2"] = test2.toJSValue(context)
        obj["name"] = name.toJSValue(context)
        obj["address"] = address.toJSValue(context)
        if(let Some(v) <- id) {
            obj["id"] = v.toJSValue(context)
        }
        obj.toJSValue()
    }
    
    public static func fromJSValue(context: JSContext, input: JSValue): UserProfile {
        let obj = input.asObject()
        UserProfile(
        fromJSArray < AutoGenType35 >(context, obj["array"], { ctx: JSContext, val: JSValue => AutoGenType35.fromJSValue(ctx, val) }),
        { num: AutoGenType35 =>
            checkThreadAndCall < String >(context, { ctx =>
                let arg0 = num.toJSValue(ctx)
                let ret = obj["test"].asFunction().call([arg0])
                String.fromJSValue(ctx, ret)
            })
        },
        AutoGenType37.fromJSValue(context, obj["test1"]),
        AutoGenType38.fromJSValue(context, obj["test2"]),
        String.fromJSValue(context, obj["name"]),
        AutoGenType39.fromJSValue(context, obj["address"]),
        id:
        if(obj["id"].isNull()) {
            None < AutoGenType35 >
        } else {
            AutoGenType35.fromJSValue(context, obj["id"])
        }
        )
    }
    
}

