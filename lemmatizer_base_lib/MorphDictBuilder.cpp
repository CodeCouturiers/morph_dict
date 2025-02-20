// =====	=====  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru), 
// ==========  Copyright by Alexey Sokirko (2004)

#include "MorphDictBuilder.h"
#include "Lemmatizers.h"
#include "fstream"
#include <thread>
#include <future>
#include <mutex>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <locale>
#include <windows.h>

const size_t MaxLemmaPrefixCount = 0x200;
const size_t MaxLemmaCount = 0x800000;
const size_t MaxFlexiaModelsCount = 0x8000;
const size_t MaxNumberFormsInOneParadigm = 0x200;

CMorphDictBuilder::CMorphDictBuilder(size_t num_threads) 
:	CMorphDict(morphUnknown),
	m_num_threads(num_threads)
{
};

CMorphDictBuilder::~CMorphDictBuilder() 
{
};

void CMorphDictBuilder::GenerateLemmas(const MorphoWizard& Wizard) 
{
	// Устанавливаем кодировку консоли для корректного отображения русского текста
	SetConsoleOutputCP(1251);
	SetConsoleCP(1251);
	std::locale::global(std::locale(""));

	std::cout << "GenerateLemmas\n\n";
	
	// Замеряем время начала
	auto start_time = std::chrono::high_resolution_clock::now();
	
	// Определяем количество потоков
	const size_t num_threads = GetNumThreads();
	const size_t lemmas_count = Wizard.m_LemmaToParadigm.size();
	const size_t chunk_size = (lemmas_count + num_threads - 1) / num_threads;
	
	std::cout << "Processing " << lemmas_count << " lemmas using " << num_threads << " threads\n\n";

	// Создаем вектор для результатов каждого потока
	std::vector<std::vector<std::set<std::string>>> thread_info_to_bases(num_threads);
	std::vector<std::set<std::string>> thread_bases(num_threads);
	std::vector<std::thread> threads;
	std::mutex bases_mutex;
	std::atomic<size_t> processed_lemmas{0};
	std::string current_lemma;
	std::mutex lemma_mutex;

	// Запускаем поток для отображения прогресса
	std::atomic<bool> processing_complete{false};
	std::thread progress_thread([&]() {
		while (!processing_complete) {
			auto current_time = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
			double seconds = elapsed.count() / 1000.0;
			size_t current_processed = processed_lemmas.load();
			double lemmas_per_second = current_processed / seconds;
			double progress_percent = (current_processed * 100.0) / lemmas_count;

			std::string lemma_str;
			{
				std::lock_guard<std::mutex> lock(lemma_mutex);
				lemma_str = current_lemma;
			}

			std::wstring wlemma(lemma_str.begin(), lemma_str.end());
			std::wcout << L"\rProcessed " << current_processed << L"/" << lemmas_count 
				<< L" lemmas (" << std::fixed << std::setprecision(1) << progress_percent << L"%) "
				<< L"Speed: " << std::setprecision(1) << lemmas_per_second << L" lemmas/sec"
				<< L" Current: " << wlemma
				<< L"    " << std::flush;

			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
	});

	// Функция для обработки части лемм в отдельном потоке
	auto process_chunk = [&](size_t thread_id, size_t start, size_t end) {
		auto& local_info_to_bases = thread_info_to_bases[thread_id];
		auto& local_bases = thread_bases[thread_id];
		
		auto it_start = std::next(Wizard.m_LemmaToParadigm.begin(), start);
		auto it_end = std::next(Wizard.m_LemmaToParadigm.begin(), std::min(end, lemmas_count));

		for(auto lemm_it = it_start; lemm_it != it_end; ++lemm_it) {
			{
				std::lock_guard<std::mutex> lock(lemma_mutex);
				current_lemma = Wizard.get_base_string(lemm_it);
			}

			std::set<std::string> curr_bases;

			if (lemm_it->second.m_PrefixSetNo != UnknownPrefixSetNo) {
				const std::set<std::string>& s = Wizard.m_PrefixSets[lemm_it->second.m_PrefixSetNo];
				for(std::set<std::string>::const_iterator it_s = s.begin(); it_s != s.end(); it_s++)
					curr_bases.insert(*it_s + Wizard.get_base_string(lemm_it));
			}
			else {
				curr_bases.insert(Wizard.get_base_string(lemm_it));
			}

			local_info_to_bases.push_back(curr_bases);
			local_bases.insert(curr_bases.begin(), curr_bases.end());
			processed_lemmas++;
		}
	};

	// Запускаем потоки
	for(size_t i = 0; i < num_threads; ++i) {
		size_t start = i * chunk_size;
		size_t end = start + chunk_size;
		threads.emplace_back(process_chunk, i, start, end);
	}

	// Ждем завершения всех потоков
	for(auto& thread : threads) {
		thread.join();
	}

	// Объединяем результаты всех потоков
	std::vector<std::set<std::string>> InfoToBases;
	std::set<std::string> Bases;

	for(auto& thread_info : thread_info_to_bases) {
		InfoToBases.insert(InfoToBases.end(), thread_info.begin(), thread_info.end());
	}

	// Замеряем время начала объединения баз
	auto start_time_bases = std::chrono::high_resolution_clock::now();
	size_t total_bases = 0;
	for(auto& thread_base : thread_bases) {
		total_bases += thread_base.size();
	}

	std::cout << "\n\nMerging " << total_bases << " bases...\n";
	
	// Сбрасываем счетчики для этапа объединения баз
	processed_lemmas = 0;
	processing_complete = false;

	// Запускаем поток для отображения прогресса объединения баз
	std::thread progress_thread_bases([&]() {
		while (!processing_complete) {
			auto current_time = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_bases);
			double seconds = elapsed.count() / 1000.0;
			size_t current_processed = Bases.size();
			double bases_per_second = current_processed / seconds;
			double progress_percent = (current_processed * 100.0) / total_bases;

			std::cout << "\rMerging bases: " << current_processed << "/" << total_bases 
				<< " (" << std::fixed << std::setprecision(1) << progress_percent << "%) "
				<< "Speed: " << std::setprecision(1) << bases_per_second << " bases/sec"
				<< "    " << std::flush;

			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
	});

	// Объединяем базы параллельно
	std::vector<std::thread> merge_threads;
	std::mutex bases_merge_mutex;
	const size_t merge_chunk_size = (thread_bases.size() + num_threads - 1) / num_threads;

	auto merge_chunk = [&](size_t start, size_t end) {
		std::set<std::string> local_bases;
		
		// Сначала объединяем локально
		for(size_t i = start; i < end && i < thread_bases.size(); ++i) {
			local_bases.insert(thread_bases[i].begin(), thread_bases[i].end());
		}

		// Затем добавляем в общий set под мьютексом
		{
			std::lock_guard<std::mutex> lock(bases_merge_mutex);
			Bases.insert(local_bases.begin(), local_bases.end());
		}
	};

	for(size_t i = 0; i < num_threads; ++i) {
		size_t start = i * merge_chunk_size;
		size_t end = start + merge_chunk_size;
		merge_threads.emplace_back(merge_chunk, start, end);
	}

	for(auto& thread : merge_threads) {
		thread.join();
	}

	// Завершаем поток прогресса объединения баз
	processing_complete = true;
	progress_thread_bases.join();

	// Выводим статистику по объединению баз
	auto end_time_bases = std::chrono::high_resolution_clock::now();
	auto duration_bases = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_bases - start_time_bases);
	double seconds_bases = duration_bases.count() / 1000.0;
	
	std::cout << "\nBases merged in " << std::fixed << std::setprecision(2) 
		<< seconds_bases << " seconds (" << Bases.size() << " unique bases)\n\n";

	// Замеряем время CreateFromSet
	std::cout << "Starting CreateFromSet...\n";
	auto start_time_create = std::chrono::high_resolution_clock::now();
	
	m_Bases.CreateFromSet(Bases);
	
	auto end_time_create = std::chrono::high_resolution_clock::now();
	auto duration_create = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_create - start_time_create);
	double seconds_create = duration_create.count() / 1000.0;
	
	std::cout << "CreateFromSet completed in " << std::fixed << std::setprecision(2) 
		<< seconds_create << " seconds\n\n";

	// Завершаем поток прогресса перед следующим этапом
	processing_complete = true;
	progress_thread.join();

	std::cout << "\n\nCreateFromSet\n\n";

	{
		std::cout << "create LemmaInfos\n\n";
		
		// Создаем вектор для хранения результатов каждого потока
		std::vector<std::vector<CLemmaInfoAndLemma>> thread_lemma_infos(num_threads);
		std::vector<std::thread> info_threads;
		
		// Сбрасываем счетчики для нового этапа
		processed_lemmas = 0;
		processing_complete = false;

		// Размер пакета для обработки
		const size_t BATCH_SIZE = 1000;
		std::atomic<size_t> batch_counter{0};

		auto start_time_infos = std::chrono::high_resolution_clock::now();
		std::thread progress_thread([&]() {
			while (!processing_complete) {
				auto current_time = std::chrono::high_resolution_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_infos);
				double seconds = elapsed.count() / 1000.0;
				size_t current_processed = processed_lemmas.load();
				double lemmas_per_second = current_processed / seconds;
				double progress_percent = (current_processed * 100.0) / lemmas_count;
				double eta_seconds = (lemmas_count - current_processed) / (lemmas_per_second > 0 ? lemmas_per_second : 1);

				std::string lemma_str;
				{
					std::lock_guard<std::mutex> lock(lemma_mutex);
					lemma_str = current_lemma;
				}

				std::cout << "\rProcessing lemma infos: " << current_processed << "/" << lemmas_count 
					<< " (" << std::fixed << std::setprecision(1) << progress_percent << "%) "
					<< "Speed: " << std::setprecision(1) << lemmas_per_second << " lemmas/sec "
					<< "ETA: " << std::setprecision(0) << eta_seconds << "s "
					<< "Current: " << lemma_str
					<< "    " << std::flush;

				std::this_thread::sleep_for(std::chrono::milliseconds(300));
			}
		});

		// Функция для обработки части лемм в отдельном потоке
		auto process_lemma_infos = [&](size_t thread_id) {
			auto& local_lemma_infos = thread_lemma_infos[thread_id];
			local_lemma_infos.reserve(lemmas_count / num_threads);  // Предварительное резервирование памяти

			while (true) {
				// Получаем следующий пакет для обработки
				size_t batch_start = batch_counter.fetch_add(BATCH_SIZE);
				if (batch_start >= lemmas_count) break;

				size_t batch_end = std::min(batch_start + BATCH_SIZE, lemmas_count);
				std::vector<CLemmaInfoAndLemma> batch_results;
				batch_results.reserve(BATCH_SIZE);

				for (size_t i = batch_start; i < batch_end; ++i) {
					auto lemm_it = std::next(Wizard.m_LemmaToParadigm.begin(), i);
					CLemmaInfoAndLemma I;
					
					{
						std::lock_guard<std::mutex> lock(lemma_mutex);
						current_lemma = Wizard.get_base_string(lemm_it);
					}
					
					for (std::set<std::string>::const_iterator it = InfoToBases[i].begin(); 
						it != InfoToBases[i].end(); ++it) {
						std::vector<CShortString>::const_iterator base_it = 
							lower_bound(m_Bases.begin(), m_Bases.end(), it->c_str(), IsLessShortString());
						assert(base_it != m_Bases.end());
						assert(*it == base_it->GetString());
						I.m_LemmaStrNo = base_it - m_Bases.begin();
						I.m_LemmaInfo = lemm_it->second;
						batch_results.push_back(I);
					}
					processed_lemmas++;
				}

				// Добавляем результаты пакета в локальный вектор
				local_lemma_infos.insert(local_lemma_infos.end(), 
									   std::make_move_iterator(batch_results.begin()),
									   std::make_move_iterator(batch_results.end()));
			}
		};

		// Запускаем потоки
		for (size_t i = 0; i < num_threads; ++i) {
			info_threads.emplace_back(process_lemma_infos, i);
		}

		// Ждем завершения всех потоков
		for (auto& thread : info_threads) {
			thread.join();
		}

		// Останавливаем поток прогресса
		processing_complete = true;
		progress_thread.join();

		// Подсчитываем общий размер результата
		size_t total_size = 0;
		for (const auto& thread_infos : thread_lemma_infos) {
			total_size += thread_infos.size();
		}

		// Резервируем память для финального результата
		m_LemmaInfos.reserve(total_size);

		// Объединяем результаты всех потоков
		std::cout << "\n\nMerging results from " << thread_lemma_infos.size() << " threads...\n";
		for (auto& thread_infos : thread_lemma_infos) {
			m_LemmaInfos.insert(m_LemmaInfos.end(),
							   std::make_move_iterator(thread_infos.begin()),
							   std::make_move_iterator(thread_infos.end()));
		}

		// Сортируем общий результат
		std::cout << "Sorting " << m_LemmaInfos.size() << " lemma infos...\n";
		sort(m_LemmaInfos.begin(), m_LemmaInfos.end());

		// Выводим финальную статистику
		auto end_time_infos = std::chrono::high_resolution_clock::now();
		auto duration_infos = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_infos - start_time_infos);
		double seconds_infos = duration_infos.count() / 1000.0;
		double final_speed = lemmas_count / seconds_infos;

		std::cout << "\nLemma infos processing completed in " << std::fixed << std::setprecision(2) 
			<< seconds_infos << " seconds\n";
		std::cout << "Final processing speed: " << std::setprecision(2) 
			<< final_speed << " lemmas/second\n\n";
	}

	if (m_LemmaInfos.size() >= MaxLemmaCount) {
		throw CExpc("Cannot be more than %i lemmas\n", MaxLemmaCount-1); 
	}

	// Замеряем время окончания и выводим финальную статистику
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
	double seconds = duration.count() / 1000.0;
	double lemmas_per_second = lemmas_count / seconds;

	std::cout << "\n\nGenerateLemmas completed in " << std::fixed << std::setprecision(2) 
		<< seconds << " seconds\n";
	std::cout << "Final processing speed: " << std::fixed << std::setprecision(2) 
		<< lemmas_per_second << " lemmas/second\n\n";
}

void CMorphDictBuilder::GenerateUnitedFlexModels(const MorphoWizard& Wizard)
{
	printf("GenerateUnitedFlexModels\n");
	
	const size_t num_threads = GetNumThreads();
	const size_t models_count = Wizard.m_FlexiaModels.size();
	const size_t chunk_size = (models_count + num_threads - 1) / num_threads;

	// Creating m_ModelInfo
	m_ModelInfo.clear();
	m_FlexiaModels.clear();
	m_ProductiveModels.clear();

	if(models_count >= MaxFlexiaModelsCount) {
		throw CExpc("Cannot be more than %i flexia models\n", MaxFlexiaModelsCount-1); 
	}

	// Предварительно резервируем память
	m_ModelInfo.resize(models_count);
	m_FlexiaModels.resize(models_count);
	m_ProductiveModels.resize(models_count);

	std::vector<std::thread> threads;
	std::mutex models_mutex;

	// Функция для обработки части моделей в отдельном потоке
	auto process_models = [&](size_t start, size_t end) {
		for(size_t i = start; i < end && i < models_count; ++i) {
			// Создаем локальную копию модели для модификации
			CFlexiaModel p = Wizard.m_FlexiaModels[i];

			// Проверяем размер модели
			if(p.m_Flexia.size() >= MaxNumberFormsInOneParadigm) {
				throw CExpc("Error: flexia %s contains more than %i forms!", 
					p.ToString().c_str(), MaxNumberFormsInOneParadigm);
			}

			// Устанавливаем признак продуктивности
			auto pos = Wizard.m_pGramTab->GetPartOfSpeech(p.get_first_code().c_str());
			m_ProductiveModels[i] = Wizard.m_pGramTab->PartOfSpeechIsProductive(pos) ? 1 : 0;

			// Создаем вектор флагов для форм
			std::vector<bool> model_info(p.m_Flexia.size(), true);

			// Объединяем одинаковые формы
			for(size_t j = 0; j < p.m_Flexia.size(); j++) {
				if(model_info[j]) {
					for(size_t k = j + 1; k < p.m_Flexia.size(); k++) {
						if((p.m_Flexia[k].m_FlexiaStr == p.m_Flexia[j].m_FlexiaStr) &&
						   (p.m_Flexia[k].m_PrefixStr == p.m_Flexia[j].m_PrefixStr))
						{
							model_info[k] = false;
							p.m_Flexia[j].m_Gramcode.insert(
								p.m_Flexia[j].m_Gramcode.end(),
								p.m_Flexia[k].m_Gramcode.begin(),
								p.m_Flexia[k].m_Gramcode.end()
							);
						}
					}
				}
			}

			// Сохраняем результаты
			m_ModelInfo[i] = std::move(model_info);
			m_FlexiaModels[i] = std::move(p);
		}
	};

	// Запускаем потоки
	for(size_t i = 0; i < num_threads; ++i) {
		size_t start = i * chunk_size;
		size_t end = start + chunk_size;
		threads.emplace_back(process_models, start, end);
	}

	// Ждем завершения всех потоков
	for(auto& thread : threads) {
		thread.join();
	}
}

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

void CMorphDictBuilder::CreateAutomat(const MorphoWizard& Wizard)
{
	GetFormBuilder()->InitTrie();
	m_AccentModels = Wizard.m_AccentModels;
	GeneratePrefixes(Wizard);

	const size_t num_threads = GetNumThreads();
	const size_t lemmas_count = Wizard.m_LemmaToParadigm.size();
	const size_t BATCH_SIZE = 10000;  // Увеличенный размер пакета
	const size_t FORMS_BUFFER_SIZE = BATCH_SIZE * 20; // Предполагаемый размер буфера форм

	printf("Generate the main automat ...\n");
	std::atomic<size_t> FormsCount{0};
	std::atomic<size_t> LemmaNo{0};
	std::mutex automat_mutex;
	std::string current_lemma;
	std::mutex lemma_mutex;

	// Запускаем поток для отображения прогресса с меньшей частотой обновления
	auto start_time = std::chrono::high_resolution_clock::now();
	std::atomic<bool> processing_complete{false};
	std::thread progress_thread([&]() {
		std::string prev_lemma;
		while (!processing_complete) {
			auto current_time = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
			double seconds = elapsed.count() / 1000.0;
			size_t current_processed = LemmaNo.load();
			double lemmas_per_second = current_processed / seconds;
			double progress_percent = (current_processed * 100.0) / lemmas_count;
			double eta_seconds = (lemmas_count - current_processed) / (lemmas_per_second > 0 ? lemmas_per_second : 1);

			std::string lemma_str;
			{
				std::lock_guard<std::mutex> lock(lemma_mutex);
				lemma_str = current_lemma;
			}

			// Обновляем только если изменилась лемма или прошло достаточно времени
			if (lemma_str != prev_lemma) {
				std::cout << "\rProcessing automat: " << current_processed << "/" << lemmas_count 
					<< " (" << std::fixed << std::setprecision(1) << progress_percent << "%) "
					<< "Speed: " << std::setprecision(1) << lemmas_per_second << " lemmas/sec "
					<< "Forms: " << FormsCount.load() << " "
					<< "ETA: " << std::setprecision(0) << eta_seconds << "s "
					<< "Current: " << lemma_str
					<< "    " << std::flush;
				prev_lemma = lemma_str;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Увеличенный интервал обновления
		}
	});

	std::vector<std::thread> threads;
	DwordVector EmptyGlobalPrefixes(1, 0);
	std::atomic<size_t> batch_counter{0};

	// Структура для хранения результатов пакетной обработки
	struct BatchResult {
		std::vector<std::string> forms;
		size_t forms_count;

		BatchResult() : forms_count(0) {
			forms.reserve(FORMS_BUFFER_SIZE);
		}
	};

	// Функция для обработки пакета лемм
	auto process_batch = [&]() {
		BatchResult local_result;
		
		while (true) {
			// Получаем следующий пакет
			size_t batch_start = batch_counter.fetch_add(BATCH_SIZE);
			if (batch_start >= lemmas_count) break;

			size_t batch_end = std::min(batch_start + BATCH_SIZE, lemmas_count);
			
			for (size_t i = batch_start; i < batch_end; ++i) {
				auto it = std::next(Wizard.m_LemmaToParadigm.begin(), i);
				
				{
					std::lock_guard<std::mutex> lock(lemma_mutex);
					current_lemma = Wizard.get_base_string(it);
				}
				
				size_t ModelNo = it->second.m_FlexiaModelNo;
				if (ModelNo > Wizard.m_FlexiaModels.size()) {
					throw CExpc("Bad flexia model: %s\n", Wizard.get_lemm_string(it).c_str());
				}

				DwordVector* pPrefixVector = &EmptyGlobalPrefixes;
				if (it->second.m_PrefixSetNo != UnknownPrefixSetNo)
					pPrefixVector = &(m_PrefixSets[it->second.m_PrefixSetNo]);

				assert(!pPrefixVector->empty());

				const CFlexiaModel& p = Wizard.m_FlexiaModels[ModelNo];
				const std::vector<bool>& Infos = m_ModelInfo[ModelNo];
				
				for (size_t PrefixNo = 0; PrefixNo < pPrefixVector->size(); PrefixNo++) {
					std::string base = Wizard.get_base_string(it);
					
					for (size_t ItemNo = 0; ItemNo < p.m_Flexia.size(); ItemNo++)
					if (Infos[ItemNo]) {
						std::string WordForm = m_Prefixes[(*pPrefixVector)[PrefixNo]];
						WordForm += p.m_Flexia[ItemNo].m_PrefixStr;
						WordForm += base;
						WordForm += p.m_Flexia[ItemNo].m_FlexiaStr;
						WordForm += GetFormBuilder()->m_AnnotChar;

						uint32_t info = GetFormBuilder()->EncodeMorphAutomatInfo(ModelNo, ItemNo, (*pPrefixVector)[PrefixNo]);
						WordForm += GetFormBuilder()->EncodeIntToAlphabet(info);
						
						local_result.forms.push_back(std::move(WordForm));
						local_result.forms_count++;
					}
				}

				LemmaNo++;

				// Добавляем формы в автомат пакетами для уменьшения блокировок
				if (local_result.forms.size() >= FORMS_BUFFER_SIZE) {
					std::lock_guard<std::mutex> lock(automat_mutex);
					for (const auto& form : local_result.forms) {
						GetFormBuilder()->AddStringDaciuk(form);
					}
					FormsCount += local_result.forms_count;
					local_result.forms.clear();
					local_result.forms_count = 0;
					local_result.forms.reserve(FORMS_BUFFER_SIZE);
				}
			}
		}

		// Добавляем оставшиеся формы
		if (!local_result.forms.empty()) {
			std::lock_guard<std::mutex> lock(automat_mutex);
			for (const auto& form : local_result.forms) {
				GetFormBuilder()->AddStringDaciuk(form);
			}
			FormsCount += local_result.forms_count;
		}
	};

	// Запускаем потоки
	for (size_t i = 0; i < num_threads; ++i) {
		threads.emplace_back(process_batch);
	}

	// Ждем завершения всех потоков
	for (auto& thread : threads) {
		thread.join();
	}

	// Останавливаем поток прогресса
	processing_complete = true;
	progress_thread.join();

	// Выводим финальную статистику
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
	double seconds = duration.count() / 1000.0;
	double final_speed = lemmas_count / seconds;

	std::cout << "\n\nAutomat generation completed in " << std::fixed << std::setprecision(2) 
		<< seconds << " seconds\n";
	std::cout << "Final processing speed: " << std::setprecision(2) 
		<< final_speed << " lemmas/second\n";
	std::cout << "Total forms generated: " << FormsCount.load() << "\n\n";

	if (LemmaNo > 0xffffff) {
		throw CExpc("Cannot be more than 0xffffff lemmas"); 
	}

	GetFormBuilder()->ClearRegister();
	std::cout << "Converting build relations to relations for word forms...\n";
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

	// Create output directory with proper permissions
	try {
		fs::path out_dir = fs::path(output_folder);
		
		// Create all parent directories if they don't exist
		if (!fs::exists(out_dir.parent_path())) {
			if (!fs::create_directories(out_dir.parent_path())) {
				throw std::runtime_error("Failed to create parent directories for " + output_folder);
			}
		}
		
		// Create the output directory if it doesn't exist
		if (!fs::exists(out_dir)) {
			if (!fs::create_directories(out_dir)) {
				throw std::runtime_error("Failed to create directory " + output_folder);
			}
		}

		// Set directory permissions to allow full access
		#ifdef _WIN32
		fs::permissions(out_dir, 
			fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec,
			fs::perm_options::replace);
		#else
		fs::permissions(out_dir, 
			fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec);
		#endif

	} catch (const fs::filesystem_error& e) {
		throw CExpc("Filesystem error: %s", e.what());
	} catch (const std::exception& e) {
		throw CExpc("Error creating output directory: %s", e.what());
	}

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
		
		// Ensure parent directory exists with proper permissions before saving
		try {
			if (!fs::exists(outFileName.parent_path())) {
				if (!fs::create_directories(outFileName.parent_path())) {
					throw std::runtime_error("Failed to create directory for " + outFileName.string());
				}
				#ifdef _WIN32
				fs::permissions(outFileName.parent_path(), 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace);
				#else
				fs::permissions(outFileName.parent_path(), 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec);
				#endif
			}
		} catch (const fs::filesystem_error& e) {
			throw CExpc("Filesystem error while creating output directory: %s", e.what());
		}

		Save(outFileName.string());
		LOGI << "Successful written indices of the main automat to " << outFileName << std::endl;
		if (!opts.get_value()["SkipPredictBase"].GetBool()) {
			if (!GenPredictIdx(wizard, postfix_len, min_freq, output_folder, opts))
			{
				throw CExpc("Cannot create prediction base");
			};
		}
	}

	{
		auto opt_path = fs::path(output_folder) / OPTIONS_FILE;
		try {
			// Ensure parent directory exists with proper permissions
			if (!fs::exists(opt_path.parent_path())) {
				if (!fs::create_directories(opt_path.parent_path())) {
					throw std::runtime_error("Failed to create directory for " + opt_path.string());
				}
				#ifdef _WIN32
				fs::permissions(opt_path.parent_path(), 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace);
				#else
				fs::permissions(opt_path.parent_path(), 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec);
				#endif
			}
			LOGI << "writing options file " << opt_path;
			opts.dump_rapidjson_pretty(opt_path.string());
		} catch (const fs::filesystem_error& e) {
			throw CExpc("Filesystem error while writing options file: %s", e.what());
		}
	}

	{
		fs::path src = wizard.m_GramtabPath;
		fs::path trg = output_folder / wizard.m_GramtabPath.filename();
		try {
			// Ensure parent directory exists with proper permissions
			if (!fs::exists(trg.parent_path())) {
				if (!fs::create_directories(trg.parent_path())) {
					throw std::runtime_error("Failed to create directory for " + trg.string());
				}
				#ifdef _WIN32
				fs::permissions(trg.parent_path(), 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace);
				#else
				fs::permissions(trg.parent_path(), 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read | fs::perms::others_exec);
				#endif
			}
			if (!fs::exists(trg) || !fs::equivalent(src, trg)) {
				fs::copy_file(src, trg, fs::copy_options::overwrite_existing);
				// Set file permissions
				#ifdef _WIN32
				fs::permissions(trg, 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read,
					fs::perm_options::replace);
				#else
				fs::permissions(trg, 
					fs::perms::owner_all | fs::perms::group_all | fs::perms::others_read);
				#endif
			}
		} catch (const fs::filesystem_error& e) {
			throw CExpc("Filesystem error while copying gramtab file: %s", e.what());
		}
	}
}
