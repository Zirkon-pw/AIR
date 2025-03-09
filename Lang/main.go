package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

// Определение типа для описания опкодов и форматов аргументов.
type Opcode struct {
	code  byte
	types []string
}

var OPCODES = map[string]Opcode{
	"NOP":      {0x00, []string{}},
	"HALT":     {0x01, []string{}},
	"JUMP":     {0x02, []string{"addr"}},
	"CALL":     {0x03, []string{"addr"}},
	"RET":      {0x04, []string{}},
	"IF":       {0x05, []string{"flags", "addr"}},
	"LOAD":     {0x10, []string{"reg", "addr"}},
	"STORE":    {0x11, []string{"reg", "addr"}},
	"MOVE":     {0x12, []string{"reg", "reg"}},
	"PUSH":     {0x13, []string{"reg"}},
	"POP":      {0x14, []string{"reg"}},
	"LOADI":    {0x15, []string{"reg", "imm"}},
	"ADD":      {0x20, []string{"reg", "reg", "reg"}},
	"SUB":      {0x21, []string{"reg", "reg", "reg"}},
	"MUL":      {0x22, []string{"reg", "reg", "reg"}},
	"DIV":      {0x23, []string{"reg", "reg", "reg"}},
	"AND":      {0x24, []string{"reg", "reg", "reg"}},
	"OR":       {0x25, []string{"reg", "reg", "reg"}},
	"XOR":      {0x26, []string{"reg", "reg", "reg"}},
	"NOT":      {0x27, []string{"reg", "reg"}},
	"CMP":      {0x28, []string{"reg", "imm"}},
	"FS_LIST":  {0x34, []string{"addr"}},
	"ENV_LIST": {0x42, []string{"addr"}},
	"PRINT":    {0x50, []string{"reg"}},
	"INPUT":    {0x51, []string{"reg"}},
	"PRINTS":   {0x52, []string{"addr"}},
	"SHL":      {0x30, []string{"reg", "reg", "imm"}},
	"SHR":      {0x31, []string{"reg", "reg", "imm"}},
	"BREAK":    {0x32, []string{}},
	"SNAPSHOT": {0x60, []string{}},
	"RESTORE":  {0x61, []string{}},
	"OPEN":     {0x70, []string{"reg", "reg", "reg"}},
	"READ":     {0x71, []string{"reg", "reg", "reg", "reg"}},
	"WRITE":    {0x72, []string{"reg", "reg", "reg", "reg"}},
	"CLOSE":    {0x73, []string{"reg"}},
	"SEEK":     {0x74, []string{"reg", "imm", "imm", "reg"}},
}

var FLAGS = map[string]int{
	"EQ": 0x01, // Equal
	"NE": 0x02, // Not equal
	"LT": 0x04, // Less than
	"GT": 0x08, // Greater than
	"GE": 0x08, // Greater or Equal (синоним GT)
}

func argSize(argType string) (int, error) {
	switch argType {
	case "reg", "flags":
		return 1, nil
	case "addr", "imm":
		return 4, nil
	default:
		return 0, fmt.Errorf("неизвестный тип аргумента: %s", argType)
	}
}

// processEscapes преобразует escape-последовательности в соответствующие символы.
func processEscapes(s string) (string, error) {
	// Используем strconv.Unquote для обработки escape-последовательностей.
	// Если строка не заключена в кавычки, добавляем их.
	if !(strings.HasPrefix(s, "\"") || strings.HasPrefix(s, "'")) {
		s = "\"" + s + "\""
	}
	return strconv.Unquote(s)
}

// splitLabel разбивает строку на метку и остальную часть,
// ищет двоеточие только вне строковых литералов.
func splitLabel(line string) (string, string) {
	inQuote := false
	var quoteChar rune
	for i, ch := range line {
		if ch == '"' || ch == '\'' {
			if !inQuote {
				inQuote = true
				quoteChar = ch
			} else if ch == quoteChar {
				inQuote = false
			}
		}
		if ch == ':' && !inQuote {
			return strings.TrimSpace(line[:i]), strings.TrimSpace(line[i+1:])
		}
	}
	return "", line
}

