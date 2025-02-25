// |jit-test| skip-if: !wasmBulkMemSupported()

// Perform a test which,
//
// * if errKind is defined, is expected to fail with an exception
//   characterised by errKind and errText.
//
// * if errKind is undefined, is expected to succeed, in which case errKind
//   and errText are ignored.
//
// The function body will be [insn1, insn2].  isMem controls whether the
// module is constructed with memory or table initializers.  haveMemOrTable
// determines whether there is actually a memory or table to work with.

function do_test(insn1, insn2, errKind, errText, isMem, haveMemOrTable)
{
    let preamble;
    if (isMem) {
        let mem_def  = haveMemOrTable ? "(memory 1 1)" : "";
        let mem_init = haveMemOrTable
                       ? `(data (i32.const 2) "\\03\\01\\04\\01")
                          (data passive "\\02\\07\\01\\08")
                          (data (i32.const 12) "\\07\\05\\02\\03\\06")
                          (data passive "\\05\\09\\02\\07\\06")`
                       : "";
        preamble
            = `;; -------- Memories --------
               ${mem_def}
               ;; -------- Memory initialisers --------
               ${mem_init}
              `;
    } else {
        let tab_def  = haveMemOrTable ? "(table 30 30 anyfunc)" : "";
        let tab_init = haveMemOrTable
                       ? `(elem (i32.const 2) 3 1 4 1)
                          (elem passive 2 7 1 8)
                          (elem (i32.const 12) 7 5 2 3 6)
                          (elem passive 5 9 2 7 6)`
                       : "";
        preamble
            = `;; -------- Tables --------
               ${tab_def}
               ;; -------- Table initialisers --------
               ${tab_init}
               ;; ------ Functions (0..9) referred by the table/esegs ------
               (func (result i32) (i32.const 0))
               (func (result i32) (i32.const 1))
               (func (result i32) (i32.const 2))
               (func (result i32) (i32.const 3))
               (func (result i32) (i32.const 4))
               (func (result i32) (i32.const 5))
               (func (result i32) (i32.const 6))
               (func (result i32) (i32.const 7))
               (func (result i32) (i32.const 8))
               (func (result i32) (i32.const 9))
              `;
    }

    let txt = "(module\n" + preamble +
              `;; -------- testfn --------
               (func (export "testfn")
                 ${insn1}
                 ${insn2}
               )
               )`;

    if (!!errKind) {
        assertErrorMessage(
            () => {
                let inst = wasmEvalText(txt);
                inst.exports.testfn();
            },
            errKind,
            errText
        );
    } else {
        let inst = wasmEvalText(txt);
        assertEq(undefined, inst.exports.testfn());
    }
}

function mem_test(insn1, insn2, errKind, errText, haveMem=true) {
    do_test(insn1, insn2, errKind, errText,
            /*isMem=*/true, haveMem);
}

function mem_test_nofail(insn1, insn2) {
    do_test(insn1, insn2, undefined, undefined,
            /*isMem=*/true, /*haveMemOrTable=*/true);
}

function tab_test(insn1, insn2, errKind, errText, haveTab=true) {
    do_test(insn1, insn2, errKind, errText,
            /*isMem=*/false, haveTab);
}

function tab_test_nofail(insn1, insn2) {
    do_test(insn1, insn2, undefined, undefined,
            /*isMem=*/false, /*haveMemOrTable=*/true);
}


//---- memory.{drop,init} -------------------------------------------------

// drop with no memory
mem_test("memory.drop 3", "",
         WebAssembly.CompileError, /can't touch memory without memory/,
         false);

// init with no memory
mem_test("(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /can't touch memory without memory/,
         false);

// drop with data seg ix out of range
mem_test("memory.drop 4", "",
         WebAssembly.CompileError, /memory.{drop,init} index out of range/);

