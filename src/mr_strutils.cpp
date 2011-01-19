#include "mr_strutils.h"
#include <cctype>
#include <string>
#include <fstream>

namespace {

/**************************************************************
 * Formats
 **************************************************************/
    class FormatCommas : public std::numpunct<char> {
        protected:
            inline char do_decimal_point()const 
            { return '.'; }

            inline char do_thousands_sep() const 
            { return ','; }

            inline std::string do_grouping() const 
            { return "\003"; }
    };


}

namespace mrutils {
    namespace formatting {
        std::ostream& formatCommas(std::ostream& os)
        { os.imbue(std::locale(os.getloc(),new FormatCommas)); return os; }

        Money::Money(const char * symbol, const double amt)
        : symbol(symbol), amt(amt) 
        { ss.imbue(std::locale(ss.getloc(), new FormatCommas)); }

        Money::Money(const Money& other) 
        : symbol(other.symbol), amt(other.amt)
        { ss.imbue(std::locale(ss.getloc(), new FormatCommas)); }

    }
}



void mrutils::ColumnBuilder::print(FILE* file) {
    printCharMatrix(file, (const char**)(&summary[0]), r+1, ncols, &colWidths[0]);
    justDidPrint = true;
}
void mrutils::ColumnBuilder::print(std::ostream&os) {
    printCharMatrix(os, (const char**)(&summary[0]), r+1, ncols, &colWidths[0]);
    justDidPrint = true;
}


std::string mrutils::formURL(const char * baseURL, const char * relative) {
    if (mrutils::startsWithI(relative,"http://") 
        ||mrutils::startsWithI(relative,"https://") 
        ||mrutils::startsWithI(relative,"ftp://") 
        ||mrutils::startsWithI(relative,"afp://") 
        ||mrutils::startsWithI(relative,"\\\\")) return relative;

    const char * secondSlash = baseURL;

    // find second slash
    secondSlash = strchr(secondSlash,'/'); if (secondSlash == NULL) return "";
    secondSlash = strchr(secondSlash+1,'/'); if (secondSlash == NULL) return "";
    ++secondSlash;

    const char * slash = secondSlash;

    if (*relative == '/') {
        // use the 3rd slash
        slash = strchr(slash,'/'); if (slash == NULL) return "";
        --slash;
    } else {
        // use the last slash
        for (const char * p = slash-1; NULL != (p = strchr(slash+1,'/')); slash = p) ;
    }

    std::string result; result.assign(baseURL,slash-baseURL+1);
    result.append(relative);

    char * p = const_cast<char*>(result.c_str()) + (secondSlash-baseURL);
    result.resize(formatPath(p) - result.c_str());
    return result;
}


