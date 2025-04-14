// ==========  This file is under  LGPL, the GNU Lesser General Public License
// ==========  Dialing Lemmatizer (www.aot.ru)
// ==========  Copyright by Alexey Sokirko

#include "../common/util_classes.h"
#include "agramtab.h"
#include "RusGramTab.h"

#include <fstream>
#include <string>


CAgramtab::CAgramtab()
{
    m_bUseNationalConstants = true;
};


part_of_speech_t CAgramtab::GetPartOfSpeechByStr(const std::string& part_of_speech  ) const
{
    auto it = m_PartOfSpeechesHashMap.find(part_of_speech);
    if (it == m_PartOfSpeechesHashMap.end()) {
        return UnknownPartOfSpeech;
    }
    return it->second;
}


CAgramtabLine::CAgramtabLine(size_t SourceLineNo)
{
    m_SourceLineNo = SourceLineNo;
};


bool CAgramtab::GetGrammems(const char* gram_code, grammems_mask_t& grammems)  const
{
    grammems = 0;
    // Check for null, empty, or invalid gram_code
    if (gram_code == 0) return false;
    if (!*gram_code) return false;
    if (gram_code[0] == '?') return false;
    
    // Additional validation checks
    if (strlen(gram_code) < 2) {
        PLOGE << "Invalid gram_code (too short): " << gram_code;
        return false;
    }
    if (!isalpha(gram_code[0])) {
        PLOGE << "Invalid gram_code (first char not alpha): " << gram_code;
        return false;
    }
    
    try {
        size_t lineIndex = GramcodeToLineIndex(gram_code);
        
        // Validate line index
        if (lineIndex >= GetMaxGrmCount()) {
            PLOGE << "Invalid line index for gram_code: " << gram_code << ", index=" << lineIndex 
                  << ", max=" << GetMaxGrmCount() << ", language=" << GetStringByLanguage(m_Language);
            return false;
        }
        
        const CAgramtabLine* L = GetLine(lineIndex);
        
        if (L == NULL) {
            PLOGE << "NULL line for gram_code: " << gram_code << ", index=" << lineIndex 
                  << ", language=" << GetStringByLanguage(m_Language);
            return false;
        }
        
        grammems = L->m_Grammems;
        return true;
    }
    catch (const std::exception& e) {
        // Catch any unexpected exceptions rather than crashing
        PLOGE << "Exception in GetGrammems for gram_code: " << gram_code << ", error: " << e.what();
        return false;
    }
    catch (...) {
        // Catch any unexpected exceptions rather than crashing
        PLOGE << "Unknown exception in GetGrammems for gram_code: " << gram_code;
        return false;
    }
};

std::string   CAgramtab::GrammemsToStr(grammems_mask_t grammems, NamingAlphabet na) const
{
    char szGrammems[64 * 5];
    grammems_to_str(grammems, szGrammems, na);
    return szGrammems;
}

bool CAgramtab::ProcessPOSAndGrammems(const char* line_in_gramtab, part_of_speech_t& PartOfSpeech, grammems_mask_t& grammems)  const
{
    if (strlen(line_in_gramtab) > 300) return false;

    StringTokenizer tok(line_in_gramtab, " ,\t\r\n");
    const char* strPos = tok();
    if (!strPos)
    {
        //printf ("unknown pos");		
        return false;
    };


    //  getting the part of speech
    if (strcmp("*", strPos))
    {
        PartOfSpeech = GetPartOfSpeechByStr(strPos);
        if (PartOfSpeech == UnknownPartOfSpeech)
            return false;
    }
    else
        PartOfSpeech = UnknownPartOfSpeech;


    //  getting grammems
    grammems = 0;
    while (tok())
    {
        const char* grm = tok.val();
        auto it = m_GrammemHashMap.find(grm);
        if (it == m_GrammemHashMap.end()) {
            return false;
        }
        grammems |= _QM(it->second);
    };

    return true;
};

