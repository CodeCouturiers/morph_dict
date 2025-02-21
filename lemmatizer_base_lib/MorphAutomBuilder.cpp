// ==========  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru), 
// ==========  Copyright by Alexey Sokirko (2004)


// The main function of this file is CMorphAutomatBuilder::AddStringDaciuk 
// This function is an implementation of algorithm for constructing minimal,
// deterministic, acyclic FSM from unordered std::set of std::strings, which was described 
// in  "Jan Daciuk, Stoyan Mihov, Bruce Watson, and Richard Watson, 
//      Incremental Construction of Minimal Acyclic Finite State Automata, 
//		Computational Linguistics, 26(1), March 2000."

#pragma warning  (disable : 4996)

#include "MorphAutomBuilder.h"
#include "queue"
#include "assert.h"
#include <algorithm>
#include <cassert>

//======================================================
//=============		CTrieNodeBuild	   =============
//======================================================
int NodeId = 0;
size_t RegisterSize = 0;

void CTrieNodeBuild::Initialize()
{
	m_bFinal = false;
	m_IncomingRelationsCount = 0;
	m_bRegistered = false;
	m_NodeId = -1;
	m_FirstChildNo = 0xff;
	m_SecondChildNo = 0xff;
	std::fill(m_Children, m_Children + MaxAlphabetSize, nullptr);
};


void CTrieNodeBuild::SetFinal(bool bFinal)
{
	m_bFinal = bFinal;

};



void CTrieNodeBuild::AddChild(CTrieNodeBuild* Child, BYTE ChildNo)
{
	assert(Child != this);
	assert(ChildNo < MaxAlphabetSize);
	
	// If there's already a child, update its incoming count
	if (m_Children[ChildNo]) {
		m_Children[ChildNo]->m_IncomingRelationsCount--;
	}
	
	m_Children[ChildNo] = Child;
	Child->m_IncomingRelationsCount++;

	// Update FirstChildNo and SecondChildNo
	if (m_FirstChildNo == 0xff || ChildNo < m_FirstChildNo)
	{
		m_SecondChildNo = m_FirstChildNo;
		m_FirstChildNo = ChildNo;
	}
	else if (ChildNo != m_FirstChildNo && (m_SecondChildNo == 0xff || ChildNo < m_SecondChildNo))
	{
		m_SecondChildNo = ChildNo;
		assert(m_FirstChildNo < m_SecondChildNo);
	}
}

void CTrieNodeBuild::ModifyChild(CTrieNodeBuild* Child, BYTE ChildNo, bool bUpdateIncoming)
{
	assert(ChildNo < MaxAlphabetSize);
	
	CTrieNodeBuild* OldChild = m_Children[ChildNo];
	if (!OldChild || OldChild == Child) return;

	if (bUpdateIncoming) {
		OldChild->m_IncomingRelationsCount--;
	}

	m_Children[ChildNo] = Child;
	if (Child) {
		Child->m_IncomingRelationsCount++;
	}

	// Update FirstChildNo and SecondChildNo
	m_FirstChildNo = 0xff;
	m_SecondChildNo = 0xff;
	
	// Recalculate FirstChildNo and SecondChildNo
	for (BYTE i = 0; i < MaxAlphabetSize; i++) {
		if (m_Children[i]) {
			if (m_FirstChildNo == 0xff) {
				m_FirstChildNo = i;
			}
			else if (i != m_FirstChildNo && (m_SecondChildNo == 0xff || i < m_SecondChildNo)) {
				m_SecondChildNo = i;
			}
		}
	}
}





CTrieNodeBuild* CTrieNodeBuild::GetNextNode(BYTE ChildNo)  const
{
	assert (ChildNo < MaxAlphabetSize);
	return m_Children[ChildNo];
};




void CTrieNodeBuild::GetIncomingRelationsCountRecursive(std::map<const CTrieNodeBuild*, size_t>& Node2Incoming) const
{
	if (Node2Incoming.find(this) != Node2Incoming.end()) {
		return;
	}
	
	Node2Incoming[this] = 0;
	for (size_t i=m_FirstChildNo; i < MaxAlphabetSize; i++)
	if (m_Children[i])
	{
		Node2Incoming[m_Children[i]]++;
		m_Children[i]->GetIncomingRelationsCountRecursive(Node2Incoming);
	};
};

