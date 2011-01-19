#ifndef _MR_CPPLIB_PARAMS_H
#define _MR_CPPLIB_PARAMS_H

#include "mr_exports.h"
#include <iostream>
#include <map>
#include <set>
#include <string>
#include "mr_delim.h"
#include "mr_strutils.h"
#include "mr_numerical.h"

namespace mrutils {

/**
 * Overrides to parameters can be specified in the argv as 
 * -prefix_name=value
 * */
class Params {
    typedef std::map<std::string,double> Doubles;
    typedef std::map<std::string,int> Ints;
    typedef std::map<std::string,unsigned> Unsigneds;
    typedef std::map<std::string,std::string> Strings;
    typedef std::map<std::string,bool> Bools;
    typedef std::set<std::string> Overrides;

    public:
        Params() {}
        
        Params(const char * file, const char * prefix="", int argc = 0, const char * argv[] = NULL) { 
            init(file, prefix,argc,argv);
        }

        void init(const char * file, const char * prefix="", int argc = 0, const char * argv[] = NULL) {
            path.assign(file);

            if (file == NULL || file[0] == '\0') return;

            mrutils::DelimitedFileReader f(file);
            f.setFixedWidth();

            while (f.nextLine()) {
                switch (*f.getString("Type")) {
                    case 'b':
                        bools.insert(std::pair<std::string,bool>(f.getString("Name"),f.getBool("Value")));
                        break;
                    case 's':
                        strings.insert(std::pair<std::string,std::string>(f.getString("Name"),f.getString("Value")));
                        break;
                    case 'i':
                        ints.insert(std::pair<std::string,int>(f.getString("Name"),f.getInt("Value")));
                        // no break -- ints also added as doubles
                    case 'd':
                        doubles.insert(std::pair<std::string,double>(f.getString("Name"),f.getDouble("Value")));
                        break;
                    case 'u':
                        unsigneds.insert(std::pair<std::string,unsigned>(f.getString("Name"),f.getUnsigned("Value")));
                        break;
                }
            }

            // read overrides from argc, argv
            if (argc > 0 && prefix != NULL && *prefix != 0) {
                std::string tmp;
                int prefixSize = strlen(prefix);
                for (int a = 0; a < argc; ++a) {
                    const char * p = &argv[a][0];
                    if (*p++ != '-') continue;
                    if (!mrutils::startsWith(p, prefix)) continue;

                    p += prefixSize + 1;
                    const char * q = strchr(p,'=');
                    if (q == NULL) continue;
                    tmp.assign(p,q++ - p);
                    overrides.insert(tmp);
                    if (!reset(tmp.c_str(), q)) {
                        std::cout << "params " << prefix << ": adding prev unknown parameter " << tmp << " as string." << std::endl;
                        set(tmp.c_str(),q);
                    } else {
                        std::cout << "params " << prefix << ": reset parameter " << tmp << " to " << q << std::endl;
                    }
                }
            }
        }
    public:
        std::string path;

        inline void dumpOverrides(std::ostream& os, const char * prefix = "") {
            for (Overrides::iterator it = overrides.begin(); 
                 it != overrides.end(); ++it) {
                os << prefix << "OVERRIDE: " << *it << " = ";
                print(os, it->c_str());
                os << std::endl;
            }
            if (!overrides.empty()) os << prefix << "\n";
        }

        template<class T>
        inline void override(const char * name, const T& value) {
            overrides.insert((mrutils::stringstream().clear() << name << "=" << value << "\n").str());

            unsigned found = 0;

            { Ints::const_iterator it = ints.find(name);
              if (it != ints.end()) { ++found; ints[name] = value; } }

            { Doubles::const_iterator it = doubles.find(name);
              if (it != doubles.end()) { ++found; doubles[name] = value; } }

            { Unsigneds::const_iterator it = unsigneds.find(name);
              if (it != unsigneds.end()) { ++found; unsigneds[name] = value; } }

            { Bools::const_iterator it = bools.find(name);
              if (it != bools.end()) { ++found; bools[name] = value; } }

            if (found == 0) {
                ints[name] = value;
                doubles[name] = value;
                unsigneds[name] = value;
                bools[name] = value;
            }
        }

        inline void override(const char * name, const char * value) {
            overrides.insert((mrutils::stringstream().clear() << name << "=" << value << "\n").str());
            strings[name] = value;
        }


    public:
       /***********************
        * By reference getters
        ***********************/

        inline bool get(const char * name, std::string& str) const {
            Strings::const_iterator it = strings.find(name);
            if (it == strings.end()) return false;
            str = it->second; return true;
        }

        inline bool get(const char * name, const char *& str) const {
            Strings::const_iterator it = strings.find(name);
            if (it == strings.end()) return false;
            str = it->second.c_str(); return true;
        }

        inline bool get(const char * name, int& i) const {
            Ints::const_iterator it = ints.find(name);
            if (it == ints.end()) return false;
            i = it->second; return true;
        }
        inline bool get(const char * name, unsigned& i) const {
            Unsigneds::const_iterator it = unsigneds.find(name);
            if (it == unsigneds.end()) return false;
            i = it->second; return true;
        }
        inline bool get(const char * name, double& d) {
            Doubles::const_iterator it = doubles.find(name);
            if (it == doubles.end()) return false;
            d = it->second; return true;
        }
        inline bool get(const char * name, bool& b) {
            Bools::const_iterator it = bools.find(name);
            if (it == bools.end()) return false;
            b = it->second; return true;
        }

