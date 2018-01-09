#include <stdio.h>
#include <string.h>
#include <string>
#include <xbyak/xbyak.h>
#include <cybozu/inttype.hpp>
#include <cybozu/test.hpp>

using namespace Xbyak;

void putNop(Xbyak::CodeGenerator *gen, int n)
{
	for (int i = 0; i < n; i++) {
		gen->nop();
	}
}

void diff(const std::string& a, const std::string& b)
{
	if (a == b) return;
	if (a.size() != b.size()) printf("size diff %d %d\n", (int)a.size(), (int)b.size());
	for (size_t i = 0; i < (std::min)(a.size(), b.size()); i++) {
		if (a[i] != b[i]) {
			printf("diff %d(%04x) %02x %02x\n", (int)i, (int)i, (unsigned char)a[i], (unsigned char)b[i]);
		}
	}
}

void dump(const std::string& m)
{
	printf("size=%d\n     ", (int)m.size());
	for (int i = 0; i < 16; i++) {
		printf("%02x ", i);
	}
	printf("\n     ");
	for (int i = 0; i < 16; i++) {
		printf("---");
	}
	printf("\n");
	for (size_t i = 0; i < m.size(); i++) {
		if ((i % 16) == 0) printf("%04x ", (int)(i / 16));
		printf("%02x ", (unsigned char)m[i]);
		if ((i % 16) == 15) putchar('\n');
	}
	putchar('\n');
}

CYBOZU_TEST_AUTO(test1)
{
	struct TestJmp : public Xbyak::CodeGenerator {
	/*
	     4                                  X0:
	     5 00000004 EBFE                    jmp short X0
	     6
	     7                                  X1:
	     8 00000006 <res 00000001>          dummyX1 resb 1
	     9 00000007 EBFD                    jmp short X1
	    10
	    11                                  X126:
	    12 00000009 <res 0000007E>          dummyX126 resb 126
	    13 00000087 EB80                    jmp short X126
	    14
	    15                                  X127:
	    16 00000089 <res 0000007F>          dummyX127 resb 127
	    17 00000108 E97CFFFFFF              jmp near X127
	    18
	    19 0000010D EB00                    jmp short Y0
	    20                                  Y0:
	    21
	    22 0000010F EB01                    jmp short Y1
	    23 00000111 <res 00000001>          dummyY1 resb 1
	    24                                  Y1:
	    25
	    26 00000112 EB7F                    jmp short Y127
	    27 00000114 <res 0000007F>          dummyY127 resb 127
	    28                                  Y127:
	    29
	    30 00000193 E980000000              jmp near Y128
	    31 00000198 <res 00000080>          dummyY128 resb 128
	    32                                  Y128:
	*/
		TestJmp(int offset, bool isBack, bool isShort, bool useNewLabel)
		{
			if (useNewLabel) {
				Label label;
				if (isBack) {
					L(label);
					putNop(this, offset);
					jmp(label);
				} else {
					if (isShort) {
						jmp(label);
					} else {
						jmp(label, T_NEAR);
					}
					putNop(this, offset);
					L(label);
				}
			} else {
				if (isBack) {
					L("@@");
					putNop(this, offset);
					jmp("@b");
				} else {
					if (isShort) {
						jmp("@f");
					} else {
						jmp("@f", T_NEAR);
					}
					putNop(this, offset);
					L("@@");
				}
			}
		}
	};
	static const struct Tbl {
		int offset;
		bool isBack;
		bool isShort;
		uint8 result[6];
		int size;
	} tbl[] = {
		{ 0, true, true, { 0xeb, 0xfe }, 2 },
		{ 1, true, true, { 0xeb, 0xfd }, 2 },
		{ 126, true, true, { 0xeb, 0x80 }, 2 },
		{ 127, true, false, {0xe9, 0x7c, 0xff, 0xff, 0xff }, 5 },
		{ 0, false, true, { 0xeb, 0x00 }, 2 },
		{ 1, false, true, { 0xeb, 0x01 }, 2 },
		{ 127, false, true, { 0xeb, 0x7f }, 2 },
		{ 128, false, false, { 0xe9, 0x80, 0x00, 0x00, 0x00 }, 5 },
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		for (int k = 0; k < 2; k++) {
			TestJmp jmp(p->offset, p->isBack, p->isShort, k == 0);
			const uint8 *q = (const uint8*)jmp.getCode();
			if (p->isBack) q += p->offset; /* skip nop */
			for (int j = 0; j < p->size; j++) {
				CYBOZU_TEST_EQUAL(q[j], p->result[j]);
			}
		}
	}
}

