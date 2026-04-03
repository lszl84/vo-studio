#include "project_document.h"
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

wxIMPLEMENT_DYNAMIC_CLASS(ProjectDocument, wxDocument);

std::ostream& ProjectDocument::SaveObject(std::ostream& stream)
{
    // Save as simple text format: script followed by clip list
    stream << "#VO_STUDIO_PROJECT_V1\n";
    stream << "TEXT_SIZE: " << textSize << "\n";
    stream << "NEXT_CLIP: " << nextClipNumber << "\n";
    stream << "SCRIPT_BEGIN\n";
    stream << scriptText.ToStdString() << "\n";
    stream << "SCRIPT_END\n";
    stream << "CLIPS: " << audioClips.size() << "\n";
    for (const auto& clip : audioClips)
    {
        stream << clip.filename.ToStdString() << "\n";
        stream << std::fixed << std::setprecision(6);
        stream << clip.duration << " " << clip.lufs << " " << clip.peakDb << "\n";
    }
    return stream;
}

std::istream& ProjectDocument::LoadObject(std::istream& stream)
{
    std::string line;
    std::getline(stream, line);
    if (line != "#VO_STUDIO_PROJECT_V1")
    {
        // Try to load as plain text for backward compatibility
        scriptText = wxString(line) + "\n";
        while (std::getline(stream, line))
        {
            scriptText += wxString(line) + "\n";
        }
        Modify(true);
        return stream;
    }
    
    // Parse header
    while (std::getline(stream, line))
    {
        if (line.substr(0, 10) == "TEXT_SIZE:")
        {
            textSize = std::stoi(line.substr(11));
        }
        else if (line.substr(0, 10) == "NEXT_CLIP:")
        {
            nextClipNumber = std::stoi(line.substr(11));
        }
        else if (line == "SCRIPT_BEGIN")
        {
            scriptText.clear();
            while (std::getline(stream, line))
            {
                if (line == "SCRIPT_END")
                    break;
                scriptText += wxString(line) + "\n";
            }
        }
        else if (line.substr(0, 7) == "CLIPS: ")
        {
            size_t numClips = std::stoul(line.substr(7));
            audioClips.clear();
            for (size_t i = 0; i < numClips && std::getline(stream, line); ++i)
            {
                AudioClip clip;
                clip.filename = wxString(line);
                clip.displayName = wxString(fs::path(line).filename().string());
                
                if (std::getline(stream, line))
                {
                    std::istringstream iss(line);
                    iss >> clip.duration >> clip.lufs >> clip.peakDb;
                    clip.analyzed = true;
                }
                audioClips.push_back(clip);
                
                // Update next clip number based on existing clips
                std::string fname = fs::path(clip.filename.ToStdString()).stem().string();
                if (fname.length() == 3 && std::isdigit(fname[0]) && std::isdigit(fname[1]) && std::isdigit(fname[2]))
                {
                    int num = std::stoi(fname);
                    if (num >= nextClipNumber)
                        nextClipNumber = num + 1;
                }
            }
        }
    }
    
    // Set project directory from document path
    wxString docPath = GetFilename();
    if (!docPath.IsEmpty())
    {
        projectDir = fs::path(docPath.ToStdString()).parent_path();
    }
    
    Modify(false);
    return stream;
}

wxString ProjectDocument::GetNextClipFilename()
{
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << nextClipNumber;
    wxString filename = wxString(oss.str()) + ".wav";
    nextClipNumber++;
    Modify(true);
    return filename;
}

void ProjectDocument::AddAudioClip(const wxString& filename)
{
    AudioClip clip;
    clip.filename = filename;
    clip.displayName = wxString(fs::path(filename.ToStdString()).filename().string());
    clip.analyzed = false;
    audioClips.push_back(clip);
    Modify(true);
}

void ProjectDocument::RemoveAudioClip(size_t index)
{
    if (index < audioClips.size())
    {
        audioClips.erase(audioClips.begin() + index);
        Modify(true);
    }
}

void ProjectDocument::UpdateClipAnalysis(size_t index, double lufs, double peakDb, double duration)
{
    if (index < audioClips.size())
    {
        audioClips[index].lufs = lufs;
        audioClips[index].peakDb = peakDb;
        audioClips[index].duration = duration;
        audioClips[index].analyzed = true;
        Modify(true);
    }
}
