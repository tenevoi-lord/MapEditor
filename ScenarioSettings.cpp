#include "ScenarioSettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <fstream>      // <-- ДОБАВЛЕНО для std::ifstream
#include "MapParser.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <filesystem>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

// --- 1. Поиск кодека PNG для GDI+ ---
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    std::vector<uint8_t> codecBuffer(size);
    Gdiplus::ImageCodecInfo* pImageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(codecBuffer.data());
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            return j;
        }
    }
    return -1;
}

// --- 2. Сохранение OpenGL-текстуры в файл PNG ---
static bool SaveTextureToPng(GLuint tex, int w, int h, const std::wstring& outPath) {
    if (!tex || w <= 0 || h <= 0) return false;

    std::vector<uint8_t> rgba(w * h * 4);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    // Переводим RGBA в BGRA для GDI+
    std::vector<uint8_t> bgra(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        bgra[i * 4 + 0] = rgba[i * 4 + 2]; // B
        bgra[i * 4 + 1] = rgba[i * 4 + 1]; // G
        bgra[i * 4 + 2] = rgba[i * 4 + 0]; // R
        bgra[i * 4 + 3] = rgba[i * 4 + 3]; // A
    }

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    bool success = false;
    {
        Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppARGB, bgra.data());
        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) >= 0) {
            if (bmp.Save(outPath.c_str(), &pngClsid, NULL) == Gdiplus::Ok) {
                success = true;
            }
        }
    }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return success;
}

// --- 3. COM-классы для Drag-and-Drop в Проводник Windows ---
class SimpleDropSource : public IDropSource {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = static_cast<IDropSource*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 1; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    STDMETHODIMP GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

// Простой энумератор форматов (обязателен для проводника Windows Explorer!)
class SimpleEnumFORMATETC : public IEnumFORMATETC {
    LONG m_ref = 1;
    ULONG m_index = 0;
    FORMATETC m_fe;
public:
    SimpleEnumFORMATETC(const FORMATETC& fe) : m_fe(fe) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IEnumFORMATETC) {
            *ppv = static_cast<IEnumFORMATETC*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG count = InterlockedDecrement(&m_ref);
        if (count == 0) delete this;
        return count;
    }
    STDMETHODIMP Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) override {
        if (!rgelt) return E_POINTER;
        if (m_index == 0 && celt > 0) {
            rgelt[0] = m_fe;
            m_index = 1;
            if (pceltFetched) *pceltFetched = 1;
            return (celt == 1) ? S_OK : S_FALSE;
        }
        if (pceltFetched) *pceltFetched = 0;
        return S_FALSE;
    }
    STDMETHODIMP Skip(ULONG celt) override {
        m_index = std::min(m_index + celt, 1UL);
        return (m_index == 1) ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() override { m_index = 0; return S_OK; }
    STDMETHODIMP Clone(IEnumFORMATETC** ppenum) override {
        if (!ppenum) return E_POINTER;
        *ppenum = new SimpleEnumFORMATETC(m_fe);
        return S_OK;
    }
};

class FileDataObject : public IDataObject {
    std::wstring m_filePath;
    FORMATETC m_fe;
public:
    FileDataObject(const std::wstring& path) : m_filePath(path) {
        m_fe.cfFormat = CF_HDROP;
        m_fe.ptd = nullptr;
        m_fe.dwAspect = DVASPECT_CONTENT;
        m_fe.lindex = -1;
        m_fe.tymed = TYMED_HGLOBAL;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = static_cast<IDataObject*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 1; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }

    STDMETHODIMP GetData(FORMATETC* pfe, STGMEDIUM* pm) override {
        if (pfe->cfFormat == CF_HDROP && (pfe->tymed & TYMED_HGLOBAL)) {
            // Структура DROPFILES + путь с двойным нуль-терминатором
            size_t pathBytes = (m_filePath.length() + 2) * sizeof(wchar_t);
            size_t totalBytes = sizeof(DROPFILES) + pathBytes;

            HGLOBAL hg = GlobalAlloc(GHND | GMEM_SHARE, totalBytes);
            if (!hg) return E_OUTOFMEMORY;

            DROPFILES* df = (DROPFILES*)GlobalLock(hg);
            df->pFiles = sizeof(DROPFILES);
            df->fWide = TRUE;

            wchar_t* dest = (wchar_t*)((BYTE*)df + sizeof(DROPFILES));
            wcscpy_s(dest, m_filePath.length() + 1, m_filePath.c_str());
            dest[m_filePath.length() + 1] = L'\0'; // двойной нуль-терминатор
            GlobalUnlock(hg);

            pm->tymed = TYMED_HGLOBAL;
            pm->hGlobal = hg;
            pm->pUnkForRelease = nullptr;
            return S_OK;
        }
        return DV_E_FORMATETC;
    }

    STDMETHODIMP QueryGetData(FORMATETC* pfe) override {
        if (pfe && pfe->cfFormat == CF_HDROP && (pfe->tymed & TYMED_HGLOBAL)) return S_OK;
        return DV_E_FORMATETC;
    }

    // ВАЖНО ДЛЯ ПРОВОДНИКА: отдаем энумератор форматов
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override {
        if (!ppenumFormatEtc) return E_POINTER;
        if (dwDirection == DATADIR_GET) {
            *ppenumFormatEtc = new SimpleEnumFORMATETC(m_fe);
            return S_OK;
        }
        *ppenumFormatEtc = nullptr;
        return E_NOTIMPL;
    }

    STDMETHODIMP GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override { return E_NOTIMPL; }
    STDMETHODIMP SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    STDMETHODIMP DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return E_NOTIMPL; }
    STDMETHODIMP DUnadvise(DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }
};

// Функция запуска перетаскивания
static void StartDragDropToExplorer(GLuint tex, int w, int h) {
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring tempPng = std::wstring(tempDir) + L"scenario_image.png";

    if (SaveTextureToPng(tex, w, h, tempPng)) {
        OleInitialize(nullptr);
        SimpleDropSource dropSource;
        FileDataObject dataObj(tempPng);
        DWORD effect = 0;
        // Разрешаем и COPY, и MOVE (Проводник требует оба флага)
        DoDragDrop(&dataObj, &dropSource, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
    }
}

// Диалог выбора LIT
static std::string OpenLitFileDialog() {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Файлы LIT (*.lit)\0*.lit\0Все файлы\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn) == TRUE) {
        return std::filesystem::path(szFile).string();
    }
    return "";
}