type AsmCompiler struct {
	symbols map[string]int
	code    []byte
	ip      int
}

func NewAsmCompiler() *AsmCompiler {
	return &AsmCompiler{
		symbols: make(map[string]int),
		code:    []byte{},
		ip:      0,
	}
}

// parseValue преобразует строковое представление аргумента в число.
func (ac *AsmCompiler) parseValue(arg string) (int, error) {
	arg = strings.TrimSpace(arg)
	// Если аргумент – символический флаг
	if val, ok := FLAGS[arg]; ok {
		return val, nil
	}
	if strings.ToLower(arg) == "flags" {
		return 0, nil
	}
	if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
		arg = strings.TrimSpace(arg[1 : len(arg)-1])
	}
	if strings.HasPrefix(arg, "0x") {
		v, err := strconv.ParseInt(arg[2:], 16, 32)
		return int(v), err
	} else if (strings.HasPrefix(arg, "'") && strings.HasSuffix(arg, "'")) ||
		(strings.HasPrefix(arg, "\"") && strings.HasSuffix(arg, "\"")) {
		if len(arg) < 3 {
			return 0, fmt.Errorf("строковая константа слишком короткая: %s", arg)
		}
		return int(arg[1]), nil
	} else if num, err := strconv.Atoi(arg); err == nil {
		return num, nil
	}
	// Регистр в формате R\d+
	match, _ := regexp.MatchString(`^R\d+$`, arg)
	if match {
		return strconv.Atoi(arg[1:])
	}
	// Проверка метки
	if val, ok := ac.symbols[arg]; ok {
		return val, nil
	}
	return 0, fmt.Errorf("неизвестная метка или значение: '%s'", arg)
}

// preprocessLine удаляет комментарии, обрезает пробелы и отделяет метку.
func (ac *AsmCompiler) preprocessLine(line string) (string, string) {
	// Удаляем комментарии (начиная с ';')
	if idx := strings.Index(line, ";"); idx != -1 {
		line = line[:idx]
	}
	line = strings.TrimSpace(line)
	if line == "" {
		return "", ""
	}
	label, rest := splitLabel(line)
	return label, rest
}