CYBOZU_TEST_AUTO(testJmpCx)
{
	struct TestJmpCx : public CodeGenerator {
		explicit TestJmpCx(void *p, bool useNewLabel)
			: Xbyak::CodeGenerator(16, p)
		{
			if (useNewLabel) {
				Label lp;
			L(lp);
#ifdef XBYAK64
				/*
					67 E3 FD ; jecxz lp
					E3 FB    ; jrcxz lp
				*/
				jecxz(lp);
				jrcxz(lp);
#else
				/*
					E3FE   ; jecxz lp
					67E3FB ; jcxz lp
				*/
				jecxz(lp);
				jcxz(lp);
#endif
			} else {
				inLocalLabel();
			L(".lp");
#ifdef XBYAK64
				/*
					67 E3 FD ; jecxz lp
					E3 FB    ; jrcxz lp
				*/
				jecxz(".lp");
				jrcxz(".lp");
#else
				/*
					E3FE   ; jecxz lp
					67E3FB ; jcxz lp
				*/
				jecxz(".lp");
				jcxz(".lp");
#endif
				outLocalLabel();
			}
		}
	};
	const struct {
		const char *p;
		size_t len;
	} tbl = {
#ifdef XBYAK64
		"\x67\xe3\xfd\xe3\xfb", 5
#else
		"\xe3\xfe\x67\xe3\xfb", 5
#endif
	};
	for (int j = 0; j < 2; j++) {
		char buf[16] = {};
		TestJmpCx code(buf, j == 0);
		CYBOZU_TEST_EQUAL(memcmp(buf, tbl.p, tbl.len), 0);
	}
}

#ifdef _MSC_VER
	#pragma warning(disable : 4310)
#endif
CYBOZU_TEST_AUTO(test2)
{
	struct TestJmp2 : public CodeGenerator {
	/*
	  1 00000000 90                      nop
	  2 00000001 90                      nop
	  3                                  f1:
	  4 00000002 <res 0000007E>          dummyX1 resb 126
	  6 00000080 EB80                     jmp f1
	  7
	  8                                  f2:
	  9 00000082 <res 0000007F>          dummyX2 resb 127
	 11 00000101 E97CFFFFFF               jmp f2
	 12
	 13
	 14 00000106 EB7F                    jmp f3
	 15 00000108 <res 0000007F>          dummyX3 resb 127
	 17                                  f3:
	 18
	 19 00000187 E980000000              jmp f4
	 20 0000018C <res 00000080>          dummyX4 resb 128
	 22                                  f4:
	*/
		TestJmp2(void *p, bool useNewLabel)
			: Xbyak::CodeGenerator(8192, p)
		{
			if (useNewLabel) {
				inLocalLabel();
				nop();
				nop();
			L(".f1");
				putNop(this, 126);
				jmp(".f1");
			L(".f2");
				putNop(this, 127);
				jmp(".f2", T_NEAR);

				jmp(".f3");
				putNop(this, 127);
			L(".f3");
				jmp(".f4", T_NEAR);
				putNop(this, 128);
			L(".f4");
				outLocalLabel();
			} else {
				nop();
				nop();
				Label f1, f2, f3, f4;
			L(f1);
				putNop(this, 126);
				jmp(f1);
			L(f2);
				putNop(this, 127);
				jmp(f2, T_NEAR);

				jmp(f3);
				putNop(this, 127);
			L(f3);
				jmp(f4, T_NEAR);
				putNop(this, 128);
			L(f4);
			}
		}
	};

	std::string ok;
	ok.resize(0x18C + 128, (char)0x90);
	ok[0x080] = (char)0xeb;
	ok[0x081] = (char)0x80;

	ok[0x101] = (char)0xe9;
	ok[0x102] = (char)0x7c;
	ok[0x103] = (char)0xff;
	ok[0x104] = (char)0xff;
	ok[0x105] = (char)0xff;

	ok[0x106] = (char)0xeb;
	ok[0x107] = (char)0x7f;

	ok[0x187] = (char)0xe9;
	ok[0x188] = (char)0x80;
	ok[0x189] = (char)0x00;
	ok[0x18a] = (char)0x00;
	ok[0x18b] = (char)0x00;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			TestJmp2 c(i == 0 ? 0 : Xbyak::AutoGrow, j == 0);
			c.ready();
			std::string m((const char*)c.getCode(), c.getSize());
			CYBOZU_TEST_EQUAL(m, ok);
		}
	}
}

