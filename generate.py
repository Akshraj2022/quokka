def generate():
    parser_code = """
// parser.qk
fn make_parser(tokens) { return [tokens, 0, false] }
fn parse(p) {
    let mut stmts = []
    while p[1] < len(p[0]) and p[0][p[1]][0] != "EOF" {
        push(stmts, statement(p))
    }
    return ["Program", stmts]
}
fn advance(p) { list_set(p, 1, p[1] + 1) }
fn peek(p) { if p[1] < len(p[0]) { return p[0][p[1]] } return ["EOF", "", 0] }
fn check(p, type) { return peek(p)[0] == type }
fn match_tok(p, type) { if check(p, type) { advance(p) return true } return false }
fn consume(p, type) {
    if check(p, type) { advance(p) return true }
    println("Parse error: expected " ++ type)
    list_set(p, 2, true)
    return false
}

fn statement(p) {
    if match_tok(p, "LET") {
        let is_mut = match_tok(p, "MUT")
        let name = peek(p)[1]
        consume(p, "IDENT")
        consume(p, "ASSIGN")
        return ["Let", name, is_mut, expression(p)]
    }
    if match_tok(p, "SHADOW") {
        let name = peek(p)[1]
        consume(p, "IDENT")
        consume(p, "ASSIGN")
        return ["Shadow", name, expression(p)]
    }
    if match_tok(p, "FN") {
        let mut name = ""
        if check(p, "IDENT") { name = peek(p)[1] advance(p) }
        consume(p, "LPAREN")
        let mut params = []
        if not check(p, "RPAREN") {
            let p1 = peek(p)[1]
            consume(p, "IDENT")
            if match_tok(p, "COLON") { consume(p, "IDENT") }
            push(params, p1)
            while match_tok(p, "COMMA") {
                let p2 = peek(p)[1]
                consume(p, "IDENT")
                if match_tok(p, "COLON") { consume(p, "IDENT") }
                push(params, p2)
            }
        }
        consume(p, "RPAREN")
        if match_tok(p, "ARROW") { consume(p, "IDENT") }
        return ["Fn", name, params, parse_block(p)]
    }
    if match_tok(p, "RETURN") {
        let mut val = ["None"]
        let pt = peek(p)[0]
        if pt=="INT" or pt=="STRING" or pt=="TRUE" or pt=="FALSE" or pt=="IDENT" or pt=="NONE" or pt=="SOME" or pt=="OK" or pt=="ERR" or pt=="LPAREN" or pt=="MINUS" or pt=="NOT" or pt=="LBRACKET" or pt=="IF" or pt=="MATCH" or pt=="FN" {
            val = expression(p)
        }
        return ["Return", val]
    }
    if match_tok(p, "WHILE") { return ["While", expression(p), parse_block(p)] }
    if match_tok(p, "FOR") {
        let var_name = peek(p)[1]
        consume(p, "IDENT")
        consume(p, "IN")
        let iter = expression(p)
        return ["For", var_name, iter, parse_block(p)]
    }
    if match_tok(p, "BREAK") { return ["Break"] }
    if match_tok(p, "CONTINUE") { return ["Continue"] }
    
    return expression(p)
}

fn parse_block(p) {
    consume(p, "LBRACE")
    let mut stmts = []
    while not check(p, "RBRACE") and not check(p, "EOF") { push(stmts, statement(p)) }
    consume(p, "RBRACE")
    return ["Block", stmts]
}

fn expression(p) { return parse_assignment(p) }
fn parse_assignment(p) {
    let expr = parse_logic_or(p)
    if match_tok(p, "ASSIGN") {
        if expr[0] == "Ident" { return ["Assign", expr[1], parse_assignment(p)] }
        list_set(p, 2, true)
    }
    return expr
}
fn parse_logic_or(p) {
    let mut expr = parse_logic_and(p)
    while match_tok(p, "OR") { expr = ["Binary", "or", expr, parse_logic_and(p)] }
    return expr
}
fn parse_logic_and(p) {
    let mut expr = parse_equality(p)
    while match_tok(p, "AND") { expr = ["Binary", "and", expr, parse_equality(p)] }
    return expr
}
fn parse_equality(p) {
    let mut expr = parse_comparison(p)
    while true {
        if match_tok(p, "EQ") { expr = ["Binary", "==", expr, parse_comparison(p)] }
        else if match_tok(p, "NEQ") { expr = ["Binary", "!=", expr, parse_comparison(p)] }
        else { return expr }
    }
}
fn parse_comparison(p) {
    let mut expr = parse_term(p)
    while true {
        if match_tok(p, "LT") { expr = ["Binary", "<", expr, parse_term(p)] }
        else if match_tok(p, "GT") { expr = ["Binary", ">", expr, parse_term(p)] }
        else if match_tok(p, "LEQ") { expr = ["Binary", "<=", expr, parse_term(p)] }
        else if match_tok(p, "GEQ") { expr = ["Binary", ">=", expr, parse_term(p)] }
        else { return expr }
    }
}
fn parse_term(p) {
    let mut expr = parse_factor(p)
    while true {
        if match_tok(p, "PLUS") { expr = ["Binary", "+", expr, parse_factor(p)] }
        else if match_tok(p, "MINUS") { expr = ["Binary", "-", expr, parse_factor(p)] }
        else if match_tok(p, "CONCAT") { expr = ["Binary", "++", expr, parse_factor(p)] }
        else { return expr }
    }
}
fn parse_factor(p) {
    let mut expr = parse_unary(p)
    while true {
        if match_tok(p, "STAR") { expr = ["Binary", "*", expr, parse_unary(p)] }
        else if match_tok(p, "SLASH") { expr = ["Binary", "/", expr, parse_unary(p)] }
        else if match_tok(p, "PERCENT") { expr = ["Binary", "%", expr, parse_unary(p)] }
        else { return expr }
    }
}
fn parse_unary(p) {
    if match_tok(p, "NOT") { return ["Unary", "not", parse_unary(p)] }
    if match_tok(p, "MINUS") { return ["Unary", "-", parse_unary(p)] }
    return parse_call(p)
}
fn parse_call(p) {
    let mut expr = parse_primary(p)
    while true {
        if match_tok(p, "LPAREN") {
            let mut args = []
            if not check(p, "RPAREN") {
                push(args, expression(p))
                while match_tok(p, "COMMA") { push(args, expression(p)) }
            }
            consume(p, "RPAREN")
            expr = ["Call", expr, args]
        } else if match_tok(p, "LBRACKET") {
            let idx = expression(p)
            consume(p, "RBRACKET")
            expr = ["Index", expr, idx]
        } else if match_tok(p, "QUESTION") {
            expr = ["Unwrap", expr]
        } else {
            return expr
        }
    }
}
fn parse_primary(p) {
    if match_tok(p, "INT") { return ["Int", p[0][p[1]-1][1]] }
    if match_tok(p, "FLOAT") { return ["Float", p[0][p[1]-1][1]] }
    if match_tok(p, "STRING") { return ["String", p[0][p[1]-1][1]] }
    if match_tok(p, "TRUE") { return ["Bool", true] }
    if match_tok(p, "FALSE") { return ["Bool", false] }
    if match_tok(p, "NONE") { return ["None"] }
    if match_tok(p, "IDENT") { return ["Ident", p[0][p[1]-1][1]] }
    
    if match_tok(p, "SOME") { consume(p, "LPAREN") let expr = expression(p) consume(p, "RPAREN") return ["Some", expr] }
    if match_tok(p, "OK") { consume(p, "LPAREN") let expr = expression(p) consume(p, "RPAREN") return ["Ok", expr] }
    if match_tok(p, "ERR") { consume(p, "LPAREN") let expr = expression(p) consume(p, "RPAREN") return ["Err", expr] }
    
    if match_tok(p, "LPAREN") { let expr = expression(p) consume(p, "RPAREN") return expr }
    if match_tok(p, "LBRACKET") {
        let mut elems = []
        if not check(p, "RBRACKET") {
            push(elems, expression(p))
            while match_tok(p, "COMMA") { push(elems, expression(p)) }
        }
        consume(p, "RBRACKET")
        return ["ListLit", elems]
    }
    
    if match_tok(p, "IF") {
        let cond = expression(p)
        let then_b = parse_block(p)
        let mut else_b = ["None"]
        if match_tok(p, "ELSE") {
            if check(p, "IF") { else_b = statement(p) }
            else { else_b = parse_block(p) }
        }
        return ["If", cond, then_b, else_b]
    }
    if match_tok(p, "MATCH") {
        let expr = expression(p)
        consume(p, "LBRACE")
        let mut arms = []
        while not check(p, "RBRACE") and not check(p, "EOF") {
            let mut pat_type = "_"
            let mut pat_var = ""
            if match_tok(p, "SOME") { pat_type = "Some" consume(p, "LPAREN") pat_var = peek(p)[1] consume(p, "IDENT") consume(p, "RPAREN") }
            else if match_tok(p, "NONE") { pat_type = "None" }
            else if match_tok(p, "OK") { pat_type = "Ok" consume(p, "LPAREN") pat_var = peek(p)[1] consume(p, "IDENT") consume(p, "RPAREN") }
            else if match_tok(p, "ERR") { pat_type = "Err" consume(p, "LPAREN") pat_var = peek(p)[1] consume(p, "IDENT") consume(p, "RPAREN") }
            else if match_tok(p, "UNDERSCORE") { pat_type = "_" }
            else { list_set(p, 2, true) }
            
            consume(p, "FAT_ARROW")
            let arm_expr = expression(p)
            match_tok(p, "COMMA")
            push(arms, [pat_type, pat_var, arm_expr])
        }
        consume(p, "RBRACE")
        return ["Match", expr, arms]
    }
    if match_tok(p, "FN") {
        consume(p, "LPAREN")
        let mut params = []
        if not check(p, "RPAREN") {
            let p1 = peek(p)[1]
            consume(p, "IDENT")
            if match_tok(p, "COLON") { consume(p, "IDENT") }
            push(params, p1)
            while match_tok(p, "COMMA") {
                let p2 = peek(p)[1]
                consume(p, "IDENT")
                if match_tok(p, "COLON") { consume(p, "IDENT") }
                push(params, p2)
            }
        }
        consume(p, "RPAREN")
        if match_tok(p, "ARROW") { consume(p, "IDENT") }
        return ["Fn", "", params, parse_block(p)]
    }
    if check(p, "LBRACE") { return parse_block(p) }
    advance(p)
    return ["Unknown"]
}
"""

    eval_code = """
// evaluator.qk

fn call_fn(fval, args) {
    if type_of(fval) == "List" and fval[0] == "FnVal" {
        let call_env = env_new(fval[4])
        let mut j = 0
        while j < len(fval[2]) {
            let mut arg_val = None
            if j < len(args) { arg_val = args[j] }
            env_define(call_env, fval[2][j], arg_val, true)
            j = j + 1
        }
        let res = eval(fval[3], call_env)
        if type_of(res) == "List" and len(res) > 0 and res[0] == "ReturnVal" { return res[1] }
        return res
    }
    return None
}

fn eval(node, env) {
    if type_of(node) != "List" { return node }
    let type = node[0]
    
    if type == "Int" { return node[1] }
    if type == "Float" { return node[1] }
    if type == "String" { return node[1] }
    if type == "Bool" { return node[1] }
    
    if type == "Program" or type == "Block" {
        let stmts = node[1]
        let mut block_env = env
        if type == "Block" { block_env = env_new(env) }
        let mut last = None
        let mut i = 0
        while i < len(stmts) {
            last = eval(stmts[i], block_env)
            if type_of(last) == "List" and len(last) > 0 {
                let ltype = last[0]
                if ltype == "ReturnVal" {
                    if type == "Program" { return last[1] }
                    return last
                }
                if ltype == "BreakVal" or ltype == "ContinueVal" { return last }
            }
            i = i + 1
        }
        return last
    }
    
    if type == "Ident" {
        let res = env_get(env, node[1])
        return match res {
            Ok(v) => v,
            Err(e) => None
        }
    }
    if type == "Let" {
        let val = eval(node[3], env)
        if type_of(val) == "List" and len(val) > 0 and val[0] == "ReturnVal" { return val }
        env_define(env, node[1], val, node[2])
        return None
    }
    if type == "Shadow" {
        let val = eval(node[2], env)
        if type_of(val) == "List" and len(val) > 0 and val[0] == "ReturnVal" { return val }
        env_define(env, node[1], val, false)
        return None
    }
    if type == "Assign" {
        let val = eval(node[2], env)
        if type_of(val) == "List" and len(val) > 0 and val[0] == "ReturnVal" { return val }
        env_set(env, node[1], val)
        return val
    }
    if type == "If" {
        if eval(node[1], env) { return eval(node[2], env) }
        else if type_of(node[3]) != "None" { return eval(node[3], env) }
        return None
    }
    if type == "While" {
        let mut last = None
        while eval(node[1], env) {
            last = eval(node[2], env)
            if type_of(last) == "List" and len(last) > 0 {
                if last[0] == "ReturnVal" { return last }
                if last[0] == "BreakVal" { return None }
                if last[0] == "ContinueVal" { last = None }
            }
        }
        return last
    }
    if type == "For" {
        let var_name = node[1]
        let iter = eval(node[2], env)
        let mut last = None
        let mut i = 0
        while i < len(iter) {
            let loop_env = env_new(env)
            env_define(loop_env, var_name, iter[i], false)
            last = eval(node[3], loop_env)
            if type_of(last) == "List" and len(last) > 0 {
                if last[0] == "ReturnVal" { return last }
                if last[0] == "BreakVal" { return None }
                if last[0] == "ContinueVal" { last = None }
            }
            i = i + 1
        }
        return last
    }
    if type == "Break" { return ["BreakVal"] }
    if type == "Continue" { return ["ContinueVal"] }
    if type == "Fn" {
        let fval = val_fn(node[1], node[2], node[3], env)
        if node[1] != "" { env_define(env, node[1], fval, false) }
        return fval
    }
    if type == "Call" {
        let callee = eval(node[1], env)
        if type_of(callee) == "List" and len(callee) > 0 and callee[0] == "ReturnVal" { return callee }
        let args_ast = node[2]
        let mut args = []
        let mut i = 0
        while i < len(args_ast) {
            let arg_val = eval(args_ast[i], env)
            if type_of(arg_val) == "List" and len(arg_val) > 0 and arg_val[0] == "ReturnVal" { return arg_val }
            push(args, arg_val)
            i = i + 1
        }
        
        if type_of(callee) == "List" {
            if callee[0] == "Builtin" {
                let bn = callee[1]
                if bn == "println" { println(args[0]) return None }
                if bn == "print" { print(args[0]) return None }
                if bn == "range" {
                    let start = args[0]
                    let end = args[1]
                    let mut arr = []
                    let mut r = start
                    while r < end { push(arr, r) r = r + 1 }
                    return arr
                }
                if bn == "capture_println" { capture_println(args[0]) return None }
                if bn == "file_read" { return file_read(args[0]) }
                if bn == "file_write" { return file_write(args[0], args[1]) }
                if bn == "exec" { return exec(args[0]) }
                if bn == "os_name" { return os_name() }
                if bn == "type_of" { return type_of(args[0]) }
                if bn == "len" { return len(args[0]) }
                if bn == "push" { push(args[0], args[1]) return None }
                if bn == "list_set" { list_set(args[0], args[1], args[2]) return None }
                if bn == "substring" { return substring(args[0], args[1], args[2]) }
                if bn == "to_int" { return to_int(args[0]) }
                if bn == "to_float" { return to_float(args[0]) }
                if bn == "to_string" { return to_string(args[0]) }
                if bn == "unwrap_or" { return unwrap_or(args[0], args[1]) }
                if bn == "map" {
                    let opt = args[0] let fval = args[1]
                    if type_of(opt) == "Option" {
                        let is_some = match opt { Some(v) => true, None => false }
                        if is_some { let v = match opt { Some(x) => x, None => None } return Some(call_fn(fval, [v])) }
                        else { return None }
                    } else if type_of(opt) == "Result" {
                        let is_ok = match opt { Ok(v) => true, Err(e) => false }
                        if is_ok { let v = match opt { Ok(x) => x, Err(e) => None } return Ok(call_fn(fval, [v])) }
                        else { return opt }
                    }
                    return None
                }
                if bn == "and_then" {
                    let opt = args[0] let fval = args[1]
                    if type_of(opt) == "Option" {
                        let is_some = match opt { Some(v) => true, None => false }
                        if is_some { let v = match opt { Some(x) => x, None => None } return call_fn(fval, [v]) }
                        else { return None }
                    }
                    return None
                }
                if bn == "hash_sha256" { return hash_sha256(args[0]) }
                if bn == "serialize_ast" { return serialize_ast(args[0]) }
                if bn == "parse_native" { return parse_native(args[0]) }
                if bn == "eval_native" { return eval_native(args[0]) }
                if bn == "get_captured" { return get_captured() }
                if bn == "set_capture_mode" { return set_capture_mode(args[0]) }
                if bn == "clear_captured" { return clear_captured() }
            } else if callee[0] == "FnVal" {
                return call_fn(callee, args)
            }
        }
        return None
    }
    
    if type == "Binary" {
        let op = node[1]
        let l = eval(node[2], env)
        if type_of(l) == "List" and len(l) > 0 and l[0] == "ReturnVal" { return l }
        let r = eval(node[3], env)
        if type_of(r) == "List" and len(r) > 0 and r[0] == "ReturnVal" { return r }
        
        if op == "+" { return l + r } if op == "-" { return l - r }
        if op == "*" { return l * r } if op == "/" { return l / r }
        if op == "%" { return l % r } if op == "==" { return l == r }
        if op == "!=" { return l != r } if op == "<" { return l < r }
        if op == ">" { return l > r } if op == "<=" { return l <= r }
        if op == ">=" { return l >= r } if op == "++" { return to_string(l) ++ to_string(r) }
        if op == "and" { return l and r } if op == "or" { return l or r }
    }
    if type == "Unary" {
        let op = node[1] let r = eval(node[2], env)
        if op == "-" { return -r } if op == "not" { return not r }
    }
    if type == "ListLit" {
        let mut list = []
        let mut i = 0
        while i < len(node[1]) { push(list, eval(node[1][i], env)) i = i + 1 }
        return list
    }
    if type == "Index" {
        let arr = eval(node[1], env)
        return arr[eval(node[2], env)]
    }
    if type == "Return" { return ["ReturnVal", eval(node[1], env)] }
    if type == "Some" { return Some(eval(node[1], env)) }
    if type == "Ok" { return Ok(eval(node[1], env)) }
    if type == "Err" { return Err(eval(node[1], env)) }
    if type == "None" { return None }
    if type == "Unwrap" {
        let res = eval(node[1], env)
        if type_of(res) == "List" and len(res) > 0 and res[0] == "ReturnVal" { return res }
        if type_of(res) == "Option" {
            let is_some = match res { Some(v) => true, None => false }
            if is_some { return match res { Some(v) => v, None => None } }
            else { return ["ReturnVal", res] }
        } else if type_of(res) == "Result" {
            let is_ok = match res { Ok(v) => true, Err(e) => false }
            if is_ok { return match res { Ok(v) => v, Err(e) => None } }
            else { return ["ReturnVal", res] }
        }
        return res
    }
    if type == "Match" {
        let val = eval(node[1], env)
        let arms = node[2]
        let mut i = 0
        while i < len(arms) {
            let arm = arms[i]
            let mut matched = false
            let mut bound_val = None
            if arm[0] == "_" { matched = true }
            else if type_of(val) == "Option" {
                if arm[0] == "Some" {
                    let is_some = match val { Some(v) => true, None => false }
                    if is_some { matched = true bound_val = match val { Some(v) => v, None => None } }
                } else if arm[0] == "None" {
                    let is_none = match val { Some(v) => false, None => true }
                    if is_none { matched = true }
                }
            } else if type_of(val) == "Result" {
                if arm[0] == "Ok" {
                    let is_ok = match val { Ok(v) => true, Err(e) => false }
                    if is_ok { matched = true bound_val = match val { Ok(v) => v, Err(e) => None } }
                } else if arm[0] == "Err" {
                    let is_err = match val { Ok(v) => false, Err(e) => true }
                    if is_err { matched = true bound_val = match val { Ok(v) => None, Err(e) => e } }
                }
            }
            if matched {
                let match_env = env_new(env)
                if arm[1] != "" { env_define(match_env, arm[1], bound_val, false) }
                return eval(arm[2], match_env)
            }
            i = i + 1
        }
        return None
    }
    return None
}
"""
    with open("src/parser.qka", "w") as f: f.write(parser_code)
    with open("src/evaluator.qka", "w") as f: f.write(eval_code)
    
    files = ["src/ast.qka", "src/errors.qka", "src/values.qka", "src/environment.qka", "src/lexer.qka", "src/parser.qka", "src/evaluator.qka", "src/main.qka"]
    with open("src/quokka_interpreter.qka", "w") as out:
        for fname in files:
            with open(fname, "r") as inf:
                out.write(inf.read() + "\n")

generate()
