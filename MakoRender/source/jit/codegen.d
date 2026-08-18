module jit.codegen;

import jit.buffer;
import jit.memory;

alias Mat4MulFn = extern(C) void function(float*, const(float)*, const(float)*) nothrow @nogc;
alias RotationFn = extern(C) void function(float*, float, float, float, float) nothrow @nogc;
alias FaceColorsFn = extern(C) void function(float*, float, float, float) nothrow @nogc;
alias Rotation2DFn = extern(C) void function(float*, float, float) nothrow @nogc;

private:
    enum { RAX=0, RCX=1, RDX=2, RBX=3, RSP=4, RBP=5, RSI=6, RDI=7 }

    void emitRex(ref CodeBuffer buf, bool needR, bool needB)
    {
        if (needR || needB)
        {
            ubyte rex = 0x40;
            if (needR) rex |= 0x04;
            if (needB) rex |= 0x01;
            buf.emit(rex);
        }
    }

    ubyte modrm(int mod, int reg, int rm)
    {
        return cast(ubyte)((mod << 6) | ((reg & 7) << 3) | (rm & 7));
    }

    void emitModrmSib(ref CodeBuffer buf, int mod, int reg, int base, int disp)
    {
        bool needSib = (base & 7) == RSP;
        buf.emit(modrm(mod, reg, base));
        if (needSib)
        {
            buf.emit(0x24);
        }
    }

    void emitMovssLoad(ref CodeBuffer buf, int xmm, int base, int disp)
    {
        emitRex(buf, xmm >= 8, base >= 8);
        buf.emit(0xF3);
        buf.emit(0x0F);
        buf.emit(0x10);
        emitModrmSib(buf, 1, xmm, base, disp);
        buf.emit(cast(ubyte)(disp & 0xFF));
    }

    void emitMovssStore(ref CodeBuffer buf, int base, int disp, int xmm)
    {
        emitRex(buf, xmm >= 8, base >= 8);
        buf.emit(0xF3);
        buf.emit(0x0F);
        buf.emit(0x11);
        emitModrmSib(buf, 1, xmm, base, disp);
        buf.emit(cast(ubyte)(disp & 0xFF));
    }

    void emitMovssReg(ref CodeBuffer buf, int dst, int src)
    {
        emitRex(buf, dst >= 8, src >= 8);
        buf.emit(0xF3);
        buf.emit(0x0F);
        buf.emit(0x10);
        buf.emit(modrm(3, dst, src));
    }

    void emitBinopXmm(ref CodeBuffer buf, ubyte opcode, int dst, int src)
    {
        emitRex(buf, dst >= 8, src >= 8);
        buf.emit(0xF3);
        buf.emit(0x0F);
        buf.emit(opcode);
        buf.emit(modrm(3, dst, src));
    }

    void emitXorps(ref CodeBuffer buf, int dst, int src)
    {
        emitRex(buf, dst >= 8, src >= 8);
        buf.emit(0x0F);
        buf.emit(0x57);
        buf.emit(modrm(3, dst, src));
    }

    void emitMulss(ref CodeBuffer buf, int dst, int src) { emitBinopXmm(buf, 0x59, dst, src); }
    void emitAddss(ref CodeBuffer buf, int dst, int src) { emitBinopXmm(buf, 0x58, dst, src); }
    void emitSubss(ref CodeBuffer buf, int dst, int src) { emitBinopXmm(buf, 0x5C, dst, src); }
    void emitMaxss(ref CodeBuffer buf, int dst, int src) { emitBinopXmm(buf, 0x5F, dst, src); }

    void emitMovImm32Mem(ref CodeBuffer buf, int base, int disp, uint val)
    {
        emitRex(buf, false, base >= 8);
        buf.emit(0xC7);
        emitModrmSib(buf, 1, 0, base, disp);
        buf.emit(cast(ubyte)(disp & 0xFF));
        buf.emit(cast(ubyte)(val));
        buf.emit(cast(ubyte)(val >> 8));
        buf.emit(cast(ubyte)(val >> 16));
        buf.emit(cast(ubyte)(val >> 24));
    }

    void emitSubRsp(ref CodeBuffer buf, int imm8)
    {
        buf.emit(0x48);
        buf.emit(0x83);
        buf.emit(0xEC);
        buf.emit(cast(ubyte)(imm8));
    }

    void emitAddRsp(ref CodeBuffer buf, int imm8)
    {
        buf.emit(0x48);
        buf.emit(0x83);
        buf.emit(0xC4);
        buf.emit(cast(ubyte)(imm8));
    }

    void emitRet(ref CodeBuffer buf) { buf.emit(0xC3); }

    ExecutableRegion finalize(ref CodeBuffer buf) @system
    {
        auto region = allocExecutable(buf.length);
        if (!region.isValid())
            return region;
        region.writeCode(buf.code);
        region.makeExecutable();
        return region;
    }

public:

ExecutableRegion genComputeRotation()
{
    auto buf = CodeBuffer(1024);

    enum srx = 0, crx = 1, sry = 2, cry = 3;
    enum t0 = 4, t1 = 5, zero = 6;
    enum o = RDI;

    emitXorps(buf, zero, zero);

    emitMovssStore(buf, o, 0, cry);
    emitMovssReg(buf, t0, srx);
    emitMulss(buf, t0, sry);
    emitMovssStore(buf, o, 4, t0);
    emitMovssReg(buf, t0, crx);
    emitMulss(buf, t0, sry);
    emitMovssStore(buf, o, 8, t0);
    emitMovssStore(buf, o, 12, zero);

    emitMovssStore(buf, o, 16, zero);
    emitMovssStore(buf, o, 20, crx);
    emitXorps(buf, t1, t1);
    emitSubss(buf, t1, srx);
    emitMovssStore(buf, o, 24, t1);
    emitMovssStore(buf, o, 28, zero);

    emitXorps(buf, t1, t1);
    emitSubss(buf, t1, sry);
    emitMovssStore(buf, o, 32, t1);
    emitMovssReg(buf, t0, srx);
    emitMulss(buf, t0, cry);
    emitMovssStore(buf, o, 36, t0);
    emitMovssReg(buf, t0, crx);
    emitMulss(buf, t0, cry);
    emitMovssStore(buf, o, 40, t0);
    emitMovssStore(buf, o, 44, zero);

    emitMovssStore(buf, o, 48, zero);
    emitMovssStore(buf, o, 52, zero);
    emitMovssStore(buf, o, 56, zero);
    emitMovImm32Mem(buf, o, 60, 0x3F800000);

    emitRet(buf);
    return finalize(buf);
}

ExecutableRegion genComputeRotation2D()
{
    auto buf = CodeBuffer(1024);

    enum sinA = 0, cosA = 1;
    enum t0 = 2, zero = 3;
    enum o = RDI;

    emitXorps(buf, zero, zero);

    emitMovssStore(buf, o, 0, cosA);
    emitXorps(buf, t0, t0);
    emitSubss(buf, t0, sinA);
    emitMovssStore(buf, o, 4, t0);
    emitMovssStore(buf, o, 8, zero);
    emitMovssStore(buf, o, 12, zero);

    emitMovssStore(buf, o, 16, sinA);
    emitMovssStore(buf, o, 20, cosA);
    emitMovssStore(buf, o, 24, zero);
    emitMovssStore(buf, o, 28, zero);

    emitMovssStore(buf, o, 32, zero);
    emitMovssStore(buf, o, 36, zero);
    emitMovImm32Mem(buf, o, 40, 0x3F800000);
    emitMovssStore(buf, o, 44, zero);

    emitMovssStore(buf, o, 48, zero);
    emitMovssStore(buf, o, 52, zero);
    emitMovssStore(buf, o, 56, zero);
    emitMovImm32Mem(buf, o, 60, 0x3F800000);

    emitRet(buf);
    return finalize(buf);
}

ExecutableRegion genMat4Multiply()
{
    auto buf = CodeBuffer(4096);

    foreach (j; 0 .. 4)
    {
        foreach (k; 0 .. 4)
            emitMovssLoad(buf, k, RDX, j * 16 + k * 4);

        foreach (i; 0 .. 4)
        {
            emitMovssLoad(buf, 4, RSI, i * 4);
            emitMulss(buf, 4, 0);

            emitMovssLoad(buf, 5, RSI, 16 + i * 4);
            emitMulss(buf, 5, 1);
            emitAddss(buf, 4, 5);

            emitMovssLoad(buf, 5, RSI, 32 + i * 4);
            emitMulss(buf, 5, 2);
            emitAddss(buf, 4, 5);

            emitMovssLoad(buf, 5, RSI, 48 + i * 4);
            emitMulss(buf, 5, 3);
            emitAddss(buf, 4, 5);

            emitMovssStore(buf, RDI, j * 16 + i * 4, 4);
        }
    }

    emitRet(buf);
    return finalize(buf);
}

ExecutableRegion genComputeFaceColors()
{
    auto buf = CodeBuffer(2048);

    enum lx = 0, ly = 1, lz = 2, amb = 3;
    enum tmp = 4, zero = 6, one = 7;
    enum o = RDI;

    emitSubRsp(buf, 16);
    emitMovImm32Mem(buf, RSP, 0, 0x3F800000);
    emitMovImm32Mem(buf, RSP, 4, 0x3E19999A);
    emitXorps(buf, zero, zero);
    emitMovssLoad(buf, one, RSP, 0);
    emitMovssLoad(buf, amb, RSP, 4);

    immutable int[6] faceLightXmm = [lz, lz, lx, lx, ly, ly];
    immutable bool[6] faceNegate  = [true, false, true, false, true, false];

    foreach (f; 0 .. 6)
    {
        int storeOff = f * 16;
        int lightReg = faceLightXmm[f];

        if (faceNegate[f])
        {
            emitXorps(buf, tmp, tmp);
            emitSubss(buf, tmp, lightReg);
        }
        else
        {
            emitMovssReg(buf, tmp, lightReg);
        }
        emitMaxss(buf, tmp, amb);
        emitMovssStore(buf, o, storeOff, tmp);
        emitMovssStore(buf, o, storeOff + 4, tmp);
        emitMovssStore(buf, o, storeOff + 8, tmp);
        emitMovssStore(buf, o, storeOff + 12, one);
    }

    emitAddRsp(buf, 16);
    emitRet(buf);
    return finalize(buf);
}
