#ifndef _MR_CPPLIB_XML_BUILDER_H
#define _MR_CPPLIB_XML_BUILDER_H

#include <iostream>
#include <sstream>
#include <vector>
#include "mr_map.h"
#include "mr_strutils.h"
#include "mr_bufferedreader.h"
#include <stdexcept>

/**
 * XML
 * ===
 * Note that this makes highly simplifying assumptions about the structure of the xml document. 
 * (1) each node starts on a separate line
 * (2) each node is either a container node or a content node 
 *     and cannot contain both content and other nodes
 */
namespace mrutils {
    
    namespace xml {

        enum NodeType {
            NODE_Container
           ,NODE_Content
        };

        class GenericNode {
            protected:
                GenericNode(const char * name, GenericNode * const parent, NodeType type)
                : name(name), parent(parent), nodeType(type)
                {}

            public:
                ~GenericNode() {
                    for (std::vector<GenericNode*>::iterator it = nodes.begin()
                        ,itE = nodes.end(); it != itE; ++it) delete *it;
                }

            public:
                virtual void print(std::ostream& os = std::cout) const;

                template<class T> 
                inline void setProp(const char * name, const T& value) {
                    std::stringstream ss; ss << value;
                    properties[name] = ss.str();
                }

                inline void setProp(const char * name, const char * value) 
                { properties[name] = value; }

                inline bool operator<(const GenericNode& other) const 
                { return (0<mrutils::stricmp(name.c_str(),other.name.c_str())); }

                inline bool operator>(const GenericNode& other) const 
                { return (0>mrutils::stricmp(name.c_str(),other.name.c_str())); }

                inline bool operator==(const char * name) const 
                { return (0==mrutils::stricmp(this->name.c_str(),name)); }

                inline bool operator!=(const char * name) const 
                { return (0!=mrutils::stricmp(this->name.c_str(),name)); }

                inline bool operator<(const char * name) const 
                { return (0<mrutils::stricmp(this->name.c_str(),name)); }

                inline bool operator>(const char * name) const 
                { return (0>mrutils::stricmp(this->name.c_str(),name)); }

                inline friend std::ostream& operator<<(std::ostream& os, const GenericNode& node) 
                { node.print(os); return os; }

            public:
                std::string name;
                mrutils::map<std::string,std::string> properties;

                GenericNode * const parent;
                NodeType nodeType;

            protected:
                virtual void printContent(_UNUSED std::ostream& os) const {}
                virtual inline void printHeader(std::ostream& os) const {
                    os << "<" << name;
                    for (mrutils::map<std::string,std::string>::iterator it = properties.begin()
                        ,itE = properties.end(); it != itE; ++it) {
                        os << " " << (*it)->first << "=";
                        if ((*it)->second.empty())
                            os << "\"\"";
                        else if (mrutils::isNumber((*it)->second.c_str()))
                            os << (*it)->second;
                        else os << '"' << (*it)->second << '"';
                    }
                    os << ">";
                }

            protected:
                std::vector<GenericNode*> nodes;

            private:
                GenericNode(const GenericNode&); // disallowed
                GenericNode& operator=(const GenericNode&); // disallowed
        };

        template<class T> 
        class ContentNode : public GenericNode {
            friend class Document;
            friend class ContainerNode;

            protected:
                ContentNode(const char * name, GenericNode* const parent, const T& content)
                : GenericNode(name,parent, NODE_Content), content(content)
                {}

            public:
                T content;

            protected:
                virtual inline void printContent(std::ostream& os = std::cout) const 
                { os.precision(15); os << content; }

        };

        class ContainerNode : public GenericNode {
            public:
                ContainerNode(const char * name, GenericNode * const parent)
                : GenericNode(name, parent, NODE_Container)
                 ,sorted(false)
                {}

            public:
               /**
                * Creates a ContentNode under this node containing the
                * given data
                */
                template<class T>
                inline ContentNode<T> & node(const char * name, const T& content) {
                    ContentNode<T> * node = new ContentNode<T>(name,this,content);
                    nodes.push_back(node); sorted = false;
                    return *node;
                }

                // this is implemented separately to make sure COW works
                // & don't get implicit conversion to char *
                inline ContentNode<std::string>& node(const char * name, std::string& content) {
                    ContentNode<std::string> * node = new ContentNode<std::string>(name,this,content);
                    nodes.push_back(node); sorted = false;
                    return *node;
                }

                inline ContentNode<std::string>& node(const char * name, const char * content) {
                    ContentNode<std::string> * node = new ContentNode<std::string>(name,this,content);
                    nodes.push_back(node); sorted = false;
                    return *node;
                }

                inline ContentNode<std::string>& node(const char * name, char * content) {
                    ContentNode<std::string> * node = new ContentNode<std::string>(name,this,content);
                    nodes.push_back(node); sorted = false;
                    return *node;
                }

               /**
                * Returns a ContainerNode under this node, which can
                * contain other nodes
                */
                inline ContainerNode& node(const char * name) {
                    ContainerNode * node = new ContainerNode(name,this);
                    nodes.push_back(node); sorted = false;
                    return *node;
                }