// expandLine обрабатывает псевдоинструкции и возвращает список строк.
func (ac *AsmCompiler) expandLine(line string, lineNumber int) ([]string, error) {
	label, instr := ac.preprocessLine(line)
	if instr == "" {
		if label != "" {
			return []string{line}, nil
		}
		return []string{}, nil
	}
	// Если инструкция начинается с точки – это директива, оставляем как есть.
	if strings.HasPrefix(instr, ".") {
		return []string{line}, nil
	}

	parts := strings.Fields(instr)
	mnemonic := strings.ToUpper(parts[0])
	args := ""
	if len(parts) > 1 {
		args = strings.TrimSpace(instr[len(parts[0]):])
	}

	// 1. Обработка MOV с MOD.
	if mnemonic == "MOV" && regexp.MustCompile(`\bMOD\b`).MatchString(args) {
		argList := strings.Split(args, ",")
		if len(argList) != 2 {
			return nil, fmt.Errorf("неверный формат инструкции MOV (Строка %d)", lineNumber)
		}
		dest := strings.TrimSpace(argList[0])
		expr := strings.TrimSpace(argList[1])
		tokens := strings.Fields(expr)
		if len(tokens) != 3 || strings.ToUpper(tokens[1]) != "MOD" {
			return nil, fmt.Errorf("неверный формат выражения в MOV с MOD (Строка %d)", lineNumber)
		}
		X := tokens[0]
		Y := tokens[2]
		var extra []string
		prefix := ""
		if label != "" {
			prefix = label + ": "
		}
		extra = append(extra, fmt.Sprintf("%sDIV R30, %s, %s", prefix, X, Y))
		extra = append(extra, fmt.Sprintf("MUL R31, R30, %s", Y))
		extra = append(extra, fmt.Sprintf("SUB %s, %s, R31", dest, X))
		return extra, nil
	}

	// 2. Обработка арифметических инструкций (с немедленными операндами).
	arithmeticOps := map[string]bool{"ADD": true, "SUB": true, "MUL": true, "DIV": true, "AND": true, "OR": true, "XOR": true}
	if arithmeticOps[mnemonic] {
		operands := []string{}
		for _, op := range strings.Split(args, ",") {
			operands = append(operands, strings.TrimSpace(op))
		}
		// Если SUB с 2 операндами, преобразуем в формат с 3 операндами.
		if mnemonic == "SUB" && len(operands) == 2 {
			operands = []string{operands[0], operands[0], operands[1]}
		}
		if len(operands) != 3 {
			return nil, fmt.Errorf("неверное число аргументов для '%s' (Строка %d)", mnemonic, lineNumber)
		}
		var extra []string
		tempUsed := map[string]string{}
		nextTemp := 30
		for i, op := range operands {
			matched, _ := regexp.MatchString(`^R\d+$`, op)
			if !matched {
				if t, ok := tempUsed[op]; ok {
					operands[i] = t
				} else {
					if nextTemp > 31 {
						return nil, errors.New("нет доступных временных регистров для немедленных значений")
					}
					temp := fmt.Sprintf("R%d", nextTemp)
					nextTemp++
					extra = append(extra, fmt.Sprintf("LOADI %s, %s", temp, op))
					tempUsed[op] = temp
					operands[i] = temp
				}
			}
		}
		newLine := fmt.Sprintf("%s %s", mnemonic, strings.Join(operands, ", "))
		if label != "" {
			newLine = label + ": " + newLine
		}
		return append(extra, newLine), nil
	}

	// 3. Для READ и WRITE: все операнды должны быть регистрами.
	if mnemonic == "READ" || mnemonic == "WRITE" {
		operands := []string{}
		for _, op := range strings.Split(args, ",") {
			operands = append(operands, strings.TrimSpace(op))
		}
		var extra []string
		tempUsed := map[string]string{}
		nextTemp := 30
		for i, op := range operands {
			matched, _ := regexp.MatchString(`^R\d+$`, op)
			if !matched {
				if t, ok := tempUsed[op]; ok {
					operands[i] = t
				} else {
					if nextTemp > 31 {
						return nil, errors.New("нет доступных временных регистров для немедленных значений")
					}
					temp := fmt.Sprintf("R%d", nextTemp)
					nextTemp++
					extra = append(extra, fmt.Sprintf("LOADI %s, %s", temp, op))
					tempUsed[op] = temp
					operands[i] = temp
				}
			}
		}
		newLine := fmt.Sprintf("%s %s", mnemonic, strings.Join(operands, ", "))
		if label != "" {
			newLine = label + ": " + newLine
		}
		return append(extra, newLine), nil
	}

	// 4. Если STORE записана как STORE [addr], reg – меняем порядок аргументов.
	if mnemonic == "STORE" {
		argList := []string{}
		for _, a := range strings.Split(args, ",") {
			argList = append(argList, strings.TrimSpace(a))
		}
		if len(argList) == 2 && strings.HasPrefix(argList[0], "[") && strings.HasSuffix(argList[0], "]") {
			args = fmt.Sprintf("%s, %s", argList[1], argList[0])
		}
	}

	// 5. Обработка адресных операндов вида [imm + Rn].
	if _, ok := OPCODES[mnemonic]; ok {
		_, argTypes := OPCODES[mnemonic].code, OPCODES[mnemonic].types
		actualArgs := []string{}
		if args != "" {
			for _, a := range strings.Split(args, ",") {
				actualArgs = append(actualArgs, strings.TrimSpace(a))
			}
		}
		newArgs := []string{}
		var extra []string
		for i, arg := range actualArgs {
			if i < len(argTypes) && (argTypes[i] == "addr" || argTypes[i] == "imm") {
				if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
					inner := strings.TrimSpace(arg[1 : len(arg)-1])
					if matched, _ := regexp.MatchString(`^R\d+$`, inner); matched {
						newArgs = append(newArgs, arg)
					} else if strings.Contains(inner, "+") {
						parts := strings.Split(inner, "+")
						if len(parts) != 2 {
							return nil, fmt.Errorf("неверное выражение адреса (Строка %d)", lineNumber)
						}
						immPart := strings.TrimSpace(parts[0])
						regPart := strings.TrimSpace(parts[1])
						if matched, _ := regexp.MatchString(`^R\d+$`, regPart); !matched {
							return nil, fmt.Errorf("ожидался регистр во второй части выражения (Строка %d)", lineNumber)
						}
						extra = append(extra, fmt.Sprintf("LOADI R30, %s", immPart))
						extra = append(extra, fmt.Sprintf("ADD R30, R30, %s", regPart))
						newArgs = append(newArgs, "[R30]")
					} else {
						newArgs = append(newArgs, arg)
					}
				} else {
					newArgs = append(newArgs, arg)
				}
			} else {
				newArgs = append(newArgs, arg)
			}
		}
		newLine := mnemonic
		if len(newArgs) > 0 {
			newLine += " " + strings.Join(newArgs, ", ")
		}
		if label != "" {
			newLine = label + ": " + newLine
		}
		return append(extra, newLine), nil
	}

	return []string{line}, nil
}