char* mrutils::copyToAscii(char * to, const int sz, const char * from, const bool stripCR) {
    unsigned char c, c2, c3;
    const char * const end = to + sz;

    for (;to < end;) {
        c = (unsigned char)*from++;
        if (c == 0x00) { *to = '\0'; return to; }
        if (c < 0x7f) {
            if (stripCR && c == 0x0d){}else *to++ = c;
        } else if (c < 0xc2) {
            switch (c) {
                case 127: *to++=' '; break;
                case 128: *to++='E'; break;
                case 129: *to++=' '; break;
                case 130: *to++=' '; break;
                case 131: *to++='f'; break;
                case 132: *to++=','; break;
                case 133: *to++='.'; break;
                case 134: *to++='T'; break;
                case 135: *to++='T'; break;
                case 136: *to++='^'; break;
                case 137: *to++=' '; break;
                case 138: *to++='S'; break;
                case 139: *to++='['; break;
                case 140: *to++='C'; break;
                case 141: *to++=' '; break;
                case 142: *to++='Z'; break;
                case 143: *to++=' '; break;
                case 144: *to++=' '; break;
                case 145: *to++='`'; break;
                case 146: *to++='\''; break;
                case 147: *to++='"'; break;
                case 148: *to++='"'; break;
                case 149: *to++='*'; break;
                case 150: *to++='-'; break;
                case 151: *to++='-'; break;
                case 152: *to++='~'; break;
                case 153: *to++='T'; break;
                case 154: *to++='s'; break;
                case 155: *to++=']'; break;
                case 156: *to++='o'; break;
                case 157: *to++=' '; break;
                case 158: *to++='z'; break;
                case 159: *to++='Y'; break;
                case 160: *to++=' '; break;
                case 161: *to++=' '; break;
                case 162: *to++='c'; break;
                case 163: *to++='L'; break;
                case 164: *to++='o'; break;
                case 165: *to++='Y'; break;
                case 166: *to++='|'; break;
                case 167: *to++='S'; break;
                case 168: *to++=' '; break;
                case 169: *to++='c'; break;
                case 170: *to++='a'; break;
                case 171: *to++='['; break;
                case 172: *to++='N'; break;
                case 173: *to++=' '; break;
                case 174: *to++='r'; break;
                case 175: *to++='-'; break;
                case 176: *to++=' '; break;
                case 177: *to++='p'; break;
                case 178: *to++='2'; break;
                case 179: *to++='3'; break;
                case 180: *to++='`'; break;
                case 181: *to++='u'; break;
                case 182: *to++='P'; break;
                case 183: *to++='*'; break;
                case 184: *to++=' '; break;
                case 185: *to++='1'; break;
                case 186: *to++=' '; break;
                case 187: *to++=']'; break;
                case 188: *to++=' '; break;
                case 189: *to++=' '; break;
                case 190: *to++=' '; break;
                case 191: *to++=' '; break;
                case 192: *to++='A'; break;
                case 193: *to++='A'; break;
            }
        } else if (c < 0xe0) {
            // 2 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xc2: *to++ = 'A'; break;
                    case 0xc3: *to++ = 'A'; break;
                    case 0xc4: *to++ = 'A'; break;
                    case 0xc5: *to++ = 'A'; break;
                    case 0xc6: *to++ = 'A'; break;
                    case 0xc7: *to++ = 'C'; break;
                    case 0xc8: *to++ = 'E'; break;
                    case 0xc9: *to++ = 'E'; break;
                    case 0xca: *to++ = 'E'; break;
                    case 0xcb: *to++ = 'E'; break;
                    case 0xcc: *to++ = 'I'; break;
                    case 0xcd: *to++ = 'I'; break;
                    case 0xce: *to++ = 'I'; break;
                    case 0xcf: *to++ = 'I'; break;
                    case 0xd0: *to++ = 'D'; break;
                    case 0xd1: *to++ = 'N'; break;
                    case 0xd2: *to++ = 'O'; break;
                    case 0xd3: *to++ = 'O'; break;
                    case 0xd4: *to++ = 'O'; break;
                    case 0xd5: *to++ = 'O'; break;
                    case 0xd6: *to++ = 'O'; break;
                    case 0xd7: *to++ = 'x'; break;
                    case 0xd8: *to++ = 'O'; break;
                    case 0xd9: *to++ = 'U'; break;
                    case 0xda: *to++ = 'U'; break;
                    case 0xdb: *to++ = 'U'; break;
                    case 0xdc: *to++ = 'U'; break;
                    case 0xdd: *to++ = 'Y'; break;
                    case 0xde: *to++ = 'p'; break;
                    case 0xdf: *to++ = 'B'; break;
                }
            } else {
                switch (c) {
                    case 0xc2: 
                        switch (c2) {
                            case 0xa2: *to++ = 'c'; break;
                            case 0xa3: *to++ = 'L'; break;
                            case 0xa4: *to++ = 'o'; break;
                            case 0xa5: *to++ = 'Y'; break;
                            case 0xa6: *to++ = '|'; break;
                            case 0xa7: *to++ = 'S'; break;
                            case 0xa9: *to++ = 'c'; break;
                            case 0xab: *to++ = '['; break;
                            case 0xae: *to++ = 'R'; break;
                            case 0xaf: *to++ = '-'; break;
                            case 0xb1: if (to < end-1) { *to++ = '+'; *to++ = '-'; } else { *to = '\0'; return to; } break;
                            case 0xb2: *to++ = '2'; break;
                            case 0xb3: *to++ = '3'; break;
                            case 0xb4: *to++ = '`'; break;
                            case 0xb5: *to++ = 'u'; break;
                            case 0xb6: *to++ = 'P'; break;
                            case 0xb7: *to++ = '*'; break;
                            case 0xb8: *to++ = ','; break;
                            case 0xb9: *to++ = '1'; break;
                            case 0xbb: *to++ = ']'; break;
                        }
                        break;
                    case 0xc3:
                        switch (c2) {
                            case 0x80: *to++ = 'A'; break;
                            case 0x81: *to++ = 'A'; break;
                            case 0x82: *to++ = 'A'; break;
                            case 0x83: *to++ = 'A'; break;
                            case 0x84: *to++ = 'A'; break;
                            case 0x85: *to++ = 'A'; break;
                            case 0x86: if (to < end - 1) { *to++ = 'A'; *to++ = 'E'; } else { *to = '\0'; return to; } break;
                            case 0x87: *to++ = 'C'; break;
                            case 0x88: *to++ = 'E'; break;
                            case 0x89: *to++ = 'E'; break;
                            case 0x8a: *to++ = 'E'; break;
                            case 0x8b: *to++ = 'E'; break;
                            case 0x8c: *to++ = 'I'; break;
                            case 0x8d: *to++ = 'I'; break;
                            case 0x8e: *to++ = 'I'; break;
                            case 0x8f: *to++ = 'I'; break;
                            case 0x90: *to++ = 'D'; break;
                            case 0x91: *to++ = 'N'; break;
                            case 0x92: *to++ = 'O'; break;
                            case 0x93: *to++ = 'O'; break;
                            case 0x94: *to++ = 'O'; break;
                            case 0x95: *to++ = 'O'; break;
                            case 0x96: *to++ = 'O'; break;
                            case 0x97: *to++ = 'x'; break;
                            case 0x98: *to++ = 'O'; break;
                            case 0x99: *to++ = 'U'; break;
                            case 0x9a: *to++ = 'U'; break;
                            case 0x9b: *to++ = 'U'; break;
                            case 0x9c: *to++ = 'U'; break;
                            case 0x9d: *to++ = 'Y'; break;
                            case 0x9e: *to++ = 'p'; break;
                            case 0x9f: *to++ = 'b'; break;
                            case 0xa0: *to++ = 'a'; break;
                            case 0xa1: *to++ = 'a'; break;
                            case 0xa2: *to++ = 'a'; break;
                            case 0xa3: *to++ = 'a'; break;
                            case 0xa4: *to++ = 'a'; break;
                            case 0xa5: *to++ = 'a'; break;
                            case 0xa6: if (to < end - 1) { *to++ = 'a'; *to++ = 'e'; } else { *to = '\0'; return to; } break;
                            case 0xa7: *to++ = 'c'; break;
                            case 0xa8: *to++ = 'e'; break;
                            case 0xa9: *to++ = 'e'; break;
                            case 0xaa: *to++ = 'e'; break;
                            case 0xab: *to++ = 'e'; break;
                            case 0xac: *to++ = 'i'; break;
                            case 0xad: *to++ = 'i'; break;
                            case 0xae: *to++ = 'i'; break;
                            case 0xaf: *to++ = 'i'; break;
                            case 0xb0: *to++ = 'd'; break;
                            case 0xb1: *to++ = 'n'; break;
                            case 0xb2: *to++ = 'o'; break;
                            case 0xb3: *to++ = 'o'; break;
                            case 0xb4: *to++ = 'o'; break;
                            case 0xb5: *to++ = 'o'; break;
                            case 0xb6: *to++ = 'o'; break;
                            case 0xb7: *to++ = '/'; break;
                            case 0xb8: *to++ = 'o'; break;
                            case 0xb9: *to++ = 'u'; break;
                            case 0xba: *to++ = 'u'; break;
                            case 0xbb: *to++ = 'u'; break;
                            case 0xbc: *to++ = 'u'; break;
                            case 0xbd: *to++ = 'y'; break;
                            case 0xbe: *to++ = 'p'; break;
                            case 0xbf: *to++ = 'y'; break;
                        }
                        break;
                    case 0xc4:
                        switch (c2) {
                            case 0x80: *to++ = 'A'; break;
                            case 0x81: *to++ = 'a'; break;
                            case 0x82: *to++ = 'A'; break;
                            case 0x83: *to++ = 'a'; break;
                            case 0x84: *to++ = 'A'; break;
                            case 0x85: *to++ = 'a'; break;
                            case 0x86: *to++ = 'C'; break;
                            case 0x87: *to++ = 'c'; break;
                            case 0x88: *to++ = 'C'; break;
                            case 0x89: *to++ = 'c'; break;
                            case 0x8a: *to++ = 'C'; break;
                            case 0x8b: *to++ = 'c'; break;
                            case 0x8c: *to++ = 'C'; break;
                            case 0x8d: *to++ = 'c'; break;
                            case 0x8e: *to++ = 'D'; break;
                            case 0x8f: *to++ = 'd'; break;
                            case 0x90: *to++ = 'D'; break;
                            case 0x91: *to++ = 'd'; break;
                            case 0x92: *to++ = 'E'; break;
                            case 0x93: *to++ = 'e'; break;
                            case 0x94: *to++ = 'E'; break;
                            case 0x95: *to++ = 'e'; break;
                            case 0x96: *to++ = 'E'; break;
                            case 0x97: *to++ = 'e'; break;
                            case 0x98: *to++ = 'E'; break;
                            case 0x99: *to++ = 'e'; break;
                            case 0x9a: *to++ = 'E'; break;
                            case 0x9b: *to++ = 'e'; break;
                            case 0x9c: *to++ = 'G'; break;
                            case 0x9d: *to++ = 'g'; break;
                            case 0x9e: *to++ = 'G'; break;
                            case 0x9f: *to++ = 'g'; break;
                            case 0xa0: *to++ = 'G'; break;
                            case 0xa1: *to++ = 'g'; break;
                            case 0xa2: *to++ = 'G'; break;
                            case 0xa3: *to++ = 'g'; break;
                            case 0xa4: *to++ = 'H'; break;
                            case 0xa5: *to++ = 'h'; break;
                            case 0xa6: *to++ = 'H'; break;
                            case 0xa7: *to++ = 'h'; break;
                            case 0xa8: *to++ = 'I'; break;
                            case 0xa9: *to++ = 'i'; break;
                            case 0xaa: *to++ = 'I'; break;
                            case 0xab: *to++ = 'i'; break;
                            case 0xac: *to++ = 'I'; break;
                            case 0xad: *to++ = 'i'; break;
                            case 0xae: *to++ = 'I'; break;
                            case 0xaf: *to++ = 'i'; break;
                            case 0xb0: *to++ = 'I'; break;
                            case 0xb1: *to++ = 'i'; break;
                            case 0xb2: if (to < end-1) {*to++ = 'I'; *to++ = 'J';} else { *to = '\0'; return to; } break;
                            case 0xb3: if (to < end-1) {*to++ = 'i'; *to++ = 'j';} else { *to = '\0'; return to; } break;
                            case 0xb4: *to++ = 'J'; break;
                            case 0xb5: *to++ = 'j'; break;
                            case 0xb6: *to++ = 'K'; break;
                            case 0xb7: *to++ = 'k'; break;
                            case 0xb8: *to++ = 'k'; break;
                            case 0xb9: *to++ = 'L'; break;
                            case 0xba: *to++ = 'l'; break;
                            case 0xbb: *to++ = 'L'; break;
                            case 0xbc: *to++ = 'l'; break;
                            case 0xbd: *to++ = 'L'; break;
                            case 0xbe: *to++ = 'l'; break;
                            case 0xbf: *to++ = 'L'; break;
                        }
                        break;
                    case 0xc5:
                        switch (c2) {
                            case 0x80: *to++ = 'l'; break;
                            case 0x81: *to++ = 'L'; break;
                            case 0x82: *to++ = 'l'; break;
                            case 0x83: *to++ = 'N'; break;
                            case 0x84: *to++ = 'n'; break;
                            case 0x85: *to++ = 'N'; break;
                            case 0x86: *to++ = 'n'; break;
                            case 0x87: *to++ = 'N'; break;
                            case 0x88: *to++ = 'n'; break;
                            case 0x89: *to++ = 'n'; break;
                            case 0x8a: *to++ = 'N'; break;
                            case 0x8b: *to++ = 'n'; break;
                            case 0x8c: *to++ = 'O'; break;
                            case 0x8d: *to++ = 'o'; break;
                            case 0x8e: *to++ = 'O'; break;
                            case 0x8f: *to++ = 'o'; break;
                            case 0x90: *to++ = 'O'; break;
                            case 0x91: *to++ = 'o'; break;
                            case 0x92: if (to < end-1) {*to++ = 'O'; *to++ = 'E';} else { *to = '\0'; return to; } break;
                            case 0x93: if (to < end-1) {*to++ = 'o'; *to++ = 'e';} else { *to = '\0'; return to; } break;
                            case 0x94: *to++ = 'R'; break;
                            case 0x95: *to++ = 'r'; break;
                            case 0x96: *to++ = 'R'; break;
                            case 0x97: *to++ = 'r'; break;
                            case 0x98: *to++ = 'R'; break;
                            case 0x99: *to++ = 'r'; break;
                            case 0x9a: *to++ = 'S'; break;
                            case 0x9b: *to++ = 's'; break;
                            case 0x9c: *to++ = 'S'; break;
                            case 0x9d: *to++ = 's'; break;
                            case 0x9e: *to++ = 'S'; break;
                            case 0x9f: *to++ = 's'; break;
                            case 0xa0: *to++ = 'S'; break;
                            case 0xa1: *to++ = 's'; break;
                            case 0xa2: *to++ = 'T'; break;
                            case 0xa3: *to++ = 't'; break;
                            case 0xa4: *to++ = 'T'; break;
                            case 0xa5: *to++ = 't'; break;
                            case 0xa6: *to++ = 'T'; break;
                            case 0xa7: *to++ = 't'; break;
                            case 0xa8: *to++ = 'U'; break;
                            case 0xa9: *to++ = 'u'; break;
                            case 0xaa: *to++ = 'U'; break;
                            case 0xab: *to++ = 'u'; break;
                            case 0xac: *to++ = 'U'; break;
                            case 0xad: *to++ = 'u'; break;
                            case 0xae: *to++ = 'U'; break;
                            case 0xaf: *to++ = 'u'; break;
                            case 0xb0: *to++ = 'U'; break;
                            case 0xb1: *to++ = 'u'; break;
                            case 0xb2: *to++ = 'U'; break;
                            case 0xb3: *to++ = 'u'; break;
                            case 0xb4: *to++ = 'W'; break;
                            case 0xb5: *to++ = 'w'; break;
                            case 0xb6: *to++ = 'Y'; break;
                            case 0xb7: *to++ = 'y'; break;
                            case 0xb8: *to++ = 'Y'; break;
                            case 0xb9: *to++ = 'Z'; break;
                            case 0xba: *to++ = 'z'; break;
                            case 0xbb: *to++ = 'Z'; break;
                            case 0xbc: *to++ = 'z'; break;
                            case 0xbd: *to++ = 'Z'; break;
                            case 0xbe: *to++ = 'z'; break;
                            case 0xbf: *to++ = 'r'; break;
                        }
                        break;
                }
            }
        } else if (c < 0xf0) {
            // 3 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xe0: *to++ = 'a'; break;
                    case 0xe1: *to++ = 'a'; break;
                    case 0xe2: *to++ = 'a'; break;
                    case 0xe3: *to++ = 'a'; break;
                    case 0xe4: *to++ = 'a'; break;
                    case 0xe5: *to++ = 'a'; break;
                    case 0xe6: *to++ = 'a'; break;
                    case 0xe7: *to++ = 'c'; break;
                    case 0xe8: *to++ = 'e'; break;
                    case 0xe9: *to++ = 'e'; break;
                    case 0xea: *to++ = 'e'; break;
                    case 0xeb: *to++ = 'e'; break;
                    case 0xec: *to++ = 'i'; break;
                    case 0xed: *to++ = 'i'; break;
                    case 0xee: *to++ = 'i'; break;
                    case 0xef: *to++ = 'i'; break;
                }
            } else {
                if (0x00 == (c3 = (unsigned char)*from++)) { *to = '\0'; return to; }
                switch (c) {
                    case 0xe2:
                        switch (c2) {
                            case 0x80:
                                switch (c3) {
                                    case 0x90: *to++ = '-'; break;
                                    case 0x91: *to++ = '-'; break;
                                    case 0x92: if (to < end-1) {*to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x93: if (to < end-1) {*to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x94: if (to < end-2) {*to++ = '-'; *to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x95: if (to < end-2) {*to++ = '-'; *to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x96: if (to < end-1) {*to++ = '|'; *to++ = '|';} else {*to = '\0'; return to; } break;
                                    case 0x98: *to++ = '`'; break;
                                    case 0x99: *to++ = '\''; break;
                                    case 0x9a: *to++ = ','; break;
                                    case 0x9b: *to++ = '`'; break;
                                    case 0x9c: *to++ = '"'; break;
                                    case 0x9d: *to++ = '"'; break;
                                    case 0x9e: if (to < end-1) {*to++ = ','; *to++ = ',';} else {*to = '\0'; return to;} break;
                                    case 0x9f: *to++ = '"'; break;
                                    case 0xa0: *to++ = 'T'; break;
                                    case 0xa1: if (to < end-1) {*to++ = 'T'; *to++ = 'T';} else {*to = '\0'; return to;} break;
                                    case 0xa4: *to++ = '.'; break;
                                    case 0xa5: if (to < end-1) {*to++ = '.'; *to++ = '.';} else {*to = '\0'; return to;} break;
                                    case 0xa6: if (to < end-2) {*to++ = '.'; *to++ = '.'; *to++ = '.';} else {*to = '\0'; return to;} break;
                                    case 0xa7: *to++ = '*'; break;
                                    case 0xb2: *to++ = '\''; break;
                                    case 0xb3: *to++ = '"'; break;
                                    case 0xb4: if (to < end-1) {*to++ = '"'; *to++ = '\'';} else {*to = '\0'; return to;} break;
                                    case 0xb5: *to++ = '`'; break;
                                    case 0xb6: *to++ = '"'; break;
                                    case 0xb7: if (to < end-1) {*to++ = '`'; *to++ = '"';} else {*to = '\0'; return to;} break;
                                }
                                break;
                        }
                        break;
                }
            }
        } else if (c < 0xf5) {
            // 4 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xf0: *to++ = 'd'; break;
                    case 0xf1: *to++ = 'n'; break;
                    case 0xf2: *to++ = 'o'; break;
                    case 0xf3: *to++ = 'o'; break;
                    case 0xf4: *to++ = 'o'; break;
                }
            } else {
                if ('\0' == *from++ || '\0' == *from++) 
                { *to = '\0'; return to; }
            }
        } else if (c < 0xf8) {
            // 5 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xf6: *to++ = 'o'; break;
                    case 0xf7: *to++ = '/'; break;
                }
            } else {
                if ('\0' == *from++ || '\0' == *from++ || '\0' == *from++) 
                { *to = '\0'; return to; }
            }
        } else if (c < 0xfc) {
            // 6 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xf8: *to++ = 'o'; break;
                    case 0xf9: *to++ = 'u'; break;
                    case 0xfa: *to++ = 'u'; break;
                    case 0xfb: *to++ = 'u'; break;
                }
            } else {
                if ('\0' == *from++ || '\0' == *from++ || '\0' == *from++ || '\0' == *from++) 
                { *to = '\0'; return to; }
            }
        } else {
            switch (c) {
                case 0xfc: *to++ = 'u'; break;
                case 0xfd: *to++ = 'y'; break;
                case 0xfe: *to++ = 'p'; break;
                case 0xff: *to++ = 'y'; break;
            }
        }
    } return to;
}
char* mrutils::copyToWindowsLatin1(char * to, const int sz, const char * from, const bool stripCR) {
    unsigned char c, c2, c3;
    const char * const end = to + sz;

    for (;to < end;) {
        c = (unsigned char)*from++;
        if (c == 0x00) { *to = '\0'; return to; }
        if (c < 0x7f) {
            if (stripCR && c == 0x0d){}else *to++ = c;
        } else if (c < 0xc2) {
            switch (c) {
                case 127: *to++=' '; break;
                case 128: *to++='E'; break;
                case 129: *to++=' '; break;
                case 130: *to++=' '; break;
                case 131: *to++='f'; break;
                case 132: *to++=','; break;
                case 133: *to++='.'; break;
                case 134: *to++='T'; break;
                case 135: *to++='T'; break;
                case 136: *to++='^'; break;
                case 137: *to++=' '; break;
                case 138: *to++='S'; break;
                case 139: *to++='['; break;
                case 140: *to++='C'; break;
                case 141: *to++=' '; break;
                case 142: *to++='Z'; break;
                case 143: *to++=' '; break;
                case 144: *to++=' '; break;
                case 145: *to++='`'; break;
                case 146: *to++='\''; break;
                case 147: *to++='"'; break;
                case 148: *to++='"'; break;
                case 149: *to++='*'; break;
                case 150: *to++='-'; break;
                case 151: *to++='-'; break;
                case 152: *to++='~'; break;
                case 153: *to++='T'; break;
                case 154: *to++='s'; break;
                case 155: *to++=']'; break;
                case 156: *to++='o'; break;
                case 157: *to++=' '; break;
                case 158: *to++='z'; break;
                case 159: *to++='Y'; break;
                case 160: *to++=' '; break;
                case 161: *to++=' '; break;
                case 162: *to++='c'; break;
                case 163: *to++='L'; break;
                case 164: *to++='o'; break;
                case 165: *to++='Y'; break;
                case 166: *to++='|'; break;
                case 167: *to++='S'; break;
                case 168: *to++=' '; break;
                case 169: *to++='c'; break;
                case 170: *to++='a'; break;
                case 171: *to++='['; break;
                case 172: *to++='N'; break;
                case 173: *to++=' '; break;
                case 174: *to++='r'; break;
                case 175: *to++='-'; break;
                case 176: *to++=' '; break;
                case 177: *to++='p'; break;
                case 178: *to++='2'; break;
                case 179: *to++='3'; break;
                case 180: *to++='`'; break;
                case 181: *to++='u'; break;
                case 182: *to++='P'; break;
                case 183: *to++='*'; break;
                case 184: *to++=' '; break;
                case 185: *to++='1'; break;
                case 186: *to++=' '; break;
                case 187: *to++=']'; break;
                case 188: *to++=' '; break;
                case 189: *to++=' '; break;
                case 190: *to++=' '; break;
                case 191: *to++=' '; break;
                case 192: *to++='A'; break;
                case 193: *to++='A'; break;
            }
        } else if (c < 0xe0) {
            // 2 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xc2: *to++ = 'A'; break;
                    case 0xc3: *to++ = 'A'; break;
                    case 0xc4: *to++ = 'A'; break;
                    case 0xc5: *to++ = 'A'; break;
                    case 0xc6: *to++ = 'A'; break;
                    case 0xc7: *to++ = 'C'; break;
                    case 0xc8: *to++ = 'E'; break;
                    case 0xc9: *to++ = 'E'; break;
                    case 0xca: *to++ = 'E'; break;
                    case 0xcb: *to++ = 'E'; break;
                    case 0xcc: *to++ = 'I'; break;
                    case 0xcd: *to++ = 'I'; break;
                    case 0xce: *to++ = 'I'; break;
                    case 0xcf: *to++ = 'I'; break;
                    case 0xd0: *to++ = 'D'; break;
                    case 0xd1: *to++ = 'N'; break;
                    case 0xd2: *to++ = 'O'; break;
                    case 0xd3: *to++ = 'O'; break;
                    case 0xd4: *to++ = 'O'; break;
                    case 0xd5: *to++ = 'O'; break;
                    case 0xd6: *to++ = 'O'; break;
                    case 0xd7: *to++ = 'x'; break;
                    case 0xd8: *to++ = 'O'; break;
                    case 0xd9: *to++ = 'U'; break;
                    case 0xda: *to++ = 'U'; break;
                    case 0xdb: *to++ = 'U'; break;
                    case 0xdc: *to++ = 'U'; break;
                    case 0xdd: *to++ = 'Y'; break;
                    case 0xde: *to++ = 'p'; break;
                    case 0xdf: *to++ = 'B'; break;
                }
            } else {
                switch (c) {
                    case 0xc2: 
                        switch (c2) {
                            case 0xa1: *to++ = '\xa1'; break;
                            case 0xa2: *to++ = '\xa2'; break;
                            case 0xa3: *to++ = '\xa3'; break;
                            case 0xa4: *to++ = '\xa4'; break;
                            case 0xa5: *to++ = '\xa5'; break;
                            case 0xa6: *to++ = '\xa6'; break;
                            case 0xa7: *to++ = '\xa7'; break;
                            case 0xa8: *to++ = '\xa8'; break;
                            case 0xa9: *to++ = '\xa9'; break;
                            case 0xaa: *to++ = '\xaa'; break;
                            case 0xab: *to++ = '\xab'; break;
                            case 0xac: *to++ = '\xac'; break;
                            case 0xad: *to++ = '\xad'; break;
                            case 0xae: *to++ = '\xae'; break;
                            case 0xaf: *to++ = '\xaf'; break;
                            case 0xb0: *to++ = '\xb0'; break;
                            case 0xb1: *to++ = '\xb1'; break;
                            case 0xb2: *to++ = '\xb2'; break;
                            case 0xb3: *to++ = '\xb3'; break;
                            case 0xb4: *to++ = '\xb4'; break;
                            case 0xb5: *to++ = '\xb5'; break;
                            case 0xb6: *to++ = '\xb6'; break;
                            case 0xb7: *to++ = '\xb7'; break;
                            case 0xb8: *to++ = '\xb8'; break;
                            case 0xb9: *to++ = '\xb9'; break;
                            case 0xba: *to++ = '\xba'; break;
                            case 0xbb: *to++ = '\xbb'; break;
                            case 0xbc: *to++ = '\xbc'; break;
                            case 0xbd: *to++ = '\xbd'; break;
                            case 0xbe: *to++ = '\xbe'; break;
                            case 0xbf: *to++ = '\xbf'; break;
                        }
                        break;
                    case 0xc3:
                        *to++ = c2+0x40; 
                        break;
                    case 0xc4:
                        switch (c2) {
                            case 0x80: *to++ = 'A'; break;
                            case 0x81: *to++ = 'a'; break;
                            case 0x82: *to++ = 'A'; break;
                            case 0x83: *to++ = 'a'; break;
                            case 0x84: *to++ = 'A'; break;
                            case 0x85: *to++ = 'a'; break;
                            case 0x86: *to++ = 'C'; break;
                            case 0x87: *to++ = 'c'; break;
                            case 0x88: *to++ = 'C'; break;
                            case 0x89: *to++ = 'c'; break;
                            case 0x8a: *to++ = 'C'; break;
                            case 0x8b: *to++ = 'c'; break;
                            case 0x8c: *to++ = 'C'; break;
                            case 0x8d: *to++ = 'c'; break;
                            case 0x8e: *to++ = 'D'; break;
                            case 0x8f: *to++ = 'd'; break;
                            case 0x90: *to++ = 'D'; break;
                            case 0x91: *to++ = 'd'; break;
                            case 0x92: *to++ = 'E'; break;
                            case 0x93: *to++ = 'e'; break;
                            case 0x94: *to++ = 'E'; break;
                            case 0x95: *to++ = 'e'; break;
                            case 0x96: *to++ = 'E'; break;
                            case 0x97: *to++ = 'e'; break;
                            case 0x98: *to++ = 'E'; break;
                            case 0x99: *to++ = 'e'; break;
                            case 0x9a: *to++ = 'E'; break;
                            case 0x9b: *to++ = 'e'; break;
                            case 0x9c: *to++ = 'G'; break;
                            case 0x9d: *to++ = 'g'; break;
                            case 0x9e: *to++ = 'G'; break;
                            case 0x9f: *to++ = 'g'; break;
                            case 0xa0: *to++ = 'G'; break;
                            case 0xa1: *to++ = 'g'; break;
                            case 0xa2: *to++ = 'G'; break;
                            case 0xa3: *to++ = 'g'; break;
                            case 0xa4: *to++ = 'H'; break;
                            case 0xa5: *to++ = 'h'; break;
                            case 0xa6: *to++ = 'H'; break;
                            case 0xa7: *to++ = 'h'; break;
                            case 0xa8: *to++ = 'I'; break;
                            case 0xa9: *to++ = 'i'; break;
                            case 0xaa: *to++ = 'I'; break;
                            case 0xab: *to++ = 'i'; break;
                            case 0xac: *to++ = 'I'; break;
                            case 0xad: *to++ = 'i'; break;
                            case 0xae: *to++ = 'I'; break;
                            case 0xaf: *to++ = 'i'; break;
                            case 0xb0: *to++ = 'I'; break;
                            case 0xb1: *to++ = 'i'; break;
                            case 0xb2: if (to < end-1) {*to++ = 'I'; *to++ = 'J';} else { *to = '\0'; return to; } break;
                            case 0xb3: if (to < end-1) {*to++ = 'i'; *to++ = 'j';} else { *to = '\0'; return to; } break;
                            case 0xb4: *to++ = 'J'; break;
                            case 0xb5: *to++ = 'j'; break;
                            case 0xb6: *to++ = 'K'; break;
                            case 0xb7: *to++ = 'k'; break;
                            case 0xb8: *to++ = 'k'; break;
                            case 0xb9: *to++ = 'L'; break;
                            case 0xba: *to++ = 'l'; break;
                            case 0xbb: *to++ = 'L'; break;
                            case 0xbc: *to++ = 'l'; break;
                            case 0xbd: *to++ = 'L'; break;
                            case 0xbe: *to++ = 'l'; break;
                            case 0xbf: *to++ = 'L'; break;
                        }
                        break;
                    case 0xc5:
                        switch (c2) {
                            case 0x80: *to++ = 'l'; break;
                            case 0x81: *to++ = 'L'; break;
                            case 0x82: *to++ = 'l'; break;
                            case 0x83: *to++ = 'N'; break;
                            case 0x84: *to++ = 'n'; break;
                            case 0x85: *to++ = 'N'; break;
                            case 0x86: *to++ = 'n'; break;
                            case 0x87: *to++ = 'N'; break;
                            case 0x88: *to++ = 'n'; break;
                            case 0x89: *to++ = 'n'; break;
                            case 0x8a: *to++ = 'N'; break;
                            case 0x8b: *to++ = 'n'; break;
                            case 0x8c: *to++ = 'O'; break;
                            case 0x8d: *to++ = 'o'; break;
                            case 0x8e: *to++ = 'O'; break;
                            case 0x8f: *to++ = 'o'; break;
                            case 0x90: *to++ = 'O'; break;
                            case 0x91: *to++ = 'o'; break;
                            case 0x92: if (to < end-1) {*to++ = 'O'; *to++ = 'E';} else { *to = '\0'; return to; } break;
                            case 0x93: if (to < end-1) {*to++ = 'o'; *to++ = 'e';} else { *to = '\0'; return to; } break;
                            case 0x94: *to++ = 'R'; break;
                            case 0x95: *to++ = 'r'; break;
                            case 0x96: *to++ = 'R'; break;
                            case 0x97: *to++ = 'r'; break;
                            case 0x98: *to++ = 'R'; break;
                            case 0x99: *to++ = 'r'; break;
                            case 0x9a: *to++ = 'S'; break;
                            case 0x9b: *to++ = 's'; break;
                            case 0x9c: *to++ = 'S'; break;
                            case 0x9d: *to++ = 's'; break;
                            case 0x9e: *to++ = 'S'; break;
                            case 0x9f: *to++ = 's'; break;
                            case 0xa0: *to++ = 'S'; break;
                            case 0xa1: *to++ = 's'; break;
                            case 0xa2: *to++ = 'T'; break;
                            case 0xa3: *to++ = 't'; break;
                            case 0xa4: *to++ = 'T'; break;
                            case 0xa5: *to++ = 't'; break;
                            case 0xa6: *to++ = 'T'; break;
                            case 0xa7: *to++ = 't'; break;
                            case 0xa8: *to++ = 'U'; break;
                            case 0xa9: *to++ = 'u'; break;
                            case 0xaa: *to++ = 'U'; break;
                            case 0xab: *to++ = 'u'; break;
                            case 0xac: *to++ = 'U'; break;
                            case 0xad: *to++ = 'u'; break;
                            case 0xae: *to++ = 'U'; break;
                            case 0xaf: *to++ = 'u'; break;
                            case 0xb0: *to++ = 'U'; break;
                            case 0xb1: *to++ = 'u'; break;
                            case 0xb2: *to++ = 'U'; break;
                            case 0xb3: *to++ = 'u'; break;
                            case 0xb4: *to++ = 'W'; break;
                            case 0xb5: *to++ = 'w'; break;
                            case 0xb6: *to++ = 'Y'; break;
                            case 0xb7: *to++ = 'y'; break;
                            case 0xb8: *to++ = 'Y'; break;
                            case 0xb9: *to++ = 'Z'; break;
                            case 0xba: *to++ = 'z'; break;
                            case 0xbb: *to++ = 'Z'; break;
                            case 0xbc: *to++ = 'z'; break;
                            case 0xbd: *to++ = 'Z'; break;
                            case 0xbe: *to++ = 'z'; break;
                            case 0xbf: *to++ = 'r'; break;
                        }
                        break;
                }
            }
        } else if (c < 0xf0) {
            // 3 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xe0: *to++ = 'a'; break;
                    case 0xe1: *to++ = 'a'; break;
                    case 0xe2: *to++ = 'a'; break;
                    case 0xe3: *to++ = 'a'; break;
                    case 0xe4: *to++ = 'a'; break;
                    case 0xe5: *to++ = 'a'; break;
                    case 0xe6: *to++ = 'a'; break;
                    case 0xe7: *to++ = 'c'; break;
                    case 0xe8: *to++ = 'e'; break;
                    case 0xe9: *to++ = 'e'; break;
                    case 0xea: *to++ = 'e'; break;
                    case 0xeb: *to++ = 'e'; break;
                    case 0xec: *to++ = 'i'; break;
                    case 0xed: *to++ = 'i'; break;
                    case 0xee: *to++ = 'i'; break;
                    case 0xef: *to++ = 'i'; break;
                }
            } else {
                if (0x00 == (c3 = (unsigned char)*from++)) { *to = '\0'; return to; }
                switch (c) {
                    case 0xe2:
                        switch (c2) {
                            case 0x80:
                                switch (c3) {
                                    case 0x90: *to++ = '-'; break;
                                    case 0x91: *to++ = '-'; break;
                                    case 0x92: if (to < end-1) {*to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x93: if (to < end-1) {*to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x94: if (to < end-2) {*to++ = '-'; *to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x95: if (to < end-2) {*to++ = '-'; *to++ = '-'; *to++ = '-';} else {*to = '\0'; return to; } break;
                                    case 0x96: if (to < end-1) {*to++ = '|'; *to++ = '|';} else {*to = '\0'; return to; } break;
                                    case 0x98: *to++ = '`'; break;
                                    case 0x99: *to++ = '\''; break;
                                    case 0x9a: *to++ = ','; break;
                                    case 0x9b: *to++ = '`'; break;
                                    case 0x9c: *to++ = '"'; break;
                                    case 0x9d: *to++ = '"'; break;
                                    case 0x9e: if (to < end-1) {*to++ = ','; *to++ = ',';} else {*to = '\0'; return to;} break;
                                    case 0x9f: *to++ = '"'; break;
                                    case 0xa0: *to++ = 'T'; break;
                                    case 0xa1: if (to < end-1) {*to++ = 'T'; *to++ = 'T';} else {*to = '\0'; return to;} break;
                                    case 0xa4: *to++ = '.'; break;
                                    case 0xa5: if (to < end-1) {*to++ = '.'; *to++ = '.';} else {*to = '\0'; return to;} break;
                                    case 0xa6: if (to < end-2) {*to++ = '.'; *to++ = '.'; *to++ = '.';} else {*to = '\0'; return to;} break;
                                    case 0xa7: *to++ = '*'; break;
                                    case 0xb2: *to++ = '\''; break;
                                    case 0xb3: *to++ = '"'; break;
                                    case 0xb4: if (to < end-1) {*to++ = '"'; *to++ = '\'';} else {*to = '\0'; return to;} break;
                                    case 0xb5: *to++ = '`'; break;
                                    case 0xb6: *to++ = '"'; break;
                                    case 0xb7: if (to < end-1) {*to++ = '`'; *to++ = '"';} else {*to = '\0'; return to;} break;
                                }
                                break;
                        }
                        break;
                }
            }
        } else if (c < 0xf5) {
            // 4 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xf0: *to++ = 'd'; break;
                    case 0xf1: *to++ = 'n'; break;
                    case 0xf2: *to++ = 'o'; break;
                    case 0xf3: *to++ = 'o'; break;
                    case 0xf4: *to++ = 'o'; break;
                }
            } else {
                if ('\0' == *from++ || '\0' == *from++) 
                { *to = '\0'; return to; }
            }
        } else if (c < 0xf8) {
            // 5 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xf6: *to++ = 'o'; break;
                    case 0xf7: *to++ = '/'; break;
                }
            } else {
                if ('\0' == *from++ || '\0' == *from++ || '\0' == *from++) 
                { *to = '\0'; return to; }
            }
        } else if (c < 0xfc) {
            // 6 byte unicode
            c2 = (unsigned char)*from++;
            if (c2 < 0x80) {
                // then interpret as iso8859-1
                --from;
                switch (c) {
                    case 0xf8: *to++ = 'o'; break;
                    case 0xf9: *to++ = 'u'; break;
                    case 0xfa: *to++ = 'u'; break;
                    case 0xfb: *to++ = 'u'; break;
                }
            } else {
                if ('\0' == *from++ || '\0' == *from++ || '\0' == *from++ || '\0' == *from++) 
                { *to = '\0'; return to; }
            }
        } else {
            switch (c) {
                case 0xfc: *to++ = 'u'; break;
                case 0xfd: *to++ = 'y'; break;
                case 0xfe: *to++ = 'p'; break;
                case 0xff: *to++ = 'y'; break;
            }
        }
    } return to;
}

