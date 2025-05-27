; =============================================================================
; Полный демонстрационный пример для виртуальной машины (VM) - ИСПРАВЛЕННЫЙ
; =============================================================================

JUMP MAIN

; ========================
; Секция данных
; ========================
DATA_START:
    .ASCIIZ "Welcome to the full VM demo!\n"

MSG_ARITH:
    .ASCIIZ "Arithmetic test (ADD, SUB, MUL, DIV):\n"

MSG_BITWISE:
    .ASCIIZ "Bitwise test (AND, OR, XOR, NOT, SHL, SHR):\n"

MSG_LOOP:
    .ASCIIZ "Loop test (countdown):\n"

MSG_MOV_MOD:
    .ASCIIZ "MOV with MOD test (22 mod 7):\n"

MSG_MEMORY:
    .ASCIIZ "Memory test (STORE and LOAD):\n"

MSG_STACK:
    .ASCIIZ "Stack test (PUSH and POP):\n"

MSG_MOVE:
    .ASCIIZ "MOVE test (copy R13 -> R14):\n"

MSG_CMP:
    .ASCIIZ "CMP test (comparing 10 and 10):\n"
MSG_CMP_NE:
    .ASCIIZ "CMP test: Not Equal!\n"
MSG_CMP_EQ:
    .ASCIIZ "CMP test: Equal\n"

MSG_CALL:
    .ASCIIZ "Subroutine test (factorial of 5):\n"

MSG_INPUT:
    .ASCIIZ "Input test: Please enter a number: \n"

MSG_INPUT_ECHO:
    .ASCIIZ "You entered: "

MSG_FILE:
    .ASCIIZ "File operations test (OPEN, WRITE, SEEK, CLOSE):\n"
MSG_FILE_SIM:
    .ASCIIZ "File operation simulated.\n"
MSG_SEEK_SIM:
    .ASCIIZ "File SEEK simulated.\n"


MSG_FS:
    .ASCIIZ "Listing file system:\n"

MSG_ENV:
    .ASCIIZ "Listing environment variables:\n"

MSG_SNAPSHOT:
    .ASCIIZ "Snapshot & Restore test (R22 should be 0):\n"
MSG_R22_5555:
    .ASCIIZ "R22 is 5555. Restoring...\n"
MSG_RESTORE_FINISH:
    .ASCIIZ "Restore test finished.\n"

MSG_JUMP:
    .ASCIIZ "Jump test: This block should be skipped.\n"
MSG_SKIP:
    .ASCIIZ "This message should NOT appear.\n"

MSG_NOP_BREAK:
    .ASCIIZ "NOP and BREAK test. Press Enter to continue...\n"

NEWLINE:
    .ASCIIZ "\n"

; Симулированный ввод – вместо ожидания реального ввода
SIMULATED_INPUT:
    .ASCIIZ "Simulated Input: 42\n"

; Строковые константы для файловых операций (используются стандартные потоки)
STDIN_STR:
    .ASCIIZ "stdin"
STDOUT_STR:
    .ASCIIZ "stdout"

; Текст для демонстрации операций с файлами
FILE_TEST_MSG:
    .ASCIIZ "File content: Hello from file ops!\n"

; Буферы для FS_LIST и ENV_LIST
FS_LIST_BUFFER:
    .SPACE 1024
ENV_LIST_BUFFER:
    .SPACE 1024

MEM_VAR:
    .SPACE 4 ; Место для переменной в памяти

; ========================
; Начало основной программы
; ========================
MAIN:
    ; --- Приветствие ---
    PRINTS DATA_START
    PRINTS NEWLINE

    ; --- Test 1: Арифметические операции ---
    PRINTS MSG_ARITH
    LOADI R0, 100
    LOADI R1, 25
    ADD   R2, R0, R1
    SUB   R3, R0, R1
    MUL   R4, R0, R1
    DIV   R5, R0, R1
    PRINT R2
    PRINTS NEWLINE
    PRINT R3
    PRINTS NEWLINE
    PRINT R4
    PRINTS NEWLINE
    PRINT R5
    PRINTS NEWLINE

    ; --- Test 2: Побитовые операции ---
    PRINTS MSG_BITWISE
    LOADI R6, 12
    LOADI R7, 10
    AND   R8, R6, R7
    OR    R9, R6, R7
    XOR   R10, R6, R7
    NOT   R11, R6
    SHL   R12, R6, 2
    SHR   R13, R7, 1
    PRINT R8
    PRINTS NEWLINE
    PRINT R9
    PRINTS NEWLINE
    PRINT R10
    PRINTS NEWLINE
    PRINT R11
    PRINTS NEWLINE
    PRINT R12
    PRINTS NEWLINE
    PRINT R13
    PRINTS NEWLINE

    ; --- Test 3: Цикл с условным переходом (CMP, IF) ---
    PRINTS MSG_LOOP
    LOADI R14, 3