// Диалог сохранения PNG
static std::string SavePngFileDialog() {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"scenario_image.png";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Изображение PNG (*.png)\0*.png\0Все файлы\0*.*\0";
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&ofn) == TRUE) {
        return std::filesystem::path(szFile).string();
    }
    return "";
}
#else
static std::string OpenLitFileDialog() { return ""; }
static std::string SavePngFileDialog() { return ""; }
static void StartDragDropToExplorer(GLuint, int, int) {}
static bool SaveTextureToPng(GLuint tex, int w, int h, const std::wstring& outPath) { return false; }
#endif



// ================= ФУНКЦИИ АВТОПЕРЕНОСА ТЕКСТА =================
static std::string AutoWrapString(const std::string& text, int max_chars = 69) {
    std::string result;
    int char_count = 0;
    size_t last_space_pos = std::string::npos;
    size_t line_start_pos = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r') continue;
        if (c == '\n') {
            result += '\n';
            char_count = 0;
            line_start_pos = result.size();
            last_space_pos = std::string::npos;
            continue;
        }

        if ((c & 0xC0) != 0x80) char_count++;
        if (c == ' ') last_space_pos = result.size();

        result += (char)c;

        if (char_count >= max_chars && last_space_pos != std::string::npos && last_space_pos >= line_start_pos) {
            result[last_space_pos] = '\t';
            result.insert(last_space_pos + 1, "\n");
            line_start_pos = last_space_pos + 2;
            char_count = 0;
            for (size_t k = line_start_pos; k < result.size(); ++k) {
                if (((unsigned char)result[k] & 0xC0) != 0x80) char_count++;
            }
            last_space_pos = std::string::npos;
        }
    }
    return result;
}

static int AutoWrapCallback(ImGuiInputTextCallbackData* data) {
    const int MAX_LINE_CHARS = 69;
    int char_count = 0;
    int line_start_byte = 0;
    int last_space_byte = -1;

    for (int i = 0; i < data->BufTextLen; ++i) {
        unsigned char c = (unsigned char)data->Buf[i];
        if (c == '\n') {
            char_count = 0; line_start_byte = i + 1; last_space_byte = -1; continue;
        }
        if ((c & 0xC0) != 0x80) char_count++;
        if (c == ' ') last_space_byte = i;

        if (char_count >= MAX_LINE_CHARS) {
            if (last_space_byte != -1 && last_space_byte >= line_start_byte) {
                data->DeleteChars(last_space_byte, 1);
                data->InsertChars(last_space_byte, "\t\n");
                i = last_space_byte + 1;
                line_start_byte = last_space_byte + 2;
                char_count = 0; last_space_byte = -1;
            }
        }
    }
    return 0;
}

static std::string UnwrapString(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\t') {
            result += ' ';
            if (i + 1 < text.size() && text[i + 1] == '\n') i++;
        }
        else {
            result += text[i];
        }
    }
    return result;
}
// ===============================================================

// ================= КОНВЕРТАЦИЯ ВРЕМЕНИ И ДАТЫ ==================
static void MinutesToDate(uint32_t total_minutes, int& y, int& m, int& d, int& h) {
    uint32_t total_hours = total_minutes / 60;
    y = total_hours / (12 * 30 * 24);
    uint32_t rem = total_hours % (12 * 30 * 24);
    m = (rem / (30 * 24)) + 1;
    rem = rem % (30 * 24);
    d = (rem / 24) + 1;
    h = rem % 24;
}

static uint32_t DateToMinutes(int y, int m, int d, int h) {
    return (y * 12 * 30 * 24 + (m - 1) * 30 * 24 + (d - 1) * 24 + h) * 60;
}

static char date_mask_prev[32] = "00h 01/01/0000";

static bool IsValidDigitForPos(int target, char c, const char* buf) {
    if (target == 0) return c >= '0' && c <= '2';
    if (target == 1) {
        if (buf[0] == '2') return c >= '0' && c <= '3';
        return c >= '0' && c <= '9';
    }
    if (target == 4) return c >= '0' && c <= '3';
    if (target == 5) {
        if (buf[4] == '3') return c == '0';
        if (buf[4] == '0') return c >= '1' && c <= '9';
        return c >= '0' && c <= '9';
    }
    if (target == 7) return c >= '0' && c <= '1';
    if (target == 8) {
        if (buf[7] == '1') return c >= '0' && c <= '2';
        if (buf[7] == '0') return c >= '1' && c <= '9';
        return c >= '0' && c <= '9';
    }
    if (target >= 10 && target <= 13) return c >= '0' && c <= '9';
    return false;
}

static void CoerceBuffer(char* buf) {
    if (buf[0] > '2') buf[0] = '2';
    if (buf[0] == '2' && buf[1] > '3') buf[1] = '3';
    if (buf[4] > '3') buf[4] = '3';
    if (buf[4] == '3' && buf[5] > '0') buf[5] = '0';
    if (buf[4] == '0' && buf[5] == '0') buf[5] = '1';
    if (buf[7] > '1') buf[7] = '1';
    if (buf[7] == '1' && buf[8] > '2') buf[8] = '2';
    if (buf[7] == '0' && buf[8] == '0') buf[8] = '1';
}

static int DateMaskCallback(ImGuiInputTextCallbackData* data) {
    char* prev = (char*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        int old_len = 14;
        int new_len = data->BufTextLen;
        int cp = data->CursorPos;

        if (new_len > old_len) {
            int N = new_len - old_len;
            int ins_start = cp - N;
            if (ins_start < 0) ins_start = 0;
            std::string inserted(data->Buf + ins_start, N);
            strcpy(data->Buf, prev);
            data->BufTextLen = old_len;
            int target = ins_start;
            for (char c : inserted) {
                while (target == 2 || target == 3 || target == 6 || target == 9) target++;
                if (target < 14) {
                    if (IsValidDigitForPos(target, c, data->Buf)) {
                        data->Buf[target] = c;
                        CoerceBuffer(data->Buf);
                        target++;
                    }
                }
            }
            data->CursorPos = target;
            while (data->CursorPos == 2 || data->CursorPos == 3 || data->CursorPos == 6 || data->CursorPos == 9) {
                data->CursorPos++;
            }
        }
        else if (new_len < old_len) {
            int N = old_len - new_len;
            int del_start = cp;
            strcpy(data->Buf, prev);
            data->BufTextLen = old_len;
            for (int i = del_start; i < del_start + N && i < 14; ++i) {
                if (i != 2 && i != 3 && i != 6 && i != 9) data->Buf[i] = '0';
            }
            CoerceBuffer(data->Buf);
            data->CursorPos = del_start;
        }
        else {
            for (int i = 0; i < 14; ++i) if (data->Buf[i] == '.') data->Buf[i] = '/';
            bool valid = true;
            for (int i = 0; i < 14; ++i) {
                if (i == 2 && data->Buf[i] != 'h') valid = false;
                else if (i == 3 && data->Buf[i] != ' ') valid = false;
                else if (i == 6 && data->Buf[i] != '/') valid = false;
                else if (i == 9 && data->Buf[i] != '/') valid = false;
                else if (i != 2 && i != 3 && i != 6 && i != 9 && (data->Buf[i] < '0' || data->Buf[i] > '9')) valid = false;
            }
            if (!valid) strcpy(data->Buf, prev);
            else CoerceBuffer(data->Buf);
        }
        strcpy(prev, data->Buf);
        data->BufDirty = true;
    }
    return 0;
}
// ===============================================================

