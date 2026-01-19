// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package my_module

import ohos.ark_interop.*
import ohos.ark_interop_helper.*
import ohos.base.*
import std.math.numeric.*

public enum AutoGenType27 {
    | BOOLEAN(Bool)
    | STRING(String)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case BOOLEAN(x) => context.boolean(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType27 {
        
        if (input.isBoolean()) {
            return BOOLEAN(Bool.fromJSValue(context, input))
        } 
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType28 {
    | NUMBER(Float64)
    | STRING(String)
    | BOOLEAN(Bool)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
            case BOOLEAN(x) => context.boolean(x).toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType28 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        }  
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        } 
        if (input.isBoolean()) {
            return BOOLEAN(Bool.fromJSValue(context, input))
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType29 {
    | NUMBER(Float64)
    | STRING(String)
    | BOOLEAN(Bool)
    | NULL
    | UNDEFINED
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
            case BOOLEAN(x) => context.boolean(x).toJSValue()
            case NULL => context.null().toJSValue()
            case UNDEFINED => context.undefined().toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType29 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        }  
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        } 
        if (input.isBoolean()) {
            return BOOLEAN(Bool.fromJSValue(context, input))
        } 
        if (input.isNull()) {
            return NULL
        } 
        if (input.isUndefined()) {
            return UNDEFINED
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType30 {
    | NUMBER(Float64)
    | STRING(String)
    | BOOLEAN(Bool)
    | BIGINT(BigInt)
    | VOID(JSValue/* FIXME: `void` */)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
            case BOOLEAN(x) => context.boolean(x).toJSValue()
            case BIGINT(x) => context.bigint(x).toJSValue()
            case VOID(x) => x
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType30 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        }  
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        } 
        if (input.isBoolean()) {
            return BOOLEAN(Bool.fromJSValue(context, input))
        } 
        if (input.isBigInt()) {
            return BIGINT(input.toBigInt())
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType31 {
    | NUMBER(Float64)
    | STRING(String)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case STRING(x) => context.string(x).toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType31 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        }  
        if(input.isString()) {
            return STRING(String.fromJSValue(context, input))
        }
        throw Exception("unsupport type!")
    }
}

public enum AutoGenType32 {
    | NUMBER(Float64)
    | BOOLEAN(Bool)
    
    public func toJSValue(context: JSContext): JSValue {
        match(this) {
            case NUMBER(x) => context.number(x).toJSValue()
            case BOOLEAN(x) => context.boolean(x).toJSValue()
        }
    }
    public static func fromJSValue(context: JSContext, input: JSValue): AutoGenType32 {
        
        if(input.isNumber()) {
            return NUMBER(Float64.fromJSValue(context, input))
        }  
        if (input.isBoolean()) {
            return BOOLEAN(Bool.fromJSValue(context, input))
        }
        throw Exception("unsupport type!")
    }
}


/*class ExportKeyword UnionTypes {
    t_null: number | null;
    null_t: null | String;
    t1_t2: boolean | String;
    t_undefined: number | undefined;
    undefined: undefined | String;
    t3: number | String | boolean;
    t4: number | String | boolean | null | undefined;
    t5: number | String | boolean | bigint| void;
    getValueOrError(): number | String
    setValue(value: string): number | boolean
    }*/

public class UnionTypes {
    
    protected UnionTypes(var arkts_object: JSObject) { }
    
    // Properties with union types
    public mut prop t_null: Option<Float64> {
        get() {
            checkThreadAndCall < Option<Float64> >(getMainContext()) {
                ctx: JSContext =>
                if(arkts_object["t_null"].isNull()) {
                    None < Float64 >
                } else {
                    Float64.fromJSValue(ctx, arkts_object["t_null"])
                }
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["t_null"] = match(v) {
                    case None => ctx.undefined().toJSValue()
                    case _ => v.getOrThrow().toJSValue(ctx)
                }
            }
        }
        
    }
    
    public mut prop null_t: Option<String> {
        get() {
            checkThreadAndCall < Option<String> >(getMainContext()) {
                ctx: JSContext =>
                if(arkts_object["null_t"].isNull()) {
                    None < String >
                } else {
                    String.fromJSValue(ctx, arkts_object["null_t"])
                }
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["null_t"] = match(v) {
                    case None => ctx.undefined().toJSValue()
                    case _ => v.getOrThrow().toJSValue(ctx)
                }
            }
        }
        
    }
    