bool CTrieNodeBuild::CheckIncomingRelationsCountRecursive(std::map<const CTrieNodeBuild*, size_t>& Node2Incoming) const
{
	if (Node2Incoming[this] != m_IncomingRelationsCount) {
		return false;
	}
	
	for (size_t i=m_FirstChildNo; i < MaxAlphabetSize; i++)
		if (m_Children[i])
			if (!m_Children[i]->CheckIncomingRelationsCountRecursive( Node2Incoming))
				return false;

	return true;
};

bool CTrieNodeBuild::CheckRegisterRecursive() const
{
	if (m_bRegistered && !m_RegisteredNode) {
		return false;
	}
	
	for (size_t i=m_FirstChildNo; i < MaxAlphabetSize; i++)
		if (m_Children[i])
			if (!m_Children[i]->CheckRegisterRecursive( ))
				return false;

	return true;
};

void	CTrieNodeBuild::SetNodeIdNullRecursive ()
{
	if (m_NodeId == -1) return;
	m_NodeId = -1;
	for (size_t i=m_FirstChildNo; i < MaxAlphabetSize; i++)
		if (m_Children[i])
			m_Children[i]->SetNodeIdNullRecursive( );
	return;			
};

void	CTrieNodeBuild::UnregisterRecursive()
{
	m_bRegistered = false;
	m_RegisteredNode = nullptr;
	for (size_t i=m_FirstChildNo; i < MaxAlphabetSize; i++)
		if (m_Children[i])
			m_Children[i]->UnregisterRecursive( );
	return;			
};




//======================================================
//=============		IsLessRegister	   =============
//======================================================


bool IsLessRegister::operator ()(const CTrieNodeBuild* pNodeNo1, const CTrieNodeBuild* pNodeNo2) const
{
	if (pNodeNo1->m_bFinal != pNodeNo2->m_bFinal)
		return pNodeNo1->m_bFinal < pNodeNo2->m_bFinal;

	assert (pNodeNo1->m_FirstChildNo == pNodeNo2->m_FirstChildNo);
	if (pNodeNo1->m_FirstChildNo == 0xff) return false;
	if (pNodeNo1->m_Children[pNodeNo1->m_FirstChildNo] < pNodeNo2->m_Children[pNodeNo2->m_FirstChildNo]) 
		return true;
	if (pNodeNo1->m_Children[pNodeNo1->m_FirstChildNo] > pNodeNo2->m_Children[pNodeNo2->m_FirstChildNo]) 
		return false;


	assert (pNodeNo1->m_SecondChildNo == pNodeNo1->m_SecondChildNo);
	if (pNodeNo1->m_SecondChildNo == 0xff) return false;

	return std::lexicographical_compare
			(pNodeNo1->m_Children+pNodeNo1->m_SecondChildNo, pNodeNo1->m_Children+MaxAlphabetSize,
			pNodeNo2->m_Children+pNodeNo2->m_SecondChildNo, pNodeNo2->m_Children+MaxAlphabetSize);
}

//======================================================
//=============		CMorphAutomatBuilder	   =============
//======================================================

CMorphAutomatBuilder::CMorphAutomatBuilder(MorphLanguageEnum Language, BYTE AnnotChar) 
    : CMorphAutomat(Language, AnnotChar)
    , m_pRoot(nullptr)
    , m_EstimatedNodes(0)
{
	InitTrie();
}

CMorphAutomatBuilder::~CMorphAutomatBuilder() {
	ClearBuildNodes();
}

void CMorphAutomatBuilder::ReserveSpace(size_t estimatedForms) {
	m_EstimatedNodes = estimatedForms * 2;  // Rough estimate
	m_NodePool.reserve(m_EstimatedNodes);
}

CTrieNodeBuild* CMorphAutomatBuilder::CreateNode() {
	CTrieNodeBuild* node = new CTrieNodeBuild();
	node->Initialize();
	m_NodePool.push_back(node);
	return node;
}

void CMorphAutomatBuilder::DeleteNode(CTrieNodeBuild* pNode) {
	if (!pNode) return;
	m_DeletedNodes.push_back(pNode);
}

CTrieNodeBuild* CMorphAutomatBuilder::CloneNode(const CTrieNodeBuild* pPrototype) {
	if (!pPrototype) return nullptr;
	
	CTrieNodeBuild* clone = CreateNode();
	clone->m_bFinal = pPrototype->m_bFinal;
	clone->m_FirstChildNo = pPrototype->m_FirstChildNo;
	clone->m_SecondChildNo = pPrototype->m_SecondChildNo;
	
	for (size_t i = 0; i < MaxAlphabetSize; i++) {
		clone->m_Children[i] = pPrototype->m_Children[i];
		if (clone->m_Children[i]) {
			clone->m_Children[i]->m_IncomingRelationsCount++;
		}
	}
	
	return clone;
}

