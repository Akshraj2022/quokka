#!/usr/bin/env python3
"""
100-Way Verification Test Suite for Quokka Full-Fledged Language
Covers:
  - Bytecode Compiler & Virtual Machine (--vm) (Tests 1 - 40)
  - Static Type Checker (--check) (Tests 41 - 70)
  - Cross-Platform Concurrency Primitives (Threads & Channels) (Tests 71 - 100)
"""

import os
import sys
import tempfile
import subprocess
import time

def find_quokka_binary():
    if os.name == 'nt':
        if os.path.exists("quokka.exe"):
            return os.path.abspath("quokka.exe")
        if os.path.exists("build/quokka.exe"):
            return os.path.abspath("build/quokka.exe")
    else:
        if os.path.exists("build/quokka"):
            return os.path.abspath("build/quokka")
        if os.path.exists("./quokka"):
            return os.path.abspath("./quokka")
    return "quokka"

def run_qk(binary, mode, code, timeout=10):
    with tempfile.NamedTemporaryFile("w", suffix=".qk", delete=False) as f:
        f.write(code)
        tmp_path = f.name

    try:
        cmd = [binary]
        if mode == "vm":
            cmd.append("--vm")
        elif mode == "check":
            cmd.append("--check")
        cmd.append(tmp_path)

        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout
        )
        return proc.returncode, proc.stdout.strip(), proc.stderr.strip()
    finally:
        if os.path.exists(tmp_path):
            try:
                os.remove(tmp_path)
            except OSError:
                pass

