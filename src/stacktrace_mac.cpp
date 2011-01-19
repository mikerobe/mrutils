// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mr_prog.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <execinfo.h>
#include <iostream>

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif

#include <string>

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#endif

void * mrutils::StackTrace::context = NULL;

namespace {
    // from Google Chrome
    // http://www.codesourcery.com/cxx-abi/abi.html#mangling
    const char kMangledSymbolPrefix[] = "_Z";
    const char kSymbolCharacters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

    void DemangleSymbols(std::string* text) {
        #if defined(__GLIBCXX__)
            std::string::size_type search_from = 0;
            while (search_from < text->size()) {
                // Look for the start of a mangled symbol, from search_from.
                std::string::size_type mangled_start =
                    text->find(kMangledSymbolPrefix, search_from);
                if (mangled_start == std::string::npos) {
                    break;  // Mangled symbol not found.
                }

                // Look for the end of the mangled symbol.
                std::string::size_type mangled_end =
                    text->find_first_not_of(kSymbolCharacters, mangled_start);
                if (mangled_end == std::string::npos) {
                    mangled_end = text->size();
                }
                std::string mangled_symbol =
                    text->substr(mangled_start, mangled_end - mangled_start);

                // Try to demangle the mangled symbol candidate.
                int status = 0;
                char * demangled_symbol = abi::__cxa_demangle(mangled_symbol.c_str(), NULL, 0, &status);
                if (status == 0) {  // Demangling is successful.
                    // Remove the mangled symbol.
                    text->erase(mangled_start, mangled_end - mangled_start);
                    // Insert the demangled symbol.
                    text->insert(mangled_start, demangled_symbol);
                    // Next time, we'll start right after the demangled symbol we inserted.
                    search_from = mangled_start + strlen(demangled_symbol);
                } else {
                    // Failed to demangle.  Retry after the "_Z" we just found.
                    search_from = mangled_start + 2;
                }

                free(demangled_symbol);
            }
        #endif  // defined(__GLIBCXX__)
    }
} 

mrutils::StackTrace::StackTrace() {
  //if (backtrace == NULL) { count_ = 0; return; }
  count_ = backtrace(trace_, MAX_TRACES);
}

void mrutils::StackTrace::print(FILE * fp) {
  //if (backtrace_symbols == NULL) return;

  char ** strs = backtrace_symbols(trace_, count_);

  if (strs == NULL) {
      fprintf(fp, "Unable get symbols for backtrace.  Dumping raw addresses in trace:\n");
      for (int i = 0; i < count_; ++i) fprintf(fp, "\t%s\n", (char*)trace_[i]);
  } else {
      fprintf(fp, "Backtrace:\n");

      for (int i = 0; i < count_; ++i) {
          std::string trace_symbol(strs[i]);
          DemangleSymbols(&trace_symbol);
          fprintf(fp, "%s\n", trace_symbol.c_str());
      }

      free(strs);
  }
}

void mrutils::StackTrace::print(std::ostream& os) {
  //if (backtrace_symbols == NULL) return;

  char ** strs = backtrace_symbols(trace_, count_);

  if (strs == NULL) {
      os << "Unable get symbols for backtrace.  Dumping raw addresses in trace:\n";
      for (int i = 0; i < count_; ++i) os << "\t" << trace_[i] << "\n";
  } else {
      os << "Backtrace:\n";

      for (int i = 0; i < count_; ++i) {
          std::string trace_symbol(strs[i]);
          DemangleSymbols(&trace_symbol);
          os << trace_symbol << "\n";
      }

      free(strs);
  }
}

// static
void mrutils::Debug::breakNow() {
#if defined(ARCH_CPU_ARM_FAMILY)
  asm("bkpt 0");
#else
  asm("int3");
#endif
}