#ifdef XBYAK32
int add5(int x) { return x + 5; }
int add2(int x) { return x + 2; }

CYBOZU_TEST_AUTO(test3)
{
	struct Grow : Xbyak::CodeGenerator {
		Grow(int dummySize)
			: Xbyak::CodeGenerator(128, Xbyak::AutoGrow)
		{
			mov(eax, 100);
			push(eax);
			call((void*)add5);
			add(esp, 4);
			push(eax);
			call((void*)add2);
			add(esp, 4);
			ret();
			for (int i = 0; i < dummySize; i++) {
				db(0);
			}
		}
	};
	for (int dummySize = 0; dummySize < 40000; dummySize += 10000) {
		printf("dummySize=%d\n", dummySize);
		Grow g(dummySize);
		g.ready();
		int (*f)() = (int (*)())g.getCode();
		int x = f();
		const int ok = 107;
		CYBOZU_TEST_EQUAL(x, ok);
	}
}
#endif

Xbyak::uint8 bufL[4096 * 32];
Xbyak::uint8 bufS[4096 * 2];

struct MyAllocator : Xbyak::Allocator {
	Xbyak::uint8 *alloc(size_t size)
	{
		if (size < sizeof(bufS)) {
			printf("test use bufS(%d)\n", (int)size);
			return bufS;
		}
		if (size < sizeof(bufL)) {
			printf("test use bufL(%d)\n", (int)size);
			return bufL;
		}
		fprintf(stderr, "no memory %d\n", (int)size);
		exit(1);
	}
	void free(Xbyak::uint8 *)
	{
	}
} myAlloc;

CYBOZU_TEST_AUTO(test4)
{
	struct Test4 : Xbyak::CodeGenerator {
		Test4(int size, void *mode, bool useNewLabel)
			: CodeGenerator(size, mode)
		{
			if (useNewLabel) {
				Label x;
				jmp(x);
				putNop(this, 10);
			L(x);
				ret();
			} else {
				inLocalLabel();
				jmp(".x");
				putNop(this, 10);
			L(".x");
				ret();
				outLocalLabel();
			}
		}
	};
	for (int i = 0; i < 2; i++) {
		const bool useNewLabel = i == 0;
		std::string fm, gm;
		Test4 fc(1024, 0, useNewLabel);
		Test4 gc(5, Xbyak::AutoGrow, !useNewLabel);
		gc.ready();
		fm.assign((const char*)fc.getCode(), fc.getSize());
		gm.assign((const char*)gc.getCode(), gc.getSize());
		CYBOZU_TEST_EQUAL(fm, gm);
	}
}

