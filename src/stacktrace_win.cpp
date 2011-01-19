#include "mr_prog.h"
#include <dbghelp.h>

/**
 * Windows methodology: 
 * http://msdn.microsoft.com/en-us/library/bb204633(VS.85).aspx
 * */

mrutils::StackTrace::StackTrace() {
  count_ = CaptureStackBackTrace(0, MAX_TRACES, trace_, NULL);
}


namespace {
    // FROM GOOGLE CHROME
    class SymbolContext {
        public:
            SymbolContext() 
                : init_error_(ERROR_SUCCESS)
            {
                // Initializes the symbols for the process.
                // Defer symbol load until they're needed, use undecorated names, and
                // get line numbers.
                SymSetOptions(SYMOPT_DEFERRED_LOADS |
                        SYMOPT_UNDNAME |
                        SYMOPT_LOAD_LINES);
                if (SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
                    init_error_ = ERROR_SUCCESS;
                } else {
                    init_error_ = GetLastError();
                    // TODO(awong): Handle error: SymInitialize can fail with
                    // ERROR_INVALID_PARAMETER.
                    // When it fails, we should not call debugbreak since it kills the current
                    // process (prevents future tests from running or kills the browser
                    // process).
                    std::cerr << "SymInitialize failed: " << init_error_;
                }
            }

        public:
            // Returns the error code of a failed initialization.
            DWORD init_error() const {
                return init_error_;
            }

            // For the given trace, attempts to resolve the symbols, and output a trace
            // to the ostream os.  The format for each line of the backtrace is:
            //
            //    <tab>SymbolName[0xAddress+Offset] (FileName:LineNo)
            //
            // This function should only be called if Init() has been called.  We do not
            // LOG(FATAL) here because this code is called might be triggered by a
            // LOG(FATAL) itself.
            void OutputTraceToStream(const void* const* trace,
                    int count,
                    std::ostream& os) {
                for (size_t i = 0; (i < count) && os.good(); ++i) {
                    const int kMaxNameLength = 256;
                    DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

                    // Code adapted from MSDN example:
                    // http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
                    ULONG64 buffer[
                        (sizeof(SYMBOL_INFO) +
                         kMaxNameLength * sizeof(wchar_t) +
                         sizeof(ULONG64) - 1) /
                        sizeof(ULONG64)];
                    memset(buffer, 0, sizeof(buffer));

                    // Initialize symbol information retrieval structures.
                    DWORD64 sym_displacement = 0;
                    PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
                    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                    symbol->MaxNameLen = kMaxNameLength - 1;
                    BOOL has_symbol = SymFromAddr(GetCurrentProcess(), frame,
                            &sym_displacement, symbol);

                    // Attempt to retrieve line number information.
                    DWORD line_displacement = 0;
                    IMAGEHLP_LINE64 line = {};
                    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                    BOOL has_line = SymGetLineFromAddr64(GetCurrentProcess(), frame,
                            &line_displacement, &line);

                    // Output the backtrace line.
                    os << "\t";
                    if (has_symbol) {
                        os << symbol->Name << " [0x" << trace[i] << "+"
                            << sym_displacement << "]";
                    } else {
                        // If there is no symbol informtion, add a spacer.
                        os << "(No symbol) [0x" << trace[i] << "]";
                    }
                    if (has_line) {
                        os << " (" << line.FileName << ":" << line.LineNumber << ")";
                    }
                    os << "\n";
                }
            }

            void OutputTraceToStream(const void* const* trace,
                    int count, FILE * fp) {
                for (size_t i = 0; (i < count); ++i) {
                    const int kMaxNameLength = 256;
                    DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

                    // Code adapted from MSDN example:
                    // http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
                    ULONG64 buffer[
                        (sizeof(SYMBOL_INFO) +
                         kMaxNameLength * sizeof(wchar_t) +
                         sizeof(ULONG64) - 1) /
                        sizeof(ULONG64)];
                    memset(buffer, 0, sizeof(buffer));

                    // Initialize symbol information retrieval structures.
                    DWORD64 sym_displacement = 0;
                    PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
                    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                    symbol->MaxNameLen = kMaxNameLength - 1;
                    BOOL has_symbol = SymFromAddr(GetCurrentProcess(), frame,
                            &sym_displacement, symbol);

                    // Attempt to retrieve line number information.
                    DWORD line_displacement = 0;
                    IMAGEHLP_LINE64 line = {};
                    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                    BOOL has_line = SymGetLineFromAddr64(GetCurrentProcess(), frame,
                            &line_displacement, &line);

                    // Output the backtrace line.
                    fprintf(fp,"\t");
                    if (has_symbol) {
                        fprintf(fp, "%s  [0x%s +%d]",symbol->Name,trace[i], sym_displacement);
                    } else {
                        // If there is no symbol informtion, add a spacer.
                        fprintf(fp,"(No symbol) [0x%s]",trace[i]);
                    }
                    if (has_line) {
                        fprintf(fp," (%s:%d)", line.FileName, line.LineNumber );
                    }
                    fprintf(fp,"\n");
                }
            }

        private:
            DWORD init_error_;
    };
} 

void * mrutils::StackTrace::context = (void*)new SymbolContext();

// static
void mrutils::Debug::breakNow() {
    __debugbreak();
}


void mrutils::StackTrace::print(std::ostream& os) {
    DWORD error = ((SymbolContext*)context)->init_error();

    if (error != ERROR_SUCCESS) {
        os << "Error initializing symbols (" << error
           << ").  Dumping unresolved backtrace:\n";

        for (int i = 0; (i < count_) && os.good(); ++i) {
            os << "\t" << trace_[i] << "\n";
        }

    } else {
        os << "Backtrace:\n";
        ((SymbolContext*)context)->OutputTraceToStream(trace_, count_, os);

    }
}

void mrutils::StackTrace::print(FILE * fp) {
    DWORD error = ((SymbolContext*)context)->init_error();

    if (error != ERROR_SUCCESS) {
        fprintf(fp, "Error initializing symbols.  Dumping unresolved backtrace:\n");

        for (int i = 0; (i < count_); ++i) {
            fprintf(fp,"\t%s\n", trace_[i]);
        }

    } else {
        fprintf(fp, "Backtrace:\n");
        ((SymbolContext*)context)->OutputTraceToStream(trace_, count_, fp);

    }
}
