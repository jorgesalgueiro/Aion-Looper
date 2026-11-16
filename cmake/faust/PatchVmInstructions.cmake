# LGPL upstream adaptation, applied only to the verified private Faust cache.
# Single-precision bitcasts must copy representation without pointer punning.
# The VM has a 32-bit int stack; reject unsupported double-width bitcasts instead
# of reading beyond a 32-bit object. The public Aion wrapper always uses -single.
set(vm "${AION_FAUST_SOURCE_DIR}/compiler/generator/interpreter/fbc_interpreter.hh")
file(READ "${vm}" code)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "AION_FAUST_DEFINED_BITCAST" patched)
if(patched EQUAL -1)
    string(SHA256 hash "${code}")
    if(NOT hash STREQUAL "4bf9d1a0ef923c66983c8a0f5b1e7bb64d5c36fcb7db14c46b0efdec979a7432")
        message(FATAL_ERROR "Faust VM differs from pinned archive; refusing to overwrite")
    endif()
    string(REPLACE "int  v2 = *reinterpret_cast<int*>(&v1);" [=[int v2 = 0;
                // AION_FAUST_DEFINED_BITCAST: int stack is exactly 32 bits.
                if constexpr (sizeof(v1) == sizeof(v2)) {
                    memcpy(&v2, &v1, sizeof(v2));
                } else {
                    throw faustexception("Unsupported non-single VM bitcast");
                }]=] code "${code}")
    string(REPLACE "REAL v2 = *reinterpret_cast<REAL*>(&v1);" [=[REAL v2 = 0;
                if constexpr (sizeof(v1) == sizeof(v2)) {
                    memcpy(&v2, &v1, sizeof(v2));
                } else {
                    throw faustexception("Unsupported non-single VM bitcast");
                }]=] code "${code}")
    string(REPLACE "InstructionIT address_stack[64];" "InstructionIT address_stack[64]{};" code "${code}")
    string(REPLACE "        memset(address_stack, 0, sizeof(InstructionIT) * 64);" "        // Iterator objects above are value-initialized, never byte-cleared." code "${code}")
    file(WRITE "${vm}" "${code}")
endif()