// calculateLineLength вычисляет длину строки в байтах для записи в байт-код.
func (ac *AsmCompiler) calculateLineLength(line string, lineNumber int) (int, error) {
	_, instr := ac.preprocessLine(line) // заменили label на _
	if instr == "" {
		return 0, nil
	}

	if strings.HasPrefix(instr, ".") {
		tokens := strings.Fields(instr)
		directive := strings.ToUpper(tokens[0])
		switch directive {
		case ".ASCIIZ":
			if len(tokens) < 2 {
				return 0, fmt.Errorf("отсутствует строка для .ASCIIZ (Строка %d)", lineNumber)
			}
			s := strings.TrimSpace(instr[len(tokens[0]):])
			s = strings.Trim(s, "\"'")
			processed, err := processEscapes(s)
			if err != nil {
				return 0, err
			}
			return len([]byte(processed)) + 1, nil
		case ".SPACE":
			if len(tokens) < 2 {
				return 0, fmt.Errorf("отсутствует аргумент для .SPACE (Строка %d)", lineNumber)
			}
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return 0, err
			}
			return val, nil
		case ".BYTE":
			return 1, nil
		case ".WORD":
			return 4, nil
		default:
			return 0, fmt.Errorf("неизвестная директива: %s", directive)
		}
	}

	parts := strings.Fields(instr)
	mnemonic := strings.ToUpper(parts[0])
	args := ""
	if len(parts) > 1 {
		args = strings.TrimSpace(instr[len(parts[0]):])
	}
	op, ok := OPCODES[mnemonic]
	if !ok {
		return 0, fmt.Errorf("неизвестная инструкция: %s", mnemonic)
	}
	length := 1 // опкод занимает 1 байт
	actualArgs := []string{}
	if args != "" {
		for _, a := range strings.Split(args, ",") {
			actualArgs = append(actualArgs, strings.TrimSpace(a))
		}
	}
	for i, argType := range op.types {
		if argType == "reg" || argType == "flags" {
			length++
		} else if argType == "addr" || argType == "imm" {
			if i < len(actualArgs) {
				arg := actualArgs[i]
				if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
					inner := strings.TrimSpace(arg[1 : len(arg)-1])
					if matched, _ := regexp.MatchString(`^R\d+$`, inner); matched {
						length += 2
					} else {
						length += 4
					}
				} else {
					length += 4
				}
			} else {
				return 0, fmt.Errorf("не хватает аргументов в строке %d", lineNumber)
			}
		} else {
			return 0, fmt.Errorf("неизвестный тип аргумента: %s", argType)
		}
	}
	return length, nil
}

