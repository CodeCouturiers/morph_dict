// ==========  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru), 
// ==========  Copyright by Alexey Sokirko (2004)

#include "MorphDict.h"
#include "LemmaInfoSerialize.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <Windows.h>
#include <debugapi.h>

namespace fs = std::filesystem;

CMorphDict::CMorphDict(MorphLanguageEnum language) :
	m_SearchInfoLess(m_Bases)
{
	m_pFormAutomat = 0;
	m_Language = language;
};

CMorphDict::~CMorphDict()
{
	if (m_pFormAutomat != nullptr)
		delete m_pFormAutomat;
	m_pFormAutomat = 0;
};

void CMorphDict::InitAutomat(CMorphAutomat* pFormAutomat)
{
	assert(m_pFormAutomat == 0);
	assert(pFormAutomat != 0);
	m_pFormAutomat = pFormAutomat;
};


void    CMorphDict::GetLemmaInfos(const std::string& Text, size_t TextPos, std::vector<CAutomAnnotationInner>& Infos) const
{
#ifdef _DEBUG
	OutputDebugStringA("\n[GetLemmaInfos] Starting lemma info processing\n");
#endif

	const size_t textLength = Text.length();
	// Pre-allocate vectors to avoid reallocations
	std::vector<CAutomAnnotationInner> additInfos;
	additInfos.reserve(Infos.size() * 2);  // Reasonable estimate for homonyms
	std::vector<CAutomAnnotationInner> validInfos;
	validInfos.reserve(Infos.size());

	// Reuse string buffer to avoid allocations
	std::string Base;
	Base.reserve(textLength);

	for (const CAutomAnnotationInner& annot : Infos)
	{
		// Basic validation checks
		if (annot.m_ModelNo >= m_FlexiaModels.size() || 
			annot.m_PrefixNo >= m_Prefixes.size() ||
			annot.m_ModelNo >= m_ModelsIndex.size() - 1) {
#ifdef _DEBUG
			OutputDebugStringA("[GetLemmaInfos] Invalid annotation parameters\n");
#endif
			continue;
		}

		const CFlexiaModel& F = m_FlexiaModels[annot.m_ModelNo];
		if (annot.m_ItemNo >= F.m_Flexia.size()) {
			continue;
		}
		
		const CMorphForm& M = F.m_Flexia[annot.m_ItemNo];
		
		// Calculate text position
		const size_t prefixLen = m_Prefixes[annot.m_PrefixNo].length();
		const size_t morphPrefixLen = M.m_PrefixStr.length();
		const size_t textStartPos = TextPos + prefixLen + morphPrefixLen;
		
		// Validate text positions
		if (textStartPos >= textLength || 
			textLength < textStartPos + M.m_FlexiaStr.length()) {
			continue;
		}

		// Build base string efficiently
		Base.clear();
		if (prefixLen > 0) {
			Base.append(m_Prefixes[annot.m_PrefixNo]);
		}
		Base.append(Text, textStartPos, textLength - textStartPos - M.m_FlexiaStr.length());

		// Get iterator range
		auto start = m_LemmaInfos.begin() + m_ModelsIndex[annot.m_ModelNo];
		auto end = m_LemmaInfos.begin() + m_ModelsIndex[annot.m_ModelNo + 1];

		if (start > end || end > m_LemmaInfos.end()) {
			continue;
		}

		auto pair_it = equal_range(start, end, Base.c_str(), m_SearchInfoLess);
		
		// Validate search results
		if (pair_it.first == m_LemmaInfos.end() || pair_it.first >= pair_it.second) {
			continue;
		}

		size_t firstPos = pair_it.first - m_LemmaInfos.begin();
		if (firstPos >= m_LemmaInfos.size()) {
			continue;
		}

		// Add first match
		CAutomAnnotationInner validAnnot = annot;
		validAnnot.m_LemmaInfoNo = firstPos;
		validInfos.push_back(validAnnot);

		// Add homonyms efficiently
		for (auto it = pair_it.first + 1; it != pair_it.second; ++it) {
			size_t lemmaInfoNo = it - m_LemmaInfos.begin();
			if (lemmaInfoNo >= m_LemmaInfos.size()) {
				continue;
			}
			
			CAutomAnnotationInner new_annot = annot;
			new_annot.m_LemmaInfoNo = lemmaInfoNo;
			additInfos.push_back(new_annot);
		}
	}

	// Efficient vector operations
	const size_t totalSize = validInfos.size() + additInfos.size();
	if (totalSize == 0) {
		Infos.clear();
		return;
	}

	Infos = std::move(validInfos);
	if (!additInfos.empty()) {
		Infos.reserve(totalSize);
		Infos.insert(Infos.end(), 
			std::make_move_iterator(additInfos.begin()),
			std::make_move_iterator(additInfos.end()));
	}

#ifdef _DEBUG
	OutputDebugStringA("[GetLemmaInfos] Completed\n");
#endif
};


