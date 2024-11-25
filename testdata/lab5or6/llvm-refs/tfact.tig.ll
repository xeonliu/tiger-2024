; ModuleID = 'lightir'
source_filename = "lightir"
target triple = "x86_64-pc-linux-gnu"

%string = type { i32, i8* }

@tigermain_framesize_global = constant i64 24
@nfactor_framesize_global = constant i64 24

declare void @flush()

declare void @exit(i32)

declare %string* @chr(i32)

declare %string* @__wrap_getchar()

declare void @print(%string*)

declare void @printi(i32)

declare i32 @ord(%string*)

declare i32 @size(%string*)

declare %string* @concat(%string*, %string*)

declare %string* @substring(%string*, i32, i32)

declare i64 @alloc_record(i32)

declare i64 @init_array(i32, i64)

declare i1 @string_equal(%string*, %string*)

define i32 @tigermain(i64 %0, i64 %1) {
tigermain:
  %2 = load i64, i64* @tigermain_framesize_global, align 4
  %tigermain_sp = sub i64 %0, %2
  %3 = call i32 @nfactor(i64 %tigermain_sp, i64 %tigermain_sp, i32 10)
  call void @printi(i32 %3)
  ret i32 0
}

define i32 @nfactor(i64 %0, i64 %1, i32 %2) {
nfactor:
  %nfactor_local_framesize = load i64, i64* @nfactor_framesize_global, align 4
  %nfactor_sp = sub i64 %0, %nfactor_local_framesize
  %3 = load i64, i64* @nfactor_framesize_global, align 4
  %4 = add i64 %3, 8
  %5 = add i64 %nfactor_sp, %4
  %sl_ptr = inttoptr i64 %5 to i64*
  store i64 %1, i64* %sl_ptr, align 4
  %6 = load i64, i64* @nfactor_framesize_global, align 4
  %7 = add i64 %6, 16
  %8 = add i64 %nfactor_sp, %7
  %9 = inttoptr i64 %8 to i32*
  store i32 %2, i32* %9, align 4
  br label %if_test

if_test:                                          ; preds = %nfactor
  %10 = load i64, i64* @nfactor_framesize_global, align 4
  %11 = add i64 %10, 16
  %12 = add i64 %nfactor_sp, %11
  %nfactor_n_ptr = inttoptr i64 %12 to i32*
  %13 = load i32, i32* %nfactor_n_ptr, align 4
  %14 = icmp eq i32 %13, 0
  br i1 %14, label %if_then, label %if_else

if_then:                                          ; preds = %if_test
  br label %if_next

if_else:                                          ; preds = %if_test
  %15 = load i64, i64* @nfactor_framesize_global, align 4
  %16 = add i64 %15, 16
  %17 = add i64 %nfactor_sp, %16
  %nfactor_n_ptr1 = inttoptr i64 %17 to i32*
  %18 = load i64, i64* @nfactor_framesize_global, align 4
  %19 = add i64 %18, 16
  %20 = add i64 %nfactor_sp, %19
  %nfactor_n_ptr2 = inttoptr i64 %20 to i32*
  %21 = load i32, i32* %nfactor_n_ptr2, align 4
  %22 = sub i32 %21, 1
  %23 = load i64, i64* @nfactor_framesize_global, align 4
  %24 = add i64 %23, 8
  %25 = add i64 %nfactor_sp, %24
  %26 = inttoptr i64 %25 to i64*
  %27 = load i64, i64* %26, align 4
  %28 = call i32 @nfactor(i64 %nfactor_sp, i64 %27, i32 %22)
  %29 = load i32, i32* %nfactor_n_ptr1, align 4
  %30 = mul i32 %29, %28
  br label %if_next

if_next:                                          ; preds = %if_else, %if_then
  %31 = phi i32 [ 1, %if_then ], [ %30, %if_else ]
  ret i32 %31
}

; ===== Frame Information =====
; tigermain 0 24
; nfactor 0 24
