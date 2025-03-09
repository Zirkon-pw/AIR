; =============================================================================
; Полный демонстрационный пример для виртуальной машины (VM)
; =============================================================================
; В данном примере демонстрируются:
; - Арифметические операции: LOADI, ADD, SUB, MUL, DIV
; - Побитовые операции: AND, OR, XOR, NOT, SHL, SHR
; - Условные переходы и циклы: CMP, IF
; - Работа с памятью: LOAD, STORE
; - Псевдоинструкция MOV с MOD (реализуется как DIV, MUL, SUB)
; - Операции со стеком: PUSH, POP
; - Инструкция MOVE для копирования регистров
; - Сравнение (CMP) с условными переходами (IF EQ, IF NE)
; - Вызов подпрограмм: CALL, RET (пример – рекурсивный факториал)
; - Ввод/вывод: PRINT, PRINTS, INPUT (INPUT симулируется)
; - Файловые операции: OPEN, READ, WRITE, SEEK, CLOSE (демонстрация через стандартный поток)
; - Вывод списков файловой системы и переменных окружения: FS_LIST, ENV_LIST
; - Снимок и восстановление состояния: SNAPSHOT, RESTORE
; - Безусловный переход: JUMP
; - Инструкции NOP и BREAK (отладочная точка)
; - Завершение: HALT
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

MSG_CALL:
    .ASCIIZ "Subroutine test (factorial of 5):\n"

MSG_INPUT:
    .ASCIIZ "Input test: Please enter a number: \n"

MSG_INPUT_ECHO:
    .ASCIIZ "You entered: "
    
MSG_FILE:
    .ASCIIZ "File operations test (OPEN, WRITE, SEEK, CLOSE):\n"

MSG_FS:
    .ASCIIZ "Listing file system:\n"

MSG_ENV:
    .ASCIIZ "Listing environment variables:\n"

MSG_SNAPSHOT:
    .ASCIIZ "Snapshot & Restore test (R22 should revert to 0):\n"

MSG_JUMP:
    .ASCIIZ "Jump test: This block should be skipped.\n"

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
    .SPACE 128
ENV_LIST_BUFFER:
    .SPACE 128

; ========================
; Начало основной программы
; ========================
MAIN:
    ; --- Приветствие ---
    PRINTS DATA_START
    PRINTS NEWLINE

    ; --- Test 1: Арифметические операции ---
    PRINTS MSG_ARITH
    LOADI R0, 100           ; R0 = 100
    LOADI R1, 25            ; R1 = 25
    ADD   R2, R0, R1        ; R2 = 125
    SUB   R3, R0, R1        ; R3 = 75
    MUL   R4, R0, R1        ; R4 = 2500
    DIV   R5, R0, R1        ; R5 = 4
    PRINT R2                ; вывод 125
    PRINTS NEWLINE
    PRINT R3                ; вывод 75
    PRINTS NEWLINE
    PRINT R4                ; вывод 2500
    PRINTS NEWLINE
    PRINT R5                ; вывод 4
    PRINTS NEWLINE

    ; --- Test 2: Побитовые операции ---
    PRINTS MSG_BITWISE
    LOADI R6, 12            ; R6 = 12 (0b1100)
    LOADI R7, 10            ; R7 = 10 (0b1010)
    AND   R8, R6, R7        ; R8 = 8
    OR    R9, R6, R7        ; R9 = 14
    XOR   R10, R6, R7       ; R10 = 6
    NOT   R11, R6           ; R11 = ~12, ожидается 4294967283
    SHL   R12, R6, 2        ; R12 = 48
    SHR   R13, R7, 1        ; R13 = 5
    PRINT R8                ; 8
    PRINTS NEWLINE
    PRINT R9                ; 14
    PRINTS NEWLINE
    PRINT R10               ; 6
    PRINTS NEWLINE
    PRINT R11               ; 4294967283
    PRINTS NEWLINE
    PRINT R12               ; 48
    PRINTS NEWLINE
    PRINT R13               ; 5
    PRINTS NEWLINE

    ; --- Test 3: Цикл с условным переходом (CMP, IF) ---
    PRINTS MSG_LOOP
    LOADI R14, 3           ; Счетчик = 3
