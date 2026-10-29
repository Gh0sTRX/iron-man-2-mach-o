# Contiguous image mapping prototype — 2026-10-29

A host-side source transformation was prepared and tested (2/2 tests). It replaces two independent VM allocations with one allocation spanning __TEXT through __DATA, computes the runtime slide from the actual returned base, and applies this same slide to pointers, symbols and constructors. It also avoids relocating local indirect symbol slots twice (once in import binding and again in RBS1).

The prototype is distributed as a ZIP in the conversation. It has **not** been compiled or run on PS Vita. Crucially, sceKernelAllocMemBlockForVM allocation alone does not prove the block is executable. RX permission and guest ABI are outstanding blockers. Existing classic Vita port was not changed.

Next: test executable memory permissions on Vita, then validate RBS1 against exact upstream output and implement import shims.