// init with data seg ix out of range
mem_test("(memory.init 4 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /memory.{drop,init} index out of range/);

// drop with data seg ix indicating an active segment
mem_test("memory.drop 2", "",
         WebAssembly.RuntimeError, /use of invalid passive data segment/);

// init with data seg ix indicating an active segment
mem_test("(memory.init 2 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.RuntimeError, /use of invalid passive data segment/);

// init, using a data seg ix more than once is OK
mem_test_nofail(
    "(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))",
    "(memory.init 1 (i32.const 4321) (i32.const 1) (i32.const 1))");

// drop, then drop
mem_test("memory.drop 1",
         "memory.drop 1",
         WebAssembly.RuntimeError, /use of invalid passive data segment/);

// drop, then init
mem_test("memory.drop 1",
         "(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))",
         WebAssembly.RuntimeError, /use of invalid passive data segment/);

// init: seg ix is valid passive, but length to copy > len of seg
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 0) (i32.const 5))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of seg
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 2) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of dst
mem_test("",
         "(memory.init 1 (i32.const 0xFFFE) (i32.const 1) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but src offset out of bounds
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 4) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but dst offset out of bounds
mem_test("",
         "(memory.init 1 (i32.const 0x10000) (i32.const 2) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// drop: too many args
mem_test("memory.drop 1 (i32.const 42)", "",
         WebAssembly.CompileError,
         /unused values not explicitly dropped by end of block/);

// init: too many args
mem_test("(memory.init 1 (i32.const 1) (i32.const 1) (i32.const 1) (i32.const 1))",
         "",
         SyntaxError, /parsing wasm text at/);

// init: too few args
mem_test("(memory.init 1 (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError,
         /popping value from empty stack/);

// invalid argument types
{
    const tys  = ['i32', 'f32', 'i64', 'f64'];

    for (let ty1 of tys) {
    for (let ty2 of tys) {
    for (let ty3 of tys) {
        if (ty1 == 'i32' && ty2 == 'i32' && ty3 == 'i32')
            continue;  // this is the only valid case
        let i1 = `(memory.init 1 (${ty1}.const 1) (${ty2}.const 1) (${ty3}.const 1))`;
        mem_test(i1, "", WebAssembly.CompileError, /type mismatch/);
    }}}
}


//---- table.{drop,init} --------------------------------------------------

// drop with no table
tab_test("table.drop 3", "",
         WebAssembly.CompileError, /can't table.drop without a table/,
         false);

// init with no table
tab_test("(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /can't table.init without a table/,
         false);

// drop with elem seg ix out of range
tab_test("table.drop 4", "",
         WebAssembly.CompileError, /table.drop index out of range/);

// init with elem seg ix out of range
tab_test("(table.init 4 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /table.init index out of range/);

// drop with elem seg ix indicating an active segment
tab_test("table.drop 2", "",
         WebAssembly.RuntimeError, /use of invalid passive element segment/);

// init with elem seg ix indicating an active segment
tab_test("(table.init 2 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.RuntimeError, /use of invalid passive element segment/);

// init, using an elem seg ix more than once is OK
tab_test_nofail(
    "(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))",
    "(table.init 1 (i32.const 21) (i32.const 1) (i32.const 1))");

// drop, then drop
tab_test("table.drop 1",
         "table.drop 1",
         WebAssembly.RuntimeError, /use of invalid passive element segment/);

// drop, then init
tab_test("table.drop 1",
         "(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))",
         WebAssembly.RuntimeError, /use of invalid passive element segment/);

// init: seg ix is valid passive, but length to copy > len of seg
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 0) (i32.const 5))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of seg
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 2) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of dst
tab_test("",
         "(table.init 1 (i32.const 28) (i32.const 1) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but src offset out of bounds
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 4) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but dst offset out of bounds
tab_test("",
         "(table.init 1 (i32.const 30) (i32.const 2) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// drop: too many args
tab_test("table.drop 1 (i32.const 42)", "",
         WebAssembly.CompileError,
         /unused values not explicitly dropped by end of block/);

// init: too many args
tab_test("(table.init 1 (i32.const 1) (i32.const 1) (i32.const 1) (i32.const 1))",
         "",
         SyntaxError, /parsing wasm text at/);

// init: too few args
tab_test("(table.init 1 (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError,
         /popping value from empty stack/);

// invalid argument types
{
    const tys  = ['i32', 'f32', 'i64', 'f64'];

    const ops = ['table.init 1', 'table.copy'];
    for (let ty1 of tys) {
    for (let ty2 of tys) {
    for (let ty3 of tys) {
    for (let op of ops) {
        if (ty1 == 'i32' && ty2 == 'i32' && ty3 == 'i32')
            continue;  // this is the only valid case
        let i1 = `(${op} (${ty1}.const 1) (${ty2}.const 1) (${ty3}.const 1))`;
        tab_test(i1, "", WebAssembly.CompileError, /type mismatch/);
    }}}}
}


//---- table.copy ---------------------------------------------------------

// There are no immediates here, only 3 dynamic args.  So we're limited to
// runtime boundary checks.

// passive-segs-smoketest.js tests the normal, non-exception cases of
// table.copy.  Here we just test the boundary-failure cases.  The
// table's valid indices are 0 .. 29 inclusive.

// copy: dst range invalid
tab_test("(table.copy (i32.const 28) (i32.const 1) (i32.const 3))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: dst wraparound end of 32 bit offset space
tab_test("(table.copy (i32.const 0xFFFFFFFE) (i32.const 1) (i32.const 2))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: src range invalid
tab_test("(table.copy (i32.const 15) (i32.const 25) (i32.const 6))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: src wraparound end of 32 bit offset space
tab_test("(table.copy (i32.const 15) (i32.const 0xFFFFFFFE) (i32.const 2))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: zero length with both offsets in-bounds is OK
tab_test_nofail(
    "(table.copy (i32.const 15) (i32.const 25) (i32.const 0))",
    "");

// copy: zero length with dst offset out of bounds
tab_test("(table.copy (i32.const 30) (i32.const 15) (i32.const 0))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: zero length with src offset out of bounds
tab_test("(table.copy (i32.const 15) (i32.const 30) (i32.const 0))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);
