#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <errno.h>

#include "cintelhex.h"

#include "opcodes.h"

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;

byte *mem = new byte[65535];
bool verb = 1;
dword cycles = 0;

//aux

std::string toRegisterPair(byte r) {
	switch (r) {
	case 0b00: return "BC";
	case 0b01: return "DE";
	case 0b10: return "HL";
	case 0b11: return "SP";
	default: return "error toRegisterPair()";
	}
}

std::string toRegister(byte r) {
	switch (r) {
	case 0b111: return "A";
	case 0b000: return "B";
	case 0b001: return "C";
	case 0b010: return "D";
	case 0b011: return "E";
	case 0b100: return "H";
	case 0b101: return "L";
	default: return "error toRegister()";
	}
}

byte parity(byte t) {
	t ^= t >> 4;
	t ^= t >> 2;
	t ^= t >> 1;
	return (~t) & 1;
}

byte parity(word t) {
	t ^= t >> 8;
	t ^= t >> 4;
	t ^= t >> 2;
	t ^= t >> 1;
	return (~t) & 1;
}

void setBit(byte *v, byte n, byte b) {
	b ? *v |= 1UL << n : *v &= ~(1UL << n);
}

constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

std::string hexStr(uint8_t *data, int len) {
	std::string s(len * 2, '\0');
	for (int i = 0; i < len; i++) {
		s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	return s;
}

std::string hexStr(uint16_t *data, int len) {
	std::string s(len * 2, '\0');
	for (int i = 0; i < len; i++) {
		s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	return s;
}

bool isPrintable(unsigned char chr) {
	int chrVal = (int)chr;
	if (isprint(chrVal) != 0) return true; else return false;
}

void listByteA(uint8_t *arr, int size, uint16_t off = 0x0000) {
	std::cout << "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f" << std::endl;

	uint8_t vOffl = off & 0xff;
	uint8_t vOffh = (off >> 8) & 0xff;

	int i = 0;
	while (i < size) {
		//std::cout << hexStr(&vOffl, 1) << hexStr(&vOffh, 1) << " ";
		std::cout << hexStr(&vOffh, 1) << hexStr(&vOffl, 1) << " ";

		char txt[16] = { '\0' };

		for (int j = 0; j < 16; j++) {
			if (i >= size) { break; }
			std::cout << hexStr(&arr[i + off], 1) << " ";
			if (isPrintable((char)arr[i + off]) == true) txt[j] = (char)arr[i + off]; else txt[j] == (char)0;
			i++;
		}

		for (int j = 0; j < 16; j++) {
			if (txt[j] != 0)
				std::cout << txt[j];
			else
				std::cout << ".";
		}
		std::cout << std::endl;

		if (vOffl == 0xF0) {
			vOffh += 0x01;
			vOffl = 0;
		}
		else {
			vOffl += 0x10;
		}

	}

	std::cout << std::endl;
}


byte in(word addr) {
	std::cout << "IN { 0h, " << hexStr(&addr, 2) << "h}" << std::endl;
	return 0x0;
}

void out(byte t, word addr) {
	std::cout << "IN { " << hexStr(&t, 1) << "h, " << hexStr(&addr, 2) << "h}" << std::endl;
}


void usage() {
	std::cout << "Usage: z80asm <intel hex bin>" << std::endl;
}



int main(int argc, char **argv) {
	std::string inputPath = "";
	//std::ifstream file(inputPath);
	//std::stringstream ss;
	//ss << file.rdbuf();
	//std::string hex;
	//hex = ss.str();


	if (argc < 2) {
		std::cout << "Too few arguments" << std::endl << std::endl;
		usage();
		return 1;
	}
	else if (argc > 2) {
		std::cout << "Too much arguments" << std::endl << std::endl;
		usage();
		return 1;
	}
	else {
		inputPath = std::string(argv[1]);
	}

	if (!std::filesystem::exists(inputPath)) { std::cout << "Nonexistant file" << std::endl; return 1; }

	ihex_recordset_t* r = ihex_rs_from_file(inputPath.c_str());
	int res = ihex_mem_copy(r, mem, 65535,
		IHEX_WIDTH_32BIT, IHEX_ORDER_BIGENDIAN); // Copy data to target region.

	word org = r->ihrs_records->ihr_address;
	
	listByteA(mem, 1024, org);

	// Registers
	byte IR = 0;
	byte EIR = 0;

	union {
		word P;
		struct {
			byte P0;
			byte P1;
		};
	} P_;
	

	union {
		word AF;
		struct {
			union {
				byte A;
				struct {
					byte Ah : 4;
					byte Al : 4;
				};
			};
			union {
				byte F;
				struct {
					byte FS : 1;
					byte FZ : 1, : 1;
					byte FH : 1, : 1;
					byte FPV : 1;
					byte FN : 1;
					byte FC : 1;
				};
			};
		};
	} AF_;

	union {
		word xAF;
		struct {
			union {
				byte xA;
				struct {
					byte xAh : 4;
					byte xAl : 4;
				};
			} xA_;
			union {
				byte Fp;
				struct {
					byte FSp : 1;
					byte FZp : 1, : 1;
					byte FHp : 1, : 1;
					byte FPVp : 1;
					byte FNp : 1;
					byte FCp : 1;
				};
			} xF_;
		};
	} xAF_;
	
	
	union {
		word BC;
		struct {
			byte B;
			byte C;
		};
	} BC_;
	union {
		word DE;
		struct {
			byte D;
			byte E;
		};
	} DE_;
	union {
		word HL;
		struct {
			byte H;
			byte L;
		};
	} HL_;

	union {
		word xBC;
		struct {
			byte xB;
			byte xC;
		};
	} xBC_;
	union {
		word xDE;
		struct {
			byte xD;
			byte xE;
		};
	} xDE_;
	union {
		word xHL;
		struct {
			byte xH;
			byte xL;
		};
	} xHL_;

	word IX = 0;;
	word IY = 0;

	union {
		word SP;
		struct {
			byte SPh;
			byte SPl;
		};
	} SP_;

	byte I = 0;
	byte R = 0;

	byte IFF1;
	byte IFF2;


	word PC = 0;

	// Fetch
	PC = 0x8000;
	IR = mem[PC];

	if (verb) std::cout << "Fetch byte " << IR << " address " << PC << std::endl;

	PC++;

	/*switch (IR) {
	case 0x00: break;
	case 0x01: P0 = mem[PC]; PC += 1; P1 = mem[PC]; PC += 1; break;
	case 0x02: break;
	case 0x03: break;
	}*/

	if (
		IR == 0x00 ||
		IR == 0x02 ||
		IR == 0x03 ||
		IR == 0x04 ||
		IR == 0x05 ||
		IR == 0x07 ||
		IR == 0x08 ||
		IR == 0x09 ||
		IR == 0x0a ||
		IR == 0x0b ||
		IR == 0x0c ||
		IR == 0x0d ||
		IR == 0x0f ||
		IR == 0x12 ||
		IR == 0x13 ||
		IR == 0x14 ||
		IR == 0x15 ||
		IR == 0x17 ||
		IR == 0x19 ||
		IR == 0x1a ||
		IR == 0x1b ||
		IR == 0x1c ||
		IR == 0x1d ||
		IR == 0x1f ||
		IR == 0x23 ||
		IR == 0x24 ||
		IR == 0x25 ||
		IR == 0x27 ||
		IR == 0x29 ||
		IR == 0x2b ||
		IR == 0x2c ||
		IR == 0x2d ||
		IR == 0x2f ||
		IR == 0x33 ||
		IR == 0x34 ||
		IR == 0x35 ||
		IR == 0x37 ||
		IR == 0x39 ||
		IR == 0x3b ||
		IR == 0x3c ||
		IR == 0x3d ||
		IR == 0x3f ||

		IR >= 0x40 ||
		IR <= 0xbf ||

		IR == 0xc0 ||
		IR == 0xc1 ||
		IR == 0xc5 ||
		IR == 0xc7 ||
		IR == 0xc8 ||
		IR == 0xc9 ||
		IR == 0xcf ||

		IR == 0xd0 ||
		IR == 0xd1 ||
		IR == 0xd5 ||
		IR == 0xd7 ||
		IR == 0xd8 ||
		IR == 0xd9 ||
		IR == 0xdf ||

		IR == 0xe0 ||
		IR == 0xe1 ||
		IR == 0xe3 ||
		IR == 0xe5 ||
		IR == 0xe7 ||
		IR == 0xe8 ||
		IR == 0xe9 ||
		IR == 0xeb ||
		IR == 0xef ||

		IR == 0xf0 ||
		IR == 0xf1 ||
		IR == 0xf3 ||
		IR == 0xf5 ||
		IR == 0xf7 ||
		IR == 0xf8 ||
		IR == 0xf9 ||
		IR == 0xfb ||
		IR == 0xff
		) {		// 1

	} else if (
		IR == 0x06 ||
		IR == 0x0e ||
		IR == 0x10 ||
		IR == 0x16 ||
		IR == 0x18 ||
		IR == 0x1e ||
		IR == 0x20 ||
		IR == 0x26 ||
		IR == 0x28 ||
		IR == 0x2e ||
		IR == 0x30 ||
		IR == 0x36 ||
		IR == 0x38 ||
		IR == 0x3e ||

		IR == 0xc6 ||
		IR == 0xce ||
		IR == 0xd3 ||
		IR == 0xd6 ||
		IR == 0xdb ||
		IR == 0xde ||
		IR == 0xe6 ||
		IR == 0xee ||
		IR == 0xf6 ||
		IR == 0xfe
		) {		// 2
			P_.P0 = mem[PC];
			PC++;
	} else if (
		IR == 0x01 ||
		IR == 0x11 ||
		IR == 0x21 ||
		IR == 0x22 ||
		IR == 0x2a ||
		IR == 0x3a ||
		IR == 0x32 ||
		IR == 0x31 ||

		IR == 0xc2 ||
		IR == 0xc3 ||
		IR == 0xc4 ||
		IR == 0xca ||
		IR == 0xcc ||
		IR == 0xcd ||
		IR == 0xd2 ||
		IR == 0xd4 ||
		IR == 0xda ||
		IR == 0xdc ||
		IR == 0xe2 ||
		IR == 0xe4 ||
		IR == 0xea ||
		IR == 0xec ||
		IR == 0xf2 ||
		IR == 0xf4 ||
		IR == 0xfa ||
		IR == 0xfc
	) {
		P_.P0 = mem[PC];
		PC++;
		P_.P1 = mem[PC];
		PC++;
	}
	else if (IR == 0xed) {	// extended
		EIR = mem[PC];
		PC++;

		if (
			EIR & 0x0f >= 0x0 && EIR & 0x0f <= 0x2 ||
			EIR & 0x0f >= 0x4 && EIR & 0x0f <= 0xa ||
			EIR & 0x0f >= 0xc && EIR & 0x0f <= 0xf ||
			EIR == 0xa3 ||
			EIR == 0xb3 ||
			EIR == 0xab ||
			EIR == 0xbb
			) {

		}
		else if (
			EIR == 0x43 ||
			EIR == 0x53 ||
			EIR == 0x63 ||
			EIR == 0x73 ||
			EIR == 0x4b ||
			EIR == 0x5b ||
			EIR == 0x6b ||
			EIR == 0x7b
			) {
			P_.P0 = mem[PC];
			PC++;
			P_.P1 = mem[PC];
			PC++;
		}
	}

	// ================================ execute

	AF_.A = 12;
	std::cout << AF_.A << std::endl;

#if 0
	// =============== MEMORY/REGISTERS ===============
	if (IR == 0xed && EIR & 0b11001111 == 0b01001011) {		// ld rr, (nn)
		byte reg = (EIR >> 4) & 0b11;
		switch (reg) {
		case 0b00: C = mem[P]; B = mem[P + 1]; break;
		case 0b01: E = mem[P]; D = mem[P + 1]; break;
		case 0b10: L = mem[P]; H = mem[P + 1]; break;
		case 0b11: SPl = mem[P]; SPh = mem[P + 1]; break;
		}
		if (verb) std::cout << "ld " << toRegisterPair(reg) << ", $" << hexStr(&P, 2) << " {" << hexStr(&mem[P], 2) << "}" << std::endl;
		cycles += 20;
	}
	else if (IR & 0b11001111 == 0b00000001) {				// ld rr, nn
		byte reg = (IR >> 4) & 0b11;
		switch (reg) {
		case 0b00: BC = P; break;
		case 0b01: DE = P; break;
		case 0b10: HL = P; break;
		case 0b11: SP = P; break;
		}
		if (verb) std::cout << "ld " << toRegisterPair(reg) << ", " << hexStr(&P, 2) << "h" << std::endl;
		cycles += 10;
	}
	else if (IR & 0b11000000 == 0b01000000) {				// ld r, r'
		byte reg = (IR >> 3) & 0b111;
		byte regp = IR & 0b111;
		byte t = 0;
		switch (regp) {
		case 0b111: t = A; break;
		case 0b000: t = B; break;
		case 0b001: t = C; break;
		case 0b010: t = D; break;
		case 0b011: t = E; break;
		case 0b100: t = H; break;
		case 0b101: t = L; break;
		}
		switch (reg) {
		case 0b111: A = t; break;
		case 0b000: B = t; break;
		case 0b001: C = t; break;
		case 0b010: D = t; break;
		case 0b011: E = t; break;
		case 0b100: H = t; break;
		case 0b101: L = t; break;
		}
		if (verb) std::cout << "ld " << toRegister(reg) << ", " << toRegister(regp) << " {" << hexStr(&t, 1) << "}" << std::endl;
		cycles += 4;
	}
	else if (IR == 0b00000010) {							// ld (BC), A
		mem[BC] = A;
		if (verb) std::cout << "ld (BC), A {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR == 0b00010010) {							// ld (DE), A
		mem[DE] = A;
		if (verb) std::cout << "ld (DE), A {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR == 0b00110110) {							// ld (HL), n
		mem[HL] = P0;
		if (verb) std::cout << "ld (HL), " << hexStr(&P0, 1) << std::endl;
		cycles += 10;
	}
	else if (IR & 0b11111000 == 0b01110000) {				// ld (HL), r
		byte reg = IR & 0b111;
		switch (reg) {
		case 0b111: mem[HL] = A; break;
		case 0b000: mem[HL] = B; break;
		case 0b001: mem[HL] = C; break;
		case 0b010: mem[HL] = D; break;
		case 0b011: mem[HL] = E; break;
		case 0b100: mem[HL] = H; break;
		case 0b101: mem[HL] = L; break;
		}
		if (verb) std::cout << "ld (HL), " << toRegister(reg) << " {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR == 0b00111010) {							// ld A, (nn)
		A = mem[P];
		if (verb) std::cout << "ld A, $" << hexStr(&P, 2) << " {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 13;
	}
	else if (IR == 0b00110010) {							// ld (nn), A
		mem[P] = A;
		if (verb) std::cout << "ld $" << hexStr(&P, 2) << ", A {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 13;
	}
	else if (IR == 0xed && EIR & 0b11001111 == 0b01000011) {// ld (nn), rr
		byte reg = (EIR >> 4) & 0b11;
		switch (reg) {
		case 0b00: mem[P] = C; mem[P + 1] = B; break;
		case 0b01: mem[P] = E; mem[P + 1] = D; break;
		case 0b10: mem[P] = L; mem[P + 1] = H; break;
		case 0b11: mem[P] = SPl; mem[P + 1] = SPh; break;
		}
		if (verb) std::cout << "ld $" << hexStr(&P, 2) << ", " << toRegisterPair(reg) << " {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 20;
	}
	else if (IR == 0b00100010) {							// ld (nn), HL
		mem[P] = L; mem[P + 1] = H;
		if (verb) std::cout << "ld $" << hexStr(&P, 2) << ", HL {" << hexStr(&HL, 2) << "}" << std::endl;
		cycles += 16;
	}
	else if (IR == 0b00001010) {							// ld A, (BC)
		A = mem[BC];
		if (verb) std::cout << "ld A, (BC) {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR == 0b00011010) {							// ld A, (DE)
		A = mem[DE];
		if (verb) std::cout << "ld A, (DE) {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR == 0xed && EIR == 0b01010111) {				// ld A, I
		A = I;
		FH = 0; FN = 0; FPV = IFF2;
		if (verb) std::cout << "ld A, I {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 9;
	}
	else if (IR == 0xed && EIR == 0b01000111) {				// ld I, A
		I = A;
		if (verb) std::cout << "ld I, A {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 9;
	}
	else if (IR == 0xed && EIR == 0b01011111) {				// ld A, R
		A = R;
		FH = 0; FN = 0; FPV = IFF2;
		if (verb) std::cout << "ld A, R {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 9;
	}
	else if (IR == 0b00101010) {							// ld HL, (nn)
		HL = mem[P];
		if (verb) std::cout << "ld HL, $" << hexStr(&P, 2) << " {" << hexStr(&HL, 2) << "}" << std::endl;
		cycles += 16;
	}
	else if (IR == 0xed && EIR == 0b01001111) {				// ld R, A
		R = A;
		if (verb) std::cout << "ld R, A {" << hexStr(&R, 1) << "}" << std::endl;
		cycles += 9;
	}
	else if (IR == 0b11111001) {							// ld SP, HL
		SP = HL;
		if (verb) std::cout << "ld SP, HL {" << hexStr(&SP, 1) << "}" << std::endl;
		cycles += 6;
	}
	else if (IR == 0xed && EIR == 0b10101000) {				// ldd
		mem[DE] = mem[HL];
		DE--; HL--; BC--;
		FH = 0; FN = 0; BC == 0 ? FPV = 0 : FPV = 1;
		if (verb) std::cout << "ldd {" << BC << " " << hexStr(&mem[DE], 1) << "}" << std::endl;
		cycles += 16;
	}
	else if (IR == 0xed && EIR == 0b10111000) {				// lddr
		if (BC != 0) {
			mem[DE] = mem[HL];
			DE--; HL--; BC--;
			PC -= 2;
			FH = 0; FN = 0; FPV = 0;
		}
		if (verb) std::cout << "lddr {" << BC << " " << hexStr(&mem[DE], 1) << "}" << std::endl;
		BC != 0 ? cycles += 21 : cycles += 16;
	}
	else if (IR == 0xed && EIR == 0b10100000) {				// ldi
		mem[DE] = mem[HL];
		DE++; HL++; BC--;
		FH = 0; FN = 0; BC == 0 ? FPV = 0 : FPV = 1;
		if (verb) std::cout << "ldi {" << BC << " " << hexStr(&mem[DE], 1) << "}" << std::endl;
		cycles += 16;
	}
	else if (IR == 0xed && EIR == 0b10110000) {				// ldir
		if (BC != 0) {
			mem[DE] = mem[HL];
			DE++; HL++; BC--;
			PC -= 2;
			FH = 0; FN = 0; FPV = 0;
		}
		if (verb) std::cout << "lddr {" << BC << " " << hexStr(&mem[DE], 1) << "}" << std::endl;
		BC != 0 ? cycles += 21 : cycles += 16;
	}
	else if (IR & 0b11000111 == 0b01000110) {				// ld r, (HL)
		byte reg = (IR >> 3) & 0b111;
		switch (reg) {
		case 0b111: A = mem[HL]; break;
		case 0b000: B = mem[HL]; break;
		case 0b001: C = mem[HL]; break;
		case 0b010: D = mem[HL]; break;
		case 0b011: E = mem[HL]; break;
		case 0b100: H = mem[HL]; break;
		case 0b101: L = mem[HL]; break;
		}
		if (verb) std::cout << "ld " << toRegister(reg) << ", (HL) {" << hexStr(&mem[HL], 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR & 0b11001111 == 0b11000101) {				// push rr
		union {
			word t;
			struct {
				byte th;
				byte tl;
			};
		};

		byte reg = (EIR >> 4) & 0b11;
		switch (reg) {
		case 0b00: t = BC; break;
		case 0b01: t = DE; break;
		case 0b10: t = HL; break;
		case 0b11: t = AF; break;
		}

		mem[SP - 1] = th;
		mem[SP - 2] = tl;
		SP -= 2;

		if (verb) std::cout << "push " << toRegisterPair(reg) << " {" << hexStr(&t, 2) << "}" << std::endl;
		cycles += 11;
	}
	else if (IR & 0b11001111 == 0b11000001) {				// pop rr
		union {
			word t;
			struct {
				byte th;
				byte tl;
			};
		};

		th = mem[SP + 1];
		tl = mem[SP];
		SP += 2;

		byte reg = (EIR >> 4) & 0b11;
		switch (reg) {
		case 0b00: BC = t; break;
		case 0b01: DE = t; break;
		case 0b10: HL = t; break;
		case 0b11: AF = t; break;
		}

		if (verb) std::cout << "pop " << toRegisterPair(reg) << " {" << hexStr(&t, 2) << "}" << std::endl;
		cycles += 10;
	}
	else if (IR == 0b00001000) {							// ex AF, AF'
		std::swap(AF, AFp);
		if (verb) std::cout << "ex AF, AF' {" << Ap << " " << Fp << "}" << std::endl;
		cycles += 4;
	}
	else if (IR == 0b11101011) {							// ex DE, HL
		std::swap(DE, HL);
		if (verb) std::cout << "ex DE, HL {" << DE << " " << HL << "}" << std::endl;
		cycles += 4;
	}
	else if (IR == 0b11100011) {							// ex (SP), HL
		std::swap(mem[SP], L);
		std::swap(mem[SP + 1], H);
		if (verb) std::cout << "ex (SP), HL {" << DE << " " << HL << "}" << std::endl;
		cycles += 19;
	}
	else if (IR == 0b11100011) {							// exx
		std::swap(BC, BCp);
		std::swap(DE, DEp);
		std::swap(HL, HLp);
		if (verb) std::cout << "exx {" << BC << " " << DE << " " << HL << "}" << std::endl;
		cycles += 4;
	}

	// =============== DEVICE I/O ===============
	else if (IR == 0xed && EIR & 0b11000111 == 0b01000000) {// in r, (C)
		byte t = in(BC);
		byte reg = (EIR >> 3) & 0b111;
		switch (reg) {
		case 0b111: A = t; break;
		case 0b000: B = t; break;
		case 0b001: C = t; break;
		case 0b010: D = t; break;
		case 0b011: E = t; break;
		case 0b100: H = t; break;
		case 0b101: L = t; break;
		}
		(char)t < 0 ? FS = 1 : 0;
		t == 0 ? FZ = 0 : 1;
		FPV = parity(t);
		FN = 0;
		if (verb) std::cout << "in " << toRegister(reg) << ", (C) {" << hexStr(&t, 1) << "h, $" << hexStr(&BC, 2) << "h}" << std::endl;
		cycles += 12;
	}
	else if (IR == 0b11011011) {							// in A, (N)
		A = in((A << 8) | P0);
		if (verb) std::cout << "in A, " << hexStr(&P0, 1) << "h {" << hexStr(&A, 1) << "h}" << std::endl;
		cycles += 11;
	}
	else if (IR == 0xed && EIR == 0b10101010) {				// ind
		mem[HL] = in(C);
		B--;
		HL--;
		B == 0 ? FZ = 1 : 0;
		if (verb) std::cout << "in A, " << hexStr(&P0, 1) << "h {" << hexStr(&A, 1) << "h}" << std::endl;
		cycles += 16;
	}



	else if (IR == 0xed && EIR == 0b01000100) {				// neg
		A == 0 ? FC = 1 : FC;
		A == 0x80 ? FPV = 1 : FPV;
		FN = 1;
		A = (byte)(-((char)A));
		if (verb) std::cout << "neg {" << A << "}" << std::endl;
		cycles += 8;
	}
	else if (IR = 0x00) {									// nop
		if (verb) std::cout << "nop" << std::endl;
		cycles += 4;
	}

	else if (IR == 0b10110110) {							// or (HL)
		A = A | mem[HL];
		if (verb) std::cout << "or (HL) {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR == 0b11110110) {							// or n
		A = A | P0;
		if (verb) std::cout << "or " << hexStr(&P0, 1) << " {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 7;
	}
	else if (IR & 0b11111000 == 0b10110000) {				// or r
		byte reg = (IR >> 3) & 0b111;
		switch (reg) {
		case 0b111: A = A | A;
		case 0b000: A = A | B;
		case 0b001: A = A | C;
		case 0b010: A = A | D;
		case 0b011: A = A | E;
		case 0b100: A = A | H;
		case 0b101: A = A | L;
		}
		if (verb) std::cout << "or " << toRegister(reg) << " {" << hexStr(&A, 1) << "}" << std::endl;
		cycles += 4;
	}
#endif
}