void    CMorphDict::PredictBySuffix(const std::string& Text, size_t& TextPos, size_t MinimalPredictSuffixlen, std::vector<CAutomAnnotationInner>& Infos) const
{
	size_t Count = Text.length();

	for (TextPos = 1; TextPos + MinimalPredictSuffixlen <= Count; TextPos++)
	{
		m_pFormAutomat->GetInnerMorphInfos(Text, TextPos, Infos);
		if (!Infos.empty()) break;
	};

};




inline size_t get_size_in_bytes(const CLemmaInfoAndLemma& t)
{
	return        get_size_in_bytes(t.m_LemmaInfo)
		+ get_size_in_bytes(t.m_LemmaStrNo);
};

inline size_t save_to_bytes(const CLemmaInfoAndLemma& t, BYTE* buf)
{
	buf += save_to_bytes(t.m_LemmaInfo, buf);
	buf += save_to_bytes(t.m_LemmaStrNo, buf);
	return get_size_in_bytes(t);
};

inline size_t restore_from_bytes(CLemmaInfoAndLemma& t, const BYTE* buf)
{
	buf += restore_from_bytes(t.m_LemmaInfo, buf);
	buf += restore_from_bytes(t.m_LemmaStrNo, buf);
	return get_size_in_bytes(t);
};


// This procedure builds CMorphDict::m_ModelsIndex, which is an index to CMorphDict::m_LemmaInfos
// *  m_LemmaInfos is sorted by m_LemmaInfo.m_FlexiaModelNo
// *  CMorphDict::m_ModelsIndex stores all periods of  CMorphDict::m_ModelsIndex with equal  m_LemmaInfo.m_FlexiaModelNo
// *  if a=m_ModelsIndex[i] and b=m_ModelsIndex[i+1], then for each j (a<=j<b)
//     LemmaInfos[j].m_LemmaInfo.m_FlexiaModelNo == i
// *  for some i m_ModelsIndex[i] can be equal to m_ModelsIndex[i+1], it means
//    that flexia model i is not used. To delete unused models the dictionary should be packed.
void CMorphDict::CreateModelsIndex()
{
	m_ModelsIndex.clear();
	if (m_LemmaInfos.empty()) return;

	m_ModelsIndex.resize(m_FlexiaModels.size() + 1);

	int CurrentModel = m_LemmaInfos[0].m_LemmaInfo.m_FlexiaModelNo;
	m_ModelsIndex[CurrentModel] = 0;

	for (size_t i = 0; i < m_LemmaInfos.size(); i++)
		for (; CurrentModel < m_LemmaInfos[i].m_LemmaInfo.m_FlexiaModelNo; CurrentModel++)
		{
			m_ModelsIndex[CurrentModel + 1] = (int)i;
		};

	for (; CurrentModel < m_FlexiaModels.size(); CurrentModel++)
		m_ModelsIndex[CurrentModel + 1] = (int)m_LemmaInfos.size();
#ifdef DEBUG    
	for (size_t i = 0; i < m_LemmaInfos.size(); i++)
	{
		int debug = m_LemmaInfos[i].m_LemmaInfo.m_FlexiaModelNo;
		assert(m_ModelsIndex[debug] <= i);
		assert(i < m_ModelsIndex[debug + 1]);
	};
#endif
};

