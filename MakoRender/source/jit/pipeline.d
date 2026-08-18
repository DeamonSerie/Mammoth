module jit.pipeline;

import jit.codegen;
import jit.memory;
import std.stdio : writefln;

struct JitFunction
{
    string name;
    ExecutableRegion region;
    uint generation;
}

struct JitPipeline
{
    JitFunction[4] functions;
    uint swapCount;

    void initialize()
    {
        writefln("[JIT] Initializing pipeline...");
        regenAll();
    }

    void regenAll()
    {
        auto rot = genComputeRotation();
        auto rot2d = genComputeRotation2D();
        auto mul = genMat4Multiply();
        auto colors = genComputeFaceColors();

        swapOrReplace("computeRotation", rot);
        swapOrReplace("computeRotation2D", rot2d);
        swapOrReplace("mat4Multiply", mul);
        swapOrReplace("computeFaceColors", colors);

        writefln("[JIT] Generated %d functions (total swaps: %d)",
            functions.length, swapCount);
    }

    void regenColors()
    {
        writefln("[JIT] Hot-swapping computeFaceColors...");
        auto colors = genComputeFaceColors();
        swapOrReplace("computeFaceColors", colors);
        writefln("[JIT] computeFaceColors swapped (gen=%d)", getGeneration("computeFaceColors"));
    }

    T getFunction(T)(string name)
    {
        foreach (ref f; functions)
        {
            if (f.name == name && f.region.isValid())
                return f.region.getFunction!T();
        }
        assert(false, "JIT function not found: " ~ name);
    }

private:
    void swapOrReplace(string name, ExecutableRegion newRegion)
    {
        foreach (ref f; functions)
        {
            if (f.name == name)
            {
                if (f.region.isValid())
                {
                    freeExecutable(f.region);
                    writefln("[JIT] Freed old '%s' (gen=%d)", name, f.generation);
                }
                f.region = newRegion;
                f.generation++;
                swapCount++;
                return;
            }
        }

        foreach (ref f; functions)
        {
            if (f.name is null)
            {
                f.name = name;
                f.region = newRegion;
                f.generation = 1;
                return;
            }
        }
        assert(false, "JitPipeline function slots full");
    }

    uint getGeneration(string name)
    {
        foreach (ref f; functions)
            if (f.name == name)
                return f.generation;
        return 0;
    }
}