bool  CAgramtab::ProcessPOSAndGrammemsIfCan(const char* tab_str, part_of_speech_t* PartOfSpeech, grammems_mask_t* grammems) const
{
    return ProcessPOSAndGrammems(tab_str, *PartOfSpeech, *grammems);
};


void CAgramtab::BuildPartOfSpeechMap()
{
    for (part_of_speech_t i = 0; i < GetPartOfSpeechesCount(); i++) {
        m_PartOfSpeechesHashMap.insert({ GetPartOfSpeechStr(i, naNational), i });
        m_PartOfSpeechesHashMap.insert({ GetPartOfSpeechStr(i, naLatin), i });
    }
    for (grammem_t g = 0; g < GetGrammemsCount(); ++g) {
        m_GrammemHashMap[GetGrammemStr(g, naNational)] = g;
        m_GrammemHashMap[GetGrammemStr(g, naLatin)] = g;
    }
}

void CAgramtab::SetUseNationalConstants(bool value)
{
    m_bUseNationalConstants = value;
    BuildPartOfSpeechMap();
}


bool CAgramtab::GetPartOfSpeechAndGrammems(const BYTE* AnCodes, uint32_t& Poses, grammems_mask_t& Grammems) const
{
    size_t len = strlen((const char*)AnCodes);
    if (len == 0) return false;

    // grammems
    Grammems = 0;
    Poses = 0;
    for (size_t l = 0; l < len; l += 2)
    {
        const CAgramtabLine* L = GetLine(GramcodeToLineIndex((const char*)AnCodes + l));

        if (L == 0) return false;

        Poses |= (1 << L->m_PartOfSpeech);
        Grammems |= L->m_Grammems;
    };

    return true;
}

CAgramtab :: ~CAgramtab()
{
};


char* CAgramtab::grammems_to_str(grammems_mask_t grammems, char* out_buf, NamingAlphabet na ) const
{
    //may be it is wizer to use
    //https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly/
    // but it must be crossplatform

    out_buf[0] = 0;
    auto GrammemsCount = GetGrammemsCount();
    for (int i = GrammemsCount - 1; i >= 0; i--)
        if (_QM(i) & grammems)
        {
            strcat(out_buf, GetGrammemStr(i, na));
            strcat(out_buf, ",");
        };
    return out_buf;
};


bool CAgramtab::FindGrammems(const char* gram_codes, grammems_mask_t grammems) const
{
    for (size_t l = 0; l < strlen(gram_codes); l += 2)
        if ((GetLine(GramcodeToLineIndex(gram_codes + l))->m_Grammems & grammems) == grammems)
            return true;

    return false;
};

bool CAgramtab::GetGramCodeByGrammemsAndPartofSpeechIfCan(part_of_speech_t Pos, grammems_mask_t grammems, std::string& gramcodes) const
{
    for (uint16_t i = 0; i < GetMaxGrmCount(); i++) {
        if (GetLine(i) != NULL)
        {
            if ((GetLine(i)->m_Grammems == grammems) && (GetLine(i)->m_PartOfSpeech == Pos))
            {
                gramcodes = LineIndexToGramcode(i);
                return true;
            }
        }
    }
    return false;
};

std::string CAgramtab::GetFirstAncodeByPattern(const std::string& slf) const{
    // todo optimize me to make morphwizard faster
    part_of_speech_t pos;
    grammems_mask_t gra;
    std::string gramcode;
    if (   ProcessPOSAndGrammemsIfCan(slf.c_str(), &pos, &gra)
        && GetGramCodeByGrammemsAndPartofSpeechIfCan(pos, gra, gramcode)
        )
        return gramcode;
    return "";

}

bool CAgramtab::CheckGramCode(const char* gram_code) const
{
    if (gram_code == 0) return true;
    if (*gram_code == 0) return true;
    if (*gram_code == '?') return true;
    size_t line_no = GramcodeToLineIndex(gram_code);
    if (line_no >= GetMaxGrmCount()) return false;
    return   GetLine(line_no) != NULL;
}


part_of_speech_t CAgramtab::GetPartOfSpeech(const char* gram_code) const
{
    // Additional validations to prevent crashes
    if (gram_code == 0) return UnknownPartOfSpeech;
    if (*gram_code == 0) return UnknownPartOfSpeech;
    if (*gram_code == '?') return UnknownPartOfSpeech;
    
    // Safety check: ensure the gram_code has at least 2 valid characters
    if (strlen(gram_code) < 2) return UnknownPartOfSpeech;
    
    // Only accept valid alpha characters as first char to prevent crashes
    if (!isalpha(gram_code[0])) return UnknownPartOfSpeech;
    
    try {
        size_t lineIndex = GramcodeToLineIndex(gram_code);
        
        // Validate the line index before attempting to use it
        if (lineIndex >= GetMaxGrmCount()) return UnknownPartOfSpeech;
        
        const CAgramtabLine* L = GetLine(lineIndex);
        if (L == 0) return UnknownPartOfSpeech;
        
        return L->m_PartOfSpeech;
    }
    catch (...) {
        // Catch any unexpected exceptions rather than crashing
        PLOGE << "Exception in GetPartOfSpeech for gram_code: " << gram_code;
        return UnknownPartOfSpeech;
    }
}

size_t CAgramtab::GetSourceLineNo(const char* gram_code) const
{
    if (gram_code == nullptr) return 0;

    if (!strcmp(gram_code, "??")) return 0;

    const CAgramtabLine* L = GetLine(GramcodeToLineIndex(gram_code));

    if (L == nullptr)
        return 0;

    return L->m_SourceLineNo;
}


grammems_mask_t CAgramtab::GetAllGrammems(const char* gram_code) const
{
    if (gram_code == nullptr) return 0;
    if (!strcmp(gram_code, "??")) return 0;

    size_t len = strlen(gram_code);

    grammems_mask_t grammems = 0;

    for (size_t l = 0; l < len; l += 2)
    {
        grammems_mask_t G = GetLine(GramcodeToLineIndex(gram_code + l))->m_Grammems;
        grammems |= G;
    };

    return grammems;
}

void CAgramtab::ReadFromFolder(std::string folder) {
    BuildPartOfSpeechMap();

    m_InputJsonPath = (std::filesystem::path(folder) / "gramtab.json").string();
    
    PLOGI << "Attempting to read grammar table from: " << m_InputJsonPath;

    // Verify file exists before attempting to open
    if (!std::filesystem::exists(m_InputJsonPath)) {
        PLOGE << "Grammar table file does not exist: " << m_InputJsonPath;
        throw CExpc("Grammar table file does not exist: %s", m_InputJsonPath.c_str());
    }

    std::ifstream inp(m_InputJsonPath);
    if (!inp.good()) {
        PLOGE << "Cannot open grammar table file: " << m_InputJsonPath;
        throw CExpc("Cannot read gramtab for language %s path=%s", GetStringByLanguage(m_Language).c_str(), m_InputJsonPath.c_str());
    }
    
    try {
        rapidjson::Document doc;
        rapidjson::IStreamWrapper isw(inp);
        doc.ParseStream(isw);
        inp.close();

        if (doc.HasParseError()) {
            PLOGE << "Failed to parse JSON grammar table: " << m_InputJsonPath << ", error: " << doc.GetParseError();
            throw CExpc("Failed to parse JSON grammar table: %s", m_InputJsonPath.c_str());
        }

        // Verify essential sections in the JSON structure
        if (!doc.HasMember("gramcodes") || !doc["gramcodes"].IsObject()) {
            PLOGE << "Missing or invalid 'gramcodes' section in grammar table: " << m_InputJsonPath;
            throw CExpc("Invalid grammar table format: missing 'gramcodes' section");
        }

        if (!doc.HasMember("plug_noun_gram_code") || !doc["plug_noun_gram_code"].IsString()) {
            PLOGE << "Missing or invalid 'plug_noun_gram_code' in grammar table: " << m_InputJsonPath;
            throw CExpc("Invalid grammar table format: missing 'plug_noun_gram_code'");
        }

        std::unordered_map<std::string, grammem_t> grammem_dict;
        for (part_of_speech_t i = 0; i < GetGrammemsCount(); i++) {
            grammem_dict.insert({ GetGrammemStr(i, naLatin), i });
        }

        for (size_t i = 0; i < GetMaxGrmCount(); i++)
            GetLine(i) = 0;

        size_t line_no = 0;
        size_t loaded_count = 0;
        for (auto& item : doc["gramcodes"].GetObject()) {
            std::string gramcode = item.name.GetString();
            auto& val = item.value;
            
            // Skip invalid gramcodes
            if (gramcode.length() < 2) {
                PLOGW << "Skipping invalid gramcode (too short): " << gramcode;
                continue;
            }
            
            part_of_speech_t pos = UnknownPartOfSpeech;
            auto pos_it = rapidjson::Pointer("/p").Get(val);
            if (pos_it != nullptr) {
                const std::string& pos_str = pos_it->GetString();
                if (!pos_str.empty()) {
                    auto it = m_PartOfSpeechesHashMap.find(pos_str);
                    if (it == m_PartOfSpeechesHashMap.end()) {
                        PLOGW << "Unknown part of speech in grammar table: " << pos_str;
                        pos = UnknownPartOfSpeech;
                    } else {
                        pos = it->second;
                    }
                }
            }
            
            grammems_mask_t grammems = 0;
            if (val.HasMember("g") && val["g"].IsArray()) {
                for (auto& s: val["g"].GetArray()) {
                    std::string grammem_str = s.GetString();
                    auto it = grammem_dict.find(grammem_str);
                    if (it == grammem_dict.end()) {
                        PLOGW << "Unknown grammem in grammar table: " << grammem_str;
                        continue;
                    }
                    grammems |= _QM(it->second);
                }
            }

            CAgramtabLine* pAgramtabLine = new CAgramtabLine(line_no);
            pAgramtabLine->m_Grammems = grammems;
            pAgramtabLine->m_PartOfSpeech = pos;
            
            size_t gram_index = GramcodeToLineIndex(gramcode.c_str());
            if (gram_index >= GetMaxGrmCount()) {
                PLOGE << "Invalid line index for gramcode: " << gramcode << ", index=" << gram_index;
                delete pAgramtabLine;
                continue;
            }
            
            if (GetLine(gram_index)) {
                PLOGE << "Duplicate gramcode in grammar table: " << gramcode;
                throw CExpc(Format("line %i in %s contains a dublicate gramcode", line_no, m_InputJsonPath.c_str()));
            }
            
            GetLine(gram_index) = pAgramtabLine;
            loaded_count++;
            line_no++;
        }
        
        PLOGI << "Successfully loaded " << loaded_count << " grammar entries from " << m_InputJsonPath;
       
        std::string gramcode = doc["plug_noun_gram_code"].GetString();
        m_PlugNoun.m_GramCode = gramcode;
        
        if (m_PlugNoun.m_GramCode.empty()) {
            PLOGE << "Empty plug_noun_gram_code in grammar table";
            throw CExpc("Empty plug_noun_gram_code in grammar table");
        }
        
        if (!doc["gramcodes"].HasMember(gramcode.c_str()) || !doc["gramcodes"][gramcode.c_str()].HasMember("l")) {
            PLOGE << "Missing lemma for plug_noun_gram_code: " << gramcode;
            throw CExpc("Missing lemma for plug_noun_gram_code");
        }
        
        m_PlugNoun.m_Lemma = doc["gramcodes"][gramcode.c_str()]["l"].GetString();
        
        if (m_PlugNoun.m_Lemma.empty()) {
            PLOGE << "Empty lemma for plug_noun_gram_code: " << gramcode;
            throw CExpc("Empty lemma for plug_noun_gram_code");
        }
        
        InitLanguageSpecific(doc);
    } catch (const std::exception& e) {
        PLOGE << "Exception while loading grammar table: " << e.what();
        throw;
    } catch (...) {
        PLOGE << "Unknown exception while loading grammar table";
        throw CExpc("Unknown error loading grammar table");
    }
}

std::string CAgramtab::GetGramtabPath() const {
    return m_InputJsonPath;
}

std::string CAgramtab::GetDefaultPath() const {
    auto key = Format("Software\\Dialing\\Lemmatizer\\%s\\DictPath", GetStringByLanguage(m_Language).c_str());
    std::string path = ::GetRegistryString(key);
    
    // If registry path is empty, use the known source location as fallback
    if (path.empty()) {
        std::filesystem::path fallbackPath = std::filesystem::path("C:\\RML\\Source\\morph_dict\\data");
        fallbackPath /= GetStringByLanguage(m_Language).c_str();
        path = fallbackPath.string();
        PLOGW << "Registry path not found, using fallback path: " << path;
    }
    
    return path;
}

std::string	CAgramtab::GetAllPossibleAncodes(part_of_speech_t pos, grammems_mask_t grammems)const
{
    std::string Result;
    for (uint16_t i = 0; i < GetMaxGrmCount(); i++)
        if (GetLine(i) != 0)
        {
            const CAgramtabLine* L = GetLine(i);
            if ((L->m_PartOfSpeech == pos)
                && ((grammems & L->m_Grammems) == grammems)
                )
                Result += LineIndexToGramcode(i);
        };
    return Result;
};

//Generate GramCodes for grammems with CompareFunc
std::string	CAgramtab::GetAllGramCodes(part_of_speech_t pos, grammems_mask_t grammems, GrammemCompare CompareFunc)const
{
    std::string Result;
    CAgramtabLine L0(0);
    L0.m_PartOfSpeech = pos;
    L0.m_Grammems = grammems;
    for (uint16_t i = 0; i < GetMaxGrmCount(); i++)
        if (GetLine(i) != 0)
        {
            const CAgramtabLine* L = GetLine(i);
            if ((L->m_PartOfSpeech == pos)
                && (CompareFunc ? CompareFunc(L, &L0) : (L->m_Grammems & grammems) == L->m_Grammems && !(pos == NOUN && (L->m_Grammems & rAllGenders) == rAllGenders)) 
                )
                Result += LineIndexToGramcode(i);
        };
    return Result;
};

grammems_mask_t CAgramtab::Gleiche(GrammemCompare CompareFunc, const char* gram_codes1, const char* gram_codes2) const
{
    grammems_mask_t grammems = 0;
    if (!gram_codes1) return false;
    if (!gram_codes2) return false;
    if (!strcmp(gram_codes1, "??")) return false;
    if (!strcmp(gram_codes2, "??")) return false;
    
    try {
        size_t len1 = strlen(gram_codes1);
        size_t len2 = strlen(gram_codes2);
        
        for (size_t l = 0; l < len1; l += 2) {
            size_t index1 = GramcodeToLineIndex(gram_codes1 + l);
            if (index1 >= GetMaxGrmCount()) {
                PLOGE << "Invalid index1 in Gleiche: " << index1 << " for code: " << std::string(gram_codes1 + l, 2);
                continue;
            }
            
            const CAgramtabLine* l1 = GetLine(index1);
            if (!l1) {
                PLOGE << "Null line pointer in Gleiche for code1: " << std::string(gram_codes1 + l, 2);
                continue;
            }
            
            for (size_t m = 0; m < len2; m += 2) {
                size_t index2 = GramcodeToLineIndex(gram_codes2 + m);
                if (index2 >= GetMaxGrmCount()) {
                    PLOGE << "Invalid index2 in Gleiche: " << index2 << " for code: " << std::string(gram_codes2 + m, 2);
                    continue;
                }
                
                const CAgramtabLine* l2 = GetLine(index2);
                if (!l2) {
                    PLOGE << "Null line pointer in Gleiche for code2: " << std::string(gram_codes2 + m, 2);
                    continue;
                }
                
                if (CompareFunc && CompareFunc(l1, l2))
                    grammems |= (l1->m_Grammems & l2->m_Grammems);
            }
        }
    }
    catch (const std::exception& e) {
        PLOGE << "Exception in Gleiche: " << e.what();
    }
    catch (...) {
        PLOGE << "Unknown exception in Gleiche";
    }

    return grammems;
}

bool EqualAncodes (const CAgramtabLine* l1, const CAgramtabLine* l2)
{
    return l1 == l2;
};


std::string CAgramtab::GleicheAncode1(GrammemCompare CompareFunc, std::string gram_codes1, std::string gram_codes2) const
{
    std::string result;
    if (gram_codes1.empty() || gram_codes2.empty()) return "";
    if (gram_codes1 == "??") return gram_codes2;
    if (gram_codes2 == "??") return gram_codes2;
    if (!CompareFunc) {
        CompareFunc = EqualAncodes;
    }
    
    try {
        for (size_t l = 0; l < gram_codes1.length(); l += 2) {
            if (l + 1 >= gram_codes1.length()) {
                PLOGE << "Incomplete gram code at position " << l << " in gram_codes1: " << gram_codes1;
                continue;
            }
            
            size_t index1 = GramcodeToLineIndex(gram_codes1.c_str() + l);
            if (index1 >= GetMaxGrmCount()) {
                PLOGE << "Invalid index1 in GleicheAncode1: " << index1 << " for code: " << gram_codes1.substr(l, 2);
                continue;
            }
            
            const CAgramtabLine* l1 = GetLine(index1);
            if (!l1) {
                PLOGE << "Null line pointer in GleicheAncode1 for code1: " << gram_codes1.substr(l, 2);
                continue;
            }
            
            for (size_t m = 0; m < gram_codes2.length(); m += 2) {
                if (m + 1 >= gram_codes2.length()) {
                    PLOGE << "Incomplete gram code at position " << m << " in gram_codes2: " << gram_codes2;
                    continue;
                }
                
                size_t index2 = GramcodeToLineIndex(gram_codes2.c_str() + m);
                if (index2 >= GetMaxGrmCount()) {
                    PLOGE << "Invalid index2 in GleicheAncode1: " << index2 << " for code: " << gram_codes2.substr(m, 2);
                    continue;
                }
                
                const CAgramtabLine* l2 = GetLine(index2);
                if (!l2) {
                    PLOGE << "Null line pointer in GleicheAncode1 for code2: " << gram_codes2.substr(m, 2);
                    continue;
                }
                
                if (CompareFunc(l1, l2)) {
                    result.append(gram_codes1.c_str() + l, 2);
                    break;
                }
            }
        }
    }
    catch (const std::exception& e) {
        PLOGE << "Exception in GleicheAncode1: " << e.what();
    }
    catch (...) {
        PLOGE << "Unknown exception in GleicheAncode1";
    }
    
    return result;
}


std::string CAgramtab::UniqueGramCodes(std::string gram_codes) const
{
    std::string Result;
    for (size_t m = 0; m < gram_codes.length(); m += 2)
        if (Result.find(gram_codes.substr(m, 2)) == std::string::npos)
            Result.append(gram_codes.substr(m, 2));
    return Result;
}

std::string  CAgramtab::GetTabStringByGramCode(const char* gram_code) const
{
    try {
        // Validate the input
        if (!gram_code || gram_code[0] == '?' || strlen(gram_code) < 2 || !isalpha(gram_code[0]))
            return "UNKNOWN";
            
        // Get part of speech safely - GetPartOfSpeech now has added validations
        part_of_speech_t POS = GetPartOfSpeech(gram_code);
        
        // Get grammems safely 
        grammems_mask_t Grammems = 0;
        bool success = GetGrammems(gram_code, Grammems);
        if (!success) {
            return "UNKNOWN";
        }
        
        // Format the output
        char buffer[256] = {0};
        grammems_to_str(Grammems, buffer);
        std::string POSstr = (POS == UnknownPartOfSpeech) ? "*" : GetPartOfSpeechStr(POS);
        return POSstr + std::string(" ") + buffer;
    }
    catch (...) {
        // Catch any unexpected exceptions to prevent crashes
        PLOGE << "Exception in GetTabStringByGramCode for gram_code: " << (gram_code ? gram_code : "null");
        return "UNKNOWN";
    }
}