    public mut prop t1_t2: AutoGenType27 {
        get() {
            checkThreadAndCall < AutoGenType27 >(getMainContext()) {
                ctx: JSContext => AutoGenType27.fromJSValue(ctx, arkts_object["t1_t2"])
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["t1_t2"] = v.toJSValue(ctx)
            }
        }
        
    }
    
    public mut prop t_undefined: Option<Float64> {
        get() {
            checkThreadAndCall < Option<Float64> >(getMainContext()) {
                ctx: JSContext =>
                if(arkts_object["t_undefined"].isNull()) {
                    None < Float64 >
                } else {
                    Float64.fromJSValue(ctx, arkts_object["t_undefined"])
                }
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["t_undefined"] = match(v) {
                    case None => ctx.undefined().toJSValue()
                    case _ => v.getOrThrow().toJSValue(ctx)
                }
            }
        }
        
    }
    
    public mut prop undefined: Option<String> {
        get() {
            checkThreadAndCall < Option<String> >(getMainContext()) {
                ctx: JSContext =>
                if(arkts_object["undefined"].isNull()) {
                    None < String >
                } else {
                    String.fromJSValue(ctx, arkts_object["undefined"])
                }
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["undefined"] = match(v) {
                    case None => ctx.undefined().toJSValue()
                    case _ => v.getOrThrow().toJSValue(ctx)
                }
            }
        }
        
    }
    
    public mut prop t3: AutoGenType28 {
        get() {
            checkThreadAndCall < AutoGenType28 >(getMainContext()) {
                ctx: JSContext => AutoGenType28.fromJSValue(ctx, arkts_object["t3"])
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["t3"] = v.toJSValue(ctx)
            }
        }
        
    }
    
    public mut prop t4: AutoGenType29 {
        get() {
            checkThreadAndCall < AutoGenType29 >(getMainContext()) {
                ctx: JSContext => AutoGenType29.fromJSValue(ctx, arkts_object["t4"])
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["t4"] = v.toJSValue(ctx)
            }
        }
        
    }
    
    public mut prop t5: AutoGenType30 {
        get() {
            checkThreadAndCall < AutoGenType30 >(getMainContext()) {
                ctx: JSContext => AutoGenType30.fromJSValue(ctx, arkts_object["t5"])
            }
        }
        set(v) {
            checkThreadAndCall < Unit >(getMainContext()) {
                ctx: JSContext => arkts_object["t5"] = v.toJSValue(ctx)
            }
        }
        
    }
    
    /**
	 * @brief getValueOrError(): number | String
	 */
	// Method returning a union type
    public func getValueOrError(): AutoGenType31 {
        jsObjApiCall < AutoGenType31 >( arkts_object, "getValueOrError", EMPTY_ARG) { ctx, info => AutoGenType31.fromJSValue(ctx, info) }
    }
    /**
	 * @brief setValue(value: string): number | boolean
	 */
	// Method with union type parameter
    public func setValue(value: String): AutoGenType32 {
        jsObjApiCall < AutoGenType32 >( arkts_object, "setValue", { ctx =>[value.toJSValue(ctx)] }) { ctx, info => AutoGenType32.fromJSValue(ctx, info) }
    }
    
    public func toJSValue(context: JSContext): JSValue {
        arkts_object.toJSValue()
    }
    
    public static func fromJSValue(context: JSContext, input: JSValue): UnionTypes {
        UnionTypes(input.asObject())
    }
}

/***********METHOD***********/
/**
	 * @brief testUnion(s: string): boolean | String
	 */
public func testUnion(s: String): AutoGenType27 {
    hmsGlobalApiCall < AutoGenType27 >( "my_module_unionTypes", "testUnion", { ctx =>[s.toJSValue(ctx)] }) { ctx, info => AutoGenType27.fromJSValue(ctx, info) }
}

/**
	 * @brief testUnion1(s: string | number): boolean | String
	 */
public func testUnion1(s: String): AutoGenType27 {
    hmsGlobalApiCall < AutoGenType27 >( "my_module_unionTypes", "testUnion1", { ctx =>[s.toJSValue(ctx)] }) { ctx, info => AutoGenType27.fromJSValue(ctx, info) }
}

/**
	 * @brief testUnion1(s: string | number): boolean | String
	 */
public func testUnion1(s: Float64): AutoGenType27 {
    hmsGlobalApiCall < AutoGenType27 >( "my_module_unionTypes", "testUnion1", { ctx =>[s.toJSValue(ctx)] }) { ctx, info => AutoGenType27.fromJSValue(ctx, info) }
}