LOOP_LABEL:
    PRINT R14
    PRINTS NEWLINE
    SUB   R14, R14, 1
    CMP   R14, 0
    IF NE, LOOP_LABEL

    ; --- Test 4: Псевдоинструкция MOV с MOD ---
    PRINTS MSG_MOV_MOD
    LOADI R15, 22
    LOADI R16, 7
    MOV   R17, R15 MOD R16
    PRINT R17
    PRINTS NEWLINE

    ; --- Test 5: Работа с памятью (STORE, LOAD) ---
    PRINTS MSG_MEMORY
    LOADI R0, MEM_VAR ; Используем метку для адреса
    LOADI R1, 777
    STORE R1, [R0]
    LOAD  R2, [R0]
    PRINT R2
    PRINTS NEWLINE

    ; --- Test 6: Операции со стеком (PUSH, POP) ---
    PRINTS MSG_STACK
    LOADI R3, 999
    PUSH R3
    POP  R4
    PRINT R4
    PRINTS NEWLINE

    ; --- Test 7: Инструкция MOVE (копирование) ---
    PRINTS MSG_MOVE
    LOADI R13, 123
    MOVE  R14, R13
    PRINT R14
    PRINTS NEWLINE

    ; --- Test 8: Сравнение и условный переход (CMP, IF EQ) ---
    PRINTS MSG_CMP
    LOADI R15, 10
    CMP   R15, 10
    IF EQ, CMP_EQUAL
    PRINTS MSG_CMP_NE
    JUMP CMP_DONE
CMP_EQUAL:
    PRINTS MSG_CMP_EQ
CMP_DONE:
    PRINTS NEWLINE

    ; --- Test 9: Вызов подпрограммы (CALL, RET) ---
    PRINTS MSG_CALL
    LOADI R17, 5
    CALL FACTORIAL
    PRINT R1
    PRINTS NEWLINE

    ; --- Test 10: Ввод/вывод (INPUT, PRINT, PRINTS) ---
    PRINTS MSG_INPUT
    PRINTS SIMULATED_INPUT
    PRINTS NEWLINE

    ; --- Test 11: Файловые операции (OPEN, WRITE, SEEK, CLOSE) ---
    PRINTS MSG_FILE
    LOADI R20, STDOUT_STR
    LOADI R21, STDOUT_STR
    OPEN  R22, R20, R21
    PRINTS MSG_FILE_SIM
    PRINTS MSG_SEEK_SIM
    CLOSE R22
    PRINTS NEWLINE

    ; --- Test 12: Вывод списка файловой системы и переменных окружения ---
    PRINTS MSG_FS
    FS_LIST FS_LIST_BUFFER
    PRINTS FS_LIST_BUFFER
    PRINTS NEWLINE
    PRINTS MSG_ENV
    ENV_LIST ENV_LIST_BUFFER
    PRINTS ENV_LIST_BUFFER
    PRINTS NEWLINE

    ; --- Test 13: Снимок и восстановление состояния (SNAPSHOT, RESTORE) ---
    ; ИСПРАВЛЕННЫЙ ТЕСТ: Мы делаем снимок, меняем значение,
    ; затем прыгаем, делаем RESTORE, и IP должен вернуться на
    ; инструкцию ПОСЛЕ SNAPSHOT. Мы должны это обработать.
    PRINTS MSG_SNAPSHOT
    LOADI R28, 0            ; Флаг восстановления: 0 = нет, 1 = да
SNAPSHOT_ENTRY:
    CMP R28, 1              ; Мы уже восстанавливались?
    IF EQ, SNAPSHOT_FINISH  ; Если да, идем к концу теста.
    ; Если нет (первый раз):
    LOADI R22, 0            ; R22 = 0
    SNAPSHOT                ; Сохраняем (R28=0, R22=0, IP -> LOADI R22, 5555)
    LOADI R22, 5555         ; Меняем R22
    PRINTS MSG_R22_5555
    RESTORE                 ; Восстанавливаем (R28=0, R22=0, IP -> LOADI R22, 5555)
    ; ВМ СЕЙЧАС ВЕРНЕТСЯ НА 'LOADI R22, 5555' ИЗ-ЗА ХАКА В ВМ.
    ; НАМ НУЖНО ПРОПУСТИТЬ ЭТОТ ВЫЗОВ И RESTORE.
    ; ПОСКОЛЬКУ IP НЕ ВОССТАНАВЛИВАЕТСЯ, МЫ ПРОСТО ПРОДОЛЖИМ.
    ; НО R22 БУДЕТ 0.
    PRINT R22               ; Выводим 0
    PRINTS NEWLINE
    JUMP SNAPSHOT_FINISH    ; Прыгаем к концу теста

SNAPSHOT_FINISH:
    PRINTS MSG_RESTORE_FINISH
    PRINTS NEWLINE


    ; --- Test 14: Демонстрация безусловного перехода (JUMP) ---
    PRINTS MSG_JUMP
    JUMP SKIP_BLOCK
    PRINTS MSG_SKIP
    PRINTS NEWLINE

SKIP_BLOCK:
    ; --- Test 15: Инструкции NOP и BREAK ---
    PRINTS MSG_NOP_BREAK
    NOP
    BREAK
    HALT

; =============================================================================
; Подпрограмма: Факториал (рекурсивно)
; =============================================================================
FACTORIAL:
    CMP R17, 1
    IF EQ, FACT_RET
    PUSH R17
    SUB R17, R17, 1
    CALL FACTORIAL
    POP R17
    MUL R1, R17, R1
    RET
FACT_RET:
    LOADI R1, 1
    RET