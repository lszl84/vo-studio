#pragma once

#include <wx/string.h>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

struct AudioClip
{
    wxString filename;     // e.g. "001.wav"
    double duration = 0.0;
    double lufs = 0.0;
    double peakDb = -96.0;
    bool analyzed = false;
};

class Workspace
{
public:
    bool Open(const fs::path& dir);
    bool Create(const fs::path& dir);
    void Close();
    bool IsOpen() const { return !dir_.empty(); }

    // Script
    wxString GetScriptText() const { return scriptText_; }
    void SetScriptText(const wxString& text);
    void ImportScript(const wxString& srcPath);

    // Clips
    const std::vector<AudioClip>& GetClips() const { return clips_; }
    std::vector<AudioClip>& GetClips() { return clips_; }
    wxString GetNextClipPath();
    wxString GetClipFullPath(const wxString& clipName) const;
    void ScanClips();
    void UpdateClipAnalysis(size_t index, double lufs, double peakDb, double duration);
    void RemoveClip(size_t index);

    // Settings
    int GetTextSize() const { return textSize_; }
    void SetTextSize(int size);

    const fs::path& GetDirectory() const { return dir_; }

private:
    void LoadScript();
    void SaveScript();
    void LoadMetadata();
    void SaveMetadata();

    fs::path dir_;
    wxString scriptText_;
    std::vector<AudioClip> clips_;
    int textSize_ = 14;
};
