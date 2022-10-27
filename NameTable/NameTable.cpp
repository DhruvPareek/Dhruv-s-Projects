#include "NameTable.h"
#include <string>
#include <list>
#include <functional>
using namespace std;

const int BUCKETS = 19997;

//*********** HashtableByDepth implementation and functions **************
//*********** HashtableByDepth implementation and functions **************
//*********** HashtableByDepth implementation and functions **************
//*********** HashtableByDepth implementation and functions **************
//*********** HashtableByDepth implementation and functions **************

class HashTable
{
    private:
        struct declaration{
            string m_Name;
            int m_Line;
            int m_Depth;
            declaration(string name, int line, int depth) : m_Name(name), m_Line(line), m_Depth(depth)
            {}
            declaration() : m_Name(""), m_Line(-1), m_Depth(-1)
            {}

        };
        list<declaration> sortedByDepth[BUCKETS];
        list<declaration> sortedRandomly[BUCKETS];
public:
    HashTable();
    int find(const string& id, const int depth) const;//returns line number of this declaration
    void insert(const string& id, const int depth, const int line);//pushes back into linked list the new declaration
    void destroyScope(const int scope);//deletes all of the items of a scope from both hashtables
};

HashTable::HashTable()
{}

void HashTable::insert(const string& id, const int depth, const int line)
{
    sortedByDepth[depth].push_back(declaration(id, line, depth));
    sortedRandomly[(hash<string>()(to_string(depth) + id) % (BUCKETS-1))].push_back(declaration(id, line, depth));
}

int HashTable::find(const string& id, const int depth) const
{//loop through the proper bucket searching for the correct name of the declaration, then return the  line of that declaration
    
        auto& cell = sortedRandomly[(hash<string>()(to_string(depth) + id) % (BUCKETS-1))];
        for(auto p = cell.begin(); p != cell.end(); p++){
            if(p->m_Name == id){
                return p->m_Line;
            }
        }
    return -1;
}

void HashTable::destroyScope(const int scope)//deletes all of the items of a scope from both hashtables
{
    for(list<declaration>::iterator p = sortedByDepth[scope].begin();sortedByDepth[scope].size() != 0 && p != sortedByDepth[scope].end();)//loop through the sortedByDepth hash table's certain scope and save the name of a declaration then delete it
    {
        string id = p->m_Name;
        p = sortedByDepth[scope].erase(p);
        for(list<declaration>::iterator p = sortedRandomly[(hash<string>()(to_string(scope) + id) % (BUCKETS-1))].begin();sortedRandomly[(hash<string>()(to_string(scope) + id) % (BUCKETS-1))].size() != 0 &&  p != sortedRandomly[(hash<string>()(to_string(scope) + id) % (BUCKETS-1))].end(); p++){
            if(p->m_Name == id){//find the declaration in sortedRandomly then delete it
                sortedRandomly[(hash<string>()(to_string(scope) + id) % (BUCKETS-1))].erase(p);
                break;
            }
        }
    }
}

//*********** NameTableImpl implementation and functions **************
//*********** NameTableImpl implementation and functions **************
//*********** NameTableImpl implementation and functions **************
//*********** NameTableImpl implementation and functions **************
//*********** NameTableImpl implementation and functions **************
// This class does the real work of the implementation.

class NameTableImpl
{
  public:
    NameTableImpl();
    void enterScope();
    bool exitScope();//need to delete all of the variables declared in this scope
    bool declare(const string& id, int lineNum);//needs to add the declaration to the hash table unless this declaration has already been made in this scope
    int find(const string& id) const;
    
  private:
    int scopeDepth;//line 1 starts at a scope of 0, then every new scope entered is one greater
    //data structure to be used - 2 hash tables
    HashTable hashy;
};

NameTableImpl::NameTableImpl():  scopeDepth(0)
{}

void NameTableImpl::enterScope()
{
    scopeDepth++;
}

bool NameTableImpl::exitScope()
{
    if(scopeDepth>0){//if you can exit the scope
        hashy.destroyScope(scopeDepth);
        scopeDepth--;
        return true;
    }
    return false;
}

bool NameTableImpl::declare(const string& id, int lineNum)
{
    if(id == "")
        return false;
    if(hashy.find(id, scopeDepth) != -1)
        return false;
    hashy.insert(id, scopeDepth, lineNum);
    return true;
}

int NameTableImpl::find(const string& id) const
{//returns the line at which this declaration was made or -1 if it has not been made
    int line = -1;
    int tempDepth = scopeDepth;
    while(line == -1 && tempDepth >= 0){
        line = hashy.find(id, tempDepth);
        tempDepth--;
    }
    return line;
}

//*********** NameTable functions **************
//*********** NameTable functions **************
//*********** NameTable functions **************
//*********** NameTable functions **************
//*********** NameTable functions **************
//*********** NameTable functions **************
//*********** NameTable functions **************

// For the most part, these functions simply delegate to NameTableImpl's
// functions.

NameTable::NameTable()
{
    m_impl = new NameTableImpl;
}

NameTable::~NameTable()
{
    delete m_impl;
}

void NameTable::enterScope()
{
    m_impl->enterScope();
}

bool NameTable::exitScope()
{
    return m_impl->exitScope();
}

bool NameTable::declare(const string& id, int lineNum)
{
    return m_impl->declare(id, lineNum);
}

int NameTable::find(const string& id) const
{
    return m_impl->find(id);
}


