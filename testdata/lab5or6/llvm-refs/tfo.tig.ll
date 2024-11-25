; ModuleID = 'lightir'
source_filename = "lightir"
target triple = "x86_64-pc-linux-gnu"

%string = type { i32, i8* }

@tigermain_framesize_global = constant i64 40

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
  %3 = load i64, i64* @tigermain_framesize_global, align 4
  %4 = add i64 %3, -8
  %5 = add i64 %tigermain_sp, %4
  %6 = inttoptr i64 %5 to i32*
  store i32 4, i32* %6, align 4
  %7 = load i64, i64* @tigermain_framesize_global, align 4
  %8 = add i64 %7, -8
  %9 = add i64 %tigermain_sp, %8
  %tigermain_a_ptr = inttoptr i64 %9 to i32*
  %10 = load i64, i64* @tigermain_framesize_global, align 4
  %11 = add i64 %10, -16
  %12 = add i64 %tigermain_sp, %11
  %13 = inttoptr i64 %12 to i32*
  store i32 0, i32* %13, align 4
  %14 = load i32, i32* %tigermain_a_ptr, align 4
  br label %for_body

for_incre:                                        ; preds = %if_next
  %15 = load i32, i32* %13, align 4
  %16 = add i32 1, %15
  store i32 %16, i32* %13, align 4
  %17 = icmp sle i32 %16, %14
  br i1 %17, label %for_body, label %for_next

for_body:                                         ; preds = %for_incre, %tigermain
  %18 = load i64, i64* @tigermain_framesize_global, align 4
  %19 = add i64 %18, -16
  %20 = add i64 %tigermain_sp, %19
  %tigermain_i_ptr = inttoptr i64 %20 to i32*
  %actual_parm = load i32, i32* %tigermain_i_ptr, align 4
  call void @printi(i32 %actual_parm)
  br label %if_test

for_next:                                         ; preds = %for_incre, %if_then
  ret i32 0

if_test:                                          ; preds = %for_body
  %21 = load i64, i64* @tigermain_framesize_global, align 4
  %22 = add i64 %21, -16
  %23 = add i64 %tigermain_sp, %22
  %tigermain_i_ptr1 = inttoptr i64 %23 to i32*
  %24 = load i32, i32* %tigermain_i_ptr1, align 4
  %25 = icmp eq i32 %24, 3
  br i1 %25, label %if_then, label %if_next

if_then:                                          ; preds = %if_test
  br label %for_next

if_next:                                          ; preds = %if_test
  br label %for_incre
}

; ===== Frame Information =====
; tigermain -16 40