char * mrutils::copyLatin1ToTerminal(char * to, const int sz, const char * from) {
    for (const char * const end = to + sz;to < end;) {
        const unsigned char c = (unsigned char)*from++;
        if (c == 0x00) { *to = '\0'; return to; }
        if (c <= 0x7e || c >= 0xa0) { *to++ = c; continue; }
        switch (c) {
            case 0x7f: *to++ = ' '; continue;
            case 0x80: *to++ = ' '; continue;
            case 0x81: *to++ = ' '; continue;
            case 0x82: *to++ = ','; continue;
            case 0x83: *to++ = 'f'; continue;
            case 0x84: *to++ = ','; continue;
            case 0x85: *to++ = '.'; continue;
            case 0x86: *to++ = 't'; continue;
            case 0x87: *to++ = 'T'; continue;
            case 0x88: *to++ = '^'; continue;
            case 0x89: *to++ = '#'; continue;
            case 0x8a: *to++ = 'S'; continue;
            case 0x8b: *to++ = '<'; continue;
            case 0x8c: *to++ = 'O'; continue;
            case 0x8d: *to++ = ' '; continue;
            case 0x8e: *to++ = ' '; continue;
            case 0x8f: *to++ = ' '; continue;
            case 0x90: *to++ = ' '; continue;
            case 0x91: *to++ = '\''; continue;
            case 0x92: *to++ = '\''; continue;
            case 0x93: *to++ = '"'; continue;
            case 0x94: *to++ = '"'; continue;
            case 0x95: *to++ = '*'; continue;
            case 0x96: *to++ = '-'; continue;
            case 0x97: *to++ = '-'; continue;
            case 0x98: *to++ = '~'; continue;
            case 0x99: *to++ = 'T'; continue;
            case 0x9a: *to++ = 's'; continue;
            case 0x9b: *to++ = '>'; continue;
            case 0x9c: *to++ = 'o'; continue;
            case 0x9d: *to++ = ' '; continue;
            case 0x9e: *to++ = ' '; continue;
            case 0x9f: *to++ = 'Y'; continue;
        }
    } return to;
}

