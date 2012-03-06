#include <iostream>
#include <stdio.h>
#include <map>

#include "mr_xml.h"
#include "mr_numerical.h"
#include "mr_strutils.h"
#include "mr_curl.h"
#include "mr_files.h"

#include "mr_threads.h"

namespace {
    // is the "second" value in map; contains second string & length of first
    struct xmlCharData { const char * str; int firstLen, secondLen; };

   /**
    * Case sensitive comparison, treats ';' like '\0'
    */
    struct caseCompSemicolon {
        inline bool operator() (const char * first, const char * second) const { 
            int c1, c2;
            do {
                c1 = (unsigned char) *first++;
                c2 = (unsigned char) *second++;
            } while (c1 == c2 && c1 != 0 && c1 != ';');
            return (c1 - c2 < 0);
        }
    };
    static std::map<const char *,xmlCharData,caseCompSemicolon> xmlCharMap;

    bool init();
    _UNUSED static bool init_ = init();

    inline void copyHelper(char *& dest, long &size, std::string * str, char * start, int nchars) {
        if (dest == NULL) {
            // resize to accomodate full amount (including \0) 
            // then write, then resize back to actual # chars
            const int orig = str->size();
            ++nchars;
            str->resize(orig + nchars);

            char * ptr = const_cast<char*>(str->c_str());
            str->resize(mrutils::copyToAscii(ptr+orig,nchars,start) - ptr);
        } else {
            const char * orig = dest;
            dest = mrutils::copyToAscii(dest,size,start);
            size -= (dest - orig);
        }
    }

    void parseCopy(char *& dest, long &size, std::string * str, char * start, char * end) {
        char endChar = *end;
        *end = 0;

        if (str != NULL) str->reserve(end - start);

        typedef std::map<const char *,xmlCharData,caseCompSemicolon> mapT;

        for (;start != end;) {
            char * p = strpbrk(start,"<&");

            if (p == NULL) {
                copyHelper(dest,size,str,start,end-start);
                break;
            } else {
                const char tmp = *p; *p = 0;
                copyHelper(dest,size,str,start,p-start);
                *p = tmp;

                if (tmp == '<') {
                    start = strchr(p,'>');
                    if (start == NULL) break;
                    ++start;
                } else {
                    if (0==strncmp(p,"&lt;",4) && NULL != (start = strstr(p,"&gt;"))) {
                        start += 4;
                    } else {
                        for (;;) {
                            start = p;

                            mapT::iterator it = xmlCharMap.find(p+1);
                            if (it == xmlCharMap.end()) {
                                if (p[1] == '#') {
                                    char * q = p + 2; for (;*q >= '0' && *q <= '9';++q) ;
                                    if (*q == ';') {
                                        start = q+1;
                                        break;
                                    }
                                }
                                if (dest == NULL) *str += '&';
                                else if (size >= 1) { *dest++ = '&'; --size; }
                                ++start;
                                break;
                            } else {
                                if (it->second.str[0] == '&' && it->second.str[1] == '\0') {
                                    p += it->second.firstLen;
                                    continue;
                                }
                                if (dest == NULL) str->append(it->second.str);
                                else {
                                    long len = std::min((long)it->second.secondLen,size);
                                    memcpy(dest,it->second.str,len);
                                    dest += len; size -= len;
                                }
                                start += it->second.firstLen + 1;
                                break;
                            }
                        }
                    }
                }
            }
        }

        *end = endChar;
    }

    void litCopy(char *& dest, long &size, std::string * str, char * start, char * end) {
        char endChar = *end;
        *end = 0;

        if (str != NULL) str->reserve(end - start);

        for (;start != end;) {
            char * p = strchr(start,'<');

            if (p == NULL) {
                copyHelper(dest,size,str,start,end-start);
                break;
            } else {
                *p = '\0'; copyHelper(dest,size,str,start,p-start); *p = '<';

                start = strchr(p,'>');
                if (start == NULL) break;
                ++start;
            }
        }

        *end = endChar;
    }
}

namespace {

