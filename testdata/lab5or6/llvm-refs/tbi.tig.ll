; ModuleID = 'lightir'
source_filename = "lightir"
target triple = "x86_64-pc-linux-gnu"

%string = type { i32, i8* }

@tigermain_framesize_global = constant i64 24
@printb_framesize_global = constant i64 40
@0 = private unnamed_addr constant [2 x i8] c"x\00", align 1
@str = constant %string { i32 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @0, i32 0, i32 0) }
@1 = private unnamed_addr constant [2 x i8] c"y\00", align 1
@str.1 = constant %string { i32 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @1, i32 0, i32 0) }
@2 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.2 = constant %string { i32 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @2, i32 0, i32 0) }
@3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.3 = constant %string { i32 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @3, i32 0, i32 0) }

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
  store i32 8, i32* %6, align 4
  call void @printb(i64 %tigermain_sp, i64 %tigermain_sp)
  ret i32 0
}

define void @printb(i64 %0, i64 %1) {
printb:
  %printb_local_framesize = load i64, i64* @printb_framesize_global, align 4
  %printb_sp = sub i64 %0, %printb_local_framesize
  %2 = load i64, i64* @printb_framesize_global, align 4
  %3 = add i64 %2, 8
  %4 = add i64 %printb_sp, %3
  %sl_ptr = inttoptr i64 %4 to i64*
  store i64 %1, i64* %sl_ptr, align 4
  %5 = load i64, i64* @printb_framesize_global, align 4
  %6 = add i64 %5, 8
  %7 = add i64 %printb_sp, %6
  %8 = inttoptr i64 %7 to i64*
  %9 = load i64, i64* %8, align 4
  %10 = load i64, i64* @tigermain_framesize_global, align 4
  %11 = add i64 %10, -8
  %12 = add i64 %9, %11
  %tigermain_N_ptr = inttoptr i64 %12 to i32*
  %13 = load i32, i32* %tigermain_N_ptr, align 4
  %14 = sub i32 %13, 1
  %15 = load i64, i64* @printb_framesize_global, align 4
  %16 = add i64 %15, -8
  %17 = add i64 %printb_sp, %16
  %18 = inttoptr i64 %17 to i32*
  store i32 0, i32* %18, align 4
  br label %for_body

for_incre:                                        ; preds = %for_next4
  %19 = load i32, i32* %18, align 4
  %20 = add i32 1, %19
  store i32 %20, i32* %18, align 4
  %21 = icmp sle i32 %20, %14
  br i1 %21, label %for_body, label %for_next

for_body:                                         ; preds = %for_incre, %printb
  %22 = load i64, i64* @printb_framesize_global, align 4
  %23 = add i64 %22, 8
  %24 = add i64 %printb_sp, %23
  %25 = inttoptr i64 %24 to i64*
  %26 = load i64, i64* %25, align 4
  %27 = load i64, i64* @tigermain_framesize_global, align 4
  %28 = add i64 %27, -8
  %29 = add i64 %26, %28
  %tigermain_N_ptr1 = inttoptr i64 %29 to i32*
  %30 = load i32, i32* %tigermain_N_ptr1, align 4
  %31 = sub i32 %30, 1
  %32 = load i64, i64* @printb_framesize_global, align 4
  %33 = add i64 %32, -16
  %34 = add i64 %printb_sp, %33
  %35 = inttoptr i64 %34 to i32*
  store i32 0, i32* %35, align 4
  br label %for_body3

for_next:                                         ; preds = %for_incre
  call void @print(%string* @str.3)
  ret void

for_incre2:                                       ; preds = %if_next
  %36 = load i32, i32* %35, align 4
  %37 = add i32 1, %36
  store i32 %37, i32* %35, align 4
  %38 = icmp sle i32 %37, %31
  br i1 %38, label %for_body3, label %for_next4

for_body3:                                        ; preds = %for_incre2, %for_body
  br label %if_test

for_next4:                                        ; preds = %for_incre2
  call void @print(%string* @str.2)
  br label %for_incre

if_test:                                          ; preds = %for_body3
  %39 = load i64, i64* @printb_framesize_global, align 4
  %40 = add i64 %39, -8
  %41 = add i64 %printb_sp, %40
  %printb_i_ptr = inttoptr i64 %41 to i32*
  %42 = load i64, i64* @printb_framesize_global, align 4
  %43 = add i64 %42, -16
  %44 = add i64 %printb_sp, %43
  %printb_j_ptr = inttoptr i64 %44 to i32*
  %45 = load i32, i32* %printb_i_ptr, align 4
  %46 = load i32, i32* %printb_j_ptr, align 4
  %47 = icmp sgt i32 %45, %46
  br i1 %47, label %if_then, label %if_else

if_then:                                          ; preds = %if_test
  br label %if_next

if_else:                                          ; preds = %if_test
  br label %if_next

if_next:                                          ; preds = %if_else, %if_then
  %48 = phi %string* [ @str, %if_then ], [ @str.1, %if_else ]
  call void @print(%string* %48)
  br label %for_incre2
}

; ===== Frame Information =====
; tigermain -8 24
; printb -16 40
