// =====	=====  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru), 
// ==========  Copyright by Alexey Sokirko (2004)

#include "MorphDictBuilder.h"
#include "MorphAutomBuilder.h"
#include "Lemmatizers.h"
#include "fstream"
#include <chrono>
#include <iomanip>

const size_t MaxLemmaPrefixCount = 0x200;
const size_t MaxLemmaCount = 0x800000;
const size_t MaxFlexiaModelsCount = 0x8000;
const size_t MaxNumberFormsInOneParadigm = 0x200;

CMorphDictBuilder::CMorphDictBuilder() 
:	CMorphDict(morphUnknown)
{
	
};

CMorphDictBuilder::~CMorphDictBuilder() 
{
};

CMorphAutomatBuilder* CMorphDictBuilder::GetFormBuilder() 
{ 
    return (CMorphAutomatBuilder*)m_pFormAutomat; 
}

void CMorphDictBuilder::GenerateLemmas(const MorphoWizard& Wizard) 
{
	std::cout << "GenerateLemmas\n";
	std::vector<std::set<std::string> > InfoToBases;
	{	// creaing CMorphDict::m_Bases
		std::set<std::string> Bases;
		
		auto start_time = std::chrono::steady_clock::now();
		auto last_update = start_time;
		const auto update_interval = std::chrono::seconds(1);
		size_t base_count = 0;
		size_t total_lemmas = Wizard.m_LemmaToParadigm.size();
		
		for( const_lemma_iterator_t lemm_it= Wizard.m_LemmaToParadigm.begin(); lemm_it!=Wizard.m_LemmaToParadigm.end(); lemm_it++ )
		{
			std::set<std::string> curr_bases;

			if (lemm_it->second.m_PrefixSetNo != UnknownPrefixSetNo)
			{
				const std::set<std::string>& s = Wizard.m_PrefixSets[lemm_it->second.m_PrefixSetNo];
				for(std::set<std::string>::const_iterator it_s = s.begin(); it_s != s.end(); it_s++)
					curr_bases.insert(*it_s+Wizard.get_base_string(lemm_it));
			}
			else
				curr_bases.insert(Wizard.get_base_string(lemm_it));

			InfoToBases.push_back(curr_bases);
			base_count += curr_bases.size();
			Bases.insert(curr_bases.begin(), curr_bases.end());
			
			if (!(base_count % 3000)) {
				auto current_time = std::chrono::steady_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_update);
				
				if (elapsed >= update_interval) {
					double bases_per_sec = static_cast<double>(base_count) / 
						std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
					std::cerr << "Processing bases: " << base_count << " (" << std::fixed 
						<< std::setprecision(1) << bases_per_sec << " bases/s)\r";
					last_update = current_time;
				}
			}
		};

		auto end_time = std::chrono::steady_clock::now();
		double total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
		double avg_bases_per_sec = static_cast<double>(base_count) / total_time;

		std::cerr << "\nProcessed " << base_count << " bases in " << total_time << " seconds ("
			<< std::fixed << std::setprecision(1) << avg_bases_per_sec << " bases/s)\n";

		std::cout << "CreateFromSet\n";
		m_Bases.CreateFromSet(Bases);
	};

	{
		std::cout << "create LemmaInfos\n";
		size_t Index = 0;
		for( const_lemma_iterator_t lemm_it= Wizard.m_LemmaToParadigm.begin(); lemm_it!=Wizard.m_LemmaToParadigm.end(); lemm_it++ )
		{
			CLemmaInfoAndLemma I;
			
			for(std::set<std::string>::const_iterator it = InfoToBases[Index].begin(); it != InfoToBases[Index].end(); it++)
			{
				std::vector<CShortString>::const_iterator base_it =  lower_bound(m_Bases.begin(), m_Bases.end(), it->c_str(), IsLessShortString());
				assert (base_it != m_Bases.end());
				assert (*it == base_it->GetString());
				I.m_LemmaStrNo = base_it - m_Bases.begin();
				I.m_LemmaInfo = lemm_it->second;
				m_LemmaInfos.push_back(I);
			}
			Index++;
		};
		sort(m_LemmaInfos.begin(), m_LemmaInfos.end());
	};

	
	if (m_LemmaInfos.size() >= MaxLemmaCount)
	{
		throw CExpc ("Cannot be more than %i lemmas\n", MaxLemmaCount-1); 
	};

}



