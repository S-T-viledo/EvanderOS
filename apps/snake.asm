; EvanderOS Snake Game (Apple & Score)
; EvanderOS 贪吃蛇游戏（苹果和分数）

    ; 1. 隐藏光标与清屏
    ; Hide cursor and clear screen
    MOV EAX, 21
    INT 0x30
    MOV EAX, 15
    MOV EBX, 0
    INT 0x30

    ; =======================================
    ; 初始化阶段 / Initialization Phase
    ; =======================================
    ; 初始方向：向右 (3) / Initial direction: right (3)
    MOV EAX, CUR_DIR
    MOV EBX, 3
    MOV [EAX], EBX

    ; 初始化头尾指针 (长度为 5) / Initialize head and tail pointers (length 5)
    MOV EAX, HEAD_VAR
    MOV EBX, 4
    MOV [EAX], EBX
    MOV EAX, TAIL_VAR
    MOV EBX, 0
    MOV [EAX], EBX

    ; 初始化分数 = 0 / Initialize score = 0
    MOV EAX, SCORE_VAR
    MOV EBX, 0
    MOV [EAX], EBX

    ; 初始化数组内存 / Initialize array memory
    MOV ECX, 5
    MOV EBX, 0
INIT_LOOP:
    MOV ESI, SNAKE_MEM
    ADD ESI, EBX
    MOV EAX, 40
    MOV [ESI], EAX      ; X
    MOV ESI, SNAKE_MEM
    ADD ESI, 400
    ADD ESI, EBX
    MOV EAX, 12
    MOV [ESI], EAX      ; Y
    ADD EBX, 4
    LOOP INIT_LOOP

    ; =======================================
    ; 绘制底部 UI 隔离区 / Draw bottom UI separator
    ; =======================================
    ; 画边界线 (Row 23) / Draw boundary line (Row 23)
    MOV EAX, 16
    MOV EBX, 23
    MOV ECX, 0
    INT 0x30
    MOV EAX, 0
    MOV EBX, LINE_STR
    INT 0x30

    ; 画 "Score: 0" (Row 24) / Draw "Score: 0" (Row 24)
    MOV EAX, 16
    MOV EBX, 24
    MOV ECX, 0
    INT 0x30
    MOV EAX, 0
    MOV EBX, SCORE_STR
    INT 0x30
    MOV EAX, 24
    MOV EBX, 0
    INT 0x30

    ; 生成第一颗苹果 / Generate first apple
    CALL SPAWN_APPLE

    ; =======================================
    ; 游戏主循环 / Main game loop
    ; =======================================

; keyboard input -> update direction -> move snake -> check collision -> loop
GAME_LOOP:
    MOV EAX, 20         ; Read keyboard input / 读取键盘输入
    INT 0x30
    
    CMP EBX, 113        ; q
    JE DO_EXIT
    CMP EBX, 81         ; Q
    JE DO_EXIT
    
    CMP EBX, 119        ; w
    JE SET_UP
    CMP EBX, 87
    JE SET_UP
    CMP EBX, 115        ; s
    JE SET_DOWN
    CMP EBX, 83
    JE SET_DOWN
    CMP EBX, 97         ; a
    JE SET_LEFT
    CMP EBX, 65
    JE SET_LEFT
    CMP EBX, 100        ; d
    JE SET_RIGHT
    CMP EBX, 68
    JE SET_RIGHT
    JMP NO_INPUT

SET_UP:
    MOV EAX, CUR_DIR
    MOV EBX, 0
    MOV [EAX], EBX
    JMP NO_INPUT
SET_DOWN:
    MOV EAX, CUR_DIR
    MOV EBX, 1
    MOV [EAX], EBX
    JMP NO_INPUT
SET_LEFT:
    MOV EAX, CUR_DIR
    MOV EBX, 2
    MOV [EAX], EBX
    JMP NO_INPUT
SET_RIGHT:
    MOV EAX, CUR_DIR
    MOV EBX, 3
    MOV [EAX], EBX