    std::string getDomain(std::string const &url)
    {
        char const * start = strstr(url.c_str(),"//");
        if (!start)
            start = url.c_str(); 

        start += 2;
        char const * end = strchr(start,'/');
        if (!end)
            end = start + strlen(start);

        return url.substr(0, end-url.c_str());
    }

/**
 * Translate a link (which may be relative or absolute) to an
 * absolute link.
 */
std::string getLink(std::string const &pageURL, std::string const &link)
{
    if (link.empty())
        return pageURL;

    if (link[0] == '/')
    {
        return getDomain(pageURL) + link;
    } else 
    {
        return link;
    }
}

}

/**
 * strategy: search forward for first <xxx (not </xxx) that doesn't end in />
 * set tagPtr to <xxx and return xxx
 */
char * mrutils::XML::nextTag()
{
	tagText[0] = '\0';

	if (tagPtr == eob)
		return tagText;

	if (*tagPtr == '<')
	{
		if (tagPtr+1 == eob)
			return tagText;

		++tagPtr;
	}

	for (;;++tagPtr)
	{
		tagPtr = strchr(tagPtr,'<');

		if (tagPtr == NULL || tagPtr+1 == eob)
		{
			tagPtr = eob;
			return tagText;
		}

		if (*(tagPtr+1) == '/')
			continue;

		char * p = strchr(tagPtr,'>');

		if (p == NULL)
		{
			tagPtr = eob;
			return tagText;
		}

		if (*(p-1) == '/')
			continue;

		*strncpy(tagText,tagPtr+1,std::min((size_t)127,
			strcspn(tagPtr+1,"> "))) = '\0';

		return tagText;
	}
}

bool mrutils::XML::next(char const *tagName)
{
	if (tagPtr == eob || tagPtr+1 == eob)
	{
		tagPtr = eob;
		return false;
	}

	char start[128];
	*start = '<';
	*mrutils::strncpy(start+1,tagName,126) = 0;

	for (;;++tagPtr)
	{
		tagPtr = mrutils::stristr(tagPtr,start);

		if (tagPtr == NULL)
		{
			tagPtr = eob;
			return false;
		}

		if (*(tagPtr+1) == '/')
			continue;

		char * p = strchr(tagPtr,'>');

		if (p == NULL)
			return false;

		if (*(p-1) == '/')
			continue;

		return true;
	}
}

void mrutils::XML::readFile(char const *path)
{
    int fd = open(path,O_RDONLY);
    if (fd > 0)
    {
        char const *EOB = buffer + buffSz - 1;
        for (int r; eob < EOB && 0 < (r = read(fd, eob, EOB-eob));)
        { eob += r; }
        *eob = '\0';
        close(fd);
    }
}

int mrutils::XML::get(mrutils::curl::urlRequest_t *request,
        int stopFD, bool checkRedir)
{
    m_url = request->url;

    mrutils::curl::curlData_t data;
    data.buffSz = buffSz;
    data.buffer = buffer;
    data.eob = buffer;
    data.errBuffer = NULL;

    // DEBUG
    //std::cerr << "XML getting url: " << request->url << std::endl;

    if (int ret = mrutils::curl::getURL(request,data,stopFD))
        return ret;

    // check for javascript redirects
    if (checkRedir) {
        if (char *p = strstr(buffer,"meta http-equiv=\"refresh\"")) {
            if (NULL != (p = strcasestr(p,"url="))) {
                p += strlen("url=");
                *strchr(p,'"') = '\0';
                std::string newLink = getLink(m_url,p);
                if (0!=strcasecmp(newLink.c_str(), m_url.c_str())) {
                    mrutils::curl::urlRequest_t requestNew(*request);
                    requestNew.url = const_cast<char*>(newLink.c_str());
                    return get(&requestNew, stopFD, false);
                }
            }
        }
    }

    m_url = request->url;

    sob = tagPtr = buffer; eob = data.eob;
    return 0;
}
char * mrutils::XML::unescapeHTML(char * start, char * end) {
    long size = end - start;
    char * dest = start;
    parseCopy(dest,size,NULL,start,end);
    return dest;
}