        inline bool reset(const char * name, const char * value) {
            if (ints.end() != ints.find(name)) {
                int v = mrutils::atoi(value);
                ints[name] = v; doubles[name] = v;
                return true;
            } else if (doubles.end() != doubles.find(name)) {
                doubles[name] = atof(value);
                return true;
            } else if (unsigneds.end() != unsigneds.find(name)) {
                unsigneds[name] = atoul(value);
                return true;
            } else if (bools.end() != bools.find(name)) {
                if (0!=isdigit(value[0])) bools[name] = 0!=mrutils::atoi(value);
                else bools[name] = (::tolower(value[0]) == 't');
                return true;
            } else if (strings.end() != strings.find(name)) {
                strings[name] = value;
                return true;
            }

            return false;
        }

        inline void set(const char * name, const char * value) {
            strings[name] = value;
        }
        inline void set(const char * name, int value) {
            ints[name] = value;
            doubles[name] = value;
        }
        inline void set(const char * name, unsigned value) {
            unsigneds[name] = value;
        }
        inline void set(const char * name, double value) {
            doubles[name] = value;
        }
        inline void set(const char * name, bool value) {
            bools[name] = value;
        }

        inline bool print(std::ostream& os, const char * name) { 
            if (ints.end() != ints.find(name)) {
                os << ints[name];
                return true;
            } else if (doubles.end() != doubles.find(name)) {
                os << doubles[name];
                return true;
            } else if (unsigneds.end() != unsigneds.find(name)) {
                os << unsigneds[name];
                return true;
            } else if (bools.end() != bools.find(name)) {
                if (bools[name]) os << "true";
                else os << "false";
                return true;
            } else if (strings.end() != strings.find(name)) {
                os << strings[name];
                return true;
            }

            return false;
        }

    public:
       /***********************
        * Inline getters
        ***********************/

        inline bool hasParam(const char *name) const { 
            if (doubles.find(name) != doubles.end()) return true;
            if (unsigneds.find(name) != unsigneds.end()) return true;
            if (strings.find(name) != strings.end()) return true;
            if (bools.find(name)   != bools.end()) return true;
            return false;
        }

        inline double getDouble(const char * name) const throw (mrutils::ExceptionNoSuchColumn) {
            Doubles::const_iterator it = doubles.find(name);
            if (it == doubles.end()) {
                std::stringstream ss;
                ss << "No such column '" << name << "' in file " << path;
                throw mrutils::ExceptionNoSuchColumn(ss);
            }
            return it->second;
        }
        inline bool getBool(const char * name) const throw (mrutils::ExceptionNoSuchColumn) {
            Bools::const_iterator it = bools.find(name);
            if (it == bools.end()) {
                std::stringstream ss;
                ss << "No such column '" << name << "' in file " << path;
                throw mrutils::ExceptionNoSuchColumn(ss);
            }
            return it->second;
        }
        inline int getInt(const char * name) const throw (mrutils::ExceptionNoSuchColumn) {
            Ints::const_iterator it = ints.find(name);
            if (it == ints.end()) {
                std::stringstream ss;
                ss << "No such column '" << name << "' in file " << path;
                throw mrutils::ExceptionNoSuchColumn(ss);
            }
            return it->second;
        }
        inline unsigned getUnsigned(const char * name) const throw (mrutils::ExceptionNoSuchColumn) {
            Unsigneds::const_iterator it = unsigneds.find(name);
            if (it == unsigneds.end()) {
                std::stringstream ss;
                ss << "No such column '" << name << "' in file " << path;
                throw mrutils::ExceptionNoSuchColumn(ss);
            }
            return it->second;
        }
        inline const char * getString(const char * name) const throw (mrutils::ExceptionNoSuchColumn) {
            Strings::const_iterator it = strings.find(name);
            if (it == strings.end()) {
                std::stringstream ss;
                ss << "No such column '" << name << "' in file " << path;
                throw mrutils::ExceptionNoSuchColumn(ss);
            }
            return it->second.c_str();
        }

    public:
       /***********************
        * Inline getters with defaults
        ***********************/

        inline double getDouble(const char * name, double def) const {
            Doubles::const_iterator it = doubles.find(name);
            if (it == doubles.end()) return def;
            return it->second;
        }
        inline bool getBool(const char * name, bool def) const {
            Bools::const_iterator it = bools.find(name);
            if (it == bools.end()) return def;
            return it->second;
        }
        inline int getInt(const char * name, int def) const {
            Ints::const_iterator it = ints.find(name);
            if (it == ints.end()) return def;
            return it->second;
        }
        inline unsigned getUnsigned(const char * name, int def) const {
            Unsigneds::const_iterator it = unsigneds.find(name);
            if (it == unsigneds.end()) return def;
            return it->second;
        }
        inline const char * getString(const char * name, const char * def) const {
            Strings::const_iterator it = strings.find(name);
            if (it == strings.end()) return def;
            return it->second.c_str();
        }

    private:
        Doubles doubles;
        Ints ints;
        Unsigneds unsigneds;
        Strings strings;
        Bools bools;
        Overrides overrides;
};
}


#endif
