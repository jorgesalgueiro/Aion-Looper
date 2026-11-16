# Off-audio error cleanup for the pinned compiler. Retain LGPL upstream headers.
set(reader "${AION_FAUST_SOURCE_DIR}/compiler/parser/sourcereader.cpp")
file(READ "${reader}" code)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "AION_FAUST_LEXER_CLEANUP" patched)
if(patched EQUAL -1)
    string(SHA256 hash "${code}")
    if(NOT hash STREQUAL "7d08273abe7ada7cf33c81bbfbe02aa13971f0b8a4d5da1e8bc4315dd8042f60")
        message(FATAL_ERROR "Pinned Faust parser changed; refusing cleanup patch")
    endif()
    string(REPLACE "Tree SourceReader::parseLocal(const char* fname)\n{\n    int r = FAUSTparse();" [=[Tree SourceReader::parseLocal(const char* fname)
{
    // AION_FAUST_LEXER_CLEANUP: syntax exceptions must also release flex buffers.
    struct LexerCleanup { ~LexerCleanup() { FAUSTlex_destroy(); } } cleanup;
    int r = FAUSTparse();]=] code "${code}")
    string(REPLACE "    FAUSTlex_destroy();\n\n    // We have parsed a valid file" "    // We have parsed a valid file; the scope guard releases the lexer." code "${code}")
    file(WRITE "${reader}" "${code}")
endif()
set(visitor "${AION_FAUST_SOURCE_DIR}/compiler/generator/interpreter/interpreter_instructions.hh")
file(READ "${visitor}" code)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "AION_FAUST_UNFINISHED_BLOCKS" patched)
if(patched EQUAL -1)
    string(SHA256 hash "${code}")
    if(NOT hash STREQUAL "3f91db2251140164c827bc2588d808211dafebc0cf6a1504269f5d5c9277e8c1")
        message(FATAL_ERROR "Pinned adapted Faust visitor changed; refusing cleanup patch")
    endif()
    string(REPLACE "virtual ~InterpreterInstVisitor() {}" [=[virtual ~InterpreterInstVisitor() {
        // AION_FAUST_UNFINISHED_BLOCKS: factory handoff clears these owners.
        delete fUserInterfaceBlock;
        delete fCurrentBlock;
    }]=] code "${code}")
    file(WRITE "${visitor}" "${code}")
endif()
set(container "${AION_FAUST_SOURCE_DIR}/compiler/generator/interpreter/interpreter_code_container.cpp")
file(READ "${container}" code)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "AION_FAUST_FACTORY_HANDOFF" patched)
if(patched EQUAL -1)
    string(SHA256 hash "${code}")
    if(NOT hash STREQUAL "6e6059775826b4af36b496dc1a225c08dbece93c29425f79de8b140488b6b63b")
        message(FATAL_ERROR "Pinned adapted Faust container changed; refusing cleanup patch")
    endif()
    string(REPLACE "// Scalar\n" [=[// AION_FAUST_FACTORY_HANDOFF: release only after successful factory creation.
template <class REAL, class Factory>
static Factory* transferVisitorBlocks(Factory* factory)
{
    getInterpreterVisitor<REAL>()->fUserInterfaceBlock = nullptr;
    getInterpreterVisitor<REAL>()->fCurrentBlock = nullptr;
    return factory;
}

// Scalar
]=] code "${code}")
    string(REPLACE "return new interpreter_dsp_factory_aux<REAL, " "return transferVisitorBlocks<REAL>(new interpreter_dsp_factory_aux<REAL, " code "${code}")
    string(REPLACE "return new interpreter_comp_dsp_factory_aux<REAL, " "return transferVisitorBlocks<REAL>(new interpreter_comp_dsp_factory_aux<REAL, " code "${code}")
    string(REPLACE "compute_dsp_block);" "compute_dsp_block));" code "${code}")
    file(WRITE "${container}" "${code}")
endif()
