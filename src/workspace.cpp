#include "workspace.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>

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
    fs::path metaPath = dir_ / ".vostudio";
    if (!fs::exists(metaPath))
        return;

    std::ifstream file(metaPath);
    std::string line;

    while (std::getline(file, line))
    {
        if (line.substr(0, 10) == "TEXT_SIZE: ")
        {
            textSize_ = std::stoi(line.substr(10));
        }
        else if (line.substr(0, 6) == "CLIP: ")
        {
            std::istringstream iss(line.substr(6));
            std::string clipName;
            double duration, lufs, peakDb;
            if (iss >> clipName >> duration >> lufs >> peakDb)
            {
                for (auto& clip : clips_)
                {
                    if (clip.filename == wxString(clipName))
                    {
                        clip.duration = duration;
                        clip.lufs = lufs;
                        clip.peakDb = peakDb;
                        clip.analyzed = true;
                        break;
                    }
                }
            }
        }
    }
}

void Workspace::SaveMetadata()
{
    if (dir_.empty())
        return;

    fs::path metaPath = dir_ / ".vostudio";
    std::ofstream file(metaPath);

    file << "TEXT_SIZE: " << textSize_ << "\n";
    for (const auto& clip : clips_)
    {
        if (clip.analyzed)
        {
            file << "CLIP: " << clip.filename.ToStdString()
                 << std::fixed << std::setprecision(6)
                 << " " << clip.duration
                 << " " << clip.lufs
                 << " " << clip.peakDb << "\n";
        }
    }
}

wxString Workspace::GetNextClipPath()
{
    int maxNum = 0;
    std::regex pattern("^(\\d{3})\\.wav$");

    // Scan directory directly for the most accurate count
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