CYBOZU_TEST_AUTO(test5)
{
	struct Test5 : Xbyak::CodeGenerator {
		explicit Test5(int size, int count, void *mode)
			: CodeGenerator(size, mode, &myAlloc)
		{
			using namespace Xbyak;
			inLocalLabel();
			mov(ecx, count);
			xor(eax, eax);
		L(".lp");
			for (int i = 0; i < count; i++) {
				L(Label::toStr(i));
				add(eax, 1);
				int to = 0;
				if (i < count / 2) {
					to = count - 1 - i;
				} else {
					to = count  - i;
				}
				if (i == count / 2) {
					jmp(".exit", T_NEAR);
				} else {
					jmp(Label::toStr(to), T_NEAR);
				}
			}
		L(".exit");
			sub(ecx, 1);
			jnz(".lp", T_NEAR);
			ret();
			outLocalLabel();
		}
	};
	std::string fm, gm;
	const int count = 50;
	int ret;
	Test5 fc(1024 * 64, count, 0);
	ret = ((int (*)())fc.getCode())();
	CYBOZU_TEST_EQUAL(ret, count * count);
	fm.assign((const char*)fc.getCode(), fc.getSize());
	Test5 gc(10, count, Xbyak::AutoGrow);
	gc.ready();
	ret = ((int (*)())gc.getCode())();
	CYBOZU_TEST_EQUAL(ret, count * count);
	gm.assign((const char*)gc.getCode(), gc.getSize());
	CYBOZU_TEST_EQUAL(fm, gm);
}

size_t getValue(const uint8* p)
{
	size_t v = 0;
	for (size_t i = 0; i < sizeof(size_t); i++) {
		v |= p[i] << (i * 8);
	}
	return v;
}

void checkAddr(const uint8 *p, size_t offset, size_t expect)
{
	size_t v = getValue(p + offset);
	CYBOZU_TEST_EQUAL(v, size_t(p) + expect);
}

CYBOZU_TEST_AUTO(MovLabel)
{
	struct MovLabelCode : Xbyak::CodeGenerator {
		MovLabelCode(bool grow, bool useNewLabel)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
#ifdef XBYAK64
			const Reg64& a = rax;
#else
			const Reg32& a = eax;
#endif
			if (useNewLabel) {
				nop(); // 0x90
				Label lp1, lp2;
			L(lp1);
				nop();
				mov(a, lp1); // 0xb8 + <4byte> / 0x48bb + <8byte>
				nop();
				mov(a, lp2); // 0xb8
				// force realloc if AutoGrow
				putNop(this, 256);
				nop();
			L(lp2);
			} else {
				inLocalLabel();
				nop(); // 0x90
			L(".lp1");
				nop();
				mov(a, ".lp1"); // 0xb8 + <4byte> / 0x48bb + <8byte>
				nop();
				mov(a, ".lp2"); // 0xb8
				// force realloc if AutoGrow
				putNop(this, 256);
				nop();
			L(".lp2");
				outLocalLabel();
			}
		}
	};

	const struct {
		int pos;
		uint8 ok;
	} tbl[] = {
#ifdef XBYAK32
		{ 0x00, 0x90 },
		// lp1:0x001
		{ 0x001, 0x90 },
		{ 0x002, 0xb8 },
		// 0x003
		{ 0x007, 0x90 },
		{ 0x008, 0xb8 },
		// 0x009
		{ 0x10d, 0x90 },
		// lp2:0x10e
#else
		{ 0x000, 0x90 },
		// lp1:0x001
		{ 0x001, 0x90 },
		{ 0x002, 0x48 },
		{ 0x003, 0xb8 },
		// 0x004
		{ 0x00c, 0x90 },
		{ 0x00d, 0x48 },
		{ 0x00e, 0xb8 },
		// 0x00f
		{ 0x117, 0x90 },
		// lp2:0x118
#endif
	};
	for (int j = 0; j < 2; j++) {
		const bool grow = j == 0;
		for (int k = 0; k < 2; k++) {
			const bool useNewLabel = k == 0;
			MovLabelCode code(grow, useNewLabel);
			if (grow) code.ready();
			const uint8* const p = code.getCode();
			for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
				int pos = tbl[i].pos;
				uint8 x = p[pos];
				uint8 ok = tbl[i].ok;
				CYBOZU_TEST_EQUAL(x, ok);
			}
#ifdef XBYAK32
			checkAddr(p, 0x03, 0x001);
			checkAddr(p, 0x09, 0x10e);
#else
			checkAddr(p, 0x04, 0x001);
			checkAddr(p, 0x0f, 0x118);
#endif
		}
	}
}