int mrutils::atoiHex(const char * str) {
    int result = 0; bool neg;
    
    if (*str == '-') { neg = true; ++str; } 
    else neg = false;

    if (*str == '0' && *++str == 'x') ++str;

    for(;;++str) {
        switch (*str) {
            default:
                return (neg?-result:result);

            case '0': result = 0x10*result; break;
            case '1': result = 0x10*result + 1; break;
            case '2': result = 0x10*result + 2; break;
            case '3': result = 0x10*result + 3; break;
            case '4': result = 0x10*result + 4; break;
            case '5': result = 0x10*result + 5; break;
            case '6': result = 0x10*result + 6; break;
            case '7': result = 0x10*result + 7; break;
            case '8': result = 0x10*result + 8; break;
            case '9': result = 0x10*result + 9; break;
            case 'A': result = 0x10*result + 0xa; break;
            case 'a': result = 0x10*result + 0xa; break;
            case 'B': result = 0x10*result + 0xb; break;
            case 'b': result = 0x10*result + 0xb; break;
            case 'C': result = 0x10*result + 0xc; break;
            case 'c': result = 0x10*result + 0xc; break;
            case 'D': result = 0x10*result + 0xd; break;
            case 'd': result = 0x10*result + 0xd; break;
            case 'E': result = 0x10*result + 0xe; break;
            case 'e': result = 0x10*result + 0xe; break;
            case 'F': result = 0x10*result + 0xf; break;
            case 'f': result = 0x10*result + 0xf; break;
        }
    }

    // unreachable
    return 0;
}