struct ScenarioEditState {
    char name[256];
    char desc[1024];
    char campaign[256];
    char next_map[256];

    int victory_event;
    int defeat_event;
    uint32_t start_date_min;

    HeroData knight;
    HeroData archmage;
    HeroData ranger;

    int8_t relations[16];

    int scenario_type;
    int scenario_icon_id;
    bool keep_gold, gods_mercy, fame, exp_and_level, personal_artifacts, all_inventory, all_army;
    std::vector<uint8_t> scenario_image_data; // <-- ДОБАВИТЬ
};

static ScenarioEditState state;
static bool initialized = false;
static char date_buf[32] = "";

// Текстура картинки сценария и её размеры
static GLuint scenario_img_tex = 0;
static int scenario_img_w = 0;
static int scenario_img_h = 0;

// Выносим функцию обновления текстуры в хелпер (теперь она знает о ScenarioEditState)
static void LoadScenarioImage(const std::string& path, TextureManager* texMgr, ScenarioEditState& target_state, GLuint& tex, int& w, int& h) {
    if (path.empty()) return;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;
    std::streamsize sz = file.tellg();
    file.seekg(0, std::ios::beg);
    if (sz > 0) {
        target_state.scenario_image_data.resize((size_t)sz);
        file.read(reinterpret_cast<char*>(target_state.scenario_image_data.data()), sz);
        if (tex) glDeleteTextures(1, &tex);
        if (texMgr) {
            tex = texMgr->LoadLitTextureFromMemory(
                target_state.scenario_image_data.data(), target_state.scenario_image_data.size(), &w, &h);
        }
    }
}