LOOP_LABEL:
    PRINT R14              ; вывод текущего значения
    PRINTS NEWLINE
    LOADI R2, 1
    SUB   R14, R14, R2     ; декремент
    CMP   R14, 0
    IF NE, LOOP_LABEL      ; повторяем цикл, пока не станет 0

    ; --- Test 4: Псевдоинструкция MOV с MOD ---
    PRINTS MSG_MOV_MOD
    LOADI R15, 22          ; R15 = 22
    LOADI R16, 7           ; R16 = 7
    MOV   R17, R15 MOD R16 ; R17 = 22 mod 7, ожидается 1
    PRINT R17              ; вывод 1
    PRINTS NEWLINE

    ; --- Test 5: Работа с памятью (STORE, LOAD) ---
    PRINTS MSG_MEMORY
    LOADI R0, 600          ; адрес 600
    LOADI R1, 777          ; значение 777
    STORE R1, [R0]         ; сохраняем 777 по адресу 600
    LOAD  R2, [R0]         ; читаем из памяти
    PRINT R2               ; вывод 777
    PRINTS NEWLINE

    ; --- Test 6: Операции со стеком (PUSH, POP) ---
    PRINTS MSG_STACK
    LOADI R3, 999
    PUSH R3                ; помещаем 999 в стек
    POP  R4                ; извлекаем значение
    PRINT R4               ; вывод 999
    PRINTS NEWLINE

    ; --- Test 7: Инструкция MOVE (копирование) ---
    PRINTS MSG_MOVE
    LOADI R13, 123         ; R13 = 123
    MOVE  R14, R13         ; копируем R13 -> R14
    PRINT R14              ; вывод 123
    PRINTS NEWLINE

    ; --- Test 8: Сравнение и условный переход (CMP, IF EQ) ---
    PRINTS MSG_CMP
    LOADI R15, 10
    LOADI R16, 10
    CMP   R15, R16         ; сравнение 10 и 10
    IF EQ, CMP_EQUAL
    PRINTS "CMP test: Not Equal!\n"
    JUMP CMP_DONE
CMP_EQUAL:
    PRINTS "CMP test: Equal\n"
CMP_DONE:
    PRINTS NEWLINE

    ; --- Test 9: Вызов подпрограммы (CALL, RET) ---
    PRINTS MSG_CALL
    LOADI R17, 5           ; число для факториала = 5
    CALL FACTORIAL         ; результат в R1 (ожидается 120)
    PRINT R1               ; вывод 120
    PRINTS NEWLINE

    ; --- Test 10: Ввод/вывод (INPUT, PRINT, PRINTS) ---
    PRINTS MSG_INPUT       ; сообщение о вводе
    ; Для демонстрации INPUT вместо ожидания ввода выводим симулированное значение
    PRINTS SIMULATED_INPUT ; вывод "Simulated Input: 42\n"
    PRINTS NEWLINE

    ; --- Test 11: Файловые операции (OPEN, WRITE, SEEK, CLOSE) ---
    PRINTS MSG_FILE
    ; Демонстрация: откроем стандартный поток stdout
    LOADI R20, STDOUT_STR   ; адрес строки "stdout"
    LOADI R21, STDOUT_STR   ; используем тот же режим
    OPEN  R22, R20, R21     ; открываем поток, дескриптор в R22
    ; Симулируем запись (реальная запись не производится)
    PRINTS "File operation simulated.\n"
    ; Симулируем операцию SEEK
    PRINTS "File SEEK simulated.\n"
    CLOSE R22               ; закрываем поток
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
    PRINTS MSG_SNAPSHOT
    SNAPSHOT               ; сохраняем состояние
    LOADI R22, 5555        ; изменяем R22 (будет восстановлено)
    RESTORE                ; восстанавливаем ранее сохранённое состояние
    PRINT R22              ; вывод восстановленного R22 (ожидается 0)
    PRINTS NEWLINE

    ; --- Test 14: Демонстрация безусловного перехода (JUMP) ---
    PRINTS MSG_JUMP
    JUMP SKIP_BLOCK        ; блок ниже не выполняется
    PRINTS "This message should NOT appear.\n"
    PRINTS NEWLINE

SKIP_BLOCK:
    ; --- Test 15: Инструкции NOP и BREAK ---
    PRINTS MSG_NOP_BREAK
    NOP                    ; без действия
    BREAK                  ; точка останова для отладки (ожидание Enter)
    HALT                   ; завершение программы

; =============================================================================
; Подпрограмма: Факториал (рекурсивно)
; Вычисляет факториал числа, находящегося в R17, результат возвращается в R1.
; Используются стековые операции для сохранения промежуточных значений.
; =============================================================================
FACTORIAL:
    CMP R17, 1
    IF EQ, FACT_RET
    PUSH R17
    LOADI R2, 1
    SUB R17, R17, R2
    CALL FACTORIAL
    POP R17
    MUL R1, R17, R1
    RET
FACT_RET:
    LOADI R1, 1
    RET