CYBOZU_TEST_AUTO(testMovLabel2)
{
	struct MovLabel2Code : Xbyak::CodeGenerator {
		MovLabel2Code()
		{
#ifdef XBYAK64
			const Reg64& a = rax;
			const Reg64& c = rcx;
#else
			const Reg32& a = eax;
			const Reg32& c = ecx;
#endif
			xor_(a, a);
			xor_(c, c);
			jmp("in");
			ud2();
		L("@@"); // L1
			add(a, 2);
			mov(c, "@f");
			jmp(c); // goto L2
			ud2();
		L("in");
			mov(c, "@b");
			add(a, 1);
			jmp(c); // goto L1
			ud2();
		L("@@"); // L2
			add(a, 4);
			ret();
		}
	};
	MovLabel2Code code;
	int ret = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, 7);
}

CYBOZU_TEST_AUTO(testF_B)
{
	struct Code : Xbyak::CodeGenerator {
		Code(int type)
		{
			inLocalLabel();
			xor_(eax, eax);
			switch (type) {
			case 0:
			L("@@");
				inc(eax);
				cmp(eax, 1);
				je("@b");
				break;
			case 1:
				test(eax, eax);
				jz("@f");
				ud2();
			L("@@");
				break;
			case 2:
			L("@@");
				inc(eax);
				cmp(eax, 1); // 1, 2
				je("@b");
				cmp(eax, 2); // 2, 3
				je("@b");
				break;
			case 3:
			L("@@");
				inc(eax);
				cmp(eax, 1); // 1, 2
				je("@b");
				cmp(eax, 2); // 2, 3
				je("@b");
				jmp("@f");
				ud2();
			L("@@");
				break;
			case 4:
			L("@@");
				inc(eax);
				cmp(eax, 1); // 1, 2
				je("@b");
				cmp(eax, 2); // 2, 3
				je("@b");
				jmp("@f");
				ud2();
			L("@@");
				inc(eax); // 4, 5
				cmp(eax, 4);
				je("@b");
				break;
			case 5:
			L("@@");
			L("@@");
				inc(eax);
				cmp(eax, 1);
				je("@b");
				break;
			case 6:
			L("@@");
			L("@@");
			L("@@");
				inc(eax);
				cmp(eax, 1);
				je("@b");
				break;
			case 7:
				jmp("@f");
			L("@@");
				inc(eax); // 1, 2
				cmp(eax, 1);
				je("@b");
				cmp(eax, 2);
				jne("@f"); // not jmp
				inc(eax); // 3
			L("@@");
				inc(eax); // 4, 5, 6
				cmp(eax, 4);
				je("@b");
				cmp(eax, 5);
				je("@b");
				jmp("@f");
				jmp("@f");
				jmp("@b");
			L("@@");
				break;
			}
			ret();
			outLocalLabel();
		}
	};
	const int expectedTbl[] = {
		2, 0, 3, 3, 5, 2, 2, 6
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(expectedTbl); i++) {
		Code code((int)i);
		int ret = code.getCode<int (*)()>()();
		CYBOZU_TEST_EQUAL(ret, expectedTbl[i]);
	}
}

