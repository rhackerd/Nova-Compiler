; ModuleID = 'nova_module'
source_filename = "nova_module"

define i64 @main() {
entry:
  %call = call i64 @MakeWindow()
  %KEY_W = alloca i64, align 8
  store i64 87, ptr %KEY_W, align 4
  %KEY_S = alloca i64, align 8
  store i64 83, ptr %KEY_S, align 4
  %KEY_A = alloca i64, align 8
  store i64 65, ptr %KEY_A, align 4
  %KEY_D = alloca i64, align 8
  store i64 68, ptr %KEY_D, align 4
  %pX = alloca i64, align 8
  store i64 0, ptr %pX, align 4
  %pY = alloca i64, align 8
  store i64 0, ptr %pY, align 4
  %test = alloca i32, align 4
  store i32 0, ptr %test, align 4
  %call1 = call i1 @SetTargetFPS(i64 180)
  br label %loop

loop:                                             ; preds = %merge27, %entry
  %call2 = call i1 @WindowShouldClose()
  %0 = icmp eq i1 %call2, true
  br i1 %0, label %then, label %merge

loop_exit:                                        ; preds = %then
  ret i64 0

then:                                             ; preds = %loop
  br label %loop_exit

else:                                             ; No predecessors!
  br label %merge

merge:                                            ; preds = %else, %loop
  %call3 = call i1 @BeginDrawing()
  %call4 = call i1 @DrawBg(i64 100, i64 100, i64 100)
  %KEY_D5 = load i64, ptr %KEY_D, align 4
  %call6 = call i1 @IsKeyDown(i64 %KEY_D5)
  %1 = icmp eq i1 %call6, true
  br i1 %1, label %then7, label %merge9

then7:                                            ; preds = %merge
  %pX10 = load i64, ptr %pX, align 4
  %2 = add i64 %pX10, 5
  store i64 %2, ptr %pX, align 4
  br label %merge9

else8:                                            ; No predecessors!
  br label %merge9

merge9:                                           ; preds = %else8, %then7, %merge
  %KEY_A11 = load i64, ptr %KEY_A, align 4
  %call12 = call i1 @IsKeyDown(i64 %KEY_A11)
  %3 = icmp eq i1 %call12, true
  br i1 %3, label %then13, label %merge15

then13:                                           ; preds = %merge9
  %pX16 = load i64, ptr %pX, align 4
  %4 = sub i64 %pX16, 5
  store i64 %4, ptr %pX, align 4
  br label %merge15

else14:                                           ; No predecessors!
  br label %merge15

merge15:                                          ; preds = %else14, %then13, %merge9
  %KEY_W17 = load i64, ptr %KEY_W, align 4
  %call18 = call i1 @IsKeyDown(i64 %KEY_W17)
  %5 = icmp eq i1 %call18, true
  br i1 %5, label %then19, label %merge21

then19:                                           ; preds = %merge15
  %pY22 = load i64, ptr %pY, align 4
  %6 = sub i64 %pY22, 5
  store i64 %6, ptr %pY, align 4
  br label %merge21

else20:                                           ; No predecessors!
  br label %merge21

merge21:                                          ; preds = %else20, %then19, %merge15
  %KEY_S23 = load i64, ptr %KEY_S, align 4
  %call24 = call i1 @IsKeyDown(i64 %KEY_S23)
  %7 = icmp eq i1 %call24, true
  br i1 %7, label %then25, label %merge27

then25:                                           ; preds = %merge21
  %pY28 = load i64, ptr %pY, align 4
  %8 = add i64 %pY28, 5
  store i64 %8, ptr %pY, align 4
  br label %merge27

else26:                                           ; No predecessors!
  br label %merge27

merge27:                                          ; preds = %else26, %then25, %merge21
  %pX29 = load i64, ptr %pX, align 4
  %pY30 = load i64, ptr %pY, align 4
  %call31 = call i1 @DrawRect(i64 %pX29, i64 %pY30, i64 100, i64 100)
  %call32 = call i1 @DrawFPS(i64 0, i64 0)
  %call33 = call i1 @EndDrawing()
  br label %loop
}

declare i64 @MakeWindow()

declare i1 @SetTargetFPS(i64)

declare i1 @WindowShouldClose()

declare i1 @BeginDrawing()

declare i1 @DrawBg(i64, i64, i64)

declare i1 @IsKeyDown(i64)

declare i1 @DrawRect(i64, i64, i64, i64)

declare i1 @DrawFPS(i64, i64)

declare i1 @EndDrawing()
