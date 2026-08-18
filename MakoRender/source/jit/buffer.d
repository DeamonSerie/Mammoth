module jit.buffer;

struct CodeBuffer
{
    ubyte[] buffer;
    size_t pos;

    this(size_t capacity)
    {
        buffer = new ubyte[capacity];
        pos = 0;
    }

    void emit(ubyte b)
    {
        buffer[pos++] = b;
    }

    void emit32(uint val)
    {
        emit(cast(ubyte)(val));
        emit(cast(ubyte)(val >> 8));
        emit(cast(ubyte)(val >> 16));
        emit(cast(ubyte)(val >> 24));
    }

    void emit64(ulong val)
    {
        for (int i = 0; i < 8; i++)
            emit(cast(ubyte)(val >> (i * 8)));
    }

    size_t length() const return { return pos; }

    ubyte[] code() return { return buffer[0 .. pos]; }

    void reset() { pos = 0; }

    void skip(size_t n) { pos += n; }

    size_t patchPoint() const { return pos; }

    void patch32(size_t offset, uint val)
    {
        buffer[offset] = cast(ubyte)(val);
        buffer[offset + 1] = cast(ubyte)(val >> 8);
        buffer[offset + 2] = cast(ubyte)(val >> 16);
        buffer[offset + 3] = cast(ubyte)(val >> 24);
    }
}
