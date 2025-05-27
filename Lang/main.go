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

// processEscapes преобразует escape-последовательности в соответствующие символы.
func processEscapes(s string) (string, error) {
	// Добавляем кавычки, если их нет, для strconv.Unquote
	if !strings.HasPrefix(s, "\"") {
		s = "\"" + s + "\""
	}
	t, err := strconv.Unquote(s)
	if err != nil {
		return "", fmt.Errorf("ошибка обработки escape-последовательности '%s': %v", s, err)
	}
	return t, nil
}

// splitLabel разбивает строку на метку и остальную часть.
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
	if val, ok := FLAGS[arg]; ok {
		return val, nil
	}

	// Убираем скобки [ ] для адресации
	if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
		arg = strings.TrimSpace(arg[1 : len(arg)-1])
	}

	// Регистр R<n>
	if matched, _ := regexp.MatchString(`^R\d+$`, arg); matched {
		return strconv.Atoi(arg[1:])
	}

	// Шестнадцатеричное число 0x...
	if strings.HasPrefix(arg, "0x") {
		v, err := strconv.ParseInt(arg[2:], 16, 32)
		return int(v), err
	}

	// Десятичное число
	if num, err := strconv.Atoi(arg); err == nil {
		return num, nil
	}

	// Символ 'c'
	if strings.HasPrefix(arg, "'") && strings.HasSuffix(arg, "'") && len(arg) == 3 {
		return int(arg[1]), nil
	}

	// Метка
	if val, ok := ac.symbols[arg]; ok {
		return val, nil
	}

	return 0, fmt.Errorf("неизвестная метка или значение: '%s'", arg)
}

// preprocessLine удаляет комментарии, обрезает пробелы и отделяет метку.
func (ac *AsmCompiler) preprocessLine(line string) (string, string) {
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

	if strings.HasPrefix(instr, ".") {
		return []string{line}, nil
	}

	parts := strings.Fields(instr)
	mnemonic := strings.ToUpper(parts[0])
	args := ""
	if len(parts) > 1 {
		args = strings.TrimSpace(instr[len(parts[0]):])
	}

	// Обработка MOV с MOD.
	if mnemonic == "MOV" && regexp.MustCompile(`\bMOD\b`).MatchString(args) {
		re := regexp.MustCompile(`(R\d+)\s*,\s*(R\d+|\d+)\s+MOD\s+(R\d+|\d+)`)
		matches := re.FindStringSubmatch(args)
		if len(matches) != 4 {
			return nil, fmt.Errorf("неверный формат MOV с MOD: %s (Строка %d)", args, lineNumber)
		}
		dest, X, Y := matches[1], matches[2], matches[3]

		var extra []string
		prefix := ""
		if label != "" {
			prefix = label + ": "
		}

		// Загружаем X, если это immediate
		xReg := X
		if !strings.HasPrefix(X, "R") {
			extra = append(extra, fmt.Sprintf("LOADI R30, %s", X))
			xReg = "R30"
		}
		// Загружаем Y, если это immediate
		yReg := Y
		if !strings.HasPrefix(Y, "R") {
			extra = append(extra, fmt.Sprintf("LOADI R31, %s", Y))
			yReg = "R31"
		}

		// Вычисляем MOD
		extra = append(extra, fmt.Sprintf("%sDIV R30, %s, %s", prefix, xReg, yReg))
		extra = append(extra, fmt.Sprintf("MUL R31, R30, %s", yReg))
		extra = append(extra, fmt.Sprintf("SUB %s, %s, R31", dest, xReg))
		return extra, nil
	}

	// Обработка арифметических операций с immediate
	arithmeticOps := map[string]bool{"ADD": true, "SUB": true, "MUL": true, "DIV": true, "AND": true, "OR": true, "XOR": true}
	if arithmeticOps[mnemonic] {
		operands := []string{}
		for _, op := range strings.Split(args, ",") {
			operands = append(operands, strings.TrimSpace(op))
		}
		if mnemonic == "SUB" && len(operands) == 2 {
			operands = []string{operands[0], operands[0], operands[1]}
		}
		if len(operands) != 3 {
			return nil, fmt.Errorf("неверное число аргументов для '%s' (Строка %d)", mnemonic, lineNumber)
		}
		var extra []string
		tempUsed := map[string]string{}
		nextTempReg := 30
		for i := 1; i <= 2; i++ { // Проверяем 2-й и 3-й операнды
			op := operands[i]
			if !strings.HasPrefix(op, "R") {
				if t, ok := tempUsed[op]; ok {
					operands[i] = t
				} else {
					if nextTempReg > 31 {
						return nil, errors.New("нет временных регистров")
					}
					temp := fmt.Sprintf("R%d", nextTempReg)
					nextTempReg++
					extra = append(extra, fmt.Sprintf("LOADI %s, %s", temp, op))
					tempUsed[op] = temp
					operands[i] = temp
				}
			}
		}
		newLine := fmt.Sprintf("%s %s, %s, %s", mnemonic, operands[0], operands[1], operands[2])
		if label != "" {
			newLine = label + ": " + newLine
		}
		return append(extra, newLine), nil
	}

	// Обработка CMP reg, reg -> CMP reg, imm (если возможно)
	if mnemonic == "CMP" {
		operands := []string{}
		for _, op := range strings.Split(args, ",") {
			operands = append(operands, strings.TrimSpace(op))
		}
		if len(operands) == 2 && strings.HasPrefix(operands[1], "R") {
			return nil, fmt.Errorf("CMP reg, reg не поддерживается. Используйте CMP reg, imm (Строка %d)", lineNumber)
		}
	}

	// Обработка PRINTS "..."
	if mnemonic == "PRINTS" && strings.HasPrefix(args, "\"") {
		return nil, fmt.Errorf("PRINTS \"...\" не поддерживается. Используйте .ASCIIZ и метку (Строка %d)", lineNumber)
	}

	return []string{line}, nil
}

