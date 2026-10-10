#pragma once
#include <string>
#include <cstdint>

// Конвертация из Windows-1251 (из файла карты) в UTF-8 (для ImGui)
inline std::string cp1251_to_utf8(const std::string& cp1251) {
    std::string res;
    for (unsigned char c : cp1251) {
        if (c <= 127) {
            res += static_cast<char>(c);
        }
        else if (c >= 192) {
            int u = c - 192 + 0x0410; // Смещение кириллицы
            res += static_cast<char>(0xC0 | ((u >> 6) & 0x1F));
            res += static_cast<char>(0x80 | (u & 0x3F));
        }
        else if (c == 168) {
            res += static_cast<char>(0xD0);
            res += static_cast<char>(0x81);
        } // Ё
        else if (c == 184) {
            res += static_cast<char>(0xD1);
            res += static_cast<char>(0x91);
        } // ё
        else {
            res += '?';
        }
    }
    return res;
}

// Конвертация из UTF-8 (из ImGui) обратно в Windows-1251 (для сохранения)
inline std::string utf8_to_cp1251(const std::string& utf8) {
    std::string res;
    for (size_t i = 0; i < utf8.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c <= 127) {
            res += static_cast<char>(c);
        }
        else if ((c & 0xE0) == 0xC0) { // Двухбайтовый символ utf-8
            if (i + 1 < utf8.length()) {
                unsigned char c2 = static_cast<unsigned char>(utf8[i + 1]);
                int u = ((c & 0x1F) << 6) | (c2 & 0x3F);
                if (u >= 0x0410 && u <= 0x044F) res += static_cast<char>(static_cast<uint8_t>(u - 0x0410 + 192));
                else if (u == 0x0401) res += static_cast<char>(static_cast<uint8_t>(168)); // Ё
                else if (u == 0x0451) res += static_cast<char>(static_cast<uint8_t>(184)); // ё
                else res += '?';
                i++;
            }
        }
        else {
            res += '?';
        }
    }
    return res;
}

// Перевод CP1251 в нижний регистр для безопасного поиска
inline std::string cp1251_tolower(std::string s) {
    for (char& c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'A' && uc <= 'Z') {
            c = static_cast<char>(uc + 32);
        }
        else if (uc >= 0xC0 && uc <= 0xDF) { // А-Я -> а-я
            c = static_cast<char>(static_cast<uint8_t>(uc + 32));
        }
        else if (uc == 0xA8) { // Ё -> ё
            c = static_cast<char>(static_cast<uint8_t>(0xB8));
        }
    }
    return s;
}

// Поиск подстроки без учета регистра
inline bool utf8_contains_icase(const std::string& text, const std::string& search) {
    if (search.empty()) return true;
    std::string t1 = cp1251_tolower(utf8_to_cp1251(text));
    std::string t2 = cp1251_tolower(utf8_to_cp1251(search));
    return t1.find(t2) != std::string::npos;
}