// compileLine генерирует байт-код для одной строки.
func (ac *AsmCompiler) compileLine(line string, lineNumber int) error {
	_, instr := ac.preprocessLine(line) // заменили label на _
	if instr == "" {
		return nil
	}
	if strings.HasPrefix(instr, ".") {
		tokens := strings.Fields(instr)
		directive := strings.ToUpper(tokens[0])
		switch directive {
		case ".ASCIIZ":
			s := strings.TrimSpace(instr[len(tokens[0]):])
			s = strings.Trim(s, "\"'")
			processed, err := processEscapes(s)
			if err != nil {
				return err
			}
			data := append([]byte(processed), 0)
			ac.code = append(ac.code, data...)
			ac.ip += len(data)
		case ".SPACE":
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return err
			}
			ac.code = append(ac.code, make([]byte, val)...)
			ac.ip += val
		case ".BYTE":
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return err
			}
			ac.code = append(ac.code, byte(val))
			ac.ip++
		case ".WORD":
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return err
			}
			buf := new(bytes.Buffer)
			if err := binary.Write(buf, binary.LittleEndian, uint32(val)); err != nil {
				return err
			}
			ac.code = append(ac.code, buf.Bytes()...)
			ac.ip += 4
		default:
			return fmt.Errorf("неизвестная директива: %s", directive)
		}
		return nil
	}

	parts := strings.Fields(instr)
	mnemonic := strings.ToUpper(parts[0])
	args := ""
	if len(parts) > 1 {
		args = strings.TrimSpace(instr[len(parts[0]):])
	}
	op, ok := OPCODES[mnemonic]
	if !ok {
		return fmt.Errorf("неизвестная инструкция: %s (Строка %d)", mnemonic, lineNumber)
	}
	actualArgs := []string{}
	if args != "" {
		for _, a := range strings.Split(args, ",") {
			actualArgs = append(actualArgs, strings.TrimSpace(a))
		}
	}
	if len(actualArgs) != len(op.types) {
		return fmt.Errorf("неверное число аргументов для '%s': ожидалось %d, получено %d (Строка %d)",
			mnemonic, len(op.types), len(actualArgs), lineNumber)
	}

	// Записываем опкод.
	ac.code = append(ac.code, op.code)
	ac.ip++

	// Обработка аргументов.
	for i, argType := range op.types {
		arg := actualArgs[i]
		if argType == "reg" {
			if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
				arg = strings.TrimSpace(arg[1 : len(arg)-1])
			}
			matched, _ := regexp.MatchString(`^R\d+$`, arg)
			if !matched {
				return fmt.Errorf("ожидался регистр (например, R0), получено: %s (Строка %d)", arg, lineNumber)
			}
			regNum, err := strconv.Atoi(arg[1:])
			if err != nil {
				return err
			}
			if regNum < 0 || regNum >= 32 {
				return fmt.Errorf("регистр должен быть в диапазоне 0-31: %s (Строка %d)", arg, lineNumber)
			}
			ac.code = append(ac.code, byte(regNum))
			ac.ip++
		} else if argType == "flags" {
			flagsVal, err := ac.parseValue(arg)
			if err != nil {
				return err
			}
			// Проверяем маску флагов
			if flagsVal & ^0x0F != 0 {
				return fmt.Errorf("некорректная маска флагов: %s (Строка %d)", arg, lineNumber)
			}
			ac.code = append(ac.code, byte(flagsVal))
			ac.ip++
		} else if argType == "addr" || argType == "imm" {
			if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
				inner := strings.TrimSpace(arg[1 : len(arg)-1])
				if matched, _ := regexp.MatchString(`^R\d+$`, inner); matched {
					regNum, err := strconv.Atoi(inner[1:])
					if err != nil {
						return err
					}
					ac.code = append(ac.code, 0xFF, byte(regNum))
					ac.ip += 2
				} else {
					value, err := ac.parseValue(arg)
					if err != nil {
						return err
					}
					if value < 0 || value >= 65536 {
						return fmt.Errorf("адрес превышает 64К: %s (Строка %d)", arg, lineNumber)
					}
					buf := new(bytes.Buffer)
					if err := binary.Write(buf, binary.LittleEndian, uint32(value)); err != nil {
						return err
					}
					ac.code = append(ac.code, buf.Bytes()...)
					ac.ip += 4
				}
			} else {
				value, err := ac.parseValue(arg)
				if err != nil {
					return err
				}
				buf := new(bytes.Buffer)
				if argType == "addr" {
					if value < 0 || value >= 65536 {
						return fmt.Errorf("адрес превышает 64К: %s (Строка %d)", arg, lineNumber)
					}
					if err := binary.Write(buf, binary.LittleEndian, uint32(value)); err != nil {
						return err
					}
				} else { // imm
					if err := binary.Write(buf, binary.LittleEndian, int32(value)); err != nil {
						return err
					}
				}
				ac.code = append(ac.code, buf.Bytes()...)
				ac.ip += 4
			}
		} else {
			return fmt.Errorf("неизвестный тип аргумента: %s (Строка %d)", argType, lineNumber)
		}
	}
	return nil
}

