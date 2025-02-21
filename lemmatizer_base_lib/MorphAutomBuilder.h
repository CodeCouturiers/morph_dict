// ==========  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru), 
// ==========  Copyright by Alexey Sokirko (2004)


//   The main function of this header is CMorphAutomatBuilder::AddStringDaciuk 
//  This function is an implementation of algorithm for constructing minimal,
// deterministic, acyclic FSM from unordered std::set of std::strings, which was described 
// in  "Jan Daciuk, Stoyan Mihov, Bruce Watson, and Richard Watson, 
//      Incremental Construction of Minimal Acyclic Finite State Automata, 
//		Computational Linguistics, 26(1), March 2000."

#ifndef MorphAutomBuilder_h
#define MorphAutomBuilder_h

#include "MorphAutomat.h"
#include <unordered_map>

// Forward declare the struct first
struct CTrieNodeBuild;

// Define the full struct before anything that uses it
struct CTrieNodeBuild
{
	bool						m_bFinal;
	int							m_IncomingRelationsCount;
	CTrieNodeBuild*				m_Children[MaxAlphabetSize];
	CTrieNodeBuild*				m_RegisteredNode;  // Points to the registered version of this node
	bool						m_bRegistered;
	int							m_NodeId;
	BYTE						m_FirstChildNo;
	BYTE						m_SecondChildNo;
	CTrieNodeBuild**			m_pRegister;

	void				Initialize();
	void				AddChild(CTrieNodeBuild* Child, BYTE ChildNo);
	void				ModifyChild(CTrieNodeBuild* Child, BYTE ChildNo, bool bUpdateIncoming);
	CTrieNodeBuild*		GetNextNode(BYTE RelationChar)  const;
	void				SetNodeIdNullRecursive ();
	void				UnregisterRecursive();
	

	//  debug function
	bool				CheckIncomingRelationsCountRecursive(std::map<const CTrieNodeBuild*, size_t>& Node2Incoming) const;
	void				GetIncomingRelationsCountRecursive(std::map<const CTrieNodeBuild*, size_t>& Node2Incoming) const;
	bool				CheckRegisterRecursive() const;
	void				SetFinal(bool bFinal);
};

// Now define the comparison and hash functions that use CTrieNodeBuild
struct IsLessRegister: public std::less<CTrieNodeBuild*>
{
	bool operator ()(const CTrieNodeBuild* pNodeNo1, const CTrieNodeBuild* pNodeNo2) const;
};

struct TrieNodeHash {
    size_t operator()(const CTrieNodeBuild* node) const {
        size_t hash = node->m_bFinal ? 1 : 0;
        if(node->m_FirstChildNo != 0xff) {
            hash = hash * 31 + std::hash<void*>{}(node->m_Children[node->m_FirstChildNo]);
            if(node->m_SecondChildNo != 0xff) {
                for(size_t i = node->m_SecondChildNo; i < MaxAlphabetSize; i++) {
                    if(node->m_Children[i]) {
                        hash = hash * 31 + std::hash<void*>{}(node->m_Children[i]);
                    }
                }
            }
        }
        return hash;
    }
};

struct TrieNodeEqual {
    bool operator()(const CTrieNodeBuild* a, const CTrieNodeBuild* b) const {
        if(a->m_bFinal != b->m_bFinal) return false;
        if(a->m_FirstChildNo != b->m_FirstChildNo) return false;
        if(a->m_FirstChildNo == 0xff) return true;
        
        if(a->m_Children[a->m_FirstChildNo] != b->m_Children[b->m_FirstChildNo]) return false;
        if(a->m_SecondChildNo != b->m_SecondChildNo) return false;
        if(a->m_SecondChildNo == 0xff) return true;
        
        for(size_t i = a->m_SecondChildNo; i < MaxAlphabetSize; i++) {
            if(a->m_Children[i] != b->m_Children[i]) return false;
        }
        return true;
    }
};

typedef std::unordered_map<CTrieNodeBuild*, CTrieNodeBuild*, TrieNodeHash, TrieNodeEqual> CTrieRegister;

class CMorphAutomatBuilder : public CMorphAutomat
{
private:
	CTrieNodeBuild*			m_pRoot;
	CTrieRegister			m_Register;
	std::vector<CTrieNodeBuild*>	m_Prefix;
	std::vector<CTrieNodeBuild*>	m_DeletedNodes;
	std::unordered_map<BYTE, std::unordered_map<BYTE, CTrieRegister>> m_RegisterHash;
	
	void				ClearBuildNodes();
	CTrieNodeBuild*		CreateNode();
	void				DeleteNode(CTrieNodeBuild* pNode);
	CTrieNodeBuild*		CloneNode(const CTrieNodeBuild* pPrototype);
	void				UpdateCommonPrefix(const std::string& WordForm);
	CTrieNodeBuild*		AddSuffix(CTrieNodeBuild* pParentNodeNo, const char* WordForm);
	CTrieNodeBuild*		ReplaceOrRegister(CTrieNodeBuild* pNode);
	void				DeleteFromRegister(CTrieNodeBuild* pNode);
	int					GetFirstConfluenceState() const;
	void				UnregisterNode(CTrieNodeBuild* pNode);
	bool                CheckRegister() const;
	bool                IsValid() const;
	CTrieRegister&      GetRegister(const CTrieNodeBuild* pNode);
	
	// Pre-allocation support
	std::vector<CTrieNodeBuild*> m_NodePool;
	size_t m_EstimatedNodes;
	
public:
	CMorphAutomatBuilder(MorphLanguageEnum Language, BYTE AnnotChar);
	~CMorphAutomatBuilder();

	void	InitTrie();
	void	AddStringDaciuk(const std::string& WordForm);
	void	ClearRegister();
	void	ConvertBuildRelationsToRelations();
	void    ReserveSpace(size_t estimatedForms);
};

#endif
