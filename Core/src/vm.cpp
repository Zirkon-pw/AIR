#include "vm.h"

VirtualMachine::VirtualMachine() {
    reset();
}

void VirtualMachine::reset() {
    pc = 0;
    sp = STACK_SIZE;
    running = false;
    memset(reg, 0, sizeof(reg));
    storage.init();
    storage.restore();
}

uint32_t VirtualMachine::read32(uint32_t address) {
    if (address + 3 >= MEM_SIZE) {
        Serial.printf("read32: Address 0x%04X out of bounds\n", address);
        return 0;
    }
    return (storage.read(address) << 24) |
           (storage.read(address + 1) << 16) |
           (storage.read(address + 2) << 8) |
           storage.read(address + 3);
}

void VirtualMachine::write32(uint32_t address, uint32_t value) {
    if (address + 3 >= MEM_SIZE) {
        Serial.printf("write32: Address 0x%04X out of bounds\n", address);
        return;
    }
    storage.write(address,     (value >> 24) & 0xFF);
    storage.write(address + 1, (value >> 16) & 0xFF);
    storage.write(address + 2, (value >> 8) & 0xFF);
    storage.write(address + 3, value & 0xFF);
}

void VirtualMachine::loadProgram(const uint8_t* program, size_t size) {
    size = min(size, static_cast<size_t>(MEM_SIZE));
    for (size_t i = 0; i < size; i++) {
        storage.write(i, program[i]);
    }
}

void VirtualMachine::run() {
    running = true;
    while (running && pc < MEM_SIZE) {
        uint8_t opcode = storage.read(pc++);
        switch (opcode) {
            case OP_LOAD: {
                uint8_t reg_num = storage.read(pc++);
                uint32_t value = read32(pc);
                if (reg_num < NUM_REGS) reg[reg_num] = value;
                else Serial.printf("LOAD: Invalid register number: %d\n", reg_num);
                pc += 4;
                break;
            }
            case OP_STORE: {
                uint8_t reg_num = storage.read(pc++);
                uint32_t address = read32(pc);
                if (reg_num < NUM_REGS) write32(address, reg[reg_num]);
                else Serial.printf("STORE: Invalid register number: %d\n", reg_num);
                pc += 4;
                break;
            }
            case OP_ADD: {
                uint8_t dst = storage.read(pc++);
                uint8_t src1 = storage.read(pc++);
                uint8_t src2 = storage.read(pc++);
                if (dst < NUM_REGS && src1 < NUM_REGS && src2 < NUM_REGS) {
                    reg[dst] = reg[src1] + reg[src2];
                } else {
                    Serial.println("ADD: Invalid register number");
                }
                break;
            }
            case OP_HALT:
                running = false;
                break;
            default:
                Serial.printf("Unknown opcode: 0x%02X at address 0x%04X\n", opcode, pc - 1);
                running = false;
                break;
        }
    }
}

bool VirtualMachine::push(uint32_t value) {
    if (sp == 0) {
        Serial.println("Stack overflow");
        return false;
    }
    stack[--sp] = value;
    return true;
}

bool VirtualMachine::pop(uint32_t &value) {
    if (sp >= STACK_SIZE) {
        Serial.println("Stack underflow");
        return false;
    }
    value = stack[sp++];
    return true;
}

void VirtualMachine::persistState() {
    storage.persist();
}

void VirtualMachine::printState() {
    Serial.println("\nVM State:");
    Serial.printf("PC: 0x%04X\n", pc);
    for (int i = 0; i < NUM_REGS; i++) {
        Serial.printf("R%d: 0x%08X\n", i, reg[i]);
    }
    Serial.println("------------------");
}