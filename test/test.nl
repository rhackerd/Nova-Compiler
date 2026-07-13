@module math

func main() {
    MakeWindow();

    let KEY_W: u64 = 87;
    let KEY_S: u64 = 83;
    let KEY_A: u64 = 65;
    let KEY_D: u64 = 68;

    let mut pX: u64 = 0;
    let mut pY: u64 = 0;

    let test: u32 = 0;

    SetTargetFPS(180);
    loop {
        if WindowShouldClose() == true {
            break;
        }
        BeginDrawing();
        DrawBg(100,100,100);

        if IsKeyDown(KEY_D) == true {
            pX += 5;
        }

        if IsKeyDown(KEY_A) == true {
            pX -= 5;
        }

        if IsKeyDown(KEY_W) == true {
            pY -= 5;
        }

        if IsKeyDown(KEY_S) == true {
            pY += 5;
        }

        DrawRect(pX, pY, 100,100);

        DrawFPS(0,0);

        EndDrawing();
    }
    ret 0;
}