static void DrawHeroSettings(HeroData& hero, int hero_index, ResourceManager* resMgr, MapData& map, TextureManager* texMgr, ObjectManager* objMgr, bool& do_save, bool& do_close) {
    // =========================================================================
    // ВЕРХНЯЯ ЧАСТЬ (Высота блоков ровно 160 px)
    // =========================================================================
    ImGui::BeginGroup();

    // ---------------------------------------------------------
    // БЛОК 1: Картинка героя (160 x 160)
    // ---------------------------------------------------------
    ImGui::BeginGroup();
    GLuint tex = texMgr ? texMgr->tex_hero[hero_index] : 0;
    if (tex) {
        ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(160, 160));
    }
    else {
        ImGui::Dummy(ImVec2(160, 160));
    }
    ImGui::EndGroup();

    ImGui::SameLine(180);

    // ---------------------------------------------------------
    // БЛОК 2: Характеристики + Наличные артефакты в строку
    // ---------------------------------------------------------
    ImGui::BeginGroup();
    ImGui::TextDisabled("Характеристики на начало");
    ImGui::Dummy(ImVec2(0, 2));

    auto DrawStat = [](const char* label, uint16_t& val) {
        int v = val;
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::SameLine(115);
        ImGui::PushItemWidth(125);
        ImGui::PushID(label);
        // Шаг 50, быстрый шаг 250
        if (ImGui::InputInt("##val", &v, 50, 250)) {
            val = static_cast<uint16_t>(std::clamp(v, 0, 65535));
        }
        ImGui::PopID();
        ImGui::PopItemWidth();
        };

    DrawStat("Боевой опыт", hero.exp);
    DrawStat("Милость богов", hero.mana);
    DrawStat("Запас золота", hero.gold);

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::TextDisabled("Наличные артефакты");
    for (int i = 0; i < 3; ++i) {
        if (i > 0) ImGui::SameLine();
        ImGui::PushID(i + 200);
        int a_id = hero.artifacts[i];
        ImGui::PushItemWidth(240);
        if (DrawGridCombo("##aid", a_id, resMgr, ResType::ARTIFACT)) {
            hero.artifacts[i] = static_cast<uint16_t>(a_id);
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
    ImGui::EndGroup();

    ImGui::SameLine(555);

    // ---------------------------------------------------------
    // БЛОК 3: Стартовое строение + Координаты/Картинка + Инфо
    // ---------------------------------------------------------
    ImGui::BeginGroup();
    ImGui::TextDisabled("Стартовое строение");
    int b_id = hero.start_building_id;
    std::string b_preview = "Отсутствует";
    int bx = 0, by = 0;
    GLuint bld_tex = 0;

    if (b_id > 0 && static_cast<size_t>(b_id - 1) < map.buildings.size()) {
        size_t b_idx = static_cast<size_t>(b_id - 1);
        const auto& bo = map.buildings[b_idx];
        bx = bo.x;
        by = bo.y;
        b_preview = "Строение #" + std::to_string(b_id);
        if (b_idx < map.building_texts.size() && !map.building_texts[b_idx].name.empty()) {
            b_preview += " - " + cp1251_to_utf8(map.building_texts[b_idx].name);
        }

        if (objMgr) {
            for (const auto& o : objMgr->objects) {
                if ((o.header.groupId * 256 + o.header.objectId) == bo.id) {
                    bld_tex = o.texture;
                    break;
                }
            }
        }
    }

    ImGui::PushItemWidth(260);
    if (ImGui::BeginCombo("##build", b_preview.c_str())) {
        if (ImGui::Selectable("Отсутствует", b_id == 0)) hero.start_building_id = 0;
        for (size_t i = 0; i < map.buildings.size(); ++i) {
            int b_num = static_cast<int>(i + 1);
            std::string name = "Строение #" + std::to_string(b_num);
            if (i < map.building_texts.size() && !map.building_texts[i].name.empty()) {
                name += " - " + cp1251_to_utf8(map.building_texts[i].name);
            }
            ImGui::PushID((int)i);
            if (ImGui::Selectable(name.c_str(), b_id == b_num)) {
                hero.start_building_id = static_cast<uint16_t>(b_num);
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 4));

    // Расчет суммарного дохода замков/фортов (id/256 == 3 или 4) и деревень (id/256 == 2)
    int castle_income = 0;
    int village_income = 0;
    for (const auto& b : map.buildings) {
        uint32_t gid = b.id / 256;
        if (gid == 3 || gid == 4) {
            castle_income += b.gold_income;
        }
        else if (gid == 2) {
            village_income += b.gold_income;
        }
    }

    ImGui::BeginGroup();
    ImGui::Text("Координаты:\nX = %d  Y = %d", bx, by);
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::TextDisabled("Информация");
    ImGui::Text("Замки: %d  Деревни: %d", castle_income, village_income);
    ImGui::EndGroup();

    ImGui::SameLine(195);
    if (bld_tex) ImGui::Image((ImTextureID)(intptr_t)bld_tex, ImVec2(53, 53));
    else ImGui::Dummy(ImVec2(53, 53));

    ImGui::EndGroup();

    ImGui::EndGroup(); // Конец верхней части

    ImGui::Dummy(ImVec2(0, 10));

    // =========================================================================
    // СРЕДНЯЯ ЧАСТЬ: Начальные войска
    // =========================================================================
    // Считаем общее число юнитов в отряде (ГГ = 1 юнит + спутники)
    int follower_units = 0;
    for (int i = 0; i < 6; i++) {
        if (hero.army[i].id != 0) {
            follower_units += hero.army[i].count;
        }
    }
    int total_units = 1 + follower_units;

    ImGui::TextDisabled("Начальные войска");
    ImGui::Dummy(ImVec2(0, 4));

    ImGui::Columns(2, "HeroTroopCols", false);
    for (int col = 0; col < 2; col++) {
        ImGui::Text("Отряд"); ImGui::SameLine(180); ImGui::Text("Уровень"); ImGui::SameLine(265); ImGui::Text("Число");
        for (int row = 0; row < 3; row++) {
            int i = col * 3 + row;
            ImGui::PushID(i + 100);

            int u_id = hero.army[i].id;
            int u_lvl = hero.army[i].level;
            int u_cnt = hero.army[i].count;

            // 1. Выбор отряда
            ImGui::PushItemWidth(165);
            bool is_disabled = (total_units >= 12 && u_id == 0);
            if (DrawGridCombo("##uid", u_id, resMgr, ResType::UNIT, is_disabled, 4)) {
                hero.army[i].id = static_cast<uint16_t>(u_id);
                if (u_id != 0 && u_cnt == 0) {
                    hero.army[i].count = 1;
                    total_units++;
                    follower_units++;
                }
                if (u_id == 0) {
                    total_units -= u_cnt;
                    follower_units -= u_cnt;
                    hero.army[i].count = 0;
                    hero.army[i].level = 0;
                }
            }
            ImGui::PopItemWidth();

            // Блокируем изменение уровня и количества, если юнит не выбран
            bool no_unit = (u_id == 0);
            if (no_unit) ImGui::BeginDisabled();

            // 2. Поле уровня
            ImGui::SameLine(180);
            ImGui::PushItemWidth(80);
            if (ImGui::InputInt("##lvl", &u_lvl, 1)) {
                hero.army[i].level = static_cast<uint8_t>(std::clamp(u_lvl, 0, 255));
            }
            ImGui::PopItemWidth();

            // 3. Поле количества
            ImGui::SameLine(265);
            ImGui::PushItemWidth(75);
            int pre_cnt = u_cnt;
            if (ImGui::InputInt("##cnt", &u_cnt, 1)) {
                u_cnt = std::max(0, u_cnt);
                if (u_cnt > pre_cnt && total_units >= 12) {
                    u_cnt = pre_cnt + (12 - total_units);
                }
                total_units += (u_cnt - pre_cnt);
                follower_units += (u_cnt - pre_cnt);
                hero.army[i].count = static_cast<uint8_t>(u_cnt);
                if (u_cnt == 0) {
                    hero.army[i].id = 0;
                    hero.army[i].level = 0;
                }
            }
            ImGui::PopItemWidth();

            if (no_unit) ImGui::EndDisabled();

            ImGui::PopID();
        }
        ImGui::NextColumn();
    }
    ImGui::Columns(1);

    //ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.0f, 1.0f), "Всего юнитов в армии (макс 12): %d (ГГ + %d)", total_units, follower_units);

    ImGui::Dummy(ImVec2(0, 10));

    // =========================================================================
    // НИЖНЯЯ ЧАСТЬ: Заклинания + Кнопки Сохранить / Выход в 6-м слоте
    // =========================================================================
    ImGui::TextDisabled("Начальные заклинания и молитвы");
    ImGui::Dummy(ImVec2(0, 4));

    // Верхняя строка (3 комбобокса: 0, 1, 2)
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine(i * 280.0f);
        ImGui::PushID(i + 200);
        int s_id = hero.spells[i];
        ImGui::PushItemWidth(260);
        if (DrawGridCombo("##sid", s_id, resMgr, ResType::SPELL)) hero.spells[i] = static_cast<uint16_t>(s_id);
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0, 5));

    // Нижняя строка (2 комбобокса: 3, 4 + Кнопки Сохранить/Выход вместо 6-го заклинания)
    for (int i = 3; i < 5; i++) {
        if (i == 4) ImGui::SameLine(280.0f);
        ImGui::PushID(i + 200);
        int s_id = hero.spells[i];
        ImGui::PushItemWidth(260);
        if (DrawGridCombo("##sid", s_id, resMgr, ResType::SPELL)) hero.spells[i] = static_cast<uint16_t>(s_id);
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    // 6-й слот (позиция 560 px): Кнопки Сохранить и Выход
    ImGui::SameLine(560.0f);
    ImGui::BeginGroup();
    if (ImGui::Button("Сохранить", ImVec2(125, 24))) {
        do_save = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Выход", ImVec2(125, 24))) {
        do_close = true;
    }
    ImGui::EndGroup();
}

void DrawScenarioSettingsModal(bool& open_flag, MapData& map, ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr, int& focus_hero_tab) {
    if (!open_flag) {
        initialized = false;
        return;
    }

    if (!initialized) {
        ImGui::OpenPopup("Параметры сценария");

        snprintf(state.name, sizeof(state.name), "%s", cp1251_to_utf8(map.map_name).c_str());
        std::string wrapped_desc = AutoWrapString(cp1251_to_utf8(map.map_desc), 69);
        snprintf(state.desc, sizeof(state.desc), "%s", wrapped_desc.c_str());
        snprintf(state.campaign, sizeof(state.campaign), "%s", cp1251_to_utf8(map.campaign_name).c_str());
        snprintf(state.next_map, sizeof(state.next_map), "%s", cp1251_to_utf8(map.next_scenario).c_str());

        state.victory_event = map.victory_event_id;
        state.defeat_event = map.defeat_event_id;
        state.start_date_min = map.start_date_min;

        int dy, dm, dd, dh;
        MinutesToDate(state.start_date_min, dy, dm, dd, dh);
        snprintf(date_buf, sizeof(date_buf), "%02dh %02d.%02d.%04d", dh, dd, dm, dy);
        strcpy(date_mask_prev, date_buf);

        state.knight = map.knight;
        state.archmage = map.archmage;
        state.ranger = map.ranger;

        memcpy(state.relations, map.relations, 16);

        state.scenario_type = map.scenario_type;
        state.scenario_icon_id = map.scenario_icon_id;
        state.keep_gold = map.keep_gold;
        state.gods_mercy = map.gods_mercy;
        state.fame = map.fame;
        state.exp_and_level = map.exp_and_level;
        state.personal_artifacts = map.personal_artifacts;
        state.all_inventory = map.all_inventory;
        state.all_army = map.all_army;

        // Копируем байты картинки и создаем текстуру
        state.scenario_image_data = map.scenario_image_data;
        if (scenario_img_tex) {
            glDeleteTextures(1, &scenario_img_tex);
            scenario_img_tex = 0;
        }
        if (!state.scenario_image_data.empty() && texMgr) {
            scenario_img_tex = texMgr->LoadLitTextureFromMemory(
                state.scenario_image_data.data(),
                state.scenario_image_data.size(),
                &scenario_img_w, &scenario_img_h
            );
        }

        initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(960, 800), ImGuiCond_FirstUseEver);

    // === ДОБАВИТЬ ЭТОТ БЛОК (18 цветов светлой темы) ===
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    // ===================================================

    if (ImGui::BeginPopupModal("Параметры сценария", &open_flag, ImGuiWindowFlags_NoSavedSettings)) {

        // ================= ВЕРХНИЙ БЛОК =================
        ImGui::BeginChild("TopPanel", ImVec2(0, 195), true);
        ImGui::Columns(2, "TopCols", false);
        ImGui::SetColumnWidth(0, 400);

        ImGui::TextDisabled("Название сценария");
        ImGui::SameLine(250);
        ImGui::TextDisabled("Размеры карты");

        ImGui::PushItemWidth(230);
        ImGui::InputText("##Name", state.name, sizeof(state.name));
        ImGui::PopItemWidth();

        ImGui::SameLine(250);
        char size_buf[32];
        snprintf(size_buf, sizeof(size_buf), "%d x %d", map.size_x, map.size_y);
        ImGui::PushItemWidth(120);
        ImGui::BeginDisabled();
        ImGui::InputText("##Size", size_buf, sizeof(size_buf));
        ImGui::EndDisabled();
        ImGui::PopItemWidth();

        auto CleanEventName = [](std::string name) {
            size_t pos = 0;
            while ((pos = name.find('%', pos)) != std::string::npos) {
                if (pos + 1 < name.size()) {
                    char next = name[pos + 1];
                    if (next == '+' || next == '-' || next == '=' || next == '^') {
                        size_t end_pos = name.find(' ', pos);
                        if (end_pos == std::string::npos) name.erase(pos);
                        else name.erase(pos, end_pos - pos + 1);
                        continue;
                    }
                }
                pos++;
            }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            return name;
            };

        auto DrawEventCombo = [&](const char* id, int& ev_id) {
            std::string preview = "Отсутствует";
            if (ev_id > 0 && ev_id <= (int)map.events.size()) {
                preview = std::to_string(ev_id) + ". ";
                if (ev_id - 1 < (int)map.event_texts.size()) {
                    preview += CleanEventName(cp1251_to_utf8(map.event_texts[ev_id - 1].name));
                }
            }
            ImGui::PushItemWidth(370);
            if (ImGui::BeginCombo(id, preview.c_str())) {
                if (ImGui::Selectable("Отсутствует", ev_id == 0)) ev_id = 0;
                for (size_t i = 0; i < map.events.size(); ++i) {
                    std::string name = std::to_string(i + 1) + ". ";
                    if (i < map.event_texts.size()) {
                        name += CleanEventName(cp1251_to_utf8(map.event_texts[i].name));
                    }
                    if (ImGui::Selectable(name.c_str(), ev_id == (int)(i + 1))) ev_id = static_cast<int>(i + 1);
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            };

        ImGui::Dummy(ImVec2(0, 2));
        ImGui::TextDisabled("Условие победы");
        DrawEventCombo("##VicEv", state.victory_event);

        ImGui::Dummy(ImVec2(0, 2));
        ImGui::TextDisabled("Причина поражения");
        DrawEventCombo("##DefEv", state.defeat_event);

        ImGui::Dummy(ImVec2(0, 2));
        ImGui::TextDisabled("День начала сценария");
        ImGui::SameLine(180);
        ImGui::PushItemWidth(120);
        ImGui::InputText("##Date", date_buf, sizeof(date_buf), ImGuiInputTextFlags_CallbackEdit, DateMaskCallback, date_mask_prev);

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            int ph = 0, pd = 1, pm = 1, py = 0;
            if (sscanf(date_buf, "%dh %d.%d.%d", &ph, &pd, &pm, &py) == 4) {
                ph = std::clamp(ph, 0, 23);
                pd = std::clamp(pd, 1, 30);
                pm = std::clamp(pm, 1, 12);
                py = std::clamp(py, 0, 9999);
                state.start_date_min = DateToMinutes(py, pm, pd, ph);
            }
            MinutesToDate(state.start_date_min, py, pm, pd, ph);
            snprintf(date_buf, sizeof(date_buf), "%02dh %02d.%02d.%04d", ph, pd, pm, py);
            strcpy(date_mask_prev, date_buf);
        }
        ImGui::PopItemWidth();

        ImGui::NextColumn();
        ImGui::TextDisabled("Описание сценария");
        ImGui::InputTextMultiline("##Desc", state.desc, sizeof(state.desc), ImVec2(-FLT_MIN, -1.0f), ImGuiInputTextFlags_CallbackEdit, AutoWrapCallback);

        ImGui::Columns(1);
        ImGui::EndChild();

        // ================= ВКЛАДКИ =================
        bool do_save = false;
        bool do_close = false;

        ImGui::BeginChild("TabsPanel", ImVec2(0, 0), true);
        if (ImGui::BeginTabBar("HeroTabs")) {

            ImGuiTabItemFlags t_flags = (focus_hero_tab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("Рыцарь", nullptr, t_flags)) {
                DrawHeroSettings(state.knight, 0, resMgr, map, texMgr, objMgr, do_save, do_close);
                ImGui::EndTabItem();
            }

            t_flags = (focus_hero_tab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("Архимат", nullptr, t_flags)) {
                DrawHeroSettings(state.archmage, 1, resMgr, map, texMgr, objMgr, do_save, do_close);
                ImGui::EndTabItem();
            }

            t_flags = (focus_hero_tab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("Следопыт", nullptr, t_flags)) {
                DrawHeroSettings(state.ranger, 2, resMgr, map, texMgr, objMgr, do_save, do_close);
                ImGui::EndTabItem();
            }

            if (focus_hero_tab != -1) {
                focus_hero_tab = -1;
            }

            // =========================================================================
            // ВКЛАДКА: НАСТРОЙКА СОЮЗОВ (Сетка 2х2 + Цветные шапки + 5 пресетов)
            // =========================================================================
            if (ImGui::BeginTabItem("-= Настройка союзов =-")) {
                ImGui::Dummy(ImVec2(0, 5));

                const char* faction_names[4] = {
                    "Зеленые (игрок)",
                    "Синие (союзник)",
                    "Желтые (сосед)",
                    "Красные (враг)"
                };

                /*const char* target_labels[4] = {
                    "К зеленым (игрок)",
                    "К синим (союзник)",
                    "К желтым (сосед)",
                    "К красным (враг)"
                };*/

                // Цвета плашек заголовков (темные для контраста с белым текстом)
                ImVec4 header_bg[4] = {
                    ImVec4(0.18f, 0.55f, 0.22f, 1.0f), // Зеленый
                    ImVec4(0.20f, 0.45f, 0.78f, 1.0f), // Синий
                    ImVec4(0.70f, 0.55f, 0.15f, 1.0f), // Желтый / Охра
                    ImVec4(0.78f, 0.25f, 0.25f, 1.0f)  // Красный
                };

                // Яркие цвета для прогресс-баров
                ImVec4 bar_colors[4] = {
                    ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                    ImVec4(0.2f, 0.5f, 1.0f, 1.0f),
                    ImVec4(0.85f, 0.75f, 0.1f, 1.0f),
                    ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
                };

                int pct_vals[] = { 0, 16, 33, 50, 66, 83, 100 }; // Маппинг от -3 до +3

                // Таблица 2х2 для размещения 4-х блоков
                if (ImGui::BeginTable("AlliancesGrid2x2", 2, ImGuiTableFlags_SizingStretchSame)) {
                    for (int src = 0; src < 4; ++src) {
                        ImGui::TableNextColumn();
                        ImGui::PushID(src * 100);

                        // --- 1. Цветная плашка-заголовок ---
                        ImVec2 p_min = ImGui::GetCursorScreenPos();
                        float block_w = ImGui::GetContentRegionAvail().x;
                        float header_h = 24.0f;
                        ImVec2 p_max = ImVec2(p_min.x + block_w, p_min.y + header_h);

                        ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(header_bg[src]);
                        ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, bg_color, 4.0f);

                        ImVec2 text_pos = ImVec2(p_min.x + 8.0f, p_min.y + (header_h - ImGui::GetFontSize()) * 0.5f);
                        ImGui::GetWindowDrawList()->AddText(text_pos, IM_COL32(255, 255, 255, 255), faction_names[src]);
                        ImGui::Dummy(ImVec2(block_w, header_h + 6.0f));

                        // --- 2. Четыре полосы отношений ([-] [Bar] [+]) ---
                        for (int dst = 0; dst < 4; ++dst) {
                            int rel_idx = src * 4 + dst;
                            int rel = (int8_t)state.relations[rel_idx];

                            ImGui::PushID(rel_idx);

                            // Название целевой фракции
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextColored(bar_colors[dst], "%s", faction_names[dst]);
                            ImGui::SameLine(160);

                            // Кнопка [-]
                            if (ImGui::Button("-", ImVec2(20, 20))) {
                                rel = std::max(-3, rel - 1);
                            }

                            // Прогресс-бар с процентами
                            ImGui::SameLine();
                            int pct = pct_vals[std::clamp(rel + 3, 0, 6)];
                            char buf[16];
                            snprintf(buf, sizeof(buf), "%d%%", pct);

                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_colors[dst]);
                            ImGui::ProgressBar(pct / 100.0f, ImVec2(230, 20), buf);
                            ImGui::PopStyleColor();

                            // Кнопка [+]
                            ImGui::SameLine();
                            if (ImGui::Button("+", ImVec2(20, 20))) {
                                rel = std::min(3, rel + 1);
                            }

                            state.relations[rel_idx] = static_cast<int8_t>(rel);
                            ImGui::PopID();
                        }

                        ImGui::Dummy(ImVec2(0, 10)); // Отступ между строками 2x2
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                // =====================================================================
                 // ПАНЕЛЬ БЫСТРОЙ НАСТРОЙКИ (5 КНОПОК ПРЕСЕТОВ)
                 // =====================================================================
                ImGui::Dummy(ImVec2(0, 5));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 5));
                ImGui::TextDisabled("Быстрая настройка отношений:");

                // Пресет: По умолчанию
                if (ImGui::Button("По умолчанию", ImVec2(120, 25))) {
                    const int8_t def_rel[16] = {
                         3,  2,  1, -2, // Зеленые к Зел, Син, Жел, Крас (100, 83, 66, 16)
                         2,  3,  1, -2, // Синие    к Зел, Син, Жел, Крас (83, 100, 66, 16)
                         1,  1,  3,  1, // Желтые   к Зел, Син, Жел, Крас (66,  66, 100, 66)
                        -2, -2,  1,  3  // Красные  к Зел, Син, Жел, Крас (16,  16,  66, 100)
                    };
                    memcpy(state.relations, def_rel, 16);
                }
                
                ImGui::SameLine();

                // Пресет: Союзники
                if (ImGui::Button("Союзники", ImVec2(100, 25))) {
                    const int8_t allies_rel[16] = {
                         3,  2,  2,  1, // Зеленые (100, 83, 83, 66)
                         2,  3,  2,  1, // Синие    (83, 100, 83, 66)
                         2,  2,  3,  2, // Желтые   (83,  83, 100, 83)
                         1,  1,  2,  3  // Красные  (66,  66,  83, 100)
                    };
                    memcpy(state.relations, allies_rel, 16);
                }
                ImGui::SameLine();

                // Пресет: Нейтралитет
                if (ImGui::Button("Нейтралитет", ImVec2(110, 25))) {
                    const int8_t neutr_rel[16] = {
                         2,  0,  0,  0, // Зеленые к себе 83, остальные 50
                         0,  2,  0,  0, // Синие    к себе 83, остальные 50
                         0,  0,  2,  0, // Желтые   к себе 83, остальные 50
                         0,  0,  0,  2  // Красные  к себе 83, остальные 50
                    };
                    memcpy(state.relations, neutr_rel, 16);
                }
                ImGui::SameLine();

                // Пресет: Мировая война
                if (ImGui::Button("Мировая война", ImVec2(125, 25))) {
                    const int8_t war_rel[16] = {
                         1, -1, -2, -3, // Зеленые (66, 33, 16,  0)
                        -2,  1, -2, -3, // Синие    (16, 66, 16,  0)
                        -1, -1,  1, -1, // Желтые   (33, 33, 66, 33)
                        -3, -3, -2,  1  // Красные  ( 0,  0, 16, 66)
                    };
                    memcpy(state.relations, war_rel, 16);
                }
                ImGui::SameLine();

                // Пресет: 50/50 (Коалиция Игрок+Союзник против Сосед+Враг)
                if (ImGui::Button("50/50", ImVec2(100, 25))) {
                    const int8_t ffifty_rel[16] = {
                         3,  2, -2, -2, // Зеленые (100, 83, 16, 16)
                         2,  3, -2, -2, // Синие    (83, 100, 16, 16)
                        -2, -2,  3,  2, // Желтые   (16,  16, 100, 83)
                        -2, -2,  2,  3  // Красные  (16,  16,  83, 100)
                    };
                    memcpy(state.relations, ffifty_rel, 16);
                }

                // =====================================================================
                // ПОДВАЛ (КНОПКИ СОХРАНИТЬ И ВЫХОД)
                // =====================================================================
                ImGui::Dummy(ImVec2(0, 15));
                float avail_w = ImGui::GetContentRegionAvail().x;
                float needed_w = 125.0f * 2 + ImGui::GetStyle().ItemSpacing.x;
                if (avail_w > needed_w) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_w - needed_w);
                }

                if (ImGui::Button("Сохранить##alliances", ImVec2(125, 24))) do_save = true;
                ImGui::SameLine();
                if (ImGui::Button("Выход##alliances", ImVec2(125, 24))) do_close = true;

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("-= Настройка кампаний =-")) {
                ImGui::Dummy(ImVec2(0, 10));

                // --- 1. ВЕРХНИЙ БЛОК: Значок и Тип (Две колонки через таблицы для надежности) ---
                if (ImGui::BeginTable("CampTopBlock", 2, ImGuiTableFlags_None)) {
                    ImGui::TableSetupColumn("IconCol", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("TypeCol", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();

                    // ЛЕВАЯ КОЛОНКА: Значок
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Значок");

                    int s_id = state.scenario_icon_id;
                    GLuint icon_tex = (s_id > 0 && s_id <= 5 && texMgr) ? texMgr->tex_scenario_icons[s_id - 1] : 0;

                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    if (ImGui::InvisibleButton("##IconBtn", ImVec2(32, 32))) {
                        ImGui::OpenPopup("IconSelectPopup");
                    }
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->AddRectFilled(p_min, ImVec2(p_min.x + 32, p_min.y + 32), ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 3.0f);
                    dl->AddRect(p_min, ImVec2(p_min.x + 32, p_min.y + 32), ImGui::GetColorU32(ImGuiCol_Border), 3.0f);

                    if (icon_tex) {
                        dl->AddImage((ImTextureID)(intptr_t)icon_tex, ImVec2(p_min.x + 3, p_min.y + 3), ImVec2(p_min.x + 29, p_min.y + 29));
                    }
                    else {
                        dl->AddText(ImVec2(p_min.x + 10, p_min.y + 8), ImGui::GetColorU32(ImGuiCol_TextDisabled), "—");
                    }

                    ImGui::SameLine(0, 4);
                    ImGui::BeginGroup();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
                    if (ImGui::ArrowButton("##IconUp", ImGuiDir_Up)) state.scenario_icon_id = (state.scenario_icon_id + 1) % 6;
                    if (ImGui::ArrowButton("##IconDn", ImGuiDir_Down)) state.scenario_icon_id = (state.scenario_icon_id + 5) % 6;
                    ImGui::PopStyleVar();
                    ImGui::EndGroup();

                    if (ImGui::BeginPopup("IconSelectPopup")) {
                        const char* names[] = { "Нет (0)", "Замок (1)", "Шлем (2)", "Мечи (3)", "Череп (4)", "Обучение (5)" };
                        for (int i = 0; i <= 5; i++) {
                            if (ImGui::Selectable(names[i], state.scenario_icon_id == i, 0, ImVec2(130, 24))) {
                                state.scenario_icon_id = i;
                            }
                        }
                        ImGui::EndPopup();
                    }

                    // ПРАВАЯ КОЛОНКА: Тип сценария
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Тип сценария");
                    ImGui::RadioButton("Обычный сценарий", &state.scenario_type, 0);
                    ImGui::RadioButton("Первый сценарий кампании", &state.scenario_type, 1);
                    ImGui::RadioButton("Обычный сценарий кампании", &state.scenario_type, 2);

                    ImGui::EndTable();
                }

                ImGui::Dummy(ImVec2(0, 10));

                // --- 2. ТЕКСТОВЫЕ ПОЛЯ ---
                ImGui::TextDisabled("Название кампании");
                ImGui::SetNextItemWidth(350);
                ImGui::InputText("##CampName", state.campaign, sizeof(state.campaign));

                ImGui::Dummy(ImVec2(0, 2));
                ImGui::TextDisabled("Имя файла следующего сценария (.DTm)");
                ImGui::SetNextItemWidth(350);
                ImGui::InputText("##NextMap", state.next_map, sizeof(state.next_map));

                ImGui::Dummy(ImVec2(0, 10));

                // --- 3. НИЖНИЙ БЛОК: Картинка и Галочки ---
                if (ImGui::BeginTable("CampBottomBlock", 2, ImGuiTableFlags_None)) {
                    ImGui::TableSetupColumn("ImgCol", ImGuiTableColumnFlags_WidthFixed, 280.0f);
                    ImGui::TableSetupColumn("CheckCol", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();

                    // ЛЕВАЯ КОЛОНКА: Картинка 267x134
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Картинка к сценарию");

                    ImVec2 box_sz(267.0f, 134.0f);
                    ImVec2 ipos = ImGui::GetCursorScreenPos();
                    ImDrawList* idl = ImGui::GetWindowDrawList();
                    idl->AddRectFilled(ipos, ImVec2(ipos.x + box_sz.x, ipos.y + box_sz.y), ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
                    idl->AddRect(ipos, ImVec2(ipos.x + box_sz.x, ipos.y + box_sz.y), ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

                    if (scenario_img_tex) {
                        float max_w = box_sz.x - 4.0f;
                        float max_h = box_sz.y - 4.0f;
                        float tw = (float)scenario_img_w;
                        float th = (float)scenario_img_h;

                        float scale = std::min(max_w / tw, max_h / th);
                        if (scale < 1.0f) {
                            tw = std::round(tw * scale);
                            th = std::round(th * scale);
                        }

                        ImVec2 cpos(ipos.x + (box_sz.x - tw) * 0.5f, ipos.y + (box_sz.y - th) * 0.5f);
                        idl->AddImage((ImTextureID)(intptr_t)scenario_img_tex, cpos, ImVec2(cpos.x + tw, cpos.y + th));

                        ImGui::SetCursorScreenPos(ipos);
                        ImGui::InvisibleButton("##ImgBtn", box_sz);

                        // --- ДРАГ И ДРОП В ПРОВОДНИК ПРИ ПЕРЕТАСКИВАНИИ МЫШКОЙ ---
                        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 5.0f)) {
                            StartDragDropToExplorer(scenario_img_tex, scenario_img_w, scenario_img_h);
                        }
                        // Обычный клик — просмотр в полный размер
                        else if (ImGui::IsItemDeactivated() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 5.0f)) {
                            ImGui::OpenPopup("ViewImgPopup");
                        }

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(
                                "Разрешение: %d x %d px\n",
                                scenario_img_w, scenario_img_h);
                        }
                    }
                    else {
                        const char* txt = "Нет картинки";
                        ImVec2 ts = ImGui::CalcTextSize(txt);
                        idl->AddText(ImVec2(ipos.x + (box_sz.x - ts.x) * 0.5f, ipos.y + (box_sz.y - ts.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
                        ImGui::Dummy(box_sz);
                    }

                    if (ImGui::BeginPopup("ViewImgPopup")) {
                        if (scenario_img_tex) ImGui::Image((ImTextureID)(intptr_t)scenario_img_tex, ImVec2((float)scenario_img_w, (float)scenario_img_h));
                        ImGui::EndPopup();
                    }

                    // --- КНОПКИ УПРАВЛЕНИЯ КАРТИНКОЙ ---
                    ImGui::Dummy(ImVec2(0, 3));
                    if (ImGui::Button("Загрузить", ImVec2(90, 24))) {
                        std::string file = OpenLitFileDialog();
                        if (!file.empty()) LoadScenarioImage(file, texMgr, state, scenario_img_tex, scenario_img_w, scenario_img_h);
                    }
                    ImGui::SameLine();
                    if (scenario_img_tex) {
                        if (ImGui::Button("Сохранить", ImVec2(85, 24))) {
                            std::string saveFile = SavePngFileDialog();
                            if (!saveFile.empty()) {
                                std::wstring wPath = std::filesystem::u8path(saveFile).wstring();
                                SaveTextureToPng(scenario_img_tex, scenario_img_w, scenario_img_h, wPath);
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Удалить", ImVec2(80, 24))) {
                            state.scenario_image_data.clear();
                            glDeleteTextures(1, &scenario_img_tex);
                            scenario_img_tex = 0;
                        }
                    }

                    // ПРАВАЯ КОЛОНКА: Галочки
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Герой сохраняет из прошлого сценария:");
                    ImGui::Checkbox("Золотой запас", &state.keep_gold);
                    ImGui::Checkbox("Милость богов", &state.gods_mercy);
                    ImGui::Checkbox("Прославленность", &state.fame);
                    ImGui::Checkbox("Опыт и уровень", &state.exp_and_level);
                    ImGui::Checkbox("Личные артефакты", &state.personal_artifacts);
                    ImGui::Checkbox("Весь свой инвентарь", &state.all_inventory);
                    ImGui::Checkbox("Всю свою армию", &state.all_army);

                    ImGui::EndTable();
                }

                // --- КНОПКИ ВНИЗУ ---
                ImGui::Dummy(ImVec2(0, 15));
                ImGui::SetCursorPosX(560.0f);
                if (ImGui::Button("Сохранить##camp", ImVec2(125, 24))) do_save = true;
                ImGui::SameLine();
                if (ImGui::Button("Выход##camp", ImVec2(125, 24))) do_close = true;

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        // ================= ОБРАБОТКА НАЖАТИЙ КНОПОК =================
        if (do_save) {
            map.map_name = utf8_to_cp1251(state.name);
            map.map_desc = utf8_to_cp1251(UnwrapString(state.desc));
            map.campaign_name = utf8_to_cp1251(state.campaign);
            map.next_scenario = utf8_to_cp1251(state.next_map);

            map.victory_event_id = static_cast<uint16_t>(state.victory_event);
            map.defeat_event_id = static_cast<uint16_t>(state.defeat_event);
            map.start_date_min = state.start_date_min;

            map.knight = state.knight;
            map.archmage = state.archmage;
            map.ranger = state.ranger;

            memcpy(map.relations, state.relations, 16);

            map.scenario_type = static_cast<uint8_t>(state.scenario_type);
            map.scenario_icon_id = static_cast<uint8_t>(state.scenario_icon_id);
            map.keep_gold = state.keep_gold;
            map.gods_mercy = state.gods_mercy;
            map.fame = state.fame;
            map.exp_and_level = state.exp_and_level;
            map.personal_artifacts = state.personal_artifacts;
            map.all_inventory = state.all_inventory;
            map.all_army = state.all_army;

            // Сохраняем картинку и её размер
            map.scenario_image_data = state.scenario_image_data;
            map.added_image_size = static_cast<uint32_t>(state.scenario_image_data.size());

            if (scenario_img_tex) {
                glDeleteTextures(1, &scenario_img_tex);
                scenario_img_tex = 0;
            }

            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
        }
        else if (do_close) {
            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
        }

        ImGui::EndPopup();
    }
    // === ДОБАВИТЬ ЭТУ СТРОКУ СРАЗУ ПОСЛЕ ЗАКРЫВАЮЩЕЙ СКОБКИ if (BeginPopupModal) ===
    ImGui::PopStyleColor(18);
    if (!open_flag) {
        if (scenario_img_tex) {
            glDeleteTextures(1, &scenario_img_tex);
            scenario_img_tex = 0;
        }
        initialized = false;
    }
}