# IR
```ll
%mystruct = type {i32, float}

[10xi32]
```

# Type System
`llvm::Type *i32Ty`
`llvm::Type *floatTy`
`llvm::PorinterType::get(i32Ty,0)`
`llvm::StructType::create(context, {i32Ty, floatTy},"mystruct ");`
`llvm::ArrayType::get(i32Ty, 10)`

+ Function Types
    + Return Type, Parameter List
    + `llvm::FunctionType::get(i32Ty, {i32Ty, i32Ty}, false);`

# Local Variable Management

+ Two Local Variables
    + Stack Allocation Using `alloca`
    + virtual registers

```llvm
%sum_ptr = alloca i32
%a_val = load i32, i32* %a_ptr
%b_val = load i32, i32* %b_ptr
%sum_val = add i32 %a_val, %b_val
store i32 %sum_val i32* %sum_ptr
```

```llvm
%sum = add i32 %a, %b
```

## Stack Allocation for Local Variables

The `alloca` instruction returns a pointer object that is automatically bound to a specific _offset_ within the _stack frame_, thereby avoiding the need for manual memory management.

## Instruction `phi`

LLVM Provides a phi instruction to merge register values from different predecessor Basic Blocks.

```C
int sum;
if(a)
    sum = b+1
else
    sum = b+2
sum = sum + c
```

```llvm
entry:
    br i1 %a, label %true, label %false
true:
    %sum1 = add i32 %b, 1
    br label %end
false:
    %sum2 = add i32 %b, 2
    br label %end
next:
    %sum3 = phi i32 [%sum1, %true], [%sum2, %false] ; phi instruction, Come from true or come from false.
    %sum4 = add i32 %sum3, %c
```

the majority of LLVM-based compiler implementations (including our lab), it is common practice to bind all variables to a memory location on the stack

## Stack Management

```llvm
%1 = add i64 %my_sp, %framesize
%2 = add i64, %1, -8
%3 = inttoptr i64 %2 to i32*
store i32 27, i32* %3, align 4
```

## Global Variables

+ Definition of a global variable and a global constant string
```llvm
@myGlobalVar = global i32 42
@main_framesize_global = constant i64 40
@myString = private unnamed_addr constant [14 x i8] c"Hello, LLVM!\00"
```

+ The global variables are inherently pointers

```llvm
entry:
%main_framesize = load i64, i64* @main_framesize_global, align 4
```

## Basic Operations

+ add, sub, mul, sdiv: 
%result1 = add i32 %a, %b
%result2 = sub i32 %a, %b
%result3 = mul i32 %a, %b
%result4 = sdiv i32 %a, %b
+ IR Builder Code:
(a and b are both llvm::Value*)
llvm::Value *result1 = ir_builder->CreateAdd(a, b, "result1"); 
llvm::Value *result2 = ir_builder->CreateSub(a, b, "result2");
llvm::Value *result3 = ir_builder->CreateMul(a, b, "result3");
llvm::Value *result4 = ir_builder->CreateSDiv(a, b, "result4");

## Memory Operations

alloca: 
%ptr = alloca i32
	↑ ptr type is i32*
load: 
%value = load i32, i32* %ptr
	↑ value type is i32
store: 
 store i32 %value, i32* %ptr

## Type Conversion Ops

Int to Int conversion
+ %result = trunc i64 %value to i32
+ %result = zext i32 %value to i64
+ %result = sext i32 %value to i64

Int to / from Ptr conversion

+ %ptr = inttoptr i64 %address to i32*
+ `%int_value = ptrtoint <type>* %ptr to i64`

Ptr to Ptr conversion
+ `%new_ptr = bitcast %type1* %old_ptr to %type2*`

## Get Element Pointer

Calculate the address of a sub-element within a data structure, such as arrays or structs, based on a base pointer and a series of indices.

`%elem_ptr = getelementptr %StructTy, %StructTy* %base_ptr, i32 %idx1, i32 %idx2 ( … … )`

+ The first idx directly computes the offset from the pointer, similar to array indexing on a pointer in C. 
+ Each subsequent idx is used to index the internal elements of the current element (array or structure).

```
struct my_struct {
  int f1;
  int f2;
};

void munge(struct my_struct *base) {
    base[0].f1 = base[1].f1 + base[2].f2;
}
```

```
%my_struct = type {i32 i32}
define void @munge(%my_struct* %base) {
entry:
  %tmp = getelementptr %my_struct, %my_struct* %base, i32 1, i32 0
  %tmp1 = load i32, i32* %tmp
  %tmp2 = getelementptr % my_struct, %my_struct* %base, i32 2, i32 1
  %tmp3 = load i32, i32* %tmp2
  %tmp4 = add i32 %tmp3, %tmp1
  %tmp5 = getelementptr % my_struct, %my_struct* %base, i32 0, i32 0
  store i32 %tmp4, i32* %tmp5
  ret void
}
```

## Logic Operations

+ and/or
    + `%result = add i32 %a, %b`
    + `%result = or i32 %a, %b`
+ icmp
    + eq, ne, sl, sg, sle, sge
    + s: signed; e: equal; l: less; g: greater; 
    + `%result = icmp sle i32 %a, %b`
	↑ result will be i1 (bool)

## Control or Data Flow Ops

+ br
    + br label %label
    + br i1 %cond, label %true_label, label %false_label

+ phi
    + `phi` instruction can combine a value from multiple predecessor basic blocks
    + make sure you enumerate all possible predecessors
    + %0 = phi i32 [%1, %label1], [%2, %label2], [%3, %label3]...

```cpp
llvm::PHINode *result = ir_builder->CreatePHI(
		ir_builder->getInt32Ty(), 2);
result->addIncoming(val1, true_bb);
result->addIncoming(val2, false_bb);
```

## Call and Ret

+ call
```llvm
%rst = call i32 @func_name(i32 %arg1, i32 %arg2)
		     ↑ret type	        ↑arg type
```
			
+ ret
```llvm
ret i32 %1
```