static size_t getCount(std::ifstream& mrdFile, const char* sectionName) {
	std::string line;
	if (!getline(mrdFile, line)) {
		throw CExpc("Cannot get size of section  %s", sectionName);
	}
	return atoi(line.c_str());
}

void CMorphDict::Load(std::string GrammarFileName)
{
    try {
        OutputDebugStringA("\n[MorphDict] Starting to load dictionary...\n");
        OutputDebugStringA(Format("[MorphDict] Loading file: %s\n", GrammarFileName.c_str()).c_str());

        // First read the main morph.bin file
        std::ifstream mainFile(GrammarFileName, std::ios::binary);
        if (!mainFile.is_open()) {
            OutputDebugStringA(Format("[MorphDict] ERROR: Cannot open %s\n", GrammarFileName.c_str()).c_str());
            throw CExpc(Format("Cannot open %s", GrammarFileName.c_str()));
        }
        OutputDebugStringA("[MorphDict] Successfully opened morph.bin\n");

        // Read and verify version
        uint32_t version;
        mainFile.read((char*)&version, sizeof(version));
        OutputDebugStringA(Format("[MorphDict] Read version: %u\n", version).c_str());
        if (version != 1) {
            OutputDebugStringA(Format("[MorphDict] ERROR: Invalid version: %u\n", version).c_str());
            throw CExpc(Format("Invalid morph.bin version: %u", version));
        }

        // Read language
        MorphLanguageEnum fileLanguage;
        mainFile.read((char*)&fileLanguage, sizeof(fileLanguage));
        OutputDebugStringA(Format("[MorphDict] Read language: %s\n", GetStringByLanguage(fileLanguage).c_str()).c_str());
        if (fileLanguage != m_Language) {
            OutputDebugStringA(Format("[MorphDict] ERROR: Language mismatch. Expected %s, got %s\n", 
                GetStringByLanguage(m_Language).c_str(),
                GetStringByLanguage(fileLanguage).c_str()).c_str());
            throw CExpc(Format("Language mismatch in %s: expected %s, got %s", 
                GrammarFileName.c_str(),
                GetStringByLanguage(m_Language).c_str(),
                GetStringByLanguage(fileLanguage).c_str()));
        }

        // Read component paths
        auto readPath = [&mainFile]() {
            uint32_t len;
            mainFile.read((char*)&len, sizeof(len));
            std::string path(len, '\0');
            mainFile.read(&path[0], len);
            return path;
        };

        std::string formsPath = readPath();
        std::string annotPath = readPath();
        std::string basesPath = readPath();

        OutputDebugStringA(Format("[MorphDict] Read paths:\n  forms: %s\n  annot: %s\n  bases: %s\n", 
            formsPath.c_str(), annotPath.c_str(), basesPath.c_str()).c_str());

        mainFile.close();

        // Get the directory containing morph.bin
        fs::path baseDir = fs::path(GrammarFileName).parent_path();
        OutputDebugStringA(Format("[MorphDict] Base directory: %s\n", baseDir.string().c_str()).c_str());

        // Load forms automaton
        std::string formsFile = (baseDir / formsPath).string();
        OutputDebugStringA(Format("[MorphDict] Loading forms automaton from %s\n", formsFile.c_str()).c_str());
        m_pFormAutomat->Load(formsFile);
        OutputDebugStringA("[MorphDict] Forms automaton loaded successfully\n");

        // Load annotations
        std::string annotFile = (baseDir / annotPath).string();
        OutputDebugStringA(Format("[MorphDict] Loading annotations from %s\n", annotFile.c_str()).c_str());
        std::ifstream annotStream(annotFile, std::ios::binary);
        if (!annotStream.is_open()) {
            OutputDebugStringA(Format("[MorphDict] ERROR: Cannot open annotations file %s\n", annotFile.c_str()).c_str());
            throw CExpc(Format("Cannot open %s", annotFile.c_str()));
        }

        try {
            // Load flexia models
            m_FlexiaModels.clear();
            size_t count = getCount(annotStream, "flexia models");
            OutputDebugStringA(Format("[MorphDict] Loading %zu flexia models\n", count).c_str());
            std::string line;
            for (size_t i = 0; i < count; ++i) {
                if (!getline(annotStream, line)) {
                    OutputDebugStringA("[MorphDict] ERROR: Cannot read flexia models\n");
                    throw CExpc("Cannot read flexia models");
                }
                m_FlexiaModels.emplace_back(CFlexiaModel().FromString(line));
            }
            OutputDebugStringA("[MorphDict] Flexia models loaded successfully\n");

            // Load accent models
            count = getCount(annotStream, "accent models");
            OutputDebugStringA(Format("[MorphDict] Loading %zu accent models\n", count).c_str());
            for (size_t i = 0; i < count; ++i) {
                std::getline(annotStream, line);
                m_AccentModels.emplace_back(CAccentModel().FromString(line));
            }
            OutputDebugStringA("[MorphDict] Accent models loaded successfully\n");

            // Load prefix sets
            count = getCount(annotStream, "prefix sets");
            OutputDebugStringA(Format("[MorphDict] Loading %zu prefix sets\n", count).c_str());
            m_Prefixes.resize(1, "");
            for (size_t num = 0; num < count; num++) {
                if (!getline(annotStream, line)) {
                    OutputDebugStringA("[MorphDict] ERROR: Cannot read prefix sets\n");
                    throw CExpc("Cannot read prefix sets");
                }
                Trim(line);
                assert(!line.empty());
                m_Prefixes.push_back(line);
            }
            OutputDebugStringA("[MorphDict] Prefix sets loaded successfully\n");

            // Load lemma infos
            count = getCount(annotStream, "lemma infos");
            OutputDebugStringA(Format("[MorphDict] Loading %zu lemma infos\n", count).c_str());
            m_LemmaInfos.clear();
            ReadVectorInner(annotStream, m_LemmaInfos, count);
            OutputDebugStringA("[MorphDict] Lemma infos loaded successfully\n");

            // Load productive models
            count = getCount(annotStream, "nps infos");
            OutputDebugStringA(Format("[MorphDict] Loading %zu productive models\n", count).c_str());
            m_ProductiveModels.clear();
            ReadVectorInner(annotStream, m_ProductiveModels, count);
            assert(m_ProductiveModels.size() == m_FlexiaModels.size());
            OutputDebugStringA("[MorphDict] Productive models loaded successfully\n");

            annotStream.close();
        }
        catch (const std::exception& e) {
            OutputDebugStringA(Format("[MorphDict] ERROR while reading annotations: %s\n", e.what()).c_str());
            throw;
        }

        // Load bases
        std::string basesFile = (baseDir / basesPath).string();
        OutputDebugStringA(Format("[MorphDict] Loading bases from %s\n", basesFile.c_str()).c_str());
        m_Bases.ReadShortStringHolder(basesFile);
        OutputDebugStringA("[MorphDict] Bases loaded successfully\n");

        CreateModelsIndex();
        OutputDebugStringA("[MorphDict] Models index created\n");
        OutputDebugStringA("[MorphDict] Dictionary loaded successfully\n");
    }
    catch (const std::exception& e) {
        OutputDebugStringA(Format("[MorphDict] FATAL ERROR: %s\n", e.what()).c_str());
        throw;
    }
    catch (...) {
        OutputDebugStringA("[MorphDict] FATAL ERROR: Unknown exception\n");
        throw;
    }
};

