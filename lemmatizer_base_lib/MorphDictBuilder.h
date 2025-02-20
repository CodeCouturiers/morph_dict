// ==========  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru)
// ==========  Copyright by Alexey Sokirko

#pragma once

#include "MorphDict.h"
#include "MorphAutomBuilder.h"

#include "../morph_wizard/wizard.h"
#include "../common/rapidjson.h"

#include <thread>

class CMorphDictBuilder : public CMorphDict
{
private:
	size_t m_num_threads;

	// m_ModelInfo[i][j] is a word which should be written into CTrieNodeBuild::m_Info
	//  where i is the index of MorphoWizard::m_FlexiaModels
	//  and j is a the index of CFlexiaModel::m_Flexia
	std::vector< std::vector <bool> >		m_ModelInfo;
	std::vector< DwordVector >	m_PrefixSets;

	void				ClearRegister();
	bool				CheckFlexiaGramInfo(const MorphoWizard& Wizard) const;
	void				GeneratePrefixes(const MorphoWizard& Wizard);
	bool				CheckRegister() const;

	CMorphAutomatBuilder* GetFormBuilder() { return (CMorphAutomatBuilder*)m_pFormAutomat; };
	void	CreateAutomat(const MorphoWizard& Wizard);
	void	GenerateLemmas(const MorphoWizard& Wizard);
	void	GenerateUnitedFlexModels(const MorphoWizard& Wizard);
	bool	GenPredictIdx(const MorphoWizard& wizard, int PostfixLength, int MinFreq, std::string path, CJsonObject& output_opts);

public:
	CMorphDictBuilder(size_t num_threads = 0);  // 0 means use hardware_concurrency
	~CMorphDictBuilder();

	size_t GetNumThreads() const { 
		return m_num_threads == 0 ? std::thread::hardware_concurrency() : m_num_threads; 
	}
	void SetNumThreads(size_t num_threads) { m_num_threads = num_threads; }

	void BuildLemmatizer(std::string mwz_path, bool allow_russian_jo, int postfix_len, int min_freq, std::string output_folder);

};