int mrutils::atoi(const char * str) {
    int result = 0, exp = 0;
    bool neg, e = false;
    
    if (*str == '-') { neg = true; ++str; } 
    else neg = false;

    for(;;++str) {
        switch (*str) {
            case 'e':
                if (!e) {
                    e = true;
                    break;
                }
            default:
            case 0:
                if (e) {
                    // compute 10^exp quickly
                    switch (exp) {
                        case 0:
                            return ( (neg?-result:result) );
                        case 1:
                            return ( (neg?-result:result) * 10 );
                        case 2:
                            return ( (neg?-result:result) * 100 );
                        case 3:
                            return ( (neg?-result:result) * 1000 );
                        case 4:
                            return ( (neg?-result:result) * 10000 );
                        case 5:
                            return ( (neg?-result:result) * 100000 );
                        case 6:
                            return ( (neg?-result:result) * 1000000 );
                        case 7:
                            return ( (neg?-result:result) * 10000000 );
                        case 8:
                            return ( (neg?-result:result) * 100000000 );
                        case 9: // this is the max value
                            return ( (neg?-result:result) * 1000000000 );
                        default:
                            return (neg?INT_MIN:INT_MAX);
                            break;
                    }
                } else return (neg?-result:result);

            case '0':
                if (e) exp    = 10*exp;
                else   result = 10*result;
                break;
            case '1':
                if (e) exp    = 10*exp    + 1;
                else   result = 10*result + 1;
                break;
            case '2':
                if (e) exp    = 10*exp    + 2;
                else   result = 10*result + 2;
                break;
            case '3':
                if (e) exp    = 10*exp    + 3;
                else   result = 10*result + 3;
                break;
            case '4':
                if (e) exp    = 10*exp    + 4;
                else   result = 10*result + 4;
                break;
            case '5':
                if (e) exp    = 10*exp    + 5;
                else   result = 10*result + 5;
                break;
            case '6':
                if (e) exp    = 10*exp    + 6;
                else   result = 10*result + 6;
                break;
            case '7':
                if (e) exp    = 10*exp    + 7;
                else   result = 10*result + 7;
                break;
            case '8':
                if (e) exp    = 10*exp    + 8;
                else   result = 10*result + 8;
                break;
            case '9':
                if (e) exp    = 10*exp    + 9;
                else   result = 10*result + 9;
                break;
        }
    }

    // unreachable
    return 0;
}