void CMorphDict::Save(std::string GrammarFileName) const
{
    try {
        std::cout << "Saving morphological dictionary to " << GrammarFileName << std::endl;
        
        // Save forms automaton
        std::string formsFile = MakeFName(GrammarFileName, "forms_autom");
        std::cout << "Saving forms automaton to " << formsFile << std::endl;
        m_pFormAutomat->Save(formsFile);
        std::cout << "Forms automaton saved successfully" << std::endl;

        // Save annotations
        std::string annotFile = MakeFName(GrammarFileName, "annot");
        std::cout << "Saving annotations to " << annotFile << std::endl;
        std::ofstream outp(annotFile, std::ios::binary);
        if (!outp.is_open()) {
            throw CExpc(Format("Cannot write to %s", annotFile.c_str()));
        }

        try {
            SerializeFlexiaModelsToAnnotFile(outp);
            SerializeAccentModelsToAnnotFile(outp);

            assert(!m_Prefixes.empty() && m_Prefixes[0].empty());
            // do not write the first empty prefix, instead add it manually each time during loading
            outp << m_Prefixes.size() - 1 << "\n";
            for (size_t i = 1; i < m_Prefixes.size(); i++) {
                outp << m_Prefixes[i] << "\n";
            }

            outp << m_LemmaInfos.size() << "\n";
            WriteVectorStream(outp, m_LemmaInfos);

            assert(m_ProductiveModels.size() == m_FlexiaModels.size());
            outp << m_ProductiveModels.size() << "\n";
            WriteVectorStream(outp, m_ProductiveModels);
            
            outp.close();
            if (outp.fail()) {
                throw CExpc(Format("Error closing file %s", annotFile.c_str()));
            }
            std::cout << "Annotations saved successfully" << std::endl;
        }
        catch (const std::exception& e) {
            outp.close();
            throw CExpc(Format("Error saving annotations to %s: %s", annotFile.c_str(), e.what()));
        }

        // Save bases
        std::string basesFile = MakeFName(GrammarFileName, "bases");
        std::cout << "Saving bases to " << basesFile << std::endl;
        m_Bases.WriteShortStringHolder(basesFile);
        std::cout << "Bases saved successfully" << std::endl;

        // Create main morph.bin file that references the components
        std::ofstream mainFile(GrammarFileName, std::ios::binary);
        if (!mainFile.is_open()) {
            throw CExpc(Format("Cannot create main file %s", GrammarFileName.c_str()));
        }

        try {
            // Write a simple header with version and component file references
            const uint32_t MORPH_BIN_VERSION = 1;
            mainFile.write((char*)&MORPH_BIN_VERSION, sizeof(MORPH_BIN_VERSION));
            
            // Write language
            mainFile.write((char*)&m_Language, sizeof(m_Language));

            // Write component file paths relative to morph.bin
            std::string formsPath = fs::path(formsFile).filename().string();
            std::string annotPath = fs::path(annotFile).filename().string();
            std::string basesPath = fs::path(basesFile).filename().string();

            // Write path lengths and paths
            uint32_t len = formsPath.length();
            mainFile.write((char*)&len, sizeof(len));
            mainFile.write(formsPath.c_str(), len);

            len = annotPath.length();
            mainFile.write((char*)&len, sizeof(len));
            mainFile.write(annotPath.c_str(), len);

            len = basesPath.length();
            mainFile.write((char*)&len, sizeof(len));
            mainFile.write(basesPath.c_str(), len);

            mainFile.close();
            if (mainFile.fail()) {
                throw CExpc(Format("Error closing main file %s", GrammarFileName.c_str()));
            }
            std::cout << "Main morph.bin file created successfully" << std::endl;
        }
        catch (const std::exception& e) {
            mainFile.close();
            throw CExpc(Format("Error creating main file %s: %s", GrammarFileName.c_str(), e.what()));
        }

        std::cout << "Morphological dictionary saved successfully" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving morphological dictionary: " << e.what() << std::endl;
        throw; // Re-throw the exception
    }
}