void  CMorphDictBuilder::GenerateUnitedFlexModels(const MorphoWizard& Wizard)
{
	printf ("GenerateUnitedFlexModels\n");
	// Creating m_ModelInfo
	m_ModelInfo.clear();
	m_FlexiaModels.clear();
	m_ProductiveModels.clear();
	if (Wizard.m_FlexiaModels.size() >=  MaxFlexiaModelsCount)
	{
		throw CExpc ("Cannot be more than %i flexia models\n", MaxFlexiaModelsCount-1); 
	};

	for(auto p : Wizard.m_FlexiaModels)
	{
		{
			auto pos = Wizard.m_pGramTab->GetPartOfSpeech(p.get_first_code().c_str());
			m_ProductiveModels.push_back(Wizard.m_pGramTab->PartOfSpeechIsProductive(pos) ? 1 : 0);
		}
		m_ModelInfo.push_back(std::vector<bool>(p.m_Flexia.size(), true));

		if ( p.m_Flexia.size() >=  MaxNumberFormsInOneParadigm)
		{
			throw CExpc ("Error: flexia %s contains more than %i forms. !", p.ToString().c_str(), MaxNumberFormsInOneParadigm);
		};

		for (size_t i=0; i <p.m_Flexia.size(); i++)
			if (m_ModelInfo.back()[i])
				for (size_t j=i+1; j <p.m_Flexia.size(); j++)
					if (		(p.m_Flexia[j].m_FlexiaStr ==  p.m_Flexia[i].m_FlexiaStr)
							&&	(p.m_Flexia[j].m_PrefixStr ==  p.m_Flexia[i].m_PrefixStr)
						)
						{
							m_ModelInfo.back()[j] = false;
							p.m_Flexia[i].m_Gramcode.insert(p.m_Flexia[i].m_Gramcode.end(),p.m_Flexia[j].m_Gramcode.begin(),p.m_Flexia[j].m_Gramcode.end());
						};
		m_FlexiaModels.push_back(p);
	};
};

// generate unique prefixes over all prefix sets
void  CMorphDictBuilder::GeneratePrefixes(const MorphoWizard& Wizard)
{
	printf ("GeneratePrefixes\n");
	m_Prefixes.clear();
	// add the empty prefix
	m_Prefixes.push_back("");
	for (auto prefix_set: Wizard.m_PrefixSets)
	{
		m_PrefixSets.push_back(DwordVector());
		
		for (auto prefix : prefix_set)
		{
			StringVector::iterator it_c = find(m_Prefixes.begin(), m_Prefixes.end(), prefix);
			if (it_c == m_Prefixes.end())
				it_c = m_Prefixes.insert(m_Prefixes.end(), prefix);
			m_PrefixSets.back().push_back(it_c - m_Prefixes.begin());
		};
		if (m_PrefixSets.back().empty())
		{
			throw CExpc("empty prefix set found"); 
		};

	};
	if (m_Prefixes.size() >= MaxLemmaPrefixCount)
	{
		throw CExpc("Cannot be more than %i prefixes\n", MaxLemmaPrefixCount-1);
	};
};



extern size_t RegisterSize;

void  CMorphDictBuilder::CreateAutomat(const MorphoWizard& Wizard)
{
	GetFormBuilder()->InitTrie();
	m_AccentModels = Wizard.m_AccentModels;
	GeneratePrefixes(Wizard);
	
	// Pre-calculate total forms for better memory allocation
	size_t totalForms = 0;
	for(const_lemma_iterator_t it = Wizard.m_LemmaToParadigm.begin(); it != Wizard.m_LemmaToParadigm.end(); it++) {
		size_t ModelNo = it->second.m_FlexiaModelNo;
		const CFlexiaModel& p = Wizard.m_FlexiaModels[ModelNo];
		const std::vector<bool>& Infos = m_ModelInfo[ModelNo];
		size_t prefixCount = (it->second.m_PrefixSetNo != UnknownPrefixSetNo) ? 
			m_PrefixSets[it->second.m_PrefixSetNo].size() : 1;
		
		for(size_t i = 0; i < p.m_Flexia.size(); i++) {
			if(Infos[i]) totalForms += prefixCount;
		}
	}

	// Reserve space for better performance
	GetFormBuilder()->ReserveSpace(totalForms);

	// Creating tries for paradigms
	size_t RuleNo = 0; 
	size_t LemmaNo = 0;
	size_t ReusedNodes = 0;
	DwordVector EmptyGlobalPrefixes(1, 0);
	printf ("Generate the main automat ...\n");
	size_t FormsCount = 0;
	
	// Add timing variables
	auto start_time = std::chrono::steady_clock::now();
	auto last_update = start_time;
	const auto update_interval = std::chrono::seconds(1);

	// Process lemmas in batches to reduce register pressure
	const size_t BATCH_SIZE = 1000;
	std::vector<std::string> wordFormBatch;
	wordFormBatch.reserve(BATCH_SIZE * 2); // Reserve extra space for variations
	
	for(const_lemma_iterator_t it = Wizard.m_LemmaToParadigm.begin(); it != Wizard.m_LemmaToParadigm.end(); it++)
	{
		if (!(LemmaNo % 3000)) {
			auto current_time = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_update);
			
			if (elapsed >= update_interval) {
				double forms_per_sec = static_cast<double>(FormsCount) / 
					std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
				std::cerr << "Lemma " << LemmaNo << "/" << Wizard.m_LemmaToParadigm.size() 
					<< " Forms: " << FormsCount << " (" << std::fixed << std::setprecision(1) 
					<< forms_per_sec << " forms/s) RegisterSize=" << RegisterSize << "\r";
				last_update = current_time;
			}
		}

		size_t ModelNo = it->second.m_FlexiaModelNo;
		if (ModelNo  > Wizard.m_FlexiaModels.size())
		{
			throw CExpc("Bad flexia model  : %s\n", Wizard.get_lemm_string(it).c_str());
		};

		DwordVector* pPrefixVector = &EmptyGlobalPrefixes;
		if (it->second.m_PrefixSetNo != UnknownPrefixSetNo)
			pPrefixVector = & (m_PrefixSets[it->second.m_PrefixSetNo] );

		assert (!pPrefixVector->empty());

		const CFlexiaModel&p = Wizard.m_FlexiaModels[ModelNo];
		const std::vector <bool>& Infos = m_ModelInfo[ModelNo];
		
		for (size_t PrefixNo = 0; PrefixNo < pPrefixVector->size(); PrefixNo++)
		{
			std::string base = Wizard.get_base_string(it);
			
			for (size_t ItemNo = 0; ItemNo < p.m_Flexia.size(); ItemNo++)
			if (Infos[ItemNo])
			{
				std::string WordForm = m_Prefixes[(*pPrefixVector)[PrefixNo]];
				WordForm += p.m_Flexia[ItemNo].m_PrefixStr;
				WordForm += base;
				WordForm += p.m_Flexia[ItemNo].m_FlexiaStr;

				WordForm += GetFormBuilder()->m_AnnotChar;
				{
					FormsCount++;
					uint32_t info = GetFormBuilder()->EncodeMorphAutomatInfo(ModelNo, ItemNo, (*pPrefixVector)[PrefixNo]);

					{	// checking encoding
						size_t checkModelNo, checkItemNo,checkPrefixNo;
						GetFormBuilder()->DecodeMorphAutomatInfo(info, checkModelNo, checkItemNo, checkPrefixNo);

						if (		(checkModelNo != ModelNo)
								||	(checkItemNo != ItemNo)
								||	(checkPrefixNo != (*pPrefixVector)[PrefixNo])
							)
						{
							throw CExpc ("General annotation encoding error!");
						};
					};

					WordForm += GetFormBuilder()->EncodeIntToAlphabet(info);
					wordFormBatch.push_back(std::move(WordForm));
				}

				// Process batch if full
				if(wordFormBatch.size() >= BATCH_SIZE) {
					for(const auto& form : wordFormBatch) {
						GetFormBuilder()->AddStringDaciuk(form);
					}
					wordFormBatch.clear();
				}
			}
		}
		LemmaNo++;
	}

	// Process remaining forms in the last batch
	for(const auto& form : wordFormBatch) {
		GetFormBuilder()->AddStringDaciuk(form);
	}

	auto end_time = std::chrono::steady_clock::now();
	double total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
	double avg_forms_per_sec = static_cast<double>(FormsCount) / total_time;

	std::cerr << "\nProcessed " << LemmaNo << " lemmas and " << FormsCount << " forms "
		<< "in " << total_time << " seconds (" 
		<< std::fixed << std::setprecision(1) << avg_forms_per_sec << " forms/s)\n";

	if (LemmaNo >  0xffffff)
	{
		throw CExpc("Cannot be more than 0xffffff lemmas"); 
	};
	fprintf (stderr,"Count of word forms =  %zu\n", FormsCount);
	
	GetFormBuilder()->ClearRegister();
	fprintf(stderr, "ConvertBuildRelationsToRelations for word forms...  \n");
	GetFormBuilder()->ConvertBuildRelationsToRelations();
}

void create_options(CJsonObject& opts, bool allow_russian_jo, int postfix_len, int min_freq) {
	opts.add_bool("AllowRussianJo", allow_russian_jo);

	bool skip_predict = false;
	if (postfix_len == -1 || min_freq == -1) {
		LOGI << "skip prediction base generation ";
		skip_predict = true;
	}
	else {
		if (!(0 < postfix_len && postfix_len <= 5))
		{
			throw CExpc("postfix_len should be between 1 and 5");
		};
		if (min_freq <= 0) {
			throw CExpc("MinFreq should be more than 0");
		};
	}
	opts.add_bool("SkipPredictBase", skip_predict);
}

void  CMorphDictBuilder::BuildLemmatizer(std::string mwz_path, bool allow_russian_jo, int postfix_len, int min_freq, std::string output_folder) {
	rapidjson::Document d_opts(rapidjson::kObjectType);
	CJsonObject opts(d_opts);

	create_options(opts, allow_russian_jo, postfix_len, min_freq);

	MorphoWizard wizard;
	wizard.load_wizard(mwz_path.c_str(), "guest", false, true, true);
	m_Language = wizard.m_Language;
	InitAutomat(new CMorphAutomatBuilder(m_Language, MorphAnnotChar));
	if (!allow_russian_jo)
	{
		wizard.convert_je_to_jo();
	};
	{
		GenerateLemmas(wizard);
		GenerateUnitedFlexModels(wizard);
		CreateAutomat(wizard);
		LOGI << "Saving...";
		auto outFileName = fs::path(output_folder) / MORPH_MAIN_FILES;
		std::cerr << "Attempting to save morphological dictionary components..." << std::endl;
		
		try {
			Save(outFileName.string());
			
			// Check component files
			auto formsFile = fs::path(outFileName).replace_extension("forms_autom");
			auto annotFile = fs::path(outFileName).replace_extension("annot");
			auto basesFile = fs::path(outFileName).replace_extension("bases");
			
			size_t total_size = 0;
			if (fs::exists(formsFile)) total_size += fs::file_size(formsFile);
			if (fs::exists(annotFile)) total_size += fs::file_size(annotFile);
			if (fs::exists(basesFile)) total_size += fs::file_size(basesFile);
			
			std::cerr << "Successfully saved morphological dictionary components, total size: " 
				<< total_size << " bytes" << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "Exception while saving morphological dictionary: " << e.what() << std::endl;
			throw; // Re-throw to maintain error propagation
		}

		LOGI << "Successfully written indices of the main automat to " << outFileName << std::endl;
		if (!opts.get_value()["SkipPredictBase"].GetBool()) {
			if (!GenPredictIdx(wizard, postfix_len, min_freq, output_folder, opts))
			{
				throw CExpc("Cannot create prediction base");
			};
		}
	}

	{
		auto opt_path = fs::path(output_folder) / OPTIONS_FILE;
		LOGI << "writing options file " << opt_path;
		opts.dump_rapidjson_pretty(opt_path.string());
	}

	{
		fs::path src = wizard.m_GramtabPath;
		fs::path trg = output_folder / wizard.m_GramtabPath.filename();
		if (!fs::exists(trg) || !fs::equivalent(src, trg)) {
			fs::copy_file(src, trg, fs::copy_options::overwrite_existing);
		}
	}
}
