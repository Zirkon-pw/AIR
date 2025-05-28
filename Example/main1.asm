; =============================================================================
; FILE OPERATIONS TEST WITH MATH (FIXED VERSION)
; =============================================================================
JUMP MAIN

; ========================
; Data Section
; ========================
WELCOME_MSG:
    .ASCIIZ "File Math Test v1.1\n"

FILENAME:
    .ASCIIZ "input.txt"    ; Формат: "10\n20\n30\n"

RESULT_FILE:
    .ASCIIZ "result.txt"

READ_MODE:
    .ASCIIZ "r"

WRITE_MODE:
    .ASCIIZ "w"

BUFFER:
    .SPACE 256           ; Буфер чтения

NUMBERS:
    .SPACE 40            ; 10 чисел по 4 байта

RESULT_MSG:
    .ASCIIZ "Results saved to result.txt\n"

NEWLINE:
    .ASCIIZ "\n"

; ========================
; Main Program
; ========================
MAIN:
    PRINTS WELCOME_MSG

    ; 1. Открываем файл
    LOADI R1, FILENAME
    LOADI R2, READ_MODE
    OPEN R3, R1, R2
    
    ; Проверка ошибок (сравнение с -1 через 0xFFFFFFFF)
    LOADI R0, 0xFFFFFFFF
    CMP R3, R0
    IF EQ, OPEN_ERROR

    ; 2. Читаем файл
    LOADI R4, BUFFER
    LOADI R5, 256
    LOADI R6, 0
    READ R3, R4, R5, R6
    CLOSE R3

    ; 3. Парсим числа (R7=позиция в буфере, R8=счетчик чисел)
    LOADI R7, BUFFER
    LOADI R8, NUMBERS
    LOADI R9, 0          ; Количество чисел

PARSE_LOOP:
    ; Пропускаем не-цифры
    LOADB R10, [R7]
    CMP R10, 0           ; Конец строки?
    IF EQ, PARSE_END
    CMP R10, 10          ; '\n'?
    IF EQ, SKIP_CHAR
    CMP R10, 13          ; '\r'?
    IF EQ, SKIP_CHAR
    CMP R10, 32          ; ' '?
    IF EQ, SKIP_CHAR
    
    ; Нашли цифру - конвертируем число
    LOADI R11, 0         ; Текущее число
    LOADI R12, 10        ; Основание системы

CONVERT_LOOP:
    ; R10 уже содержит текущий символ
    CMP R10, 48          ; < '0'?
    IF LT, PARSE_ERROR
    CMP R10, 57          ; > '9'?
    IF GT, PARSE_ERROR
    
    ; Добавляем цифру к числу
    SUB R13, R10, 48     ; ASCII -> цифра
    MUL R11, R11, R12
    ADD R11, R11, R13
    
    ; Следующий символ
    ADD R7, R7, 1
    LOADB R10, [R7]
    CMP R10, 48          ; < '0'?
    IF LT, SAVE_NUMBER
    CMP R10, 57          ; > '9'?
    IF GT, SAVE_NUMBER
    JUMP CONVERT_LOOP

SAVE_NUMBER:
    STORE R11, [R8]      ; Сохраняем число
    ADD R8, R8, 4
    ADD R9, R9, 1
    JUMP PARSE_LOOP

SKIP_CHAR:
    ADD R7, R7, 1
    JUMP PARSE_LOOP

PARSE_END:
    ; 4. Вычисляем сумму (R14=sum)
    LOADI R14, 0
    LOADI R15, NUMBERS
    LOADI R16, 0         ; Счетчик

SUM_LOOP:
    CMP R16, R9          ; Сравниваем с количеством чисел
    IF EQ, CALC_AVG
    LOAD R17, [R15]
    ADD R14, R14, R17
    ADD R15, R15, 4
    ADD R16, R16, 1
    JUMP SUM_LOOP

CALC_AVG:
    ; 5. Вычисляем среднее (R18=avg)
    DIV R18, R14, R9

    ; 6. Записываем результаты
    LOADI R19, RESULT_FILE
    LOADI R20, WRITE_MODE
    OPEN R21, R19, R20
    
    ; Проверка ошибок
    LOADI R0, 0xFFFFFFFF
    CMP R21, R0
    IF EQ, WRITE_ERROR

    ; Преобразуем сумму в строку
    LOADI R22, BUFFER
    MOVE R11, R14        ; Число для конвертации
    CALL INT_TO_STR
    
    ; Записываем сумму
    WRITE R21, BUFFER, R23, R24
    
    ; Записываем пробел
    LOADI R25, 32        ; ASCII пробел
    STOREB R25, [BUFFER]
    WRITE R21, BUFFER, 1, R24
    
    ; Преобразуем среднее в строку
    LOADI R22, BUFFER
    MOVE R11, R18
    CALL INT_TO_STR
    
    ; Записываем среднее
    WRITE R21, BUFFER, R23, R24
    
    ; Закрываем файл
    CLOSE R21
    
    PRINTS RESULT_MSG
    HALT

; ========================
; Подпрограммы
; ========================

; INT_TO_STR (R11=число, R22=буфер) -> R23=длина
INT_TO_STR:
    LOADI R23, 0
    LOADI R26, 10
    
    ; Обработка нуля
    CMP R11, 0
    IF NE, CONVERT_DIGITS
    LOADI R27, 48        ; '0'
    STOREB R27, [R22]
    ADD R22, R22, 1
    LOADI R23, 1
    JUMP END_CONVERT

CONVERT_DIGITS:
    ; Сохраняем цифры в стек
    LOADI R28, 0         ; Счетчик цифр
DIGIT_LOOP:
    CMP R11, 0
    IF EQ, POP_DIGITS
    DIV R29, R11, R26    ; R29 = R11 / 10
    MUL R30, R29, R26
    SUB R1, R11, R30     ; R1 = цифра
    PUSH R1
    ADD R28, R28, 1
    MOVE R11, R29
    JUMP DIGIT_LOOP

POP_DIGITS:
    CMP R28, 0
    IF EQ, END_CONVERT
    SUB R28, R28, 1
    POP R1
    ADD R1, R1, 48       ; В ASCII
    STOREB R1, [R22]
    ADD R22, R22, 1
    ADD R23, R23, 1
    JUMP POP_DIGITS

END_CONVERT:
    STOREB 0, [R22]      ; Нуль-терминатор
    RET

; ========================
; Обработчики ошибок
; ========================
OPEN_ERROR:
    PRINTS "Cannot open input file\n"
    HALT

WRITE_ERROR:
    PRINTS "Cannot create result file\n"
    HALT

PARSE_ERROR:
    PRINTS "Invalid number format\n"
    HALT