CYBOZU_TEST_AUTO(test6)
{
	struct TestLocal : public Xbyak::CodeGenerator {
		TestLocal(bool grow)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			xor_(eax, eax);
			inLocalLabel();
			jmp("start0", T_NEAR);
			L(".back");
			inc(eax); // 8
			jmp(".next", T_NEAR);
			L("start2");
			inc(eax); // 7
			jmp(".back", T_NEAR);
				inLocalLabel();
				L(".back");
				inc(eax); // 5
				putNop(this, 128);
				jmp(".next", T_NEAR);
				L("start1");
				inc(eax); // 4
				jmp(".back", T_NEAR);
					inLocalLabel();
					L(".back");
					inc(eax); // 2
					jmp(".next", T_NEAR);
					L("start0");
					inc(eax); // 1
					jmp(".back", T_NEAR);
					L(".next");
					inc(eax); // 3
					jmp("start1", T_NEAR);
					outLocalLabel();
				L(".next");
				inc(eax); // 6
				jmp("start2", T_NEAR);
				outLocalLabel();
			L(".next");
			inc(eax); // 9
			jmp("start3", T_NEAR);
				inLocalLabel();
				L(".back");
				inc(eax); // 14
				jmp("exit", T_NEAR);
			L("start4");
				inc(eax); // 13
				jmp(".back", T_NEAR);
				outLocalLabel();
			L("start3");
				inc(eax); // 10
				inLocalLabel();
				jmp(".next", T_NEAR);
				L(".back");
				inc(eax); // 12
				jmp("start4", T_NEAR);
				L(".next");
				inc(eax); // 11
				jmp(".back", T_NEAR);
				outLocalLabel();
			outLocalLabel();
			L("exit");
			inc(eax); // 15
			ret();
		}
	};

	for (int i = 0; i < 2; i++) {
		const bool grow = i == 1;
		printf("test6 grow=%d\n", i);
		TestLocal code(grow);
		if (grow) code.ready();
		int (*f)() = code.getCode<int (*)()>();
		int a = f();
		CYBOZU_TEST_EQUAL(a, 15);
	}
}

CYBOZU_TEST_AUTO(test_jcc)
{
	struct A : Xbyak::CodeGenerator {
		A()
		{
			add(eax, 5);
			ret();
		}
	};
	struct B : Xbyak::CodeGenerator {
		B(bool grow, const void *p) : Xbyak::CodeGenerator(grow ? 0 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			mov(eax, 1);
			add(eax, 2);
			jnz(p);
		}
	};
	A a;
	const void *p = a.getCode<const void*>();
	for (int i = 0; i < 2; i++) {
		bool grow = i == 1;
		B b(grow, p);
		if (grow) {
			b.ready();
		}
		int (*f)() = b.getCode<int (*)()>();
		CYBOZU_TEST_EQUAL(f(), 8);
	}
}

CYBOZU_TEST_AUTO(testNewLabel)
{
	struct Code : Xbyak::CodeGenerator {
		Code(bool grow)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			xor_(eax, eax);
			{
				Label label1;
				Label label2;
				Label label3;
				Label label4;
				Label exit;
				jmp(label1, T_NEAR);
			L(label2);
				inc(eax); // 2
				jmp(label3, T_NEAR);
			L(label4);
				inc(eax); // 4
				jmp(exit, T_NEAR);
				putNop(this, 128);
			L(label3);
				inc(eax); // 3
				jmp(label4, T_NEAR);
			L(label1);
				inc(eax); // 1
				jmp(label2, T_NEAR);
			L(exit);
			}
			{
				Label label1;
				Label label2;
				Label label3;
				Label label4;
				Label exit;
				jmp(label1);
			L(label2);
				inc(eax); // 6
				jmp(label3);
			L(label4);
				inc(eax); // 8
				jmp(exit);
			L(label3);
				inc(eax); // 7
				jmp(label4);
			L(label1);
				inc(eax); // 5
				jmp(label2);
			L(exit);
			}
			Label callLabel;
			{	// eax == 8
				Label label1;
				Label label2;
			L(label1);
				inc(eax); // 9, 10, 11, 13
				cmp(eax, 9);
				je(label1);
				// 10, 11, 13
				inc(eax); // 11, 12, 13
				cmp(eax, 11);
				je(label1);
				// 12, 13
				cmp(eax, 12);
				je(label2);
				inc(eax); // 14
				cmp(eax, 14);
				je(label2);
				ud2();
			L(label2); // 14
				inc(eax); // 13, 15
				cmp(eax, 13);
				je(label1);
			}
			call(callLabel);
			ret();
		L(callLabel);
			inc(eax); // 16
			ret();
		}
	};
	for (int i = 0; i < 2; i++) {
		const bool grow = i == 1;
		printf("testNewLabel grow=%d\n", grow);
		Code code(grow);
		if (grow) code.ready();
		int (*f)() = code.getCode<int (*)()>();
		int r = f();
		CYBOZU_TEST_EQUAL(r, 16);
	}
}