const char * mrutils::XML::tagInternal(char * dest, int size_, std::string* str)
{
    if (str == NULL && size_ == 0)
		return dest;

    long size = (long)size_;

    if (tagPtr == eob) return "";

    // put </tagname> into endTag
    char endTag[128] = "</";
    int chars = strcspn(tagPtr+1,"> ")+3;
    if (chars > 127) return "";
    strcpy(mrutils::strncpy(endTag+2,tagPtr+1,chars-3),">");

    // find the start & end 
    char * start = strchr(tagPtr,'>');
    if (start == NULL) return ""; else ++start;
    char * end = strstr(start,endTag);
    if (end == NULL) return "";

    // make this substitution so strchr and strstr stop at end
    char endChar = *end; *end = 0;

    // used to denote literal (unparsed) segments
    static const char * litStart = "<![CDATA[";
    static const size_t litStartSz = strlen(litStart);
    static const char * litEnd = "]]>";
    static const size_t litEndSz = strlen(litEnd);

    for (;start != end;)
	{
        char * lit = strstr(start,litStart);

        if (lit == NULL)
		{
            parseCopy(dest,size,str,start,end);
            break;
        }
		else
		{
            parseCopy(dest,size,str,start,lit);

            char * elit = strstr(lit, litEnd);

            if (elit == NULL)
			{
                //litCopy(dest,size,str,lit+litStartSz,end);
                parseCopy(dest,size,str,lit+litStartSz,end);
                break;
            }
			else
			{
                //litCopy(dest,size,str,lit+litStartSz,elit);
                parseCopy(dest,size,str,lit+litStartSz,elit);
                start = elit + litEndSz;
            }
        }
    }

    if (dest != NULL) { if (size > 0) *dest = 0; else *(dest-1) = 0; }

    *end = endChar;
    return (dest == NULL ? str->c_str() : dest);
}

