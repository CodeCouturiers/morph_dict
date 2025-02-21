// ==========  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru), 
// ==========  Copyright by Alexey Sokirko (2004)

#include "MorphDict.h"
#include "LemmaInfoSerialize.h"
#include <fstream>
#include <iostream>
#include <filesystem>

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
	const size_t textLength = Text.length();
	std::vector<CAutomAnnotationInner> additInfos;
	for (CAutomAnnotationInner& annot : Infos)
	{
		const CFlexiaModel& F = m_FlexiaModels[annot.m_ModelNo];
		const CMorphForm& M = F.m_Flexia[annot.m_ItemNo];
		size_t textStartPos = TextPos + m_Prefixes[annot.m_PrefixNo].length() + M.m_PrefixStr.length();
		std::string Base = m_Prefixes[annot.m_PrefixNo] + Text.substr(textStartPos, textLength - textStartPos - M.m_FlexiaStr.length());

		auto start = m_LemmaInfos.begin() + m_ModelsIndex[annot.m_ModelNo];
		auto end = m_LemmaInfos.begin() + m_ModelsIndex[annot.m_ModelNo + 1];

		auto pair_it = equal_range(start, end, Base.c_str(), m_SearchInfoLess);
		size_t size = pair_it.second - pair_it.first;

		assert(pair_it.first != m_LemmaInfos.end());
		{
			int LemmaStrNo = pair_it.first->m_LemmaStrNo;
			assert(Base == m_Bases[LemmaStrNo].GetString());
		}
		
		annot.m_LemmaInfoNo = pair_it.first - m_LemmaInfos.begin();

		for (decltype(pair_it.first) it = pair_it.first + 1; it != pair_it.second; ++it) {
			CAutomAnnotationInner new_annot = annot;
			annot.m_LemmaInfoNo = it - m_LemmaInfos.begin();
			additInfos.emplace_back(new_annot);
		}

	};
	Infos.insert(Infos.end(), additInfos.begin(), additInfos.end());
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
        // First read the main morph.bin file
        std::ifstream mainFile(GrammarFileName, std::ios::binary);
        if (!mainFile.is_open()) {
            throw CExpc(Format("Cannot open %s", GrammarFileName.c_str()));
        }

        // Read and verify version
        uint32_t version;
        mainFile.read((char*)&version, sizeof(version));
        if (version != 1) {
            throw CExpc(Format("Invalid morph.bin version: %u", version));
        }

        // Read language
        MorphLanguageEnum fileLanguage;
        mainFile.read((char*)&fileLanguage, sizeof(fileLanguage));
        if (fileLanguage != m_Language) {
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

        mainFile.close();

        // Get the directory containing morph.bin
        fs::path baseDir = fs::path(GrammarFileName).parent_path();

        // Load forms automaton
        std::string formsFile = (baseDir / formsPath).string();
        std::cout << "Loading forms automaton from " << formsFile << std::endl;
        m_pFormAutomat->Load(formsFile);

        // Load annotations
        std::string annotFile = (baseDir / annotPath).string();
        std::cout << "Loading annotations from " << annotFile << std::endl;
        std::ifstream annotStream(annotFile, std::ios::binary);
        if (!annotStream.is_open()) {
            throw CExpc(Format("Cannot open %s", annotFile.c_str()));
        }

        {
            m_FlexiaModels.clear();
            size_t count = getCount(annotStream, "flexia models");
            std::string l;
            for (size_t i = 0; i < count; ++i) {
                if (!getline(annotStream, l)) throw CExpc("cannot read flexia models");
                m_FlexiaModels.emplace_back(CFlexiaModel().FromString(l));
            }
        }

        {
            size_t count = getCount(annotStream, "accent models");
            std::string l;
            for (size_t i = 0; i < count; ++i) {
                std::getline(annotStream, l);
                m_AccentModels.emplace_back(CAccentModel().FromString(l));
            }
        }

        {
            size_t count = getCount(annotStream, "prefix sets");
            m_Prefixes.resize(1, "");
            for (size_t num = 0; num < count; num++) {
                std::string q;
                if (!getline(annotStream, q)) throw CExpc("cannot read annots");
                Trim(q);
                assert(!q.empty());
                m_Prefixes.push_back(q);
            }
        }

        {
            size_t count = getCount(annotStream, "lemma infos");
            m_LemmaInfos.clear();
            ReadVectorInner(annotStream, m_LemmaInfos, count);
        }

        {
            size_t count = getCount(annotStream, "nps infos");
            m_ProductiveModels.clear();
            ReadVectorInner(annotStream, m_ProductiveModels, count);
            assert(m_ProductiveModels.size() == m_FlexiaModels.size());
        }

        annotStream.close();

        // Load bases
        std::string basesFile = (baseDir / basesPath).string();
        std::cout << "Loading bases from " << basesFile << std::endl;
        m_Bases.ReadShortStringHolder(basesFile);

        CreateModelsIndex();
        std::cout << "Morphological dictionary loaded successfully" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading morphological dictionary: " << e.what() << std::endl;
        throw;
    }
}

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