CYBOZU_TEST_AUTO(testAssign)
{
	struct Code : Xbyak::CodeGenerator {
		Code(bool grow)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			xor_(eax, eax);
			Label dst, src;
		L(src);
			inc(eax);
			cmp(eax, 1);
			je(dst);
			inc(eax); // 2, 3, 5
			cmp(eax, 5);
			putNop(this, 128);
			jne(dst, T_NEAR);
			ret();
		assignL(dst, src);
			// test of copy  label
			{
				Label sss(dst);
				{
					Label ttt;
					ttt = src;
				}
			}
		}
	};
	for (int i = 0; i < 2; i++) {
		const bool grow = i == 0;
		printf("testAssign grow=%d\n", grow);
		Code code(grow);
		if (grow) code.ready();
		int (*f)() = code.getCode<int (*)()>();
		int ret = f();
		CYBOZU_TEST_EQUAL(ret, 5);
    }
}

CYBOZU_TEST_AUTO(doubleDefine)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			{
				Label label;
			L(label);
				// forbitten double L()
				CYBOZU_TEST_EXCEPTION(L(label), Xbyak::Error);
			}
			{
				Label label;
				jmp(label);
				CYBOZU_TEST_ASSERT(hasUndefinedLabel());
			}
			{
				Label label1, label2;
			L(label1);
				jmp(label2);
				assignL(label2, label1);
				// forbitten double assignL
				CYBOZU_TEST_EXCEPTION(assignL(label2, label1), Xbyak::Error);
			}
			{
				Label label1, label2;
			L(label1);
				jmp(label2);
				// forbitten assignment to label1 set by L()
				CYBOZU_TEST_EXCEPTION(assignL(label1, label2), Xbyak::Error);
			}
		}
	} code;
}

#ifdef XBYAK64
CYBOZU_TEST_AUTO(rip)
{
	int a[] = { 1, 10 };
	int b[] = { 100, 1000 };
	struct Code : Xbyak::CodeGenerator {
		Code(const int *a, const int *b)
		{
			Label label1, label2;
			jmp("@f");
		L(label1);
			db(a[0], 4);
			db(a[1], 4);
		L("@@");
			mov(eax, ptr [rip + label1]);       // a[0]
			mov(ecx, ptr [rip + label1+4]);     // a[1]
			mov(edx, ptr [rip + label2-8+2+6]); // b[0]
			add(ecx, ptr [rip + 16+label2-12]); // b[1]
			add(eax, ecx);
			add(eax, edx);
			ret();
		L(label2);
			db(b[0], 4);
			db(b[1], 4);

			// error
			CYBOZU_TEST_EXCEPTION(rip + label1 + label2, Xbyak::Error);
		}
	} code(a, b);
	int ret = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, a[0] + a[1] + b[0] + b[1]);
}

int ret1234()
{
	return 1234;
}

int ret9999()
{
	return 9999;
}

CYBOZU_TEST_AUTO(rip_jmp)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			Label label;
			xor_(eax, eax);
			call(ptr [rip + label]);
			mov(ecx, eax);
			call(ptr [rip + label + 8]);
			add(eax, ecx);
			ret();
		L(label);
			db((size_t)ret1234, 8);
			db((size_t)ret9999, 8);
		}
	} code;
	int ret = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, ret1234() + ret9999());
}
#endif