namespace {
    inline void add(const int num, const char * sym, const char * repl) {
        static char buffer[16];
        static xmlCharData d;

        d.str = repl;
        d.secondLen = strlen(repl);

        xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#%d;",num))),buffer)] = d;
        xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#x%x;",num))),buffer)] = d;
        xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#x%X;",num))),buffer)] = d;
        if (sym) xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"%s;",sym))),buffer)] = d;

        if (num < 100) {
            xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#%03d;",num))),buffer)] = d;
            xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#x%03x;",num))),buffer)] = d;
            xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#x%03X;",num))),buffer)] = d;
        }

        if (num < 10) {
            xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#%02d;",num))),buffer)] = d;
            xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#x%02x;",num))),buffer)] = d;
            xmlCharMap[::strcpy((char*)malloc(1+(d.firstLen = sprintf(buffer,"#x%02X;",num))),buffer)] = d;
        }
    }

    bool init() {
        add(9   ,NULL,"\t");
        add(10  ,NULL,"\r");
        add(13  ,NULL,"\n");
        add(32  ,NULL," ");
        add(33  ,NULL,"!");
        add(34  ,"quot","\"");
        add(35  ,NULL,"#");
        add(36  ,NULL,"$");
        add(37  ,NULL,"%");
        add(38  ,"amp","&");
        add(39  ,"apos","'");
        add(40  ,NULL,"(");
        add(41  ,NULL,")");
        add(42  ,NULL,"*");
        add(43  ,NULL,"+");
        add(44  ,NULL,",");
        add(45  ,NULL,"-");
        add(46  ,NULL,".");
        add(47  ,NULL,"/");
        add(48  ,NULL,"0");
        add(49  ,NULL,"1");
        add(50  ,NULL,"2");
        add(51  ,NULL,"3");
        add(52  ,NULL,"4");
        add(53  ,NULL,"5");
        add(54  ,NULL,"6");
        add(55  ,NULL,"7");
        add(56  ,NULL,"8");
        add(57  ,NULL,"9");
        add(58  ,NULL,":");
        add(59  ,NULL,";");
        add(60  ,"lt","<");
        add(61  ,NULL,"=");
        add(62  ,"gt",">");
        add(63  ,NULL,"?");
        add(64  ,NULL,"@");
        add(65  ,NULL,"A");
        add(66  ,NULL,"B");
        add(67  ,NULL,"C");
        add(68  ,NULL,"D");
        add(69  ,NULL,"E");
        add(70  ,NULL,"F");
        add(71  ,NULL,"G");
        add(72  ,NULL,"H");
        add(73  ,NULL,"I");
        add(74  ,NULL,"J");
        add(75  ,NULL,"K");
        add(76  ,NULL,"L");
        add(77  ,NULL,"M");
        add(78  ,NULL,"N");
        add(79  ,NULL,"O");
        add(80  ,NULL,"P");
        add(81  ,NULL,"Q");
        add(82  ,NULL,"R");
        add(83  ,NULL,"S");
        add(84  ,NULL,"T");
        add(85  ,NULL,"U");
        add(86  ,NULL,"V");
        add(87  ,NULL,"W");
        add(88  ,NULL,"X");
        add(89  ,NULL,"Y");
        add(90  ,NULL,"Z");
        add(91  ,NULL,"[");
        add(92  ,NULL,"\\");
        add(93  ,NULL,"]");
        add(94  ,NULL,"^");
        add(95  ,NULL,"_");
        add(96  ,NULL,"`");
        add(97  ,NULL,"a");
        add(98  ,NULL,"b");
        add(99  ,NULL,"c");
        add(100 ,NULL,"d");
        add(101 ,NULL,"e");
        add(102 ,NULL,"f");
        add(103 ,NULL,"g");
        add(104 ,NULL,"h");
        add(105 ,NULL,"i");
        add(106 ,NULL,"j");
        add(107 ,NULL,"k");
        add(108 ,NULL,"l");
        add(109 ,NULL,"m");
        add(110 ,NULL,"n");
        add(111 ,NULL,"o");
        add(112 ,NULL,"p");
        add(113 ,NULL,"q");
        add(114 ,NULL,"r");
        add(115 ,NULL,"s");
        add(116 ,NULL,"t");
        add(117 ,NULL,"u");
        add(118 ,NULL,"v");
        add(119 ,NULL,"w");
        add(120 ,NULL,"x");
        add(121 ,NULL,"y");
        add(122 ,NULL,"z");
        add(123 ,NULL,"{");
        add(124 ,NULL,"|");
        add(125 ,NULL,"}");
        add(126 ,NULL,"~");
        add(127 ,NULL,"");
        add(128 ,NULL,"EUR");
        add(129 ,NULL,"");
        add(130 ,NULL,",");
        add(131 ,NULL,"f");
        add(132 ,NULL,",,");
        add(133 ,NULL,"...");
        add(134 ,NULL,"(T)");
        add(135 ,NULL,"(TT)");
        add(136 ,NULL,"^");
        add(137 ,NULL,"0/00");
        add(138 ,NULL,"S");
        add(139 ,NULL,"<");
        add(140 ,NULL,"CE");
        add(141 ,NULL,"");
        add(142 ,NULL,"Z");
        add(143 ,NULL,"");
        add(144 ,NULL,"");
        add(145 ,NULL,"`");
        add(146 ,NULL,"'");
        add(147 ,NULL,"\"");
        add(148 ,NULL,"\"");
        add(149 ,NULL,"*");
        add(150 ,NULL,"-");
        add(151 ,NULL,"--");
        add(152 ,NULL,"~");
        add(153 ,NULL,"(TM)");
        add(154 ,NULL,"s");
        add(155 ,NULL,">");
        add(156 ,NULL,"oe");
        add(157 ,NULL,"");
        add(158 ,NULL,"z");
        add(159 ,NULL,"Y");
        add(160 ,"nbsp"," ");
        add(161 ,"iexcl","i");
        add(162 ,"cent","c");
        add(163 ,"pound","GBP");
        add(164 ,"curren","o");
        add(165 ,"yen","JPY");
        add(166 ,"brvbar","|");
        add(167 ,"sect","S");
        add(168 ,"uml","");
        add(169 ,"copy","(c)");
        add(170 ,"ordf","a");
        add(171 ,"laquo","<<");
        add(172 ,"not","NOT");
        add(173 ,"shy","");
        add(174 ,"reg","(r)");
        add(175 ,"macr","-");
        add(176 ,"deg"," ");
        add(177 ,"plusmn","+-");
        add(178 ,"sup2","2");
        add(179 ,"sup3","3");
        add(180 ,"acute","'");
        add(181 ,"micro","u");
        add(182 ,"para","P");
        add(183 ,"middot","*");
        add(184 ,"cedil",",");
        add(185 ,"sup1","1");
        add(186 ,"ordm","");
        add(187 ,"raquo",">>");
        add(188 ,"frac14","1/4");
        add(189 ,"frac12","1/2");
        add(190 ,"frac34","3/4");
        add(191 ,"iquest","?");
        add(192 ,"Agrave","A");
        add(193 ,"Aacute","A");
        add(194 ,"Acirc","A");
        add(195 ,"Atilde","A");
        add(196 ,"Auml","A");
        add(197 ,"Aring","A");
        add(198 ,"AElig","AE");
        add(199 ,"Ccedil","C");
        add(200 ,"Egrave","E");
        add(201 ,"Eacute","E");
        add(202 ,"Ecirc","E");
        add(203 ,"Euml","E");
        add(204 ,"lgrave","l");
        add(205 ,"lacute","l");
        add(206 ,"lcirc","l");
        add(207 ,"luml","l");
        add(208 ,"ETH","D");
        add(209 ,"Ntilde","N");
        add(210 ,"Ograve","O");
        add(211 ,"Oacute","O");
        add(212 ,"Ocirc" ,"O");
        add(213 ,"Otilde","O");
        add(214 ,"Ouml"  ,"O");
        add(215 ,"times" ,"x");
        add(216 ,"Oslash","0");
        add(217 ,"Ugrave","U");
        add(218 ,"Uacute","U");
        add(219 ,"Ucirc" ,"U");
        add(220 ,"Uuml"  ,"U");
        add(221 ,"Yacute","Y");
        add(222 ,"THORN" ,"p");
        add(223 ,"szlig" ,"B");
        add(224 ,"agrave","a");
        add(225 ,"aacute","a");
        add(226 ,"acirc" ,"a");
        add(227 ,"atilde","a");
        add(228 ,"auml"  ,"a");
        add(229 ,"aring" ,"a");
        add(230 ,"aelig","ae");
        add(231 ,"ccedil","c");
        add(232 ,"egrave","e");
        add(233 ,"eacute","e");
        add(234 ,"ecirc" ,"e");
        add(235 ,"euml"  ,"e");
        add(236 ,"igrave","i");
        add(237 ,"iacute","i");
        add(238 ,"icirc" ,"i");
        add(239 ,"iuml"  ,"i");
        add(240 ,"eth"   ,"d");
        add(241 ,"ntilde","n");
        add(242 ,"ograve","o");
        add(243 ,"oacute","o");
        add(244 ,"ocirc" ,"o");
        add(245 ,"otilde","o");
        add(246 ,"ouml"  ,"o");
        add(247 ,"divide","/");
        add(248 ,"oslash","o");
        add(249 ,"ugrave","u");
        add(250 ,"uacute","u");
        add(251 ,"ucirc" ,"u");
        add(252 ,"uuml"  ,"u");
        add(253 ,"yacute","y");
        add(254 ,"thorn" ,"p");
        add(255 ,"yuml"  ,"y");
        add(338 ,"OElig","OE");
        add(339 ,"oelig","oe");
        add(352 ,"Scaron","S");
        add(353 ,"scaron","s");
        add(376 ,"Yuml"  ,"Y");
        add(402 ,"fnof"  ,"f");
        add(8211,"ndash","--");
        add(8212,"mdash","---");
        add(8216,"lsquo","`");
        add(8217,"rsquo","'");
        add(8218,"sbquo",",");
        add(8220,"ldquo","``");
        add(8221,"rdquo","''");
        add(8222,"bdquo",",,");
        add(8224,"dagger","(T)");
        add(8225,"Dagger","(TT)");
        add(8226,NULL,"*");
        add(8230,"helip","...");
        add(8240,"permil","0/100");
        add(8242,"prime","'");
        add(8243,"Prime","''");
        add(8249,"lsaquo","<");
        add(8250,"rsaquo",">");
        add(8364,"euro"  ,"EUR");
        add(8482,"trade" ,"TM");
        add(8629,"crarr" ,"");
        add(8722,"minus" ,"-");
        add(8727,"lowast","*");
        add(8730,"radic" ,"SQRT ");
        add(8733,"prop"  ,"PROP ");
        add(8734,"infin" ,"infty");
        add(8736,"ang"   ,"");
        add(8747,"int"   ,"INT");
        add(8756,"there4","==>");
        add(8764,"sim"   ,"~");
        add(8773,"cong"  ,"~=");
        add(8776,"asymp" ,"~~");
        add(8800,"ne"    ,"!=");
        add(8801,"equiv" ,"=");
        add(8804,"le"    ,"<=");
        add(8805,"ge"    ,">=");
        add(8901,"sdot"  ,"*");
        add(8901,"sdot"  ,"*");
        return true;
    }
}
