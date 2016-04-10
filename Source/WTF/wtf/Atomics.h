/*
 * Copyright (C) 2007, 2008, 2010, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Note: The implementations of InterlockedIncrement and InterlockedDecrement are based
 * on atomic_increment and atomic_exchange_and_add from the Boost C++ Library. The license
 * is virtually identical to the Apple license above but is included here for completeness.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 * 
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 * 
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef Atomics_h
#define Atomics_h

#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

#if OS(WINDOWS)
#if !COMPILER(GCC)
extern "C" void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#endif
#include <windows.h>
#endif

namespace WTF {

#if OS(WINDOWS) && !COMPILER(GCC)
inline bool weakCompareAndSwap(volatile unsigned* location, unsigned expected, unsigned newValue)
{
#if OS(WINCE)
    return InterlockedCompareExchange(reinterpret_cast<LONG*>(const_cast<unsigned*>(location)), static_cast<LONG>(newValue), static_cast<LONG>(expected)) == static_cast<LONG>(expected);
#else
    return InterlockedCompareExchange(reinterpret_cast<LONG volatile*>(location), static_cast<LONG>(newValue), static_cast<LONG>(expected)) == static_cast<LONG>(expected);
#endif
}

inline bool weakCompareAndSwap(void*volatile* location, void* expected, void* newValue)
{
    return InterlockedCompareExchangePointer(location, newValue, expected) == expected;
}
#else // OS(WINDOWS) && !COMPILER(GCC) --> not windows, but maybe mingw
inline bool weakCompareAndSwap(volatile unsigned* location, unsigned expected, unsigned newValue) 
{
#if ENABLE(COMPARE_AND_SWAP)
#if CPU(X86) || CPU(X86_64)
    unsigned char result;
    asm volatile(
        "lock; cmpxchgl %3, %2\n\t"
        "sete %1"
        : "+a"(expected), "=q"(result), "+m"(*location)
        : "r"(newValue)
        : "memory"
        );
#elif CPU(ARM_THUMB2)
    unsigned tmp;
    unsigned result;
    asm volatile(
        "movw %1, #1\n\t"
        "ldrex %2, %0\n\t"
        "cmp %3, %2\n\t"
        "bne.n 0f\n\t"
        "strex %1, %4, %0\n\t"
        "0:"
        : "+Q"(*location), "=&r"(result), "=&r"(tmp)
        : "r"(expected), "r"(newValue)
        : "memory");
    result = !result;
#elif CPU(ARM64) && COMPILER(GCC)
    unsigned tmp;
    unsigned result;
    asm volatile(
        "mov %w1, #1\n\t"
        "ldxr %w2, [%0]\n\t"
        "cmp %w3, %w2\n\t"
        "b.ne 0f\n\t"
        "stxr %w1, %w4, [%0]\n\t"
        "0:"
        : "+r"(location), "=&r"(result), "=&r"(tmp)
        : "r"(expected), "r"(newValue)
        : "memory");
    result = !result;
#elif CPU(ARM64)
    unsigned tmp;
    unsigned result;
    asm volatile(
        "mov %w1, #1\n\t"
        "ldxr %w2, %0\n\t"
        "cmp %w3, %w2\n\t"
        "b.ne 0f\n\t"
        "stxr %w1, %w4, %0\n\t"
        "0:"
        : "+m"(*location), "=&r"(result), "=&r"(tmp)
        : "r"(expected), "r"(newValue)
        : "memory");
    result = !result;
#else
#error "Bad architecture for compare and swap."
#endif
    return result;
#else
    UNUSED_PARAM(location);
    UNUSED_PARAM(expected);
    UNUSED_PARAM(newValue);
    CRASH();
    return false;
#endif
}

inline bool weakCompareAndSwap(void*volatile* location, void* expected, void* newValue)
{
#if ENABLE(COMPARE_AND_SWAP)
#if CPU(X86_64)
    bool result;
    asm volatile(
        "lock; cmpxchgq %3, %2\n\t"
        "sete %1"
        : "+a"(expected), "=q"(result), "+m"(*location)
        : "r"(newValue)
        : "memory"
        );
    return result;
#elif CPU(ARM64) && COMPILER(GCC)
    bool result;
    void* tmp;
    asm volatile(
        "mov %w1, #1\n\t"
        "ldxr %x2, [%0]\n\t"
        "cmp %x3, %x2\n\t"
        "b.ne 0f\n\t"
        "stxr %w1, %x4, [%0]\n\t"
        "0:"
        : "+r"(location), "=&r"(result), "=&r"(tmp)
        : "r"(expected), "r"(newValue)
        : "memory");
    return !result;
#elif CPU(ARM64)
    bool result;
    void* tmp;
    asm volatile(
        "mov %w1, #1\n\t"
        "ldxr %x2, %0\n\t"
        "cmp %x3, %x2\n\t"
        "b.ne 0f\n\t"
        "stxr %w1, %x4, %0\n\t"
        "0:"
        : "+m"(*location), "=&r"(result), "=&r"(tmp)
        : "r"(expected), "r"(newValue)
        : "memory");
    return !result;
#else
    return weakCompareAndSwap(bitwise_cast<unsigned*>(location), bitwise_cast<unsigned>(expected), bitwise_cast<unsigned>(newValue));
#endif
#else // ENABLE(COMPARE_AND_SWAP)
    UNUSED_PARAM(location);
    UNUSED_PARAM(expected);
    UNUSED_PARAM(newValue);
    CRASH();
    return 0;
#endif // ENABLE(COMPARE_AND_SWAP)
}
#endif // OS(WINDOWS) && !COMPILER(GCC) (end of the not-windows (but maybe mingw) case)

inline bool weakCompareAndSwapUIntPtr(volatile uintptr_t* location, uintptr_t expected, uintptr_t newValue)
{
    return weakCompareAndSwap(reinterpret_cast<void*volatile*>(location), reinterpret_cast<void*>(expected), reinterpret_cast<void*>(newValue));
}

inline bool weakCompareAndSwapSize(volatile size_t* location, size_t expected, size_t newValue)
{
    return weakCompareAndSwap(reinterpret_cast<void*volatile*>(location), reinterpret_cast<void*>(expected), reinterpret_cast<void*>(newValue));
}

// Just a compiler fence. Has no effect on the hardware, but tells the compiler
// not to move things around this call. Should not affect the compiler's ability
// to do things like register allocation and code motion over pure operations.
inline void compilerFence()
{
#if OS(WINDOWS) && !COMPILER(GCC)
    _ReadWriteBarrier();
#else
    asm volatile("" ::: "memory");
#endif
}

#if CPU(ARM_THUMB2) || CPU(ARM64)

// Full memory fence. No accesses will float above this, and no accesses will sink
// below it.
inline void armV7_dmb()
{
    asm volatile("dmb sy" ::: "memory");
}

// Like the above, but only affects stores.
inline void armV7_dmb_st()
{
    asm volatile("dmb st" ::: "memory");
}

inline void loadLoadFence() { armV7_dmb(); }
inline void loadStoreFence() { armV7_dmb(); }
inline void storeLoadFence() { armV7_dmb(); }
inline void storeStoreFence() { armV7_dmb_st(); }
inline void memoryBarrierAfterLock() { armV7_dmb(); }
inline void memoryBarrierBeforeUnlock() { armV7_dmb(); }

#elif CPU(X86) || CPU(X86_64)

inline void x86_mfence()
{
#if OS(WINDOWS) && !COMPILER(GCC)
    // I think that this does the equivalent of a dummy interlocked instruction,
    // instead of using the 'mfence' instruction, at least according to MSDN. I
    // know that it is equivalent for our purposes, but it would be good to
    // investigate if that is actually better.
    MemoryBarrier();
#else
    asm volatile("mfence" ::: "memory");
#endif
}

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { x86_mfence(); }
inline void storeStoreFence() { compilerFence(); }
inline void memoryBarrierAfterLock() { compilerFence(); }
inline void memoryBarrierBeforeUnlock() { compilerFence(); }

#else

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { compilerFence(); }
inline void storeStoreFence() { compilerFence(); }
inline void memoryBarrierAfterLock() { compilerFence(); }
inline void memoryBarrierBeforeUnlock() { compilerFence(); }

#endif

inline bool weakCompareAndSwap(uint8_t* location, uint8_t expected, uint8_t newValue)
{
#if ENABLE(COMPARE_AND_SWAP)
#if !COMPILER(MSVC) && (CPU(X86) || CPU(X86_64))
    // !COMPILER(MSVC) here means "ASM_SYNTAX(AT_AND_T)"
    unsigned char result;
    asm volatile(
        "lock; cmpxchgb %3, %2\n\t"
        "sete %1"
        : "+a"(expected), "=q"(result), "+m"(*location)
        : "q"(newValue)
        : "memory"
        );
    return result;
#elif COMPILER(MSVC) && CPU(X86)
    // COMPILER(MSVC) here means "ASM_SYNTAX(INTEL)"
    // FIXME: We need a 64-bit ASM implementation, but this cannot be inline due to
    // Microsoft's decision to exclude it from the compiler.
    bool result = false;

    __asm {
        mov al, expected
        mov edx, location
        mov cl, newValue
        lock cmpxchg byte ptr[edx], cl
        setz result
    }

    return result;
#else
    uintptr_t locationValue = bitwise_cast<uintptr_t>(location);
    uintptr_t alignedLocationValue = locationValue & ~(sizeof(unsigned) - 1);
    uintptr_t locationOffset = locationValue - alignedLocationValue;
    ASSERT(locationOffset < sizeof(unsigned));
    unsigned* alignedLocation = bitwise_cast<unsigned*>(alignedLocationValue);
    // Make sure that this load is always issued and never optimized away.
    unsigned oldAlignedValue = *const_cast<volatile unsigned*>(alignedLocation);

    struct Splicer {
        static unsigned splice(unsigned value, uint8_t byte, uintptr_t byteIndex)
        {
            union {
                unsigned word;
                uint8_t bytes[sizeof(unsigned)];
            } u;
            u.word = value;
            u.bytes[byteIndex] = byte;
            return u.word;
        }
    };
    
    unsigned expectedAlignedValue = Splicer::splice(oldAlignedValue, expected, locationOffset);
    unsigned newAlignedValue = Splicer::splice(oldAlignedValue, newValue, locationOffset);

    return weakCompareAndSwap(alignedLocation, expectedAlignedValue, newAlignedValue);
#endif
#else
    UNUSED_PARAM(location);
    UNUSED_PARAM(expected);
    UNUSED_PARAM(newValue);
    CRASH();
    return false;
#endif
}

} // namespace WTF

#endif // Atomics_h
