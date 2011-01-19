#include "mr_xmlbuilder.h"
#include <stack>

namespace {
    static const unsigned tabWidth = 4;
}

namespace mrutils {
    namespace xml {
        ContainerNode::nodeStrCmp_t ContainerNode::nodeStrCmp;

        void GenericNode::print(std::ostream& os) const {
            printHeader(os); os << "\n";

            std::stack<std::vector<GenericNode*>::const_iterator> stack;
            const GenericNode * node = this;

            unsigned width = 0;

            for (std::vector<GenericNode*>::const_iterator it = nodes.begin()
                ;;++it) {

                if (it == node->nodes.end()) {
                    oprintf(os,"%*s</%s>\n",width = tabWidth * stack.size(),"",node->name);
                    if (stack.empty()) break;
                    it = stack.top(); stack.pop();
                    node = node->parent; continue;
                }

                os.width(tabWidth * (stack.size()+1)); os << "";
                (*it)->printHeader(os);

                for (;!(*it)->nodes.empty();) {
                    stack.push(it);
                    node = *it;
                    it = node->nodes.begin();
                    (os << "\n").width(tabWidth * (stack.size()+1)); os << "";
                    (*it)->printHeader(os);
                }

                (*it)->printContent(os);
                oprintf(os,"</%s>\n",(*it)->name);
            }
        }

        void Document::read(mrutils::BufferedReader& reader) throw (std::runtime_error) {
            clear();

            std::stack<ContainerNode*> nodes;

            std::string name; mrutils::stringstream content;
            mrutils::map<std::string,std::string> properties;

            bool inContent = false;
            char buffer[1024];

            if (reader.type == mrutils::BufferedReader::FT_NONE)
                throw std::runtime_error("xml::Document call to read on unopen file");

            for (unsigned lineNumber = 1; reader.nextLine(); ++lineNumber) {
                char * p = reader.line; for (;*p == ' ' || *p == '\t';++p){}

                if (inContent) {
                    if (*p != '<' || 0==strncmp(p,buffer,strlen(buffer))) {
                        char * q = strstr(p,buffer);
                        if (q == NULL) { content << reader.line << "\n"; continue; }
                        *q = 0; content << reader.line;

                        ContentNode<std::string>& node = nodes.top()->node(name.c_str(),content.c_str());
                        node.properties = properties;
                        properties.clear(); content.clear();
                        inContent = false; continue;
                    } else {
                        if (nodes.empty()) {
                            this->name = name;
                            this->properties = properties;
                            nodes.push(this);
                        } else {
                            ContainerNode& node = nodes.top()->node(name.c_str());
                            node.properties = properties;
                            nodes.push(&node);
                        }

                        properties.clear(); content.clear(); 
                        inContent = false; 
                    }
                }

                if (*p != '<') continue;

                switch (*++p) {
                    case '?': continue;
                    case '/': 
                        nodes.pop();
                        if (nodes.empty()) return;
                        continue;
                    default: break;
                }

                reader.line = p;
                char * q = strchr(p,'>'); 
                if (q == NULL) throw std::runtime_error((content.clear()
                        << "xml::Document start of tag and no terminating > on line " << lineNumber).c_str());
                *q = 0; 

                const bool oneLine = *(q-1) == '/';

                // search for properties
                for (;*p != 0; p = q) {
                    q = strchr(p,' '); if (q == NULL) { q = p + strlen(p); break; }
                    *q = 0; for (++q;*q==' ';++q){}
                    p = q; for (++q; *q != '=' && *q != ' ';++q){}

                    // store the property name in buffer
                    size_t nameLen = q-p;
                    memcpy(buffer,p,nameLen); buffer[nameLen] = 0;

                    if (*q != '=') {
                        for (++q;*q == ' ';++q){}
                        if (*q != '=') throw std::runtime_error((content.clear()
                            << "xml::Document badly formatted property on line " << lineNumber).c_str());
                    }
                    for (++q; *q == ' '; ++q){}
                    if (*q == '"') {
                        p = ++q;
                        q = strchr(p,'"'); if (q == NULL) throw std::runtime_error((content.clear()
                            << "xml::Document badly formatted property on line " << lineNumber).c_str());
                    } else {
                        p = q; for (++q; *q != 0 && *q != ' ';++q){}
                    }

                    // value is from p to q
                    memcpy(buffer+nameLen+1,p,q-p); buffer[nameLen+1+q-p] = 0;
                    properties[buffer] = buffer+nameLen+1;
                }

                p = ++q;

                if (oneLine || (sprintf(buffer,"</%s>",reader.line),q = strstr(p,buffer)) != NULL) {
                    if (oneLine) {
                        // if the name ends in /, then fix it
                        size_t len = strlen(reader.line);
                        if (len > 0 && reader.line[len-1] == '/') reader.line[len-1] = 0;
                    }

                    // then this is a one-liner content node
                    if (nodes.empty()) {
                        this->name.assign(reader.line);
                        this->properties = properties;
                        return;
                    }

                    *q = 0;
                    ContentNode<std::string>& node = nodes.top()->node(reader.line,p);
                    node.properties = properties;
                    properties.clear(); content.clear();
                } else {
                    name.assign(reader.line);
                    content << p << "\n";
                    inContent = true;
                }
            }
        }
    }
}