NO_INPUT:
    ; 读取并涂黑旧蛇头 / Read and black out old snake head
    MOV EAX, HEAD_VAR
    MOV EBX, [EAX]
    MOV ECX, EBX
    ADD EBX, EBX
    ADD EBX, EBX
    
    MOV ESI, SNAKE_MEM
    ADD ESI, EBX
    MOV EDI, [ESI]      ; EDI = 旧头 X / EDI = old head X
    MOV ESI, SNAKE_MEM
    ADD ESI, 400
    ADD ESI, EBX
    MOV EBP, [ESI]      ; EBP = 旧头 Y / EBP = old head Y
    
    MOV EAX, 16
    MOV EBX, EBP
    MOV ECX, EDI
    INT 0x30
    
    MOV EAX, 0
    MOV EBX, BODY_CHAR
    INT 0x30

    ; 计算新蛇头坐标 / Calculate new snake head coordinates
    MOV EAX, CUR_DIR
    MOV EBX, [EAX]
    CMP EBX, 0
    JE MOVE_UP
    CMP EBX, 1
    JE MOVE_DOWN
    CMP EBX, 2
    JE MOVE_LEFT
    CMP EBX, 3
    JE MOVE_RIGHT

MOVE_UP:
    DEC EBP
    JMP WRAP_CHK
MOVE_DOWN:
    INC EBP
    JMP WRAP_CHK
MOVE_LEFT:
    DEC EDI
    JMP WRAP_CHK
MOVE_RIGHT:
    INC EDI

WRAP_CHK:
    CMP EDI, 80
    JNE CHK_X_L
    MOV EDI, 0
CHK_X_L:
    CMP EDI, 0xFFFFFFFF
    JNE CHK_Y_B
    MOV EDI, 79
CHK_Y_B:
    CMP EBP, 23
    JNE CHK_Y_T
    MOV EBP, 0
CHK_Y_T:
    CMP EBP, 0xFFFFFFFF
    JNE CHECK_APPLE
    MOV EBP, 22

    ; =======================================
    ; 核心：苹果碰撞检测！ / Core: Apple collision detection!
    ; =======================================