char * mrutils::uitoa(unsigned int n, char * dst) {
    const int radix = 10;

    if (n == 0) {
        *dst++ = '0';
        *dst = '\0';
        return dst;
    } else {
        char * start = dst;
        do { *dst++ = n % radix + '0'; }
        while (n /= radix);

        char * end = dst;
        *dst-- = '\0';		// NULL-terminate then reverse

        while (start < dst) {
            const char temp = *start;
            *start++ = *dst;
            *dst-- = temp;
        }
        return end;
    }
}

char * mrutils::itoa(int n, char * dst) {
    const int radix = 10;
    if (n < 0) {
        *dst++ = '-';
        n = -n;
    }

    if (n == 0) {
        *dst++ = '0';
        *dst = '\0';
        return dst;
    } else {
        char * start = dst;
        do { *dst++ = n % radix + '0'; }
        while (n /= radix);

        char * end = dst;
        *dst-- = '\0';		// NULL-terminate then reverse

        while (start < dst) {
            const char temp = *start;
            *start++ = *dst;
            *dst-- = temp;
        }

        return end;
    }
}

char * mrutils::itoa(int n, char * dst, int size) {
    switch (size) {
        case 0:
            return dst;
        case 1:
            *dst = 0; return dst;
    }

    char * end = dst + size - 1;
    memset(dst,' ',size-1); *end = 0;

    if (n == 0) { *(end-1) = '0'; return end; } 
    else {
        bool neg; char * last;
        if (n < 0) { neg = true; n = -n; last = end-1;} 
        else { neg = false; last = end; }

        char * start = dst;
        do {
            if (dst == last) {
                memset(start,' ',dst-start); 
                return dst;
            }
            *dst++ = n % 10 + '0'; 
        } while (n /= 10);

        n = dst - start; dst = end-1;
        for (int i = n;i > 0 && start < dst;--i) {
            const char temp = *start;
            *start++ = *dst;
            *dst-- = temp;
        }

        if (neg) *(end-1-n) = '-';

        return end;
    }
}