void CMorphAutomatBuilder::ClearBuildNodes() {
	for (auto* node : m_NodePool) {
		delete node;
	}
	m_NodePool.clear();
	m_pRoot = nullptr;
	m_Register.clear();
	m_RegisterHash.clear();
	m_Prefix.clear();
	m_DeletedNodes.clear();
}

void CMorphAutomatBuilder::InitTrie() {
	ClearBuildNodes();
	m_pRoot = CreateNode();
}

void CMorphAutomatBuilder::UpdateCommonPrefix(const std::string& WordForm)
{
	m_Prefix.resize(1);
	m_Prefix[0] = m_pRoot;
	
	size_t Length = WordForm.length();
	for (size_t i=0; i < Length; i++)
	{
		BYTE CharNo = m_Alphabet2Code[(BYTE)WordForm[i]];
		CTrieNodeBuild* pNode =  m_Prefix.back()->GetNextNode(CharNo);
		if (!pNode) 
			break;
		m_Prefix.push_back(pNode);
	};
	
};


int CMorphAutomatBuilder::GetFirstConfluenceState() const
{
	for (size_t i =0; i <m_Prefix.size(); i++)
		if (m_Prefix[i]->m_IncomingRelationsCount > 1)
				return i;
	return -1;
};


CTrieRegister& CMorphAutomatBuilder::GetRegister(const CTrieNodeBuild* pNode) {
	if (pNode->m_FirstChildNo == 0xff) {
		return m_Register;
	}
	return m_RegisterHash[pNode->m_FirstChildNo][pNode->m_SecondChildNo];
}



CTrieNodeBuild* CMorphAutomatBuilder::ReplaceOrRegister(CTrieNodeBuild* pNode)
{
	auto it = m_Register.find(pNode);
	if(it != m_Register.end())
	{
		// Node already exists in register, use the registered version
		CTrieNodeBuild* registeredNode = it->second;
		if(pNode != registeredNode) {
			DeleteNode(pNode);
			pNode = registeredNode;
		}
	}
	else
	{
		// New unique node, register it
		m_Register[pNode] = pNode;
		pNode->m_RegisteredNode = pNode;
		pNode->m_bRegistered = true;
		RegisterSize++;
	}
	return pNode;
}


bool CheckRegisterOrder(const CTrieRegister& Register)
{
	const CTrieNodeBuild* pPrevNode = nullptr;
	IsLessRegister Less;
	for (const auto& pair : Register)
	{
		const CTrieNodeBuild* pNode = pair.first;
		if (pPrevNode)
		{	
			if (!Less(pPrevNode, pNode))
			{
				assert(Less(pPrevNode, pNode));
				return false;
			}
		}
		pPrevNode = pNode;
	}
	return true;
}

bool CMorphAutomatBuilder::CheckRegister() const
{
	if (!m_pRoot) return true;
	
	for (const auto& pair : m_Register) {
		if (!pair.first->CheckRegisterRecursive()) {
			return false;
		}
	}
	return true;
}


bool CMorphAutomatBuilder::IsValid() const
{
	if (!m_pRoot) return false;
	
	std::map<const CTrieNodeBuild*, size_t> node2Incoming;
	m_pRoot->GetIncomingRelationsCountRecursive(node2Incoming);
	return m_pRoot->CheckIncomingRelationsCountRecursive(node2Incoming);
};

void CMorphAutomatBuilder::UnregisterNode(CTrieNodeBuild* pNode)
{
	if(pNode->m_bRegistered)
	{
		pNode->m_bRegistered = false;
		m_Register.erase(pNode);
		RegisterSize--;
	}
}

// we do not register the parent node; we  register only the children
CTrieNodeBuild* CMorphAutomatBuilder::AddSuffix(CTrieNodeBuild* pParentNodeNo, const char* WordForm)
{
	// save current char
	BYTE RelationChar = (BYTE)*WordForm;
	WordForm++;

	//  adding new node child
	CTrieNodeBuild* pChildNode = CreateNode();
	 
	//  adding the rest of the suffix 
	if (*WordForm)
		AddSuffix(pChildNode, WordForm); 

	// making it final
	if (*WordForm == 0)
		pChildNode->SetFinal( true ) ;

	//  replace or register (the children should be already registered)
	pChildNode = ReplaceOrRegister(pChildNode);
		

	//  adding this child to the parent
	{
		assert (!pParentNodeNo->m_bRegistered);
		pParentNodeNo->AddChild(pChildNode, m_Alphabet2Code[RelationChar]);
	}
	return 	pChildNode;
};