CHECK_APPLE:
    MOV ESI, APPLE_X
    MOV EAX, [ESI]
    CMP EDI, EAX        ; 新蛇头 X == 苹果 X 吗？ / New head X == apple X?
    JNE ERASE_TAIL
    
    MOV ESI, APPLE_Y
    MOV EAX, [ESI]
    CMP EBP, EAX        ; 新蛇头 Y == 苹果 Y 吗？ / New head Y == apple Y?
    JNE ERASE_TAIL

    ; --- 吃到苹果了！(吃货分歧点) --- / --- Ate the apple! (Glutton divergence point) ---
    ; 1. 分数 + 1 / Score +1
    MOV ESI, SCORE_VAR
    MOV EAX, [ESI]
    ADD EAX, 10
    MOV [ESI], EAX
    
    ; 2. 更新计分板 / Update scoreboard
    MOV EAX, 16
    MOV EBX, 24
    MOV ECX, 7          ; 把光标定位在 "Score: " 后面 / Position cursor after "Score: "
    INT 0x30
    MOV EAX, 24
    MOV ESI, SCORE_VAR
    MOV EBX, [ESI]
    INT 0x30
    
    ; 3. 重新生成一个苹果 / Regenerate an apple
    CALL SPAWN_APPLE
    
    ; 4. 重点！跳过擦除蛇尾，直接去存新蛇头！(蛇就变长了) / Key! Skip erasing tail, go directly to save new head! (Snake grows)
    JMP SAVE_NEW_HEAD


    ; =======================================
    ; 擦除蛇尾 (如果没有吃到苹果) / Erase tail (if didn't eat apple)
    ; =======================================
ERASE_TAIL:
    MOV EAX, TAIL_VAR
    MOV EBX, [EAX]
    MOV ECX, EBX
    ADD EBX, EBX
    ADD EBX, EBX
    
    MOV ESI, SNAKE_MEM
    ADD ESI, EBX
    MOV EAX, [ESI]      ; 尾部 X / Tail X
    MOV ESI, SNAKE_MEM
    ADD ESI, 400
    ADD ESI, EBX
    MOV ECX, [ESI]      ; 尾部 Y / Tail Y
    
    PUSH EBX            ; 保护 EBX(指针) / Protect EBX (pointer)
    MOV EBX, ECX        ; Row
    MOV ECX, EAX        ; Col
    MOV EAX, 16
    INT 0x30
    MOV EAX, 0
    MOV EBX, SPACE_CHAR
    INT 0x30
    POP EBX
    
    ; 推动 TAIL 指针 / Advance TAIL pointer
    MOV EAX, TAIL_VAR
    MOV EBX, [EAX]
    INC EBX
    CMP EBX, 100
    JNE SAVE_TAIL
    MOV EBX, 0
SAVE_TAIL:
    MOV [EAX], EBX

    ; =======================================
    ; 存入并绘制新蛇头 / Save and draw new snake head
    ; =======================================
SAVE_NEW_HEAD:
    MOV EAX, HEAD_VAR
    MOV EBX, [EAX]
    INC EBX
    CMP EBX, 100
    JNE WRITE_HEAD
    MOV EBX, 0
WRITE_HEAD:
    MOV [EAX], EBX
    ADD EBX, EBX
    ADD EBX, EBX
    
    MOV ESI, SNAKE_MEM
    ADD ESI, EBX
    MOV [ESI], EDI
    MOV ESI, SNAKE_MEM
    ADD ESI, 400
    ADD ESI, EBX
    MOV [ESI], EBP
    
    MOV EAX, 16
    MOV EBX, EBP
    MOV ECX, EDI
    INT 0x30
    MOV EAX, 0
    MOV EBX, HEAD_CHAR
    INT 0x30

    ; 休眠与循环 / Sleep and loop
    MOV EAX, 19
    MOV EBX, 5        ; 速度！改小变快，改大变慢 / Speed! Smaller faster, larger slower
    INT 0x30
    JMP GAME_LOOP

    ; =======================================
    ; 退出程序 / Exit program
    ; =======================================
DO_EXIT:
    MOV EAX, 22       ; 恢复光标！ / Restore cursor!
    INT 0x30
    MOV EAX, 15
    MOV EBX, 0
    INT 0x30
    MOV EAX, 1
    MOV EBX, 0
    INT 0x30

    ; =======================================
    ; 子程序：生成随机苹果 / Subroutine: Generate random apple
    ; =======================================
SPAWN_APPLE:
    ; 随机 X (0~79) / Random X (0~79)
    MOV EAX, 23
    MOV EBX, 80
    INT 0x30
    MOV ESI, APPLE_X
    MOV [ESI], EBX
    
    ; 随机 Y (0~22) / Random Y (0~22)
    MOV EAX, 23
    MOV EBX, 23
    INT 0x30
    MOV ESI, APPLE_Y
    MOV [ESI], EBX
    
    ; 绘制苹果 '@' / Draw apple '@'
    MOV EAX, 16
    MOV ESI, APPLE_Y
    MOV EBX, [ESI]
    MOV ESI, APPLE_X
    MOV ECX, [ESI]
    INT 0x30
    
    MOV EAX, 0
    MOV EBX, APPLE_CHAR
    INT 0x30
    RET

CS:
    JMP CS
    JMP $
    ; =======================================
    ; 数据段 / Data section
    ; =======================================
CUR_DIR:    
    DB "    "
HEAD_VAR:   
    DB "    "
TAIL_VAR:   
    DB "    "
APPLE_X:    
    DB "    "
APPLE_Y:    
    DB "    "
SCORE_VAR:  
    DB "    "

SPACE_CHAR: DB " ", 0
BODY_CHAR:  DB "O", 0
HEAD_CHAR:  DB "@", 0
APPLE_CHAR: DB "*", 0

SCORE_STR:  DB " Score: ", 0
LINE_STR:   DB "-------------------------------------------------------------------", 0

SNAKE_MEM: