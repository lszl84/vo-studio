#pragma once

#include <wx/docview.h>
#include <wx/string.h>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

struct AudioClip
{
    wxString filename;
    wxString displayName;
    double duration = 0.0;
    double lufs = 0.0;
    double peakDb = -96.0;
    bool analyzed = false;
};

class ProjectDocument : public wxDocument
{
public:
    std::ostream& SaveObject(std::ostream& stream) override;
    std::istream& LoadObject(std::istream& stream) override;

    // Project data
    wxString scriptText;
    std::vector<AudioClip> audioClips;
    fs::path projectDir;
    int nextClipNumber = 1;
    
    // Text settings
    int textSize = 14;
    
    // Methods
    wxString GetNextClipFilename();
    void AddAudioClip(const wxString& filename);
    void RemoveAudioClip(size_t index);
    void UpdateClipAnalysis(size_t index, double lufs, double peakDb, double duration);
    
    wxDECLARE_DYNAMIC_CLASS(ProjectDocument);
};
