#pragma once

/*
    Font Manager Class
    Handles loading, fetching, freeing & reloading fonts
*/
class FontMgr
{
private:
    struct FontInfo
    {
        ImFont *m_pFont;
        size_t m_nSize;
        float m_fMul;
        std::string m_path;
    };
    static inline std::vector<FontInfo> m_vecFonts;

public:
    FontMgr() = delete;
    FontMgr(FontMgr&) = delete;

    // Returns font pointer from name
    static ImFont* Get(const char* fontName);

    // Loads a font into memory
    static ImFont* Load(const char* fontName, float fontMul = 1.0f);

    // Unloads all the loaded fonts from fontmgr
    // ImGui::GetIO().Default font must be loaded after unloading all fonts
    static void UnloadAll();

    // Reloads all the fonts 
    static void ReloadAll();
};