// compile читает входной файл, обрабатывает строки, проводит два прохода и записывает байт-код.
func (ac *AsmCompiler) compile(inputFile, outputFile string) error {
	file, err := os.Open(inputFile)
	if err != nil {
		return err
	}
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		return err
	}

	// Первый проход: расширяем псевдоинструкции.
	var expanded []string
	for i, line := range lines {
		exLines, err := ac.expandLine(line, i+1)
		if err != nil {
			return err
		}
		expanded = append(expanded, exLines...)
	}

	// Первый проход: вычисление адресов меток.
	codeOffset := 0
	for i, line := range expanded {
		label, instr := ac.preprocessLine(line)
		if label != "" {
			if _, exists := ac.symbols[label]; exists {
				return fmt.Errorf("метка '%s' определена дважды (Строка %d)", label, i+1)
			}
			ac.symbols[label] = codeOffset
		}
		if instr != "" {
			length, err := ac.calculateLineLength(line, i+1)
			if err != nil {
				return err
			}
			codeOffset += length
		}
	}

	// Второй проход: генерация байт-кода.
	ac.code = []byte{}
	ac.ip = 0
	for i, line := range expanded {
		if err := ac.compileLine(line, i+1); err != nil {
			return fmt.Errorf("ошибка в строке %d: %v", i+1, err)
		}
	}

	// Записываем заголовок (4-байтовый размер кода) и сам код в выходной файл.
	outFile, err := os.Create(outputFile)
	if err != nil {
		return err
	}
	defer outFile.Close()
	if err := binary.Write(outFile, binary.LittleEndian, uint32(len(ac.code))); err != nil {
		return err
	}
	_, err = outFile.Write(ac.code)
	if err != nil {
		return err
	}
	fmt.Printf("Компиляция завершена. Байт-код сохранён в %s\n", outputFile)
	return nil
}

func main() {
	if len(os.Args) != 3 {
		progName := filepath.Base(os.Args[0])
		fmt.Printf("Использование: %s [input.asm] [output.bin]\n", progName)
		os.Exit(1)
	}
	inputFile := os.Args[1]
	outputFile := os.Args[2]
	compiler := NewAsmCompiler()
	if err := compiler.compile(inputFile, outputFile); err != nil {
		fmt.Printf("Ошибка компиляции: %v\n", err)
		os.Exit(1)
	}
}