            public:
               /******************
                * Finding nodes
                ******************/
                
                inline void sortNodes() {
                    std::sort(nodes.begin(), nodes.end(), mrutils::ptrCmp<GenericNode>);
                    sorted = true;
                }

                inline ContainerNode& getContainer(const char * name) const throw (std::runtime_error) {
                    if (sorted) {
                        const unsigned count = nodes.size();
                        GenericNode * const * start = &nodes[0];
                        GenericNode * const * it = mrutils::lower_bound(start, count, name, nodeStrCmp);
                        if (it - start > (int)count || **it != name) throw std::runtime_error("xml::ContainerNode can't find requested node");
                        return *dynamic_cast<ContainerNode*>(*it);
                    } else {
                        for (std::vector<GenericNode*>::const_iterator it = nodes.begin()
                            ,itE = nodes.end(); it != itE; ++it) {
                            if (**it == name) return *dynamic_cast<ContainerNode*>(*it);
                        }
                    }
                    throw std::runtime_error("xml::ContainerNode can't find requested node");
                }

                inline ContainerNode * findContainer(const char * name) const {
                    if (sorted) {
                        const unsigned count = nodes.size();
                        GenericNode * const * start = &nodes[0];
                        GenericNode * const * it = mrutils::lower_bound(start, count, name, nodeStrCmp);
                        if (it - start > (int)count || **it != name) return NULL;
                        return dynamic_cast<ContainerNode*>(*it);
                    } else {
                        for (std::vector<GenericNode*>::const_iterator it = nodes.begin()
                            ,itE = nodes.end(); it != itE; ++it) {
                            if (**it == name) return dynamic_cast<ContainerNode*>(*it);
                        }
                    }
                    return NULL;
                }

               /**
                * Returns what amounts to an iterator to the first node 
                * matching the name, or NULL
                */
                inline GenericNode ** begin(const char * name) {
                    if (!sorted) sortNodes();
                    return mrutils::lower_bound(&nodes[0], nodes.size(), name, nodeStrCmp);
                }

                inline GenericNode ** end(const char * name) {
                    if (!sorted) sortNodes();
                    return mrutils::upper_bound(&nodes[0], nodes.size(), name, nodeStrCmp);
                }

                template<class T>
                inline T& get(const char * name) const throw (std::runtime_error) {
                    if (sorted) {
                        const unsigned count = nodes.size();
                        GenericNode * const * start = &nodes[0];
                        GenericNode * const * it = mrutils::lower_bound(start, count, name, nodeStrCmp);
                        if (it - start > count || **it != name) throw std::runtime_error(std::string("xml::ContainerNode can't get requested data: ") + name);
                        return dynamic_cast<ContentNode<T>*>(*it)->content;
                    } else {
                        for (std::vector<GenericNode*>::const_iterator it = nodes.begin()
                            ,itE = nodes.end(); it != itE; ++it) 
                            if (**it == name) return dynamic_cast<ContentNode<T>*>(*it)->content;
                    } throw std::runtime_error(std::string("xml::ContainerNode can't get requested data: ") + name);
                }

                template<class T>
                inline bool get(const char * name, T* value) const {
                    if (sorted) {
                        const unsigned count = nodes.size();
                        GenericNode * const * start = &nodes[0];
                        GenericNode * const * it = mrutils::lower_bound(start, count, name, nodeStrCmp);
                        if (it - start > count || **it != name) return false;
                        *value = dynamic_cast<ContentNode<T>*>(*it)->content;
                        return true;
                    } else {
                        for (std::vector<GenericNode*>::const_iterator it = nodes.begin()
                            ,itE = nodes.end(); it != itE; ++it) {
                            if (**it == name) {
                                *value = dynamic_cast<ContentNode<T>*>(*it)->content;
                                return true;
                            }
                        }
                    } return false;
                }

            public:
                using GenericNode::nodes;

            protected:
                bool sorted; 

                static struct nodeStrCmp_t {
                    inline bool operator()(const GenericNode * lhs, const char * rhs) const 
                    { return (*lhs < rhs); }
                    inline bool operator()(const char * lhs, GenericNode * rhs) const 
                    { return (*rhs > lhs); }
                } nodeStrCmp;
        };

        class Document : public ContainerNode {
            public:
                Document(const char * name = "Document")
                : ContainerNode(name,NULL)
                {}

                Document(mrutils::BufferedReader& reader) throw (std::runtime_error) 
                : ContainerNode("",NULL)
                { read(reader); }

            public:
                void read(mrutils::BufferedReader& reader) throw (std::runtime_error);

                inline void clear() {
                    for (std::vector<GenericNode*>::iterator it = nodes.begin()
                        ,itE = nodes.end(); it != itE; ++it) delete *it;
                    nodes.clear();
                }

                inline void print(std::ostream& os = std::cout) const {
                    os << "<?xml version=\"1.0\" ?>\n\n";
                    GenericNode::print(os);
                }

            private:
                Document(const Document&); // disallowed
                Document& operator=(const Document&); // disallowed
        };

    }

}


#endif