void CMorphAutomatBuilder::AddStringDaciuk(const std::string& WordForm)
{
	CheckABCWithAnnotator(WordForm);
	
	if (WordForm.rfind(m_AnnotChar) == WordForm.length() - 1)
	{
		throw CExpc ("%s - bad annotation", WordForm.c_str());
	};

	UpdateCommonPrefix(WordForm);

	if	(m_Prefix.size() == WordForm.length()+1 && m_Prefix.back()->m_bFinal)
	{
		// String already exists
		return;
	};

	CTrieNodeBuild*	pLastNode = m_Prefix.back();
	int FirstConfluenceState = GetFirstConfluenceState();

	if (FirstConfluenceState != -1) {
		pLastNode = CloneNode(pLastNode);
	}
	else {
		UnregisterNode(pLastNode);
	}
	
	if (m_Prefix.size() == WordForm.length() + 1) {
		pLastNode->SetFinal(true);
	}
	else {
		AddSuffix(pLastNode, WordForm.c_str() + m_Prefix.size() - 1);
	}

	// Process nodes from bottom up
	int CurrentIndex = m_Prefix.size() - 1;
	
	while(CurrentIndex > 0) {
		CTrieNodeBuild* currentNode = m_Prefix[CurrentIndex-1];
		UnregisterNode(currentNode);

		pLastNode = ReplaceOrRegister(pLastNode);

		if(pLastNode == m_Prefix[CurrentIndex]) {
			ReplaceOrRegister(currentNode);
			break;
		}

		BYTE CharNo = m_Alphabet2Code[(BYTE)WordForm[CurrentIndex-1]];
		bool updateIncoming = FirstConfluenceState == CurrentIndex;
		currentNode->ModifyChild(pLastNode, CharNo, updateIncoming);
		
		pLastNode = currentNode;
		CurrentIndex--;
	}
};



void CMorphAutomatBuilder::ConvertBuildRelationsToRelations()
{
	if (!m_pRoot) return;
	m_pRoot->SetNodeIdNullRecursive();
	std::queue<CTrieNodeBuild*> NodesQueue;
	NodesQueue.push(m_pRoot);
	m_pRoot->m_NodeId = 0;

	std::vector<CMorphAutomNode> Nodes;
	std::vector<CMorphAutomRelation> Relations;

	while (!NodesQueue.empty())
	{
		//  getting an element from the queue
		CTrieNodeBuild* pNode = NodesQueue.front();
		NodesQueue.pop();

		CMorphAutomNode N;
		N.SetFinal(pNode->m_bFinal);
		
		N.SetChildrenStart(Relations.size());
		assert (N.GetChildrenStart() == Relations.size());
		assert (N.IsFinal() == pNode->m_bFinal);

		Nodes.push_back(N);

		int CurrentNodeId = Nodes.size() + NodesQueue.size();

		for (size_t i=0; i < MaxAlphabetSize; i++)
		if (pNode->m_Children[i])
		{
			CTrieNodeBuild* Child = pNode->m_Children[i];
			if (Child->m_NodeId == -1)
			{
				Child->m_NodeId = CurrentNodeId++;
				NodesQueue.push(Child);
			};

			// adding new relation
			CMorphAutomRelation R;
			R.SetRelationalChar(m_Code2Alphabet[i]);
			R.SetChildNo(Child->m_NodeId);
			assert (R.GetChildNo() == Child->m_NodeId);
			assert (R.GetRelationalChar() == m_Code2Alphabet[i]);

			Relations.push_back(R);
			if (Relations.size() > 0xffffff)
			{
				throw CExpc("Too many children in the automat. It cannot be more than 0xffffff");
			};
		};
	};

	Clear();

	m_NodesCount = Nodes.size();
	m_pNodes = new CMorphAutomNode[m_NodesCount];
	std::copy(Nodes.begin(), Nodes.end(), m_pNodes);

	m_RelationsCount = Relations.size();
	m_pRelations = new CMorphAutomRelation[m_RelationsCount];
	copy(Relations.begin(), Relations.end(), m_pRelations);

};

void CMorphAutomatBuilder::ClearRegister() {
    m_Register.clear();
    m_RegisterHash.clear();
    if (m_pRoot) {
        m_pRoot->UnregisterRecursive();
    }
    RegisterSize = 0;
}

