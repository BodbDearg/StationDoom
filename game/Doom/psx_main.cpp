#include "psx_main.h"

#include "Base/i_main.h"
#include "PsxVm/PsxVm.h"
#include "PsyQ/LIBAPI.h"

// A global that is used to save the return address for the entire .EXE
VmPtr<uint32_t> gProgramReturnAddr(0x80077E58);

//------------------------------------------------------------------------------------------------------------------------------------------
// This is the reverse engineered entrypoint for PSXDOOM.EXE.
//
// This code is generated by the compiler and/or toolchain and does stuff which the user application cannot do such as deciding on the
// value of the 'gp' register, zero initializing the BSS section and so on.
//------------------------------------------------------------------------------------------------------------------------------------------
void psx_main() noexcept {
    // Zero initialize the BSS section globals.
    // These are the globals that aren't explicitly defined with a value in the .EXE image.
    {
        // Oddness: I don't know why but for some reason PSX DOOM starts zero initializing the globals at address '80077E30'.
        // The BSS section starts at '80078000', so this does not really make sense?
        // This means we are zero initializing 464 bytes of globals that are already defined in the .EXE.
        // Those globals have a defined value of '0' anyway so it makes no difference in practice but I'm curious as to
        // why this particular code was generated this way...
        //
        VmPtr<uint32_t>         pCurWord(0x80077E30);
        const VmPtr<uint32_t>   pEndWord(HeapStartAddr);

        do {
            *pCurWord = 0;
            ++pCurWord;
        } while (pCurWord < pEndWord);
    }

    // Setup the stack pointer.
    // This reserves room for a 64-bit value and bring into the memory segment normally used by the .EXE:
    constexpr uint32_t InitialStackPtrAddr = StackEndAddr - sizeof(uint64_t);
    sp = InitialStackPtrAddr | 0x80000000;

    // Save the return address once the .EXE is done
    *gProgramReturnAddr = ra;
    
    // Figure out the size and start address to use when initializing the heap
    constexpr uint32_t WrappedHeapStartAddr = ((HeapStartAddr << 3) >> 3);
    constexpr uint32_t StackStartAddr = InitialStackPtrAddr - StackSize;
    constexpr uint32_t InitHeapSize = StackStartAddr - WrappedHeapStartAddr;
    constexpr uint32_t InitHeapBaseAddr = WrappedHeapStartAddr | 0x80000000;
    
    // Setup the value that the 'gp' register will have for the entire program.
    // Also setup the initial value of the 'fp' register.
    gp = GpRegisterValue;
    fp = sp;

    // Initialize the heap for the PsyQ SDK.
    // Note that DOOM uses it's own memory management system, so this call is effectively useless.
    {
        // Note: I don't know why '4' was added here, was that incorrect? Would only be a problem anyway if we reached the end of the stack.
        a0 = InitHeapBaseAddr + 4;
        a1 = InitHeapSize;
        LIBAPI_InitHeap();
    }

    // Restore the return address to outside the .EXE
    ra = *gProgramReturnAddr;

    // Call the real 'main()' function of DOOM, as it was on the PSX.
    //
    // Note: in the original PSX machine code following this call there was also an illegal 'break' instruction.
    // The break instruction is not supported by the MIPS I R3000A and thus would generate some sort of exception.
    // This error was probably deliberate to let the programmer know that a PSX program should never exit.
    //
    // I'm removing this instruction for the purposes of this conversion however because a PC program obviously
    // can (and should!) exit properly once it is done:
    //
    I_Main();
}