def main():
    binary = find_quokka_binary()
    print("=" * 70)
    print(" QUOKKA FULL-FLEDGED 100-WAY VERIFICATION SUITE")
    print(f" Target Binary: {binary}")
    print("=" * 70)

    tests = []

    # =========================================================================
    # PART 1: BYTECODE COMPILER & VIRTUAL MACHINE (VM) (Tests 1 - 40)
    # =========================================================================
    tests.append((
        "vm_add_int", "vm",
        "println(10 + 25)",
        lambda code, out, err: code == 0 and "35" in out
    ))
    tests.append((
        "vm_sub_int", "vm",
        "println(50 - 18)",
        lambda code, out, err: code == 0 and "32" in out
    ))
    tests.append((
        "vm_mul_int", "vm",
        "println(6 * 7)",
        lambda code, out, err: code == 0 and "42" in out
    ))
    tests.append((
        "vm_div_int", "vm",
        "println(100 / 4)",
        lambda code, out, err: code == 0 and "25" in out
    ))
    tests.append((
        "vm_mod_int", "vm",
        "println(17 % 5)",
        lambda code, out, err: code == 0 and "2" in out
    ))
    tests.append((
        "vm_precedence", "vm",
        "println(2 + 3 * 4)",
        lambda code, out, err: code == 0 and "14" in out
    ))
    tests.append((
        "vm_parentheses", "vm",
        "println((2 + 3) * 4)",
        lambda code, out, err: code == 0 and "20" in out
    ))
    tests.append((
        "vm_unary_negate", "vm",
        "println(-42 + 50)",
        lambda code, out, err: code == 0 and "8" in out
    ))
    tests.append((
        "vm_float_add", "vm",
        "println(1.5 + 2.5)",
        lambda code, out, err: code == 0 and ("4" in out)
    ))
    tests.append((
        "vm_float_mul", "vm",
        "println(2.5 * 4.0)",
        lambda code, out, err: code == 0 and ("10" in out)
    ))
    tests.append((
        "vm_bool_true", "vm",
        "println(true)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_bool_false", "vm",
        "println(false)",
        lambda code, out, err: code == 0 and "false" in out
    ))
    tests.append((
        "vm_bool_not", "vm",
        "println(not false)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_cmp_eq", "vm",
        "println(10 == 10)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_cmp_neq", "vm",
        "println(10 != 20)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_cmp_lt", "vm",
        "println(5 < 10)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_cmp_gt", "vm",
        "println(10 > 5)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_cmp_leq", "vm",
        "println(10 <= 10)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_cmp_geq", "vm",
        "println(15 >= 10)",
        lambda code, out, err: code == 0 and "true" in out
    ))
    tests.append((
        "vm_str_lit", "vm",
        "println(\"hello world\")",
        lambda code, out, err: code == 0 and "hello world" in out
    ))
    tests.append((
        "vm_str_concat", "vm",
        "println(\"foo\" ++ \"bar\")",
        lambda code, out, err: code == 0 and "foobar" in out
    ))
    tests.append((
        "vm_str_index", "vm",
        "let s = \"quokka\"\nprintln(s[0])",
        lambda code, out, err: code == 0 and "q" in out
    ))
    tests.append((
        "vm_if_true", "vm",
        "if true { println(\"yes\") } else { println(\"no\") }",
        lambda code, out, err: code == 0 and "yes" in out
    ))
    tests.append((
        "vm_if_false", "vm",
        "if false { println(\"yes\") } else { println(\"no\") }",
        lambda code, out, err: code == 0 and "no" in out
    ))
    tests.append((
        "vm_if_expr", "vm",
        "let x = if 5 > 3 { 100 } else { 200 }\nprintln(x)",
        lambda code, out, err: code == 0 and "100" in out
    ))
    tests.append((
        "vm_while_basic", "vm",
        "let mut count = 0\nwhile count < 5 {\n    count = count + 1\n}\nprintln(count)",
        lambda code, out, err: code == 0 and "5" in out
    ))
    tests.append((
        "vm_while_accum", "vm",
        "let mut i = 1\nlet mut sum = 0\nwhile i <= 10 {\n    sum = sum + i\n    i = i + 1\n}\nprintln(sum)",
        lambda code, out, err: code == 0 and "55" in out
    ))
    tests.append((
        "vm_while_nested", "vm",
        "let mut i = 0\nlet mut sum = 0\nwhile i < 3 {\n    let mut j = 0\n    while j < 3 {\n        sum = sum + 1\n        j = j + 1\n    }\n    i = i + 1\n}\nprintln(sum)",
        lambda code, out, err: code == 0 and "9" in out
    ))
    tests.append((
        "vm_for_range", "vm",
        "let mut sum = 0\nfor x in range(1, 5) {\n    sum = sum + x\n}\nprintln(sum)",
        lambda code, out, err: code == 0 and "10" in out
    ))
    tests.append((
        "vm_for_list", "vm",
        "let mut acc = 0\nfor x in [10, 20, 30] {\n    acc = acc + x\n}\nprintln(acc)",
        lambda code, out, err: code == 0 and "60" in out
    ))
    tests.append((
        "vm_block_scope", "vm",
        "let x = 10\n{\n    let x = 20\n    println(x)\n}\nprintln(x)",
        lambda code, out, err: code == 0 and "20\n10" in out.replace("\r", "")
    ))
    tests.append((
        "vm_block_expr", "vm",
        "let y = {\n    let a = 15\n    let b = 25\n    a + b\n}\nprintln(y)",
        lambda code, out, err: code == 0 and "40" in out
    ))
    tests.append((
        "vm_list_lit", "vm",
        "let l = [1, 2, 3, 4, 5]\nprintln(len(l))",
        lambda code, out, err: code == 0 and "5" in out
    ))
    tests.append((
        "vm_list_index", "vm",
        "let l = [100, 200, 300]\nprintln(l[1])",
        lambda code, out, err: code == 0 and "200" in out
    ))
    tests.append((
        "vm_struct_basic", "vm",
        "struct Point { x, y }\nlet p = Point(10, 20)\nprintln(p.x)\nprintln(p.y)",
        lambda code, out, err: code == 0 and "10\n20" in out.replace("\r", "")
    ))
    tests.append((
        "vm_struct_mutation", "vm",
        "struct Point { x, y }\nlet p = Point(10, 20)\np.x = 99\nprintln(p.x)",
        lambda code, out, err: code == 0 and "99" in out
    ))
    tests.append((
        "vm_fn_call", "vm",
        "fn square(x) { x * x }\nprintln(square(7))",
        lambda code, out, err: code == 0 and "49" in out
    ))
    tests.append((
        "vm_fn_multiarity", "vm",
        "fn avg(a, b, c) { (a + b + c) / 3 }\nprintln(avg(10, 20, 30))",
        lambda code, out, err: code == 0 and "20" in out
    ))
    tests.append((
        "vm_fn_recursion", "vm",
        "fn fib(n) {\n    if n <= 1 { n } else { fib(n - 1) + fib(n - 2) }\n}\nprintln(fib(8))",
        lambda code, out, err: code == 0 and "21" in out
    ))
    tests.append((
        "vm_option_result", "vm",
        "let opt = Some(42)\nlet res = Ok(99)\nprintln(unwrap_or(opt, 0))\nprintln(unwrap_or(res, 0))",
        lambda code, out, err: code == 0 and "42\n99" in out.replace("\r", "")
    ))

    # =========================================================================
    # PART 2: STATIC TYPE CHECKER (Tests 41 - 70)
    # =========================================================================
    tests.append((
        "tc_valid_int", "check",
        "let x = 42",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_float", "check",
        "let f = 3.14159",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_bool", "check",
        "let b = true",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_str", "check",
        "let s = \"hello quokka\"",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_add", "check",
        "let c = 10 + 20",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_concat", "check",
        "let s = \"hello \" ++ \"world\"",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_cmp", "check",
        "let b = 10 < 20",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_if", "check",
        "let x = if true { 1 } else { 2 }",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_while", "check",
        "let mut i = 0\nwhile i < 10 { i = i + 1 }",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_list", "check",
        "let l = [1, 2, 3, 4]",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_list_idx", "check",
        "let l = [10, 20]\nlet x = l[0]",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_fn", "check",
        "fn add(a, b) { a + b }\nlet x = add(1, 2)",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_struct", "check",
        "struct Point { x, y }",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_field", "check",
        "struct Point { x, y }\nlet p = Point(1, 2)\nlet px = p.x",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_opt", "check",
        "let o = Some(10)",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_res", "check",
        "let r = Ok(20)",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_for", "check",
        "for x in [1, 2, 3] { print(x) }",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_mut", "check",
        "let mut x = 10\nx = 20",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_channels", "check",
        "let ch = channel_new(1)\nchannel_send(ch, 10)\nchannel_recv(ch)\nchannel_close(ch)",
        lambda code, out, err: code == 0
    ))
    tests.append((
        "tc_valid_threads", "check",
        "let th = thread_spawn(fn() { 42 })\nthread_join(th)",
        lambda code, out, err: code == 0
    ))
    # Negative typecheck tests: MUST FAIL (code == 1)
    tests.append((
        "tc_err_add_str_int", "check",
        "let x = 10 + \"hello\"",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_sub_str", "check",
        "let x = \"foo\" - \"bar\"",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_mul_bool", "check",
        "let x = true * 5",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_div_str", "check",
        "let x = 100 / \"two\"",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_concat_int", "check",
        "let s = 10 ++ 20",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_if_cond", "check",
        "if 123 { 1 }",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_while_cond", "check",
        "while \"not_bool\" { 1 }",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_negate_str", "check",
        "let x = -\"text\"",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_not_int", "check",
        "let x = not 42",
        lambda code, out, err: code != 0 and "Type error" in err
    ))
    tests.append((
        "tc_err_struct_field", "check",
        "struct Point { x, y }\nlet p = Point(1, 2)\nlet z = p.z",
        lambda code, out, err: code != 0 and "Type error" in err
    ))

    # =========================================================================
    # PART 3: CONCURRENCY PRIMITIVES (Tests 71 - 100)
    # =========================================================================
    tests.append((
        "conc_chan_unbuf_create", "run",
        "let ch = channel_new(0)\nprintln(\"created unbuffered\")",
        lambda code, out, err: code == 0 and "created unbuffered" in out
    ))
    tests.append((
        "conc_chan_buf_create", "run",
        "let ch = channel_new(5)\nprintln(\"created buffered\")",
        lambda code, out, err: code == 0 and "created buffered" in out
    ))
    tests.append((
        "conc_chan_close_empty", "run",
        "let ch = channel_new(2)\nchannel_close(ch)\nlet r = channel_recv(ch)\nprintln(unwrap_or(r, \"closed\"))",
        lambda code, out, err: code == 0 and "closed" in out
    ))
    tests.append((
        "conc_chan_close_drain", "run",
        "let ch = channel_new(2)\nchannel_send(ch, 777)\nchannel_close(ch)\nlet r1 = channel_recv(ch)\nlet r2 = channel_recv(ch)\nprintln(unwrap_or(r1, 0))\nprintln(unwrap_or(r2, -1))",
        lambda code, out, err: code == 0 and "777\n-1" in out.replace("\r", "")
    ))
    tests.append((
        "conc_thread_basic", "run",
        "let th = thread_spawn(fn() { 12345 })\nlet r = thread_join(th)\nprintln(unwrap_or(r, 0))",
        lambda code, out, err: code == 0 and "12345" in out
    ))
    tests.append((
        "conc_thread_str", "run",
        "let th = thread_spawn(fn() { \"thread output\" })\nlet r = thread_join(th)\nprintln(unwrap_or(r, \"\"))",
        lambda code, out, err: code == 0 and "thread output" in out
    ))
    tests.append((
        "conc_thread_calc", "run",
        "let th = thread_spawn(fn() {\n    let mut sum = 0\n    for x in range(1, 101) { sum = sum + x }\n    sum\n})\nlet r = thread_join(th)\nprintln(unwrap_or(r, 0))",
        lambda code, out, err: code == 0 and "5050" in out
    ))
    tests.append((
        "conc_thread_channel_single", "run",
        "let ch = channel_new(1)\nlet th = thread_spawn(fn() {\n    channel_send(ch, 42)\n    0\n})\nlet msg = channel_recv(ch)\nthread_join(th)\nprintln(unwrap_or(msg, 0))",
        lambda code, out, err: code == 0 and "42" in out
    ))
    tests.append((
        "conc_mpsc_2", "run",
        """
let ch = channel_new(2)
let t1 = thread_spawn(fn() {
    channel_send(ch, 10)
    0
})
let t2 = thread_spawn(fn() {
    channel_send(ch, 20)
    0
})
let r1 = unwrap_or(channel_recv(ch), 0)
let r2 = unwrap_or(channel_recv(ch), 0)
thread_join(t1)
thread_join(t2)
println(r1 + r2)
""",
        lambda code, out, err: code == 0 and "30" in out
    ))
    tests.append((
        "conc_mpsc_4", "run",
        """
let ch = channel_new(4)
let t1 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t2 = thread_spawn(fn() {
    channel_send(ch, 2)
    0
})
let t3 = thread_spawn(fn() {
    channel_send(ch, 3)
    0
})
let t4 = thread_spawn(fn() {
    channel_send(ch, 4)
    0
})
let mut sum = 0
for i in range(1, 5) {
    sum = sum + unwrap_or(channel_recv(ch), 0)
}
thread_join(t1)
thread_join(t2)
thread_join(t3)
thread_join(t4)
println(sum)
""",
        lambda code, out, err: code == 0 and "10" in out
    ))
    tests.append((
        "conc_mpsc_8", "run",
        """
let ch = channel_new(8)
let t1 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t2 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t3 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t4 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t5 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t6 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t7 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let t8 = thread_spawn(fn() {
    channel_send(ch, 1)
    0
})
let mut sum = 0
for i in range(1, 9) {
    sum = sum + unwrap_or(channel_recv(ch), 0)
}
thread_join(t1)
thread_join(t2)
thread_join(t3)
thread_join(t4)
thread_join(t5)
thread_join(t6)
thread_join(t7)
thread_join(t8)
println(sum)
""",
        lambda code, out, err: code == 0 and "8" in out
    ))
    tests.append((
        "conc_pipeline", "run",
        """
let ch_in = channel_new(4)
let ch_out = channel_new(4)
let worker = thread_spawn(fn() {
    for i in range(1, 4) {
        let n = unwrap_or(channel_recv(ch_in), 0)
        channel_send(ch_out, n * n)
    }
    0
})
channel_send(ch_in, 2)
channel_send(ch_in, 3)
channel_send(ch_in, 4)
let mut sum = 0
for i in range(1, 4) {
    sum = sum + unwrap_or(channel_recv(ch_out), 0)
}
thread_join(worker)
println(sum)
""",
        lambda code, out, err: code == 0 and "29" in out # 4 + 9 + 16 = 29
    ))
    tests.append((
        "conc_ping_pong", "run",
        """
let ping = channel_new(1)
let pong = channel_new(1)
let peer = thread_spawn(fn() {
    let msg = unwrap_or(channel_recv(ping), "")
    channel_send(pong, msg ++ " pong")
    0
})
channel_send(ping, "ping")
let res = unwrap_or(channel_recv(pong), "")
thread_join(peer)
println(res)
""",
        lambda code, out, err: code == 0 and "ping pong" in out
    ))
    tests.append((
        "conc_producer_consumer", "run",
        """
let ch = channel_new(5)
let prod = thread_spawn(fn() {
    for i in range(1, 6) {
        channel_send(ch, i * 10)
    }
    channel_close(ch)
    0
})
let mut sum = 0
let mut active = true
while active {
    let r = channel_recv(ch)
    match r {
        Ok(v) => { sum = sum + v },
        Err(e) => { active = false }
    }
}
thread_join(prod)
println(sum)
""",
        lambda code, out, err: code == 0 and "150" in out
    ))
    tests.append((
        "conc_buffered_batch", "run",
        """
let ch = channel_new(3)
channel_send(ch, 100)
channel_send(ch, 200)
channel_send(ch, 300)
let v1 = unwrap_or(channel_recv(ch), 0)
let v2 = unwrap_or(channel_recv(ch), 0)
let v3 = unwrap_or(channel_recv(ch), 0)
println(v1 + v2 + v3)
""",
        lambda code, out, err: code == 0 and "600" in out
    ))
    tests.append((
        "conc_chan_fifo", "run",
        """
let ch = channel_new(3)
channel_send(ch, 1)
channel_send(ch, 2)
channel_send(ch, 3)
println(unwrap_or(channel_recv(ch), 0))
println(unwrap_or(channel_recv(ch), 0))
println(unwrap_or(channel_recv(ch), 0))
""",
        lambda code, out, err: code == 0 and "1\n2\n3" in out.replace("\r", "")
    ))
    tests.append((
        "conc_thread_closure", "run",
        """
let captured = 42
let th = thread_spawn(fn() {
    captured * 2
})
let r = thread_join(th)
println(unwrap_or(r, 0))
""",
        lambda code, out, err: code == 0 and "84" in out
    ))
    tests.append((
        "conc_worker_pool", "run",
        """
let tasks = channel_new(6)
let results = channel_new(6)
let w1 = thread_spawn(fn() {
    let t = unwrap_or(channel_recv(tasks), 0)
    channel_send(results, t * 10)
    0
})
let w2 = thread_spawn(fn() {
    let t = unwrap_or(channel_recv(tasks), 0)
    channel_send(results, t * 10)
    0
})
channel_send(tasks, 5)
channel_send(tasks, 7)
let r1 = unwrap_or(channel_recv(results), 0)
let r2 = unwrap_or(channel_recv(results), 0)
thread_join(w1)
thread_join(w2)
println(r1 + r2)
""",
        lambda code, out, err: code == 0 and "120" in out
    ))
    tests.append((
        "conc_heavy_sequential", "run",
        """
let mut total = 0
for i in range(1, 6) {
    let th = thread_spawn(fn() { 10 })
    total = total + unwrap_or(thread_join(th), 0)
}
println(total)
""",
        lambda code, out, err: code == 0 and "50" in out
    ))
    tests.append((
        "conc_multi_join", "run",
        """
let t1 = thread_spawn(fn() { 100 })
let t2 = thread_spawn(fn() { 200 })
let t3 = thread_spawn(fn() { 300 })
let r1 = unwrap_or(thread_join(t1), 0)
let r2 = unwrap_or(thread_join(t2), 0)
let r3 = unwrap_or(thread_join(t3), 0)
println(r1 + r2 + r3)
""",
        lambda code, out, err: code == 0 and "600" in out
    ))
    tests.append((
        "conc_chan_struct", "run",
        """
struct Msg { id, text }
let ch = channel_new(1)
let th = thread_spawn(fn() {
    channel_send(ch, Msg(1, "hello struct"))
    0
})
let m = unwrap_or(channel_recv(ch), Msg(0, ""))
thread_join(th)
println(m.text)
""",
        lambda code, out, err: code == 0 and "hello struct" in out
    ))
    tests.append((
        "conc_chan_list", "run",
        """
let ch = channel_new(1)
let th = thread_spawn(fn() {
    channel_send(ch, [10, 20, 30])
    0
})
let lst = unwrap_or(channel_recv(ch), [])
thread_join(th)
println(len(lst))
println(lst[1])
""",
        lambda code, out, err: code == 0 and "3\n20" in out.replace("\r", "")
    ))
    tests.append((
        "conc_thread_struct_return", "run",
        """
struct Res { code, desc }
let th = thread_spawn(fn() {
    Res(200, "OK")
})
let r = unwrap_or(thread_join(th), Res(0, ""))
println(r.code)
println(r.desc)
""",
        lambda code, out, err: code == 0 and "200\nOK" in out.replace("\r", "")
    ))
    tests.append((
        "conc_thread_list_return", "run",
        """
let th = thread_spawn(fn() {
    [100, 200, 300]
})
let r = unwrap_or(thread_join(th), [])
println(r[2])
""",
        lambda code, out, err: code == 0 and "300" in out
    ))
    tests.append((
        "conc_concurrent_counters", "run",
        """
let t1 = thread_spawn(fn() {
    let mut c = 0
    while c < 100 { c = c + 1 }
    c
})
let t2 = thread_spawn(fn() {
    let mut c = 0
    while c < 200 { c = c + 1 }
    c
})
let r1 = unwrap_or(thread_join(t1), 0)
let r2 = unwrap_or(thread_join(t2), 0)
println(r1 + r2)
""",
        lambda code, out, err: code == 0 and "300" in out
    ))
    tests.append((
        "conc_fan_out", "run",
        """
let ch1 = channel_new(1)
let ch2 = channel_new(1)
let publisher = thread_spawn(fn() {
    channel_send(ch1, 50)
    channel_send(ch2, 70)
    0
})
let r1 = unwrap_or(channel_recv(ch1), 0)
let r2 = unwrap_or(channel_recv(ch2), 0)
thread_join(publisher)
println(r1 + r2)
""",
        lambda code, out, err: code == 0 and "120" in out
    ))
    tests.append((
        "conc_fan_in", "run",
        """
let agg = channel_new(2)
let w1 = thread_spawn(fn() {
    channel_send(agg, 15)
    0
})
let w2 = thread_spawn(fn() {
    channel_send(agg, 25)
    0
})
let r1 = unwrap_or(channel_recv(agg), 0)
let r2 = unwrap_or(channel_recv(agg), 0)
thread_join(w1)
thread_join(w2)
println(r1 + r2)
""",
        lambda code, out, err: code == 0 and "40" in out
    ))
    tests.append((
        "conc_close_broadcast", "run",
        """
let ch = channel_new(1)
let th = thread_spawn(fn() {
    channel_close(ch)
    0
})
let r = channel_recv(ch)
thread_join(th)
match r {
    Ok(v) => { println("unexpected") },
    Err(e) => { println("unblocked on close") }
}
""",
        lambda code, out, err: code == 0 and "unblocked on close" in out
    ))
    tests.append((
        "conc_bidirectional", "run",
        """
let c_req = channel_new(1)
let c_resp = channel_new(1)
let server = thread_spawn(fn() {
    let req = unwrap_or(channel_recv(c_req), 0)
    channel_send(c_resp, req * 2)
    0
})
channel_send(c_req, 21)
let ans = unwrap_or(channel_recv(c_resp), 0)
thread_join(server)
println(ans)
""",
        lambda code, out, err: code == 0 and "42" in out
    ))
    tests.append((
        "conc_full_matrix", "run",
        """
struct Job { id, val }
let ch = channel_new(2)
let worker = thread_spawn(fn() {
    let j = unwrap_or(channel_recv(ch), Job(0, 0))
    j.val * 3
})
channel_send(ch, Job(1, 33))
let res = unwrap_or(thread_join(worker), 0)
println(res)
""",
        lambda code, out, err: code == 0 and "99" in out
    ))

    # Verify we have exactly 100 tests
    total_tests = len(tests)
    print(f"Total test count: {total_tests}")

    passed = 0
    failed = 0
    failures = []

    start_time = time.time()

    for idx, (name, mode, code, validator) in enumerate(tests, 1):
        try:
            ret, out, err = run_qk(binary, mode, code)
            ok = validator(ret, out, err)
            if ok:
                passed += 1
                status = "PASS"
            else:
                failed += 1
                status = f"FAIL (code={ret}, out={repr(out[:40])}, err={repr(err[:40])})"
                failures.append((idx, name, mode, ret, out, err))
        except Exception as e:
            failed += 1
            status = f"ERROR ({e})"
            failures.append((idx, name, mode, -1, "", str(e)))

        print(f"[{idx:3d}/{total_tests:3d}] [{mode.upper():5s}] {name:28s} ... {status}")

    elapsed = time.time() - start_time

    print("=" * 70)
    print(f" SUMMARY: {passed}/{total_tests} passed ({failed} failed) in {elapsed:.2f}s")
    print("=" * 70)

    if failures:
        print("\nFailed Tests Details:")
        for idx, name, mode, ret, out, err in failures:
            print(f"Test #{idx} [{name}] ({mode}): code={ret}")
            if out:
                print(f"  STDOUT: {out}")
            if err:
                print(f"  STDERR: {err}")
        sys.exit(1)
    else:
        print("ALL 100 VERIFICATION TESTS PASSED PERFECTLY!")
        sys.exit(0)

if __name__ == "__main__":
    main()
