module jit.memory;

import core.stdc.string : memcpy;

version (linux)
{
    import core.sys.posix.sys.mman : mmap, munmap, mprotect,
           PROT_READ, PROT_WRITE, PROT_EXEC,
           MAP_PRIVATE, MAP_ANON;
    enum MAP_FAILED = cast(void*) -1;
}
else
{
    static assert(false, "JIT memory not implemented for this platform");
}

struct ExecutableRegion
{
    void* ptr;
    size_t size;

    T getFunction(T)() const
    {
        return cast(T) ptr;
    }

    bool isValid() const { return ptr !is null && ptr != MAP_FAILED; }
}

package enum PAGE_SIZE = 4096;

size_t alignUp(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

ExecutableRegion allocExecutable(size_t codeSize)
{
    size_t allocSize = alignUp(codeSize, PAGE_SIZE);

    void* mem = mmap(
        null,
        allocSize,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON,
        -1,
        0
    );

    if (mem is null || mem == MAP_FAILED)
        return ExecutableRegion(null, 0);

    return ExecutableRegion(mem, allocSize);
}

bool makeExecutable(ref ExecutableRegion region)
{
    if (!region.isValid())
        return false;

    if (mprotect(region.ptr, region.size, PROT_READ | PROT_EXEC) != 0)
        return false;

    return true;
}

void freeExecutable(ref ExecutableRegion region)
{
    if (region.isValid())
    {
        munmap(region.ptr, region.size);
        region.ptr = null;
        region.size = 0;
    }
}

bool writeCode(ref ExecutableRegion region, const(ubyte)[] code) @system
{
    if (!region.isValid())
        return false;
    if (code.length > region.size)
        return false;

    memcpy(region.ptr, cast(const(void)*) code.ptr, code.length);
    return true;
}
