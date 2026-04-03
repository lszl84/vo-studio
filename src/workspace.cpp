#include "workspace.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>

using json = nlohmann::json;

bool Workspace::Create(const fs::path& dir)
{
    if (!fs::exists(dir))
        fs::create_directories(dir);
    return Open(dir);
}

bool Workspace::Open(const fs::path& dir)
{
    if (!fs::exists(dir) || !fs::is_directory(dir))
        return false;

    dir_ = dir;
    clips_.clear();
    scriptText_.clear();
    textSize_ = 14;

    ScanClips();
    LoadMetadata();
    LoadScript();

    return true;
}

void Workspace::Close()
{
    if (!dir_.empty())
    {
        SaveScript();
        SaveMetadata();
    }
    dir_.clear();
    clips_.clear();
    scriptText_.clear();
}

void Workspace::ScanClips()
{
    clips_.clear();
    if (dir_.empty())
        return;

    std::regex pattern("^\\d{3}\\.wav$");

    for (const auto& entry : fs::directory_iterator(dir_))
    {
        if (!entry.is_regular_file())
            continue;

        std::string name = entry.path().filename().string();
        if (std::regex_match(name, pattern))
        {
            AudioClip clip;
            clip.filename = wxString(name);
            clips_.push_back(clip);
        }
    }

    std::sort(clips_.begin(), clips_.end(),
        [](const AudioClip& a, const AudioClip& b) {
            return a.filename < b.filename;
        });
}

void Workspace::LoadScript()
{
    fs::path scriptPath = dir_ / "script.txt";
    if (!fs::exists(scriptPath))
    {
        scriptText_.clear();
        return;
    }

    std::ifstream file(scriptPath);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    scriptText_ = wxString::FromUTF8(content);
}

void Workspace::SaveScript()
{
    if (dir_.empty())
        return;

    fs::path scriptPath = dir_ / "script.txt";
    std::ofstream file(scriptPath);
    file << scriptText_.ToUTF8();
}

void Workspace::SetScriptText(const wxString& text)
{
    scriptText_ = text;
    SaveScript();
}

void Workspace::ImportScript(const wxString& srcPath)
{
    if (dir_.empty())
        return;

    fs::path dest = dir_ / "script.txt";
    fs::copy_file(fs::path(srcPath.ToStdString()), dest,
                  fs::copy_options::overwrite_existing);
    LoadScript();
}

void Workspace::LoadMetadata()
{
    fs::path metaPath = dir_ / "project.json";
    if (!fs::exists(metaPath))
        return;

    try
    {
        std::ifstream file(metaPath);
        json j = json::parse(file);

        if (j.contains("textSize"))
            textSize_ = j["textSize"].get<int>();

        if (j.contains("clips") && j["clips"].is_object())
        {
            for (auto& clip : clips_)
            {
                std::string name = clip.filename.ToStdString();
                if (j["clips"].contains(name))
                {
                    auto& c = j["clips"][name];
                    if (c.contains("duration"))  clip.duration = c["duration"].get<double>();
                    if (c.contains("lufs"))      clip.lufs = c["lufs"].get<double>();
                    if (c.contains("peakDb"))    clip.peakDb = c["peakDb"].get<double>();
                    if (c.contains("active"))    clip.active = c["active"].get<bool>();
                    clip.analyzed = c.contains("duration");
                }
            }
        }
    }
    catch (const json::exception&) {}
}

void Workspace::SaveMetadata()
{
    if (dir_.empty())
        return;

    json j;
    j["textSize"] = textSize_;

    json clips = json::object();
    for (const auto& clip : clips_)
    {
        json c;
        c["active"] = clip.active;
        if (clip.analyzed)
        {
            c["duration"] = clip.duration;
            c["lufs"] = clip.lufs;
            c["peakDb"] = clip.peakDb;
        }
        clips[clip.filename.ToStdString()] = c;
    }
    j["clips"] = clips;

    fs::path metaPath = dir_ / "project.json";
    std::ofstream file(metaPath);
    file << j.dump(2) << "\n";
}

wxString Workspace::GetNextClipPath()
{
    int maxNum = 0;
    std::regex pattern("^(\\d{3})\\.wav$");

    for (const auto& entry : fs::directory_iterator(dir_))
    {
        std::string name = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(name, match, pattern))
        {
            int num = std::stoi(match[1]);
            if (num > maxNum)
                maxNum = num;
        }
    }

    int nextNum = maxNum + 1;
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << nextNum;

    return wxString((dir_ / (oss.str() + ".wav")).string());
}

wxString Workspace::GetClipFullPath(const wxString& clipName) const
{
    return wxString((dir_ / clipName.ToStdString()).string());
}

void Workspace::ToggleClipActive(size_t index)
{
    if (index < clips_.size())
    {
        clips_[index].active = !clips_[index].active;
        SaveMetadata();
    }
}

void Workspace::UpdateClipAnalysis(size_t index, double lufs, double peakDb, double duration)
{
    if (index < clips_.size())
    {
        clips_[index].lufs = lufs;
        clips_[index].peakDb = peakDb;
        clips_[index].duration = duration;
        clips_[index].analyzed = true;
        SaveMetadata();
    }
}

void Workspace::RemoveClip(size_t index)
{
    if (index < clips_.size())
    {
        fs::path clipPath = dir_ / clips_[index].filename.ToStdString();
        if (fs::exists(clipPath))
            fs::remove(clipPath);

        clips_.erase(clips_.begin() + index);
        SaveMetadata();
    }
}

void Workspace::SetTextSize(int size)
{
    textSize_ = size;
    SaveMetadata();
}