char * mrutils::uitoa(unsigned n, char * dst, int size) {
    memset(dst,' ',size);
    char * end = dst + size - 1;

    const int radix = 10;

    if (n == 0) {
        *end-- = '\0';
        *end++ = '0';
        return end;
    } else {
        char * start = dst;
        do { *dst++ = n % radix + '0'; }
        while (n /= radix);

        n = dst - start;

        dst = end;
        *dst-- = '\0';		// NULL-terminate then reverse

        while (n > 0 && start < dst) {
            const char temp = *start;
            *start++ = *dst;
            *dst-- = temp;
            --n;
        }

        return end;
    }
}

char * mrutils::formatPath(char * path, bool * isWindows) {
    std::vector<char *> dirStack;
    char * start = path, *end = path;
    bool endSlash = true;
    bool outputWindows = false;
    
    while ((end = strdir(start)) != NULL) {
        if (*end == '\\') outputWindows = true;
        *end++ = '\0';
        if (start[0] != '\0') {
            if (start[0] == '.') {
                if (start[1] != '\0') {
                    if (start[1] == '.' && start[2] == '\0' 
                        && dirStack.size() > 0
                        && strcmp(dirStack[dirStack.size()-1],"..") != 0) {
                        dirStack.pop_back();
                    } else {
                        dirStack.push_back(start);
                    }
                } else {
                    if (dirStack.size() == 0) 
                        dirStack.push_back(start);
                }
            } else {
                dirStack.push_back(start);
            }
        }
        start = end;
    }

    if (start[0] != '\0') (endSlash=false, dirStack.push_back(start));

    start = path;
    if (path[0] == '\0') { 
        if (outputWindows) {
            path[0] = '\\';
            ++start; 

            if (path[1] == '\0') {
                path[1] = '\\';
                ++start;
            }
        } else {
            path[0] = '/'; 
            ++start; 
        }
    } 
    for (std::vector<char*>::iterator it = dirStack.begin();
         it != dirStack.end(); ++it) {
        end = *it;
        while (*end != '\0') *start++ = *end++;
        if (outputWindows) *start++ = '\\';
        else *start++ = '/';
    }

    if (dirStack.empty()) *start = '\0';
    else *(endSlash?start:--start) = '\0';

    if (isWindows != NULL) *isWindows = outputWindows;

    return start;
}

