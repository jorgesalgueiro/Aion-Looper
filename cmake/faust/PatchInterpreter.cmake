# Pinned Faust 2.88.0 local embedding adaptation. Preserve the upstream license.
# Normal processing remains unchanged. The original compute exception handler
# allocated diagnostics and wrote files; return a detectable non-finite sentinel
# instead, so our host can latch a silent fault without doing I/O on audio.
set(header "${AION_FAUST_SOURCE_DIR}/compiler/generator/interpreter/interpreter_dsp_aux.hh")
file(READ "${header}" code)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "AION_FAUST_REALTIME_CATCH" already_patched)
if(already_patched EQUAL -1)
    string(SHA256 original_hash "${code}")
    if(NOT original_hash STREQUAL "743bd5f76423fd27c38e24bb998230e6b0654f48796a6f9b7268cfc243b5c045")
        message(FATAL_ERROR "Faust compute header differs from pinned archive; refusing to overwrite")
    endif()
    set(start_text "            } catch (faustexception& e) {\n                std::cerr << e.Message();")
    string(FIND "${code}" "${start_text}" begin)
    if(begin EQUAL -1)
        message(FATAL_ERROR "Pinned Faust compute-handler context changed; review patch")
    endif()
    string(SUBSTRING "${code}" ${begin} -1 suffix)
    string(FIND "${suffix}" "            fCycle++;" end)
    if(end EQUAL -1)
        message(FATAL_ERROR "Pinned Faust compute-handler end changed")
    endif()
    string(SUBSTRING "${code}" 0 ${begin} prefix)
    string(SUBSTRING "${suffix}" ${end} -1 tail)
    set(replacement [=[            } catch (...) {
                // AION_FAUST_REALTIME_CATCH: host detects and latches silence.
                for (int chan = 0; chan < fFactory->fNumOutputs; ++chan) {
                    for (int frame = 0; frame < count; ++frame) {
                        outputs[chan][frame] = std::numeric_limits<REAL>::quiet_NaN();
                    }
                }
            }

]=])
    file(WRITE "${header}" "${prefix}${replacement}${tail}")
endif()
# This diagnostic counter is otherwise unused when tracing is disabled. Avoid
# signed overflow after 2^31 callbacks during long-running audio sessions.
file(READ "${header}" code)
string(FIND "${code}" "            fCycle++;" counter)
if(NOT counter EQUAL -1)
    string(REPLACE "            fCycle++;" "            // AION_FAUST_NO_CYCLE_COUNTER: no trace bookkeeping on audio." code "${code}")
    file(WRITE "${header}" "${code}")
endif()

# Trace selection is process-environment controlled upstream. The audio host must
# never accidentally instantiate a diagnostic VM that prints from compute.
set(container "${AION_FAUST_SOURCE_DIR}/compiler/generator/interpreter/interpreter_code_container.cpp")
file(READ "${container}" code)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "AION_FAUST_TRACE_DISABLED" patched)
if(patched EQUAL -1)
    string(SHA256 original_hash "${code}")
    if(NOT original_hash STREQUAL "90460034a36e49a39e38082214ad6d2ecbbb2dc6bdc70c17b28d217f370351ad")
        message(FATAL_ERROR "Faust trace source differs from pinned archive; refusing to overwrite")
    endif()
    set(original "const char* trace = getenv(\"FAUST_INTERP_TRACE\");")
    string(FIND "${code}" "${original}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned Faust trace-selection context changed")
    endif()
    string(REPLACE "${original}" "const char* trace = nullptr; // AION_FAUST_TRACE_DISABLED" code "${code}")
    file(WRITE "${container}" "${code}")
endif()