// calculateLineLength вычисляет длину строки в байтах.
func (ac *AsmCompiler) calculateLineLength(line string, lineNumber int) (int, error) {
	_, instr := ac.preprocessLine(line)
	if instr == "" {
		return 0, nil
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
				return 0, fmt.Errorf("строка %d: %w", lineNumber, err)
			}
			return len([]byte(processed)) + 1, nil
		case ".SPACE":
			if len(tokens) < 2 {
				return 0, fmt.Errorf("нет аргумента для .SPACE (Строка %d)", lineNumber)
			}
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return 0, fmt.Errorf("строка %d: %w", lineNumber, err)
			}
			return val, nil
		case ".BYTE":
			return 1, nil
		case ".WORD":
			return 4, nil
		default:
			return 0, fmt.Errorf("неизвестная директива: %s (Строка %d)", directive, lineNumber)
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
		return 0, fmt.Errorf("неизвестная инструкция: %s (Строка %d)", mnemonic, lineNumber)
	}
	length := 1
	actualArgs := []string{}
	if args != "" {
		for _, a := range strings.Split(args, ",") {
			actualArgs = append(actualArgs, strings.TrimSpace(a))
		}
	}
	if len(actualArgs) != len(op.types) {
		return 0, fmt.Errorf("неверное число аргументов для '%s' (Строка %d)", mnemonic, lineNumber)
	}

	for i, argType := range op.types {
		arg := actualArgs[i]
		if argType == "reg" || argType == "flags" {
			length++
		} else if argType == "addr" || argType == "imm" {
			if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
				inner := strings.TrimSpace(arg[1 : len(arg)-1])
				if matched, _ := regexp.MatchString(`^R\d+$`, inner); matched {
					length += 2 // 0xFF + reg
				} else {
					length += 4 // [imm] -> imm
				}
			} else {
				length += 4 // imm или addr
			}
		} else {
			return 0, fmt.Errorf("неизвестный тип аргумента: %s (Строка %d)", argType, lineNumber)
		}
	}
	return length, nil
}

// compileLine генерирует байт-код для одной строки.
func (ac *AsmCompiler) compileLine(line string, lineNumber int) error {
	_, instr := ac.preprocessLine(line)
	if instr == "" {
		return nil
	}

	if strings.HasPrefix(instr, ".") {
		tokens := strings.Fields(instr)
		directive := strings.ToUpper(tokens[0])
		argsStr := strings.TrimSpace(instr[len(tokens[0]):])
		switch directive {
		case ".ASCIIZ":
			s := strings.Trim(argsStr, "\"'")
			processed, err := processEscapes(s)
			if err != nil {
				return fmt.Errorf("строка %d: %w", lineNumber, err)
			}
			data := append([]byte(processed), 0)
			ac.code = append(ac.code, data...)
			ac.ip += len(data)
		case ".SPACE":
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return fmt.Errorf("строка %d: %w", lineNumber, err)
			}
			ac.code = append(ac.code, make([]byte, val)...)
			ac.ip += val
		case ".BYTE":
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return fmt.Errorf("строка %d: %w", lineNumber, err)
			}
			ac.code = append(ac.code, byte(val))
			ac.ip++
		case ".WORD":
			val, err := ac.parseValue(tokens[1])
			if err != nil {
				return fmt.Errorf("строка %d: %w", lineNumber, err)
			}
			buf := new(bytes.Buffer)
			if err := binary.Write(buf, binary.LittleEndian, uint32(val)); err != nil {
				return err
			}
			ac.code = append(ac.code, buf.Bytes()...)
			ac.ip += 4
		default:
			return fmt.Errorf("неизвестная директива: %s (Строка %d)", directive, lineNumber)
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
		return fmt.Errorf("неверное число аргументов для '%s' (Строка %d)", mnemonic, lineNumber)
	}

	ac.code = append(ac.code, op.code)
	ac.ip++

	for i, argType := range op.types {
		arg := actualArgs[i]
		val, err := ac.parseValue(arg)
		if err != nil {
			// Если parseValue не справился, но это адрес [Rn], пробуем иначе
			if (argType == "addr" || argType == "imm") && strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
				inner := strings.TrimSpace(arg[1 : len(arg)-1])
				if matched, _ := regexp.MatchString(`^R\d+$`, inner); matched {
					regNum, _ := strconv.Atoi(inner[1:])
					ac.code = append(ac.code, 0xFF, byte(regNum))
					ac.ip += 2
					continue // Переходим к следующему аргументу
				}
			}
			return fmt.Errorf("ошибка парсинга '%s' (Строка %d): %v", arg, lineNumber, err)
		}

		if argType == "reg" || argType == "flags" {
			ac.code = append(ac.code, byte(val))
			ac.ip++
		} else if argType == "addr" || argType == "imm" {
			// Проверяем, не было ли это [Rn], что уже обработано
			if strings.HasPrefix(arg, "[") && strings.HasSuffix(arg, "]") {
				inner := strings.TrimSpace(arg[1 : len(arg)-1])
				if matched, _ := regexp.MatchString(`^R\d+$`, inner); matched {
					regNum, _ := strconv.Atoi(inner[1:])
					ac.code = append(ac.code, 0xFF, byte(regNum))
					ac.ip += 2
				} else { // [imm]
					buf := new(bytes.Buffer)
					binary.Write(buf, binary.LittleEndian, uint32(val))
					ac.code = append(ac.code, buf.Bytes()...)
					ac.ip += 4
				}
			} else { // imm или addr
				buf := new(bytes.Buffer)
				binary.Write(buf, binary.LittleEndian, uint32(val))
				ac.code = append(ac.code, buf.Bytes()...)
				ac.ip += 4
			}
		} else {
			return fmt.Errorf("неизвестный тип аргумента: %s (Строка %d)", argType, lineNumber)
		}
	}
	return nil
}

// compile читает входной файл, обрабатывает строки и записывает байт-код.
func (ac *AsmCompiler) compile(inputFile, outputFile string) error {
	file, err := os.Open(inputFile)
	if err != nil {
		return err
	}
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	lineNumber := 0
	for scanner.Scan() {
		lineNumber++
		line := scanner.Text()
		exLines, err := ac.expandLine(line, lineNumber)
		if err != nil {
			return err
		}
		lines = append(lines, exLines...)
	}
	if err := scanner.Err(); err != nil {
		return err
	}

	ac.symbols = make(map[string]int)
	codeOffset := 0
	for i, line := range lines {
		label, instr := ac.preprocessLine(line)
		if label != "" {
			if _, exists := ac.symbols[label]; exists {
				return fmt.Errorf("метка '%s' определена дважды (Строка ~%d)", label, i+1)
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

	ac.code = []byte{}
	ac.ip = 0
	for i, line := range lines {
		if err := ac.compileLine(line, i+1); err != nil {
			return err
		}
	}

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
	fmt.Printf("Компиляция завершена. Байт-код (%d байт) сохранён в %s\n", len(ac.code), outputFile)